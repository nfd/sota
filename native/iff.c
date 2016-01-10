/* IFF ILBM loading */
#define _DEFAULT_SOURCE

#include <stdbool.h>
#include <inttypes.h>
#include <stdio.h>
#include <endian.h>
#include <string.h>
#include <stdlib.h>

#include "files.h"
#include "graphics.h"

#define ILBM_FULLSCREEN 0
#define ILBM_CENTRE 1

#define IFF_FORM 0x464f524d  /* "FORM" */
#define IFF_ILBM 0x494c424d  /* "ILBM" */
#define IFF_BMHD 0x424d4844  /* "BMHD" */
#define IFF_CMAP 0x434d4150  /* "CMAP" */
#define IFF_BODY 0x424f4459  /* "BODY" */

struct BitmapHeader {
	uint16_t w, h;             /* raster width & height in pixels      */
	int16_t  x, y;             /* pixel position for this image        */
	uint8_t  nPlanes;          /* # source bitplanes                   */
	uint8_t  masking;
	uint8_t  compression;
	uint8_t  pad1;             /* unused; ignore on read, write as 0   */
	uint16_t transparentColor; /* transparent "color number" (sort of) */
	uint8_t  xAspect, yAspect; /* pixel aspect, a ratio width : height */
	int16_t  pageWidth, pageHeight; /* source "page" size in pixels    */
} __attribute__((__packed__));

struct BitmapHeader currentHeader;

static uint8_t *iff_find_chunk(uint8_t *data, uint8_t *end, uint32_t chunk_name, uint32_t *size_out)
{
	uint32_t *data32 = (uint32_t *)data;
	chunk_name = htobe32(chunk_name);

	if(be32toh(*data32) != IFF_FORM) {
		fprintf(stderr, "iff_find_chunk: FORM\n", NULL);
		return NULL;
	}

	data32 += 2;

	if(be32toh(*data32) != IFF_ILBM) {
		fprintf(stderr, "iff_find_chunk: ILBM\n", NULL);
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
			data32 = (uint32_t *) (((uint8_t *)data32) + chunk_size + 8);
		}
	}

	return NULL;
}

static inline void expand_one(uint8_t src, uint8_t *dst, uint8_t pen)
{
	dst[0] = src & 0x80 ? pen : 0;
	dst[1] = src & 0x40 ? pen : 0;
	dst[2] = src & 0x20 ? pen : 0;
	dst[3] = src & 0x10 ? pen : 0;
	dst[4] = src & 0x08 ? pen : 0;
	dst[5] = src & 0x04 ? pen : 0;
	dst[6] = src & 0x02 ? pen : 0;
	dst[7] = src & 0x01 ? pen : 0;
}

static uint8_t *expand_scanline_byterun1(int8_t *src, uint16_t wanted, uint8_t *dst, int8_t plane)
{
	uint8_t pen = 1 << plane;

	while(wanted) {
		int8_t b = *src++;

		if(b > 0) {
			/* copy the next b + 1 bytes literally */
			int16_t remaining = ((int16_t)b) + 1;
			wanted -= remaining;

			for(; remaining; remaining--) {
				expand_one(*src++, dst, pen);
				dst += 8;
			}
		} else if (b != -128) {
			/* replicate the next byte -n + 1 times */
			int16_t remaining = -b + 1;
			wanted -= remaining;

			uint8_t replicateme = *src++;

			for(; remaining; remaining--) {
				expand_one(replicateme, dst, pen);
				dst += 8;
			}
		}
	}

	return src;
}

static uint8_t *expand_scanline(uint8_t *src, uint16_t wanted, uint8_t *dst, int8_t plane)
{
	// This is untested
	uint8_t pen = 1 << plane;

	for(; wanted; wanted--) {
		expand_one(*src++, dst, pen);
		dst += 8;
	}
	return src;
}

