#define _DEFAULT_SOURCE
#include <math.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "graphics.h"
#include "iff.h"
#include "font.h"
#include "backend.h"

extern struct backend_interface_struct g_backend;

#define VOTEVOTEVOTE_DISPLAY_MS 40

static int scale_x, scale_y;
static int global_scale;

static int votevotevote_last_palette = 0;
static int votevotevote_last_ms;
static int votevotevote_topline_count, votevotevote_midline_count, votevotevote_botline_count; // # texts available for each line
static int votevotevote_top, votevotevote_mid, votevotevote_bot; // the previous choice
static uint32_t *votevotevote_palette_a, *votevotevote_palette_b;

static struct Bitplane bitplanes[2];

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

static void precalculate_pastels_brush() {
	/* Precalculate the pastels brush, which is a filled circle with intensity
	 * a function of distance from the centre.
	*/
	int centrex = COPPER_PASTELS_HEIGHT / 2;
	int centrey = COPPER_PASTELS_WIDTH / 2;

	float max_distance = sqrtf((centrex * centrex) + (centrey * centrey));

	for(int y = 0; y < COPPER_PASTELS_HEIGHT; y++) {
		for(int x = 0; x < COPPER_PASTELS_WIDTH; x++) {
			int idx = (y * COPPER_PASTELS_WIDTH) + x;

			float distance = sqrtf(SQUARE(abs(x - centrex)) + SQUARE(abs(y - centrey)));

			pastels_brush[idx] = 255 - ((distance / max_distance) * 256);
		}
	}
}

void background_init()
{
	scale_x = g_backend.width / 256;
	scale_y = g_backend.height / 256;
	global_scale = scale_x > scale_y? scale_x: scale_y;

	precalculate_pastels_brush();
}

bool background_init_spotlights() {
	// spot 0 trails all over the display
	bitplanes[0].width = g_backend.width * 2;
	bitplanes[0].height = g_backend.height * 2;
	bitplanes[0].stride = bitplanes[0].width / 8;
	bitplanes[0].offsetx = bitplanes[0].offsety = 0;
	bitplanes[0].idx = 1;
	bitplanes[0].data = g_backend.malloc(bitplanes[0].width * bitplanes[0].height / 8);
	if(bitplanes[0].data == NULL) 
		return false;

	// spot 1 moves up and down, reusing the same data.
	bitplanes[1].width = bitplanes[0].width;
	bitplanes[1].height = bitplanes[0].height;
	bitplanes[1].stride = bitplanes[0].stride;
	bitplanes[1].offsetx = bitplanes[1].offsety = 0;
	bitplanes[1].idx = 2;
	bitplanes[1].data = bitplanes[0].data;

	/* Scale backgrounds sensibly */
	int thickness = 4 * global_scale;
	int gap = (2 * thickness) / 3;

	int half_bitplane_width = g_backend.width / 2;
	int half_bitplane_height = g_backend.height / 2;

	int longest_distance = sqrtf(half_bitplane_width * half_bitplane_width + half_bitplane_height * half_bitplane_height);

	for(int radius = thickness; radius < longest_distance; radius+= (thickness + gap)) {
		draw_thick_circle(bitplanes[0].data, half_bitplane_width, half_bitplane_height, radius, thickness);
	}

	return true;
}

void background_deinit_spotlights() {
	g_backend.free(bitplanes[0].data);
	g_backend.free(bitplanes[1].data);
}

void background_spotlights_tick(int cnt) {

	// Paths:
	// 1 trails from bottom left all over the screen.
	// 2 moves from bottom left to top left and back.
	//
	// We do everything on a nominal 256x256 scale and then scale up.
	bitplanes[0].offsetx = (128 + (128 * sinf(((float)cnt) / 800))) * scale_x;
	bitplanes[0].offsety = (128 + (128 * sinf(((float)cnt) / 2000))) * scale_y;

	bitplanes[1].offsetx = (30 * scale_x);
	bitplanes[1].offsety = (128 + (128 * sinf(1.0 + ((float)cnt) / 1200))) * scale_y;

	g_backend->blit(g_backend.width, g_backend.height, BLIT_MODE_AND, 2, bitplanes);
}

void background_init_votevotevote(void *effect_data, uint32_t *palette_a, uint32_t *palette_b)
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

void background_deinit_votevotevote()
{
	// nothing to do
}

