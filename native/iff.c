/* IFF ILBM loading */
#define _DEFAULT_SOURCE

#include <stdbool.h>
#include <inttypes.h>
#include <stdio.h>
#include "endian_compat.h"
#include <string.h>
#include <stdlib.h>
#include <assert.h>

#include "graphics.h"
#include "iff.h"
#include "backend.h"
#include "minmax.h"
#include "align.h"

struct BitmapHeader currentHeader;

static inline bool _is_chunk(const uint8_t *data, const char *chunk_name)
{
	return memcmp(data, chunk_name, 4) == 0;
}

static uint8_t *iff_find_chunk(uint8_t *data, uint8_t *end, char *chunk_name, uint32_t *size_out)
{
	if(!_is_chunk(data, "FORM")) {
		fprintf(stderr, "iff_find_chunk: FORM\n");
		return NULL;
	}

	data += 2 * 4;

	if(!_is_chunk(data, "ILBM")) {
		fprintf(stderr, "iff_find_chunk: ILBM\n");
		return NULL;
	}

	data += 4;

	while(data < end) {
		if(_is_chunk(data, chunk_name)) {
			if(size_out)
				*size_out = be32toh(*((uint32_t *)(data + 4)));

			data += (2 * 4); // skip chunk name and size
			return data;
		} else {
			uint32_t chunk_size = be32toh(*((uint32_t *)(data + 4)));

			if(chunk_size % 2 == 1)
				chunk_size ++; // alignment

			data += (chunk_size + 8);
		}
	}

	return NULL;
}

static int8_t *expand_scanline_byterun1(int8_t *src, int16_t wanted, int8_t *dst)
{
	while(wanted > 0) {
		int8_t b = *src++;

		if(b > 0) {
			/* copy the next b + 1 bytes literally */
			int16_t remaining = min(wanted, ((int16_t)b) + 1);
			wanted -= remaining;

			for(; remaining; remaining--) {
				*dst++ = *src++;
			}
		} else if (b != -128) {
			/* replicate the next byte -n + 1 times */
			int16_t remaining = min(wanted, -b + 1);
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

static void scale_scanline(uint32_t *src, uint16_t src_w, uint32_t *dst, uint16_t dst_w)
{
	/* This is from Dr. Dobbs, http://www.drdobbs.com/image-scaling-with-bresenham/184405045#l1 */
	int16_t int_part = src_w / dst_w;
	int16_t fract_part = src_w % dst_w;
	int16_t e = 0;
	int src_bit = 0, dst_bit = 0;
	uint32_t src_byte = be32toh(*src), dst_byte = 0;

	assert(src_w % 4 == 0);

	for(int num_pixels = dst_w; num_pixels; num_pixels--) {
		if(src_bit > 31) {
			src += (src_bit / 32);
			src_byte = be32toh(*src);
			src_bit %= 32;
		}

		if(src_byte & (1 << (31 - src_bit))) {
			dst_byte |= (1 << (31 - dst_bit));
		}

		dst_bit ++;
		if(dst_bit == 32) {
			*dst++ = htobe32(dst_byte);
			dst_byte = dst_bit = 0;
		}

		src_bit += int_part;

		e += fract_part;
		if(e > dst_w) {
			e -= dst_w;
			src_bit++;
		}
	}
}


static void iff_stretch(uint16_t src_w, uint16_t src_h, uint16_t dst_x, uint16_t dst_y, uint16_t dst_w, uint16_t dst_h, uint16_t nPlanes, int8_t *src, uint8_t compression, struct Bitplane *planes, int start_plane)
{
	int16_t row_bytes = ((src_w + 15) >> 4) << 1;
	int8_t row_byte_data[row_bytes]; // NB this will overflow the stack on Pebble

	int end_y = dst_y + dst_h;
	int intPart = (src_h / dst_h);
	int fractPart = (src_h % dst_h);
	int e = 0;

	int src_row = 0, src_prev_row = -1;

	while(dst_y < end_y) {
		if(src_row != src_prev_row) {
			// src_prev_row may be one or more rows behind. If it's more than one row behind then
			// we still have to decompress the intermediate rows so we know how far to advance the
			// pointer.
			for(int catchup = src_row - src_prev_row; catchup; catchup--) {
				for(int plane = 0; plane < nPlanes; plane++) {
					if(compression == 1) {
						src = expand_scanline_byterun1(src, row_bytes, row_byte_data);
					} else {
						src = expand_scanline(src, row_bytes, row_byte_data);
					}

					struct Bitplane *dest_plane = &planes[plane + start_plane];

					/* Write the scanline to the destination */
					if(catchup == 1) {
						// TODO obviously this means we can only draw to multiples-of-8 destinations
						int dst_offset = (dst_y * dest_plane->stride) + (dst_x / 8);

						scale_scanline((uint32_t *)row_byte_data, src_w,
								(uint32_t *)(dest_plane->data + dst_offset), dst_w);
					}
				}
			}
		} else {
			/* replicate all bitplanes of previous row (dest to dest copy) */
			int plane_mask = ( (1 << nPlanes) - 1) << start_plane;
			graphics_blit(planes, planes, plane_mask, dst_x, dst_y - 1, dst_w, 1, dst_x, dst_y);
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
	size_t size;
	uint8_t *data = backend_wad_load_file(file_idx, &size);
	if(data == NULL) {
		fprintf(stderr, "iff_load: data\n");
		return false;
	}

	iff->data = data;

	uint8_t *end = data + size;

	iff->bmhd = (struct BitmapHeader *)iff_find_chunk(data, end, "BMHD", NULL);
	if(iff->bmhd == NULL) {
		fprintf(stderr, "iff_load: bmhd\n");
		backend_wad_unload_file(data);
		return false;
	}

	if(iff->bmhd->masking) {
		fprintf(stderr, "iff_display: can't handle masked images\n"); // it's not hard, I'm just lazy
		backend_wad_unload_file(data);
		return false;
	}

	iff->body = (int8_t *)iff_find_chunk(data, end, "BODY", NULL);
	if(iff->body == NULL) {
		fprintf(stderr, "iff_load: body\n");
		backend_wad_unload_file(data);
		return false;
	}

	iff->cmap = iff_find_chunk(data, end, "CMAP", &iff->cmap_count);
	if(iff->cmap) {
		iff->cmap_count /= 3;
	} else {
		iff->cmap_count = 0;
	}

	return true;
}

void iff_unload(struct LoadedIff *iff)
{
	backend_wad_unload_file(iff->data);
}

void iff_get_dimensions(struct LoadedIff *iff, uint16_t *w, uint16_t *h)
{
	*w = be16toh(iff->bmhd->w);
	*h = be16toh(iff->bmhd->h);
}


bool iff_display(struct LoadedIff *iff, int dst_x, int dst_y, int dst_w, int dst_h, uint32_t *num_colours, uint32_t *palette_out, struct Bitplane *planes, int start_plane)
{
	/* Doesn't change palette, but will write it to palette_out if that argument is not null. */
	uint16_t src_w = be16toh(iff->bmhd->w);
	uint16_t src_h = be16toh(iff->bmhd->h);

	// ILBMs can be 24-bit if they like!
	uint16_t nplanes = min(iff->bmhd->nPlanes, 5);

	if(iff->bmhd->compression == 0)
		fprintf(stderr, "uncompressed expand untested\n");

	iff_stretch(src_w, src_h, dst_x, dst_y, dst_w, dst_h, nplanes, iff->body, iff->bmhd->compression, planes, start_plane);

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

