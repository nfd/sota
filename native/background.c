#define _DEFAULT_SOURCE
#include <math.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

#include "graphics.h"
#include "iff.h"
#include "font.h"

#define VOTEVOTEVOTE_DISPLAY_MS 40

static int scale_x, scale_y;
static int global_scale;
static int bitplane_width, bitplane_height; // copied from graphics

int votevotevote_last_palette = 0;
int votevotevote_last_ms;
int votevotevote_topline_count, votevotevote_midline_count, votevotevote_botline_count; // # texts available for each line
int votevotevote_top, votevotevote_mid, votevotevote_bot; // the previous choice
uint32_t *votevotevote_palette_a, *votevotevote_palette_b;

int delayed_blit_next_blit = 0; // ms
int delayed_blit_delay = 40; // ms

struct {
	uint32_t block_length;
	uint32_t num_entries;
	uint16_t entries[];
} *current_text_block;

void background_init()
{
	scale_x = graphics_width() / 256;
	scale_y = graphics_height() / 256;
	global_scale = scale_x > scale_y? scale_x: scale_y;

	bitplane_width = graphics_bitplane_width();
	bitplane_height = graphics_bitplane_height();
}

void background_init_spotlights() {
	/* Scale backgrounds sensibly */
	int thickness = 4 * global_scale;
	int gap = (2 * thickness) / 3;

	int half_bitplane_width = graphics_bitplane_width() / 2;
	int half_bitplane_height = graphics_bitplane_height() / 2;

	int longest_distance = sqrtf(half_bitplane_width * half_bitplane_width + half_bitplane_height * half_bitplane_height);

	for(int radius = thickness; radius < longest_distance; radius+= (thickness + gap)) {
		draw_thick_circle(half_bitplane_width, half_bitplane_height, radius, thickness, 1);
		draw_thick_circle(half_bitplane_width, half_bitplane_height, radius, thickness, 2);
	}
}

void background_deinit_spotlights() {
	graphics_bitplane_set_offset(1, 0, 0);
	graphics_bitplane_set_offset(2, 0, 0);
}

void background_spotlights_tick(int cnt) {

	// Paths:
	// 1 trails from bottom left all over the screen.
	// 2 moves from bottom left to top left and back.
	//
	// We do everything on a nominal 256x256 scale and then scale up.

	graphics_bitplane_set_offset(2, (30 * scale_x), (128 + (128 * sinf(1.0 + ((float)cnt) / 1200))) * scale_y);

	graphics_bitplane_set_offset(1, (128 + (128 * sinf(((float)cnt) / 800))) * scale_x,
			(128 + (128 * sinf(((float)cnt) / 2000))) * scale_y);
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
	int mid_text_idx = (random() % votevotevote_midline_count);
	int bot_text_idx = (random() % votevotevote_botline_count);

	// ensure we don't pick the same thing twice.
	if(top_text_idx == votevotevote_top) 
		top_text_idx = (top_text_idx + 1) % votevotevote_topline_count;

	if(mid_text_idx == votevotevote_mid) 
		mid_text_idx = (mid_text_idx + 1) % votevotevote_midline_count;

	if(bot_text_idx == votevotevote_bot) 
		bot_text_idx = (bot_text_idx + 1) % votevotevote_botline_count;

	mid_text_idx += votevotevote_topline_count;
	bot_text_idx += votevotevote_topline_count + votevotevote_midline_count;

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

	text = find_text_for_idx(mid_text_idx, &text_length);
	font_centre(text_length, text, y);

	y += font_height;

	text = find_text_for_idx(bot_text_idx, &text_length);
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


