/* IFF ILBM loading */
#define _DEFAULT_SOURCE

#include <stdbool.h>
#include <inttypes.h>
#include <stdio.h>
#include <endian.h>
#include <string.h>

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

	// the current scanline, after decompression (only used if compressed)
	uint8_t decompressed_scanline[src_w];

	printf("iff %d x %d\n", be16toh(bmhd->w), be16toh(bmhd->h));

	// destination bitplanes
	uint8_t *dst_plane[5];
	for(int i = 0; i < 5; i++)
		dst_plane[i] = graphics_bitplane_get(i);

	uint16_t row_bytes = ((src_w + 15) >> 4) << 1;

	if(bmhd->compression == 0)
		fprintf(stderr, "uncompressed expand untested\n");

	// ILBMs can be 24-bit if they like!
	uint16_t nplanes = min(bmhd->nPlanes, 5);

	if(display_type == ILBM_FULLSCREEN) {
		/* Stretch -- todo */
		//printf("%p\n", body);
		for(int y = 0; y < src_h; y++) {
			for(int plane = 0; plane < bmhd->nPlanes; plane++) {
				uint8_t *src_scanline;

				if(bmhd->compression == 1) {
					body = expand_scanline_byterun1(body, row_bytes, decompressed_scanline, plane);
				} else {
					body = expand_scanline(body, row_bytes, decompressed_scanline, plane);
				}

				scale_scanline(decompressed_scanline, src_w, dst_plane[plane], dst_w);

				//memcpy(dst_plane[plane], decompressed_scanline, src_w);
				dst_plane[plane] += dst_stride;
			}

		}
		//printf("%p\n", body);

	} else {
		/* Centre */
	}

	/* Copy the palette if requested */
	uint32_t cmap_count;
	uint8_t *cmap = iff_find_chunk(data, end, IFF_CMAP, &cmap_count);
	cmap_count /= 3;

	printf("%d\n", cmap_count);
	if(num_colours)
		*num_colours = cmap_count;

	if(palette_out && cmap) {
		for(int idx = 0; idx < cmap_count; idx++, cmap += 3) {
			palette_out[idx] = 0xff000000
				| (cmap[0] << 16)
				| (cmap[1] << 8)
				| (cmap[2]);
		}
	}
}

uint32_t *iff_get_palette(int file_idx, uint16_t *count)
{
	/*
	ssize_t size;
	uint8_t *data = file_get(file_idx, &size);
	uint8_t *end = data + size;

	uint8_t *cmap = iff_find_chunk(data, end, IFF_CMAP);
	if(cmap == NULL) {
		return NULL;
	}

	uint32_t

	return cmap;
	*/

	// TODO: replace all this with a "load iff" thing which stores chunk body indices and sizes, OR
	// (better) replace it with a loader which copies the palette data out on load.

	return NULL;
}

