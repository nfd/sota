#define _DEFAULT_SOURCE
#include <inttypes.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "heap.h"
#include "graphics.h"
#include "iff.h"
#include "backend.h"
#include "scene.h"
#include "minmax.h"
#include "pcgrandom.h"

#define VOTEVOTEVOTE_DISPLAY_MS 60

#define COPPER_PASTELS_SWITCH_SPEED_MS 1600
#define COPPER_PASTELS_WIDTH 16
#define COPPER_PASTELS_HEIGHT 18

#define STATIC_SWITCH_SPEED_TICKS 2

struct pastel {
	uint8_t tl, tr, bl, br;
};

struct copperpastels_effect_data_struct {
	uint32_t num_pastels;
	struct pastel pastels[]; // * num_pastels
};


static float scale_x, scale_y;
static float global_scale;

static int votevotevote_last_palette = 0;
static int votevotevote_last_ms;
static int votevotevote_topline_count, votevotevote_midline_count, votevotevote_botline_count; // # texts available for each line
static int votevotevote_top, votevotevote_mid, votevotevote_bot; // the previous choice
static uint32_t *votevotevote_palette_a, *votevotevote_palette_b;

int delayed_blit_next_blit = 0; // ms
int delayed_blit_delay = 40; // ms

static int copperpastels_ms_start;
static struct copperpastels_effect_data_struct *g_copperpastels_effect_data;

// Static effects
#define STATIC_EFFECT_CIRCLES_AND_BLITTER_COPY 1
static int static_ticks_count;
static int static_effect_num;
static pcg32_random_t static_rngstate;

struct {
	uint32_t block_length;
	uint32_t num_entries;
	uint16_t entries[];
} *current_text_block;

