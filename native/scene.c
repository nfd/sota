#define _DEFAULT_SOURCE
#include <inttypes.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "graphics.h"
#include "iff.h"
#include "backend.h"
#include "scene.h"
#include "minmax.h"

#define VOTEVOTEVOTE_DISPLAY_MS 60

static float scale_x, scale_y;
static float global_scale;

static int votevotevote_last_palette = 0;
static int votevotevote_last_ms;
static int votevotevote_topline_count, votevotevote_midline_count, votevotevote_botline_count; // # texts available for each line
static int votevotevote_top, votevotevote_mid, votevotevote_bot; // the previous choice
static uint32_t *votevotevote_palette_a, *votevotevote_palette_b;

int delayed_blit_next_blit = 0; // ms
int delayed_blit_delay = 40; // ms

struct {
	uint32_t block_length;
	uint32_t num_entries;
	uint16_t entries[];
} *current_text_block;

#define COPPER_PASTELS_UPDATE_MS 200
#define COPPER_PASTELS_WIDTH 16
#define COPPER_PASTELS_HEIGHT 18
#define COPPER_PASTELS_NUM_SPOTS 2
static uint8_t pastels_brush[COPPER_PASTELS_WIDTH * COPPER_PASTELS_HEIGHT];

struct {
	uint32_t colours[COPPER_PASTELS_HEIGHT * COPPER_PASTELS_WIDTH];
	int next_update_ms;
} copperpastels;

#define SQUARE(x) ((x) * (x))

/* Inaccurate and slow sqrtf replacement because Pebble doesn't have it (working) */
/* Source: http://www.codeproject.com/Articles/69941/Best-Square-Root-Method-Algorithm-Function-Precisi */
float sqrt1(const float x)  
{
	union
	{
		int i;
		float x;
	} u;
	u.x = x;
	u.i = (1<<29) + (u.i >> 1) - (1<<22); 

	// Two Babylonian Steps (simplified from:)
	// u.x = 0.5f * (u.x + x/u.x);
	// u.x = 0.5f * (u.x + x/u.x);
	u.x =       u.x + x/u.x;
	u.x = 0.25f*u.x + x/u.x;

	return u.x;
}  

#define PI 3.1415192

float lame_fmod(float x, float y)
{
	if (y == 0.0) {
		return 0.0;
	}

	float res = x - y * ((int)(x / y));
	return res;
}

float lame_fabs(float x)
{
	return x < 0? -x : x;
}

/* Sinf replacement for the same reason. See http://forum.devmaster.net/t/fast-and-accurate-sine-cosine/9648 */
float dmsin(float x)
{
	const float B = 4/PI;
	const float C = -4/(PI*PI);

	x = lame_fmod(x, 2 * PI);
	int neg = 1;
	if(x > PI) {
		x -= PI;
		neg = -1;
	}

	float y = B * x + C * x * lame_fabs(x);

#ifdef EXTRA_PRECISION
	//  const float Q = 0.775;
	const float P = 0.225;

	y = P * (y * lame_abs(y) - y) + y;   // Q * y + P * y * abs(y)
#endif
	
	return neg * y;
}

static void precalculate_pastels_brush() {
	/* Precalculate the pastels brush, which is a filled circle with intensity
	 * a function of distance from the centre.
	*/
	int centrex = COPPER_PASTELS_HEIGHT / 2;
	int centrey = COPPER_PASTELS_WIDTH / 2;

	float max_distance = sqrt1((centrex * centrex) + (centrey * centrey));

	for(int y = 0; y < COPPER_PASTELS_HEIGHT; y++) {
		for(int x = 0; x < COPPER_PASTELS_WIDTH; x++) {
			int idx = (y * COPPER_PASTELS_WIDTH) + x;

			float distance = sqrt1(SQUARE(abs(x - centrex)) + SQUARE(abs(y - centrey)));

			pastels_brush[idx] = 255 - ((distance / max_distance) * 256);
		}
	}
}

void scene_init()
{
	scale_x = (float)window_width / 256;
	scale_y = (float)window_height / 256;
	global_scale = scale_x > scale_y? scale_x: scale_y;

	precalculate_pastels_brush();
}

bool scene_init_spotlights() {
	// spot 0 trails all over the display -- this is defined in choreography

	// spot 1 moves up and down, reusing the same data as spot 0
	backend_bitplane[2].width = backend_bitplane[1].width;
	backend_bitplane[2].height = backend_bitplane[1].height;
	backend_bitplane[2].stride = backend_bitplane[1].stride;
	backend_bitplane[2].data = backend_bitplane[1].data;
	backend_bitplane[2].data_start = backend_bitplane[1].data_start;

	/* Scale backgrounds sensibly */
	int thickness = max(4 * global_scale, 4);
	int gap = (2 * thickness) / 3;

	int longest_distance = sqrt1(window_width * window_width + window_height * window_height);

	for(int radius = thickness; radius < longest_distance; radius+= (thickness + gap)) {
		planar_draw_thick_circle(&backend_bitplane[1], window_width, window_height, radius, thickness);
	}

	return true;
}

void scene_deinit_spotlights() {
}

