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
	struct LoadedIff iff; // information about the image
	struct Bitplane *planes; // location of the font
} font;

bool ifffont_init()
{
	font.positions = NULL;
	font.firstchar = font.numchars = 0;

	return true;
}

static struct position *get_position(char c)
{
	int idx = (c - font.firstchar);

	return (struct position *)(&(font.positions[idx]));
}

bool ifffont_load(int file_idx, uint32_t startchar, uint32_t numchars, uint16_t *positions, struct Bitplane *planes)
{
	uint16_t w, h;

	if(iff_load(file_idx, &font.iff) == false) {
		fprintf(stderr, "Couldn't load iff for font\n");
		return false;
	}

	iff_get_dimensions(&font.iff, &w, &h);

	// TODO
	assert(w == 320 && h == 256);

	font.positions = (struct position *)positions;

	/* We want the font to be an integer multiple of the 
	 * width, because blocky scaling of text looks bad.
	 * Fonts are always 320x256
	*/
	// TODO: scaling -- affects bitmap allocation obviously (though may be better to do raster scaling
	// in the backend at display time).
	int max_scale = 1; // chosen by fair dice roll.
	//int ypos = max(0, window_height - (h * max_scale));
	//iff_display(file_idx, 0, window_height + ypos, w * max_scale, h * max_scale, NULL, NULL);
	
	// TODO: check height display?
	iff_display(&font.iff, 0, 0, w, h, NULL, NULL, planes);

	font.firstchar = startchar;
	font.numchars = numchars;

	// find the font height
	struct position *first_position = get_position(startchar);
	font.height = first_position->ey - first_position->sy;
	font.kerning = UNSCALED_KERNING * max_scale;
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
		struct position *pos = get_position(text[i]);

		width_px += ((pos->ex - pos->sx) + font.kerning + 1);
	}

	return width_px;
}

int ifffont_get_height()
{
	return font.height;
}

void ifffont_draw(int numchars, char *text, int x, int y, struct Bitplane dest_planes[])
{
	// TODO apply integer-multiplicative scaling here if appropriate.

	for(int i = 0; i < numchars; i++) {
		struct position *pos = get_position(text[i]);
		int char_width = (pos->ex - pos->sx) + 1;
		int height = pos->ey - pos->sy;

		graphics_blit(font.planes, dest_planes, FONT_BITPLANE_MASK, pos->sx, pos->sy, char_width, height, x, y);

		x += (char_width + font.kerning);
	}
}

void ifffont_centre(int numchars, char *text, int y, struct Bitplane dest_planes[])
{
	int x = (window_width / 2) - (ifffont_measure(numchars, text) / 2);

	ifffont_draw(numchars, text, x, y, dest_planes);
}

