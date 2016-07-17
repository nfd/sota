/* IFF ILBM loading */
#define _DEFAULT_SOURCE

#include <stdbool.h>
#include <inttypes.h>
#include <stdio.h>

#ifdef MAC_ENDIAN_H
#include "mac_endian.h"
#else
#include <endian.h>
#endif

#include <string.h>
#include <stdlib.h>

#include "files.h"
#include "graphics.h"
#include "iff.h"
#include "backend.h"
#include "minmax.h"

#define IFF_FORM 0x464f524d  /* "FORM" */
#define IFF_ILBM 0x494c424d  /* "ILBM" */
#define IFF_BMHD 0x424d4844  /* "BMHD" */
#define IFF_CMAP 0x434d4150  /* "CMAP" */
#define IFF_BODY 0x424f4459  /* "BODY" */


struct BitmapHeader currentHeader;

static uint8_t *iff_find_chunk(uint8_t *data, uint8_t *end, uint32_t chunk_name, uint32_t *size_out)
{
	uint32_t *data32 = (uint32_t *)data;
	chunk_name = htobe32(chunk_name);

	if(be32toh(*data32) != IFF_FORM) {
		fprintf(stderr, "iff_find_chunk: FORM\n");
		return NULL;
	}

	data32 += 2;

	if(be32toh(*data32) != IFF_ILBM) {
		fprintf(stderr, "iff_find_chunk: ILBM\n");
		return NULL;
	}

	data32 ++;

	while(data32 < (uint32_t *)end) {
		if(*data32 == chunk_name) {
			if(size_out)
				*size_out = be32toh(data32[1]);

			data32 += 2; // skip chunk name and size
			return (uint8_t *)data32;
		} else {
			uint32_t chunk_size = be32toh(data32[1]);

			if(chunk_size % 2 == 1)
				chunk_size ++; // alignment

			data32 = (uint32_t *) (((uint8_t *)data32) + chunk_size + 8);
		}
	}

	return NULL;
}

static int8_t *expand_scanline_byterun1(int8_t *src, uint16_t wanted, int8_t *dst)
{
	while(wanted) {
		int8_t b = *src++;

		if(b > 0) {
			/* copy the next b + 1 bytes literally */
			int16_t remaining = ((int16_t)b) + 1;
			wanted -= remaining;

			for(; remaining; remaining--) {
				*dst++ = *src++;
			}
		} else if (b != -128) {
			/* replicate the next byte -n + 1 times */
			int16_t remaining = -b + 1;
			wanted -= remaining;

			uint8_t replicateme = *src++;

			for(; remaining; remaining--) {
				*dst++ = replicateme;
			}
		}
	}

	return src;
}

static int8_t *expand_scanline(int8_t *src, uint16_t wanted, int8_t *dst)
{
	// This is untested
	for(; wanted; wanted--) {
		*dst++ = *src++;
	}
	return src;
}

static void scale_scanline(int8_t *src, uint16_t src_w, uint8_t *dst, uint16_t dst_w)
{
	/* This is from Dr. Dobbs, http://www.drdobbs.com/image-scaling-with-bresenham/184405045#l1 */

	// TODO must adapt this to deal with bits not bytes

	int16_t num_pixels = dst_w;
	int16_t int_part = src_w / dst_w;
	int16_t fract_part = src_w % dst_w;
	int16_t e = 0;

	while(num_pixels-- > 0) {
		*dst++ = *src;
		src += int_part;

		e += fract_part;
		if(e > dst_w) {
			e -= dst_w;
			src++;
		}
	}
}


