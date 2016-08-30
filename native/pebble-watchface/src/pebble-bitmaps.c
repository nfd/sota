/* One-bit image manipulation */
#include <inttypes.h>
#include <stddef.h>
#include <pebble.h>

#include "bits.h"
#include "../../heap.h"
#include "../../align.h"
#include "../../backend.h"
#include "../../mbit.h"
#include "../../graphics.h"

#define NUM_BITPLANES 4

// Defined in wscript
//#define WIDTH 180
//#define HEIGHT 180

// window width and height, part of the backend contract
int window_width = WIDTH;
int window_height = HEIGHT;

// directly-accessible bitplanes, also part of the backend contract.
// The non-pebble demo code expects 5 bitplanes, so we oblige it, but actually
// we never create more than NUM_BITPLANES bitplanes.
struct Bitplane backend_bitplane[5];

// Our palette -- Pebble is very pleasantly AARRGGBB in 8 bits, giving 64 colours and 4 opacities.
// Again, 32 entries to satisfy the demo code.
static uint8_t pebble_palette[32];

// TODO ensure this is inlined!
static inline int render_bits(GBitmapDataRowInfo *info, int dst_x, int src_x, uint8_t *plane0_column, uint8_t *plane1_column, uint8_t *plane2_column, uint8_t *plane3_column, int start, int end)
{
	uint8_t plane0 = plane0_column ? plane0_column[src_x] : 0;
	uint8_t plane1 = plane1_column ? plane1_column[src_x] : 0;
	uint8_t plane2 = plane2_column ? plane2_column[src_x] : 0;
	uint8_t plane3 = plane3_column ? plane3_column[src_x] : 0;

	for(int bit = start; bit >= end; bit --) {
		int mask = 1 << bit;
		info->data[dst_x] = pebble_palette[
			  ((plane0 & mask) ? 1 : 0)
			| ((plane1 & mask) ? 2 : 0)
			| ((plane2 & mask) ? 4 : 0)
			| ((plane3 & mask) ? 8 : 0)];

		dst_x++;
	}

	return dst_x;
}

// Copied from the version in posix_sdl2_backend.c, with modifications to
// handle pebble's round display.
void render_bitplanes(Layer *root, GContext *ctx)
{
	GBitmap *fb = graphics_capture_frame_buffer(ctx);

	uint8_t *plane_idx_0 = (uint8_t *)backend_bitplane[0].data;
	uint8_t *plane_idx_1 = (uint8_t *)backend_bitplane[1].data;
	uint8_t *plane_idx_2 = (uint8_t *)backend_bitplane[2].data;
	uint8_t *plane_idx_3 = (uint8_t *)backend_bitplane[3].data;

	int fb_idx = 0;
	GRect bounds = layer_get_bounds(root);

	for(int y = 0; y < bounds.size.h; y++) {
		GBitmapDataRowInfo info = gbitmap_get_data_row_info(fb, y);

		int dst_x = info.min_x;
		int src_x = dst_x / 8;

		if(dst_x % 8) {
			// Display doesn't start at a multiple of 8.
			int remainder = 7 - (dst_x % 8);
			dst_x = render_bits(&info, dst_x, src_x, plane_idx_0, plane_idx_1, plane_idx_2, plane_idx_3, remainder, 0);
			src_x ++;
		}

		// Copy the bytes in the middle.
		while(info.max_x - dst_x > 7) {
			dst_x = render_bits(&info, dst_x, src_x, plane_idx_0, plane_idx_1, plane_idx_2, plane_idx_3, 7, 0);
			src_x ++;
		}
		
		// Copy the bits at the end which don't form a complete byte.
		if(dst_x <= info.max_x) {
			int end_bit = (7 - (info.max_x & 7));
			//APP_LOG(APP_LOG_LEVEL_DEBUG, "start 7 end %d\n", end_bit);
			render_bits(&info, dst_x, src_x, plane_idx_0, plane_idx_1, plane_idx_2, plane_idx_3, 7, end_bit);
		}

		plane_idx_0 += backend_bitplane[0].stride;
		plane_idx_1 += backend_bitplane[1].stride;
		plane_idx_2 += backend_bitplane[2].stride;
		plane_idx_3 += backend_bitplane[3].stride;

		fb_idx += window_width;
	}

	graphics_release_frame_buffer(ctx, fb);
}

static void set_palette_from_argb(uint16_t num_palette, uint32_t *palette_in)
{
	for(int i = 0; i < num_palette; i++) {
		pebble_palette[i] = GColorFromHEX(palette_in[i]).argb;
		APP_LOG(APP_LOG_LEVEL_DEBUG, "palette  %u %lx->%x", i, palette_in[i], pebble_palette[i]);
	}
}

void backend_set_palette_element(int idx, uint32_t element) {
	pebble_palette[idx] = GColorFromHEX(element).argb;
	APP_LOG(APP_LOG_LEVEL_DEBUG, "palette %u %lx->%x", idx, element, pebble_palette[idx]);
}

void backend_set_palette(int num_elements, uint32_t *elements)
{
	set_palette_from_argb(num_elements, elements);
}