static void scale_scanline(uint8_t *src, uint16_t src_w, uint8_t *dst, uint16_t dst_w)
{
	/* This is from Dr. Dobbs, http://www.drdobbs.com/image-scaling-with-bresenham/184405045#l1 */

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


static void iff_display_fullscreen(uint16_t src_w, uint16_t src_h, uint16_t dst_w, uint16_t dst_h, uint16_t dst_stride, uint16_t nPlanes, uint8_t *src, uint8_t **dst_plane, uint8_t compression)
{
	uint16_t row_bytes = ((src_w + 15) >> 4) << 1;
	uint16_t decompressed_stride = row_bytes * 8;

	uint8_t decompressed[nPlanes * decompressed_stride];

	int targetRows = dst_h;
	int intPart = (src_h / dst_h);
	int fractPart = (src_h % dst_h);
	int e = 0;

	int src_row = 0, src_prev_row = -1;
	
	while(targetRows-- > 0) {
		uint8_t *decompressed_scanline = decompressed;

		if(src_row != src_prev_row) {

			for(int plane = 0; plane < nPlanes; plane++) {
				if(compression == 1) {
					src = expand_scanline_byterun1(src, row_bytes, decompressed_scanline, plane);
				} else {
					src = expand_scanline(src, row_bytes, decompressed_scanline, plane);
				}
				decompressed_scanline += decompressed_stride;
			}
		}

		decompressed_scanline = decompressed;
		for(int plane = 0; plane < nPlanes; plane++) {
			//memcpy(dst_plane[plane], decompressed_scanline[plane], src_w);
			scale_scanline(decompressed_scanline, src_w, dst_plane[plane], dst_w);

			dst_plane[plane] += dst_stride;
			decompressed_scanline += decompressed_stride;
		}
		src_prev_row = src_row;

		src_row += intPart;
		e += fractPart;
		if(e > dst_h) {
			e -= dst_h;
			src_row ++;
		}
	}
}


bool iff_display(int file_idx, int display_type, uint32_t *num_colours, uint32_t *palette_out)
{
	/* Doesn't change palette, but will write it to palette_out if that argument is not null. */

	ssize_t size;
	uint8_t *data = file_get(file_idx, &size);
	if(data == NULL) {
		fprintf(stderr, "iff_display: data\n");
		return false;
	}

	uint8_t *end = data + size;

	struct BitmapHeader *bmhd = (struct BitmapHeader *)iff_find_chunk(data, end, IFF_BMHD, NULL);
	if(bmhd == NULL) {
		fprintf(stderr, "iff_display: bmhd\n");
		return false;
	}

	if(bmhd->masking) {
		fprintf(stderr, "iff_display: can't handle masked images\n"); // it's not hard, I'm just lazy
		return false;
	}

	uint8_t *body = iff_find_chunk(data, end, IFF_BODY, NULL);
	if(body == NULL) {
		fprintf(stderr, "iff_display: body\n");
		return false;
	}

	uint16_t dst_w = graphics_width();
	uint16_t dst_h = graphics_height();
	uint16_t dst_stride = graphics_bitplane_stride();
	uint16_t src_w = be16toh(bmhd->w);
	uint16_t src_h = be16toh(bmhd->h);

	//printf("iff %d x %d\n", be16toh(bmhd->w), be16toh(bmhd->h));

	// ILBMs can be 24-bit if they like!
	uint16_t nplanes = min(bmhd->nPlanes, 5);

	// destination bitplanes
	uint8_t *dst_plane[5];
	for(int i = 0; i < 5; i++)
		dst_plane[i] = graphics_bitplane_get(i);

	if(bmhd->compression == 0)
		fprintf(stderr, "uncompressed expand untested\n");

	if(display_type == ILBM_FULLSCREEN) {
		/* Stretch -- todo */
		iff_display_fullscreen(src_w, src_h, dst_w, dst_h, dst_stride, nplanes, body, dst_plane, bmhd->compression);
	} else {
		/* Centre */
	}

	/* Copy the palette if requested */
	uint32_t cmap_count;
	uint8_t *cmap = iff_find_chunk(data, end, IFF_CMAP, &cmap_count);
	cmap_count /= 3;

	if(num_colours)
		*num_colours = cmap_count;

	if(palette_out && cmap) {
		for(int idx = 0; idx < cmap_count; idx++) {
			palette_out[idx] = 0xff000000
				| (cmap[0] << 16)
				| (cmap[1] << 8)
				| (cmap[2]);

			cmap += 3;
		}
	}
}