char *find_text_for_idx(int idx, int *length_out)
{
	int offset = current_text_block->entries[idx];
	if(length_out)
		*length_out = current_text_block->entries[idx + 1] - offset;

	return ((char *)current_text_block->entries) + offset;
}

void background_votevotevote_tick(int ms)
{
	if(votevotevote_last_ms + VOTEVOTEVOTE_DISPLAY_MS > ms)
		return;

	int top_text_idx = random() % votevotevote_topline_count;
	int mid_text_idx = random() % votevotevote_midline_count;
	int bot_text_idx = random() % votevotevote_botline_count;

	// ensure we don't pick the same thing twice.
	if(top_text_idx == votevotevote_top) 
		top_text_idx = (top_text_idx + 1) % votevotevote_topline_count;

	if(mid_text_idx == votevotevote_mid) 
		mid_text_idx = (mid_text_idx + 1) % votevotevote_midline_count;

	if(bot_text_idx == votevotevote_bot) 
		bot_text_idx = (bot_text_idx + 1) % votevotevote_botline_count;

	int font_height = font_get_height();
	int text_length;
	char *text;

	if(votevotevote_last_palette == 1)
		graphics_set_palette(32, votevotevote_palette_a);
	else
		graphics_set_palette(32, votevotevote_palette_b);

	votevotevote_last_palette = 1 - votevotevote_last_palette;

	graphics_clear_visible();

	int y = ((graphics_height() / 2) - (font_height / 2)) - font_height;

	text = find_text_for_idx(top_text_idx, &text_length);
	font_centre(text_length, text, y);

	y += font_height;

	text = find_text_for_idx(mid_text_idx + votevotevote_topline_count, &text_length);
	font_centre(text_length, text, y);

	y += font_height;

	text = find_text_for_idx(bot_text_idx + votevotevote_topline_count + votevotevote_midline_count, &text_length);
	font_centre(text_length, text, y);
	
	votevotevote_last_ms = ms;
	votevotevote_top = top_text_idx;
	votevotevote_mid = mid_text_idx;
	votevotevote_bot = bot_text_idx;
}


/* Delayed blit: every N ms, blit blitplane n over bitplane n + 1 */
void background_init_delayedblit()
{
	delayed_blit_delay = 80;
	delayed_blit_next_blit = 0;
	bitplane_width = graphics_bitplane_width();
	bitplane_height = graphics_bitplane_height();
}

void background_delayedblit_tick(int ms)
{
	if(ms < delayed_blit_next_blit)
		return;

	for(int plane_idx = 4; plane_idx > 0; plane_idx--)
		graphics_bitplane_blit(plane_idx - 1, plane_idx, 0, 0, bitplane_width, bitplane_height, 0, 0);

	delayed_blit_next_blit += delayed_blit_delay;
}

void background_deinit_delayedblit()
{
}


/* Copper effects: x is between 0 and 40 (every 8 pixels on a 320-width display), y is between 0 and 256. */
static void background_copperpastels_do_copper_effect(int x, int y, uint32_t *palette)
{
	x = (x * COPPER_PASTELS_WIDTH) / 40;
	y = (y * COPPER_PASTELS_HEIGHT) / 256;

	palette[0] = copperpastels.colours[(y * COPPER_PASTELS_WIDTH) + x];
}

void background_init_copperpastels()
{
	/* We have colour spotlights, yellow, blue, green, and red, which take turns entering and exiting the display.
	 * The background is a blend between these spotlights and white (the default colour).
	 *
	 * We only ever show two spotlights at once.
	 * 
	 * The spotlights are very granular, so we can calculate them once (e.g. on a 16x18 grid)
	 * and then just reference that grid when blitting. We can update the grid less frequently,
	 * say five times a second.
	*/
	graphics_copper_effect_register(&background_copperpastels_do_copper_effect);
	copperpastels.next_update_ms = 0;
}

void background_copperpastels_tick(int ms)
{
	if(ms >= copperpastels.next_update_ms) {
		memset(copperpastels.colours, 0xff, COPPER_PASTELS_WIDTH * COPPER_PASTELS_HEIGHT * sizeof(uint32_t));

		copperpastels.next_update_ms = ms + COPPER_PASTELS_UPDATE_MS;
	}
}

void background_deinit_copperpastels()
{
	graphics_copper_effect_unregister();
}