static void iff_stretch(uint16_t src_w, uint16_t src_h, uint16_t dst_x, uint16_t dst_y, uint16_t dst_w, uint16_t dst_h, uint16_t nPlanes, int8_t *src, uint8_t compression)
{
	uint16_t row_bytes = ((src_w + 15) >> 4) << 1;
	int8_t row_byte_data[decompressed_stride]; // NB this will overflow the stack on Pebble

	int end_y = dst_y + dst_h;
	int intPart = (src_h / dst_h);
	int fractPart = (src_h % dst_h);
	int e = 0;

	int src_row = 0, src_prev_row = -1;

	while(dst_y <= end_y) {
		if(src_row != src_prev_row) {
			for(int plane = 0; plane < nPlanes; plane++) {
				if(compression == 1) {
					src = expand_scanline_byterun1(src, row_bytes, row_byte_data);
				} else {
					src = expand_scanline(src, row_bytes, row_byte_data);
				}

				/* Write the scanline to the destination */
				scale_scanline(row_byte_data, src_w, backend_bitplane[plane].data + (dst_y * backend_bitplane[plane].stride), dst_w);
			}
		} else {
			/* replicate all bitplanes of previous row (dest to dest copy): TODO */
			//backend_blit_native(dst_x, dst_y - 1, dst_w, dst_h, dst_x, dst_y, 0xff, 0xff, display, display);
		}

		src_prev_row = src_row;

		src_row += intPart;
		e += fractPart;
		if(e > dst_h) {
			e -= dst_h;
			src_row ++;
		}

		dst_y ++;
	}
}

bool iff_load(int file_idx, struct LoadedIff *iff)
{
	ssize_t size;
	uint8_t *data = file_get(file_idx, &size);
	if(data == NULL) {
		fprintf(stderr, "iff_load: data\n");
		return false;
	}

	uint8_t *end = data + size;

	iff->bmhd = (struct BitmapHeader *)iff_find_chunk(data, end, IFF_BMHD, NULL);
	if(iff->bmhd == NULL) {
		fprintf(stderr, "iff_load: bmhd\n");
		return false;
	}

	if(iff->bmhd->masking) {
		fprintf(stderr, "iff_display: can't handle masked images\n"); // it's not hard, I'm just lazy
		return false;
	}

	iff->body = (int8_t *)iff_find_chunk(data, end, IFF_BODY, NULL);
	if(iff->body == NULL) {
		fprintf(stderr, "iff_load: body\n");
		return false;
	}

	iff->cmap = iff_find_chunk(data, end, IFF_CMAP, &iff->cmap_count);
	iff->cmap_count /= 3;

	return true;
}

void iff_get_dimensions(struct LoadedIff *iff, uint16_t *w, uint16_t *h)
{
	*w = be16toh(iff->bmhd->w);
	*h = be16toh(iff->bmhd->h);
}


bool iff_display(struct LoadedIff *iff, int dst_x, int dst_y, int dst_w, int dst_h, uint32_t *num_colours, uint32_t *palette_out)
{
	/* Doesn't change palette, but will write it to palette_out if that argument is not null. */

	//uint16_t dst_w = graphics_width();
	//uint16_t dst_h = graphics_height();
	//uint16_t dst_stride = graphics_bitplane_stride();
	uint16_t src_w = be16toh(iff->bmhd->w);
	uint16_t src_h = be16toh(iff->bmhd->h);

	//printf("iff %d x %d\n", be16toh(bmhd->w), be16toh(bmhd->h));

	// ILBMs can be 24-bit if they like!
	uint16_t nplanes = min(iff->bmhd->nPlanes, 5);

	if(iff->bmhd->compression == 0)
		fprintf(stderr, "uncompressed expand untested\n");

	iff_stretch(src_w, src_h, dst_x, dst_y, window_width, window_height, nplanes, iff->body, iff->bmhd->compression);

	/* Copy the palette if requested */
	if(num_colours)
		*num_colours = iff->cmap_count;

	uint8_t *cmap = iff->cmap;
	if(palette_out && cmap) {
		for(int idx = 0; idx < iff->cmap_count; idx++) {
			palette_out[idx] = 0xff000000
				| (cmap[0] << 16)
				| (cmap[1] << 8)
				| (cmap[2]);

			cmap += 3;
		}
	}
	return true;
}

