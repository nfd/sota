#include <math.h>

#include "graphics.h"

static int scale_x, scale_y;
static int global_scale;
static int bitplane_width, bitplane_height; // copied from graphics

void background_init()
{
	scale_x = graphics_width() / 256;
	scale_y = graphics_height() / 256;
	global_scale = scale_x > scale_y? scale_x: scale_y;

	bitplane_width = graphics_bitplane_width();
	bitplane_height = graphics_bitplane_height();
}

void background_init_concentric_circles() {
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

void background_concentric_circles_tick(int cnt) {

	// Paths:
	// 1 trails from bottom left all over the screen.
	// 2 moves from bottom left to top left and back.
	//
	// We do everything on a nominal 256x256 scale and then scale up.

	graphics_bitplane_set_offset(2, (30 * scale_x), (128 + (128 * sinf(1.0 + ((float)cnt) / 30))) * scale_y);

	graphics_bitplane_set_offset(1, (128 + (128 * sinf(((float)cnt) / 20))) * scale_x,
			(128 + (128 * sinf(((float)cnt) / 50))) * scale_y);
}

