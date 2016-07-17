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
} font;

bool font_init()
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

bool font_load(int file_idx, uint32_t startchar, uint32_t numchars, uint16_t *positions)
{
	//int i;
	uint16_t w, h;

	if(iff_load(file_idx, &font.iff) == false) {
		fprintf(stderr, "Couldn't load iff for font\n");
		return false;
	}

	iff_get_dimensions(&font.iff, &w, &h);

	font.positions = (struct position *)positions;

	// Load font into lower display, look at upper display.
	// TODO: This is old now.

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
	iff_display(&font.iff, 0, window_height + h, w, h, NULL, NULL);

	font.firstchar = startchar;
	font.numchars = numchars;

	// find the font height
	struct position *first_position = get_position(startchar);
	font.height = first_position->ey - first_position->sy;
	font.kerning = UNSCALED_KERNING * max_scale;

	return true;
}

void font_uninit()
{
	font.positions = NULL;
}

int font_measure(int numchars, char *text)
{
	int width_px = 0;

	for(int i = 0; i < numchars; i++) {
		struct position *pos = get_position(text[i]);

		width_px += ((pos->ex - pos->sx) + font.kerning + 1);
	}

	return width_px;
}

int font_get_height()
{
	return font.height;
}

void font_draw(int numchars, char *text, int x, int y)
{
	// TODO apply integer-multiplicative scaling here if appropriate.

	for(int i = 0; i < numchars; i++) {
		struct position *pos = get_position(text[i]);
		int char_width = (pos->ex - pos->sx) + 1;

		graphics_blit(FONT_BITPLANE_MASK, pos->sx, pos->sy, char_width, (pos->ey - pos->sy) + 1, x, y);

		x += (char_width + font.kerning);
	}
}

void font_centre(int numchars, char *text, int y)
{
	int x = (window_width / 2) - (font_measure(numchars, text) / 2);

	font_draw(numchars, text, x, y);
}