struct {
	uint32_t colours[COPPER_PASTELS_WIDTH * COPPER_PASTELS_HEIGHT]; // precalculated
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

void scene_init()
{
	scale_x = (float)window_width / 256;
	scale_y = (float)window_height / 256;
	global_scale = scale_x > scale_y? scale_x: scale_y;
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

static void delayedblit_do_copy(int top, int bottom)
{
	for(int plane_idx = top; plane_idx > bottom; plane_idx--)
		backend_copy_bitplane(&backend_bitplane[plane_idx], &backend_bitplane[plane_idx - 1]);
}

void scene_delayedblit_tick(int ms)
{
	if(ms < delayed_blit_next_blit)
		return;


	delayedblit_do_copy(4, 0);

	delayed_blit_next_blit += delayed_blit_delay;
}

void scene_deinit_delayedblit()
{
}


static inline void copperpastels_set(int x, int y, uint32_t colour)
{
	copperpastels.colours[(y * COPPER_PASTELS_WIDTH) + x] = colour;
}

static inline uint32_t copperpastels_get(int x, int y)
{
	return copperpastels.colours[(y * COPPER_PASTELS_WIDTH) + x];
}

static inline uint8_t interpolate_one(uint8_t from, uint8_t to, float amt)
{
	if(amt > 1.0 || amt < 0.0) {
		backend_debug("terp fail: %f\n", amt);
		return 0;
	}

	float total = to - from;
	total *= amt;

	return from + total;
}

static uint32_t copperpastels_interpolate(uint32_t from, uint32_t to, float amt)
{
	return  0xff000000
		| interpolate_one((from & 0xff0000) >> 16, (to & 0xff0000) >> 16, amt) << 16
		| interpolate_one((from & 0xff00) >> 8, (to & 0xff00) >> 8, amt) << 8
		| interpolate_one((from & 0xff), (to & 0xff), amt);
}

static uint32_t copperpastels_code_to_colour(uint8_t code)
{
	switch(code) {
		case 'W':
			return 0xffffffff;
		case 'R':
			return 0xffff0000;
		case 'G':
			return 0xff00ff00;
		case 'B':
			return 0xff0000ff;
		case 'Y':
			return 0xffffff00;
		case 'X':
			return 0xff000000;
		default:
			return 0xffbadc01;
	}
}

static uint32_t copperpastels_interpolate_corner(uint8_t first, uint8_t second, float amt)
{
	return copperpastels_interpolate(
			copperpastels_code_to_colour(first),
			copperpastels_code_to_colour(second),
			amt);
}

static void copperpastels_set_corners(int ms) {
	int total_effect_time_ms = COPPER_PASTELS_SWITCH_SPEED_MS * g_copperpastels_effect_data->num_pastels;
	int ms_into_effect = (ms - copperpastels_ms_start) % total_effect_time_ms;
	float amt = ((float)ms_into_effect) / COPPER_PASTELS_SWITCH_SPEED_MS;
	int first_index = (int)amt;
	amt -= first_index;

	int second_index = first_index + 1;
	if(second_index == g_copperpastels_effect_data->num_pastels)
		second_index = 0;

	struct pastel *first = &g_copperpastels_effect_data->pastels[first_index];
	struct pastel *second = &g_copperpastels_effect_data->pastels[second_index];

	copperpastels_set(0, 0, copperpastels_interpolate_corner(first->tl, second->tl, amt));
	copperpastels_set(COPPER_PASTELS_WIDTH - 1, 0, copperpastels_interpolate_corner(first->tr, second->tr, amt));
	copperpastels_set(COPPER_PASTELS_WIDTH - 1, COPPER_PASTELS_HEIGHT - 1, copperpastels_interpolate_corner(first->br, second->br, amt));
	copperpastels_set(0, COPPER_PASTELS_HEIGHT - 1, copperpastels_interpolate_corner(first->bl, second->bl, amt));
}

/* Copper effects: x is between 0 and 40 (every 8 pixels on a 320-width display), y is between 0 and 256. */
static void scene_copperpastels_do_copper_effect(int x, int y, uint32_t *palette)
{
	// palette[0] = x % 2  ?0xffffffff: 0xff00ffff;
	y = y * COPPER_PASTELS_HEIGHT / 256;
	x = x * COPPER_PASTELS_WIDTH / 40;
	palette[0] = copperpastels_get(x, y);
}

void scene_init_copperpastels(int ms, void *v_data)
{
	struct copperpastels_effect_data_struct *effect_data_in = v_data;
	g_copperpastels_effect_data = heap_alloc(
			sizeof(struct copperpastels_effect_data_struct)
			+ (sizeof(struct pastel) * effect_data_in->num_pastels));

	g_copperpastels_effect_data->num_pastels = effect_data_in->num_pastels;
	memcpy(g_copperpastels_effect_data->pastels, effect_data_in->pastels, sizeof(struct pastel) * effect_data_in->num_pastels);

	backend_register_blitter_func(scene_copperpastels_do_copper_effect);
	copperpastels_ms_start = ms;
}

void scene_copperpastels_tick(int ms)
{
	copperpastels_set_corners(ms);

	/* Do interpolation: */
	/* ... interpolate across top and bottom... */
	for(int x = 1; x < COPPER_PASTELS_WIDTH - 1; x++) {
		copperpastels_set(x, 0,
				copperpastels_interpolate(
					copperpastels_get(0, 0),
					copperpastels_get(COPPER_PASTELS_WIDTH - 1, 0),
					(float)x / COPPER_PASTELS_WIDTH));
		copperpastels_set(x, COPPER_PASTELS_HEIGHT - 1,
				copperpastels_interpolate(
					copperpastels_get(0, COPPER_PASTELS_HEIGHT - 1),
					copperpastels_get(COPPER_PASTELS_WIDTH - 1, COPPER_PASTELS_HEIGHT - 1),
					(float)x / COPPER_PASTELS_WIDTH));
	}

	/* ... use top and bottom to interpolate down */
	for(int y = 1; y < COPPER_PASTELS_HEIGHT - 1; y++) {
		for(int x = 0; x < COPPER_PASTELS_WIDTH; x++) {
			copperpastels_set(x, y,
					copperpastels_interpolate(copperpastels_get(x, 0),
						copperpastels_get(x, COPPER_PASTELS_HEIGHT - 1),
						(float)y / COPPER_PASTELS_HEIGHT));
		}
	}
}

void scene_deinit_copperpastels()
{
	backend_register_blitter_func(NULL);
}

/* TODO: fitting this into memory constraints -- we can do font at 4 bitplanes, buuut... 
 * ... it is just random bits plus some circle parts, so maybe regenerating it at each
 * change (every 80 ms) is okay */
void scene_init_static(int ms, void *data)
{
	static_ticks_count = 0;
	static_rngstate.state = 0x57a7e0fa27ULL;

	int *effect_num = (int *)data;
	static_effect_num = *effect_num;
}

/* In terms of global_scale which is based on a 256x256 display.
 * Each frame has two circle groups. */
static const int16_t static_circles_radii[] = {
	25, 64,
	50, 88,
	75, 115,
	105, 135};

static const int static_num_frames = 4; // fair dice roll

#define STATIC_BITPLANE_NUM 0

void scene_static_tick(int ms)
{
	uint32_t random;
	int bytes_avail;

	if(static_ticks_count % STATIC_SWITCH_SPEED_TICKS == 0) {
		int frame = (static_ticks_count / STATIC_SWITCH_SPEED_TICKS) % static_num_frames;

		static_rngstate.inc = 37 + frame;
		random = pcg32_random_r(&static_rngstate);
		bytes_avail = 4;

		uint8_t *ptr = backend_bitplane[STATIC_BITPLANE_NUM].data;

		int radii_idx = frame * 2;

		for(int y = 0; y < backend_bitplane[STATIC_BITPLANE_NUM].height; y++) {
			for(int x = 0; x < backend_bitplane[STATIC_BITPLANE_NUM].width / 8; x++) {
				ptr[x] = random & 0xff;
				bytes_avail --;
				if(!bytes_avail) {
					random = pcg32_random_r(&static_rngstate);
					bytes_avail = 4;
				} else {
					random >>= 8;
				}
			}

			ptr += backend_bitplane[STATIC_BITPLANE_NUM].stride;
		}

		switch(static_effect_num) {
			case STATIC_EFFECT_CIRCLES_AND_BLITTER_COPY: {
				// Several circles clustered around the midpoint.
				//planar_draw_thick_circle(&backend_bitplane[0], window_width / 2, window_height / 2, 25, 2);
				struct Bitplane *plane = &backend_bitplane[STATIC_BITPLANE_NUM];

				for(int max_idx = radii_idx + 2; radii_idx < max_idx; radii_idx ++) {
					int radius = static_circles_radii[radii_idx] * global_scale;
					planar_circle(plane, window_width / 2, window_height / 2, radius);
					planar_circle(plane, 4 * global_scale + window_width / 2, window_height / 2, radius);
					planar_circle(plane, window_width / 2, 4 * global_scale + window_height / 2, radius);
					planar_circle(plane, 2 * global_scale + window_width / 2, 2 * global_scale + window_height / 2, radius);
				}

				delayedblit_do_copy(3, 1);
				break;
			}
		}
	}
	static_ticks_count += 1;
}

void scene_deinit_static()
{
}

