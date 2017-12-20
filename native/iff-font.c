#include <stdbool.h>
#include <inttypes.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#include "graphics.h"
#include "iff.h"
#include "backend.h"
#include "minmax.h"

/* only use the first 3 bitplanes for the font. */
#define FONT_BITPLANE_MASK (1 | 2 | 4)
#define UNSCALED_KERNING 2

struct position {
	uint16_t sx, sy, ex, ey;
};

static struct
{
	int firstchar;
	int numchars;
	struct position *positions;
	int height; // cached
	int kerning; // px between letters
	int scale;
	struct LoadedIff iff; // information about the image
	struct Bitplane *planes; // location of the font
} font;

bool ifffont_init()
{
	font.positions = NULL;
	font.firstchar = font.numchars = 0;

	return true;
}

static void get_position(char c, struct position *dst)
{
	int idx = (c - font.firstchar);

	struct position *pos = &font.positions[idx];
	dst->sx = pos->sx * font.scale;
	dst->sy = pos->sy * font.scale;
	dst->ex = pos->ex * font.scale;
	dst->ey = pos->ey * font.scale;
}

bool ifffont_load(int file_idx, uint32_t startchar, uint32_t numchars, uint16_t *positions, struct Bitplane *planes)
{
	uint16_t w, h;

	if(iff_load(file_idx, &font.iff) == false) {
		fprintf(stderr, "Couldn't load iff for font\n");
		return false;
	}

	iff_get_dimensions(&font.iff, &w, &h);

	font.positions = (struct position *)positions;

	/* Make the font as big as possible without exceeding either screen
	 * dimension and while remaining an integer multiple (because non-integer
	 * scaling looks bad). */
	/* TODO to be honest it looks pretty bad at any scaling other than 1 --
	 * solution would be to use a real font */
	int max_int_scale = min(planes[0].width / w, planes[0].height / h);
	font.scale = max_int_scale;

	iff_display(&font.iff, 0, 0, w * font.scale, h * font.scale, NULL, NULL, planes, 0);

	font.firstchar = startchar;
	font.numchars = numchars;

	// find the font height
	struct position first_position;
	get_position(startchar, &first_position);
	font.height = first_position.ey - first_position.sy;
	font.kerning = UNSCALED_KERNING;
	font.planes = planes;

	return true;
}
void ifffont_unload()
{
	iff_unload(&font.iff);
}

void ifffont_uninit()
{
	font.positions = NULL;
}

int ifffont_measure(int numchars, char *text)
{
	int width_px = 0;

	for(int i = 0; i < numchars; i++) {
		struct position pos;
		get_position(text[i], &pos);

		width_px += ((pos.ex - pos.sx) + (font.kerning * font.scale) + 1);
	}

	return width_px;
}

int ifffont_get_height()
{
	return font.height;
}

void ifffont_draw(int numchars, char *text, int x, int y, struct Bitplane dest_planes[])
{
	for(int i = 0; i < numchars; i++) {
		struct position pos;
		get_position(text[i], &pos);
		int char_width = (pos.ex - pos.sx) + 1;
		int height = pos.ey - pos.sy;

		graphics_blit(font.planes, dest_planes, FONT_BITPLANE_MASK, pos.sx, pos.sy, char_width, height, x, y);

		x += (char_width + font.kerning);
	}
}

void ifffont_centre(int numchars, char *text, int y, struct Bitplane dest_planes[])
{
	int x = (window_width / 2) - (ifffont_measure(numchars, text) / 2);

	ifffont_draw(numchars, text, x, y, dest_planes);
}

