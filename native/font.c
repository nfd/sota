#include <stdbool.h>
#include <inttypes.h>
#include <stdlib.h>
#include <stdio.h>

#include "graphics.h"
#include "iff.h"

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
	int i;
	int dwidth = graphics_width(), dheight = graphics_height();

	if(font.positions)
		free(font.positions);

	font.positions = malloc(numchars * sizeof(struct position));
	if(font.positions == NULL)
		return false;

	// Load font into lower display, look at upper display.

	/* We want the font to be an integer multiple of the 
	 * width, because blocky scaling of text looks bad.
	 * Fonts are always 320x256
	*/
	int max_scale = dwidth / 320;
	max_scale = min(dheight / 256, max_scale);
	max_scale = max(max_scale, 1);

	max_scale = 2;

	int ypos = max(0, dheight - (256 * max_scale));

	iff_display(file_idx, 0, dheight + ypos, 320 * max_scale, 256 * max_scale, NULL, NULL);

	font.firstchar = startchar;
	font.numchars = numchars;
	for(i = 0; i < numchars; i++) {
		struct position *pos = (struct position *)(&(positions[i * 4]));

		font.positions[i].sx = pos->sx * max_scale;
		font.positions[i].sy = (pos->sy * max_scale) + ypos + dheight;
		font.positions[i].ex = pos->ex * max_scale;
		font.positions[i].ey = (pos->ey * max_scale) + ypos + dheight;
	}

	// find the font height
	struct position *first_position = get_position(startchar);
	font.height = first_position->ey - first_position->sy;
	font.kerning = UNSCALED_KERNING * max_scale;

	return true;
}

void font_uninit()
{
	if(font.positions)
		free(font.positions);
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
	for(int i = 0; i < numchars; i++) {
		struct position *pos = get_position(text[i]);
		int char_width = (pos->ex - pos->sx) + 1;

		graphics_blit(FONT_BITPLANE_MASK, pos->sx, pos->sy, char_width, (pos->ey - pos->sy) + 1, x, y);

		x += (char_width + font.kerning);
	}
}

void font_centre(int numchars, char *text, int y)
{
	int x = (graphics_width() / 2) - (font_measure(numchars, text) / 2);

	font_draw(numchars, text, x, y);
}