static inline uint32_t GColorToHex(uint8_t pebble_colour)
{
	/* We get two bits per colour, so set the top two bits
	 * of each byte. */
	return 0xff000000 
		| ((pebble_colour & 0b00110000) >> 4 << 22)
		| ((pebble_colour & 0b00001100) >> 2 << 14)
		| ((pebble_colour & 0b00000011) >> 0 << 6);

}

void backend_get_palette(int num_elements, uint32_t *elements)
{
	for(int i = 0; i < num_elements; i++) {
		elements[i] = GColorToHex(pebble_palette[i]);
	}
}

/* Bitplane manipulation */
struct Bitplane *backend_allocate_bitplane(int idx, int width, int height)
{
	if(idx < NUM_BITPLANES) {
		backend_bitplane[idx].width = width;
		backend_bitplane[idx].height = height;
		backend_bitplane[idx].stride = align(width, 8) / 8;
		backend_bitplane[idx].data = backend_bitplane[idx].data_start
			= heap_alloc(backend_bitplane[idx].height * backend_bitplane[idx].stride);
	} else {
		backend_bitplane[idx].width = backend_bitplane[idx].height = backend_bitplane[idx].stride = 0;
		backend_bitplane[idx].data = backend_bitplane[idx].data_start = 0;
	}

	APP_LOG(APP_LOG_LEVEL_DEBUG, "plane width %d height %d stride %d data %p",
			backend_bitplane[idx].width,
			backend_bitplane[idx].height,
			backend_bitplane[idx].stride,
			backend_bitplane[idx].data);
	return &backend_bitplane[idx];
}

void backend_allocate_standard_bitplanes()
{
	for(int i = 0; i < 5; i++) {
		backend_allocate_bitplane(i, WIDTH, HEIGHT);
	}
}

void backend_copy_bitplane(struct Bitplane *dst, struct Bitplane *src)
{
	if(dst->height == src->height && dst->stride == src->stride) {
		memcpy(dst->data_start, src->data_start, src->stride * src->height);
	}
}

#define MAX_FONT_BITPLANES 3
#define FONT_BLIT_MASK ((1 << MAX_FONT_BITPLANES) - 1)

struct Bitplane font_bitplanes[MAX_FONT_BITPLANES];

struct LetterPosition {
	uint16_t sx, sy, ex, ey;
};

struct MbitFont {
	uint32_t startchar;
	uint32_t numchars;
	int height;
	int kerning;
	struct LetterPosition positions[];
};

struct MbitFont *font;

/* Fonts */
bool backend_font_load(int file_idx, uint32_t startchar, uint32_t numchars, uint16_t *positions)
{
	size_t size;

	struct multibit_compressed *mbit = (struct multibit_compressed *)backend_wad_load_file(file_idx, &size);
	if(mbit == NULL)
		return false;

	if(mbit->num_planes > MAX_FONT_BITPLANES) {
		backend_debug("too many font bitplanes");
		backend_wad_unload_file(mbit);
		return false;
	}

	if(mbit->width % 8 != 0) {
		backend_debug("non-byte-aligned width\n");
		backend_wad_unload_file(mbit);
		return false;
	}

	/* Allocate font bitplanes */
	int stride = mbit->width / 8;

	for(int i = 0; i < mbit->num_planes; i++) {
		font_bitplanes[i].width = mbit->width;
		font_bitplanes[i].height = mbit->height;
		font_bitplanes[i].stride = stride;
		font_bitplanes[i].data_start = font_bitplanes[i].data = heap_alloc(stride * mbit->height);
	}

	mbit_display(mbit, &backend_set_palette, font_bitplanes);

	// TODO we can't unload the file because stuff has been allocated after it. 
	// backend_wad_unload_file(mbit);
	font = heap_alloc(sizeof(struct MbitFont) + (numchars * sizeof(struct LetterPosition)));
	if(font == NULL) {
		backend_debug("Couldn't create font map");
		return false;
	}

	font = (struct MbitFont *)mbit;
	font->startchar = startchar;
	font->numchars = numchars;
	memcpy(font->positions, positions, numchars * sizeof(struct LetterPosition));

	font->height = font->positions[0].ey - font->positions[0].sy;
	font->kerning = 2;

	backend_debug("font loaded");
	
	// TODO
	return true;
}

int backend_font_get_height()
{
	return font->height;
}

static inline struct LetterPosition *backend_font_letter_position(char c)
{
	return &font->positions[c - font->startchar];
}

static int backend_font_measure(int numchars, const char *text)
{
	int length = 0;
	for(; numchars; text++, numchars--) {
		struct LetterPosition *position = backend_font_letter_position(*text);
		length += (position->ex - position->sx);
		length += font->kerning;
	}

	length -= font->kerning;
	return length;
}

void backend_font_draw(int numchars, char *text, int x, int y)
{
	if(x == -1) {
		x = (window_width / 2) - (backend_font_measure(numchars, text) / 2);
	}

	for(; numchars; text++, numchars--) {
		struct LetterPosition *position = backend_font_letter_position(*text);
		int width = position->ex - position->sx;
		int height = position->ey - position->sy;

		graphics_blit(font_bitplanes, backend_bitplane, FONT_BLIT_MASK, position->sx, position->sy, width, height, x, y);

		x += width;
		x += font->kerning;
	}
}