//#include <math.h>
void scene_spotlights_tick(int cnt) {

	// Paths:
	// 1 trails from bottom left all over the screen.
	// 2 moves from bottom left to top left and back.
	//
	// We do everything on a nominal 256x256 scale and then scale up.
	int offsetx, offsety;

	offsetx = (128 + (128 * dmsin(((float)cnt) / 800))) * scale_x;
	offsety = (128 + (128 * dmsin(((float)cnt) / 2000))) * scale_y;
	backend_bitplane[1].data = backend_bitplane[1].data_start + (offsety * backend_bitplane[1].stride) + (offsetx / 8);

	offsetx = (30 * scale_x);
	offsety = (128 + (128 * dmsin(1.0 + ((float)cnt) / 1200))) * scale_y;
	backend_bitplane[2].data = backend_bitplane[2].data_start + (offsety * backend_bitplane[2].stride) + (offsetx / 8);
}

void scene_init_votevotevote(void *effect_data, uint32_t *palette_a, uint32_t *palette_b)
{
	votevotevote_last_palette = 0; // light palette
	current_text_block = effect_data;

	votevotevote_last_ms = 0;

	// number of words available for each line: we assume 4 at the bottom and
	// the remainder split evenly between top and mid.
	votevotevote_botline_count = 4; // we just assume
	votevotevote_midline_count = votevotevote_topline_count = (current_text_block->num_entries - 4) / 2;
	votevotevote_top = votevotevote_mid = votevotevote_bot = -1;
	votevotevote_palette_a = palette_a;
	votevotevote_palette_b = palette_b;

	//printf("counts %d %d %d %d\n", votevotevote_topline_count, votevotevote_midline_count, votevotevote_botline_count, current_text_block->num_entries);
}

void scene_deinit_votevotevote()
{
	// nothing to do
}

static char *find_text_for_idx(int idx, int *length_out)
{
	int offset = current_text_block->entries[idx];
	if(length_out)
		*length_out = current_text_block->entries[idx + 1] - offset;

	return ((char *)current_text_block->entries) + offset;
}

void scene_votevotevote_tick(int ms)
{
	if(votevotevote_last_ms + VOTEVOTEVOTE_DISPLAY_MS > ms)
		return;

	// TODO we only need to clear the bounding box.
	for(int i = 0; i < 3; i++) {
		planar_clear(&backend_bitplane[i]);
	}

	int top_text_idx = backend_random() % votevotevote_topline_count;
	int mid_text_idx = backend_random() % votevotevote_midline_count;
	int bot_text_idx = backend_random() % votevotevote_botline_count;

	// ensure we don't pick the same thing twice.
	if(top_text_idx == votevotevote_top) 
		top_text_idx = (top_text_idx + 1) % votevotevote_topline_count;

	if(mid_text_idx == votevotevote_mid) 
		mid_text_idx = (mid_text_idx + 1) % votevotevote_midline_count;

	if(bot_text_idx == votevotevote_bot) 
		bot_text_idx = (bot_text_idx + 1) % votevotevote_botline_count;

	int font_height = backend_font_get_height();
	int text_length;
	char *text;

	if(votevotevote_last_palette == 1)
		backend_set_palette(32, votevotevote_palette_a);
	else
		backend_set_palette(32, votevotevote_palette_b);

	votevotevote_last_palette = 1 - votevotevote_last_palette;

	int y = ((window_height / 2) - (font_height / 2)) - font_height;

	text = find_text_for_idx(top_text_idx, &text_length);
	backend_font_draw(text_length, text, -1, y);

	y += font_height;

	text = find_text_for_idx(mid_text_idx + votevotevote_topline_count, &text_length);
	backend_font_draw(text_length, text, -1, y);

	y += font_height;

	text = find_text_for_idx(bot_text_idx + votevotevote_topline_count + votevotevote_midline_count, &text_length);
	backend_font_draw(text_length, text, -1, y);
	
	votevotevote_last_ms = ms;
	votevotevote_top = top_text_idx;
	votevotevote_mid = mid_text_idx;
	votevotevote_bot = bot_text_idx;
}


/* Delayed blit: every N ms, blit blitplane n over bitplane n + 1 */
void scene_init_delayedblit()
{
	delayed_blit_delay = 80;
	delayed_blit_next_blit = 0;
}

void scene_delayedblit_tick(int ms)
{
	if(ms < delayed_blit_next_blit)
		return;

	for(int plane_idx = 4; plane_idx > 0; plane_idx--)
		backend_copy_bitplane(&backend_bitplane[plane_idx], &backend_bitplane[plane_idx - 1]);

	delayed_blit_next_blit += delayed_blit_delay;
}

void scene_deinit_delayedblit()
{
}


/* Copper effects: x is between 0 and 40 (every 8 pixels on a 320-width display), y is between 0 and 256. */
static void scene_copperpastels_do_copper_effect(int x, int y, uint32_t *palette)
{
	x = (x * COPPER_PASTELS_WIDTH) / 40;
	y = (y * COPPER_PASTELS_HEIGHT) / 256;

	palette[0] = copperpastels.colours[(y * COPPER_PASTELS_WIDTH) + x];
}

void scene_init_copperpastels()
{
	// TODO
}

void scene_copperpastels_tick(int ms)
{
}

void scene_deinit_copperpastels()
{
}


