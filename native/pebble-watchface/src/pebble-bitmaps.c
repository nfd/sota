/* One-bit image manipulation */
#include <inttypes.h>
#include <stddef.h>
#include <pebble.h>

#include "bits.h"
#include "heap.h"
#include "../../align.h"

#include "../tinf/src/tinf.h"
#include "../../backend.h"

/* Multi-bit bitmaps: memory representation */
struct multibit_compressed {
	uint32_t signature; // "img1"
	uint16_t width;
	uint16_t height;
	uint16_t num_planes;
	uint16_t num_palette;
	// palette entries follow (4 bytes each)
	// compressed bitplane lengths follow (4 bytes each)
	// compressed bitplanes follow
};

/* Bitplanes -- we need the number of bits per row to be a multiple of 8, so it's slightly larger than necessary */
#define BITPLANE_STRIDE_BYTES (184 / 8)
#define NUM_BITPLANES 4
#define WIDTH 180
#define HEIGHT 180

struct Bitplane backend_bitplane[5];

/* Our palette -- Pebble is very pleasantly AARRGGBB in 8 bits, giving 64 colours and 4 opacities. */
static uint8_t pebble_palette[32] = { GColorBlackARGB8, GColorOxfordBlueARGB8, GColorDukeBlueARGB8, GColorBlueARGB8, GColorDarkGreenARGB8, GColorMidnightGreenARGB8, GColorCobaltBlueARGB8, GColorBlueMoonARGB8, GColorIslamicGreenARGB8, GColorJaegerGreenARGB8, GColorTiffanyBlueARGB8, GColorVividCeruleanARGB8, GColorGreenARGB8, GColorMalachiteARGB8, GColorMediumSpringGreenARGB8, GColorCyanARGB8, GColorBulgarianRoseARGB8, GColorImperialPurpleARGB8, GColorIndigoARGB8, GColorElectricUltramarineARGB8, GColorArmyGreenARGB8, GColorDarkGrayARGB8, GColorLibertyARGB8, GColorVeryLightBlueARGB8, GColorKellyGreenARGB8, GColorMayGreenARGB8, GColorCadetBlueARGB8, GColorPictonBlueARGB8, GColorBrightGreenARGB8, GColorScreaminGreenARGB8, GColorMediumAquamarineARGB8, GColorElectricBlueARGB8 };

// TODO ensure this is inlined!
static inline int render_bits(GBitmapDataRowInfo *info, int dst_x, uint8_t plane0, uint8_t plane1, uint8_t plane2, uint8_t plane3, int start, int end)
{
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

// Copied and from the version in posix_sdl2_backend.c, with modifications to
// handle pebble's non-square display.
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
			dst_x = render_bits(&info, dst_x, plane_idx_0[src_x], plane_idx_1[src_x], plane_idx_2[src_x], plane_idx_3[src_x], remainder, 0);
			src_x ++;
		}

		// Copy the bytes in the middle.
		//APP_LOG(APP_LOG_LEVEL_DEBUG, "dst_x %d mdxb %d\n", dst_x, max_dst_x_byte);

		while(info.max_x - dst_x > 7) {
			dst_x = render_bits(&info, dst_x, plane_idx_0[src_x], plane_idx_1[src_x], plane_idx_2[src_x], plane_idx_3[src_x], 7, 0);
			src_x ++;
		}
		
		// Copy the bits at the end which don't form a complete byte.
		if(dst_x <= info.max_x) {
			int end_bit = (7 - (info.max_x & 7));
			//APP_LOG(APP_LOG_LEVEL_DEBUG, "start 7 end %d\n", end_bit);
			render_bits(&info, dst_x, plane_idx_0[src_x], plane_idx_1[src_x], plane_idx_2[src_x], plane_idx_3[src_x], 7, end_bit);
		}

		plane_idx_0 += backend_bitplane[0].stride;
		plane_idx_1 += backend_bitplane[1].stride;
		plane_idx_2 += backend_bitplane[2].stride;
		plane_idx_3 += backend_bitplane[3].stride;

		fb_idx += window_width;
	}

	graphics_release_frame_buffer(ctx, fb);
}

static void draw_1bit(uint16_t src_width, uint16_t src_height, uint8_t *src_data, struct Bitplane *dst_bitplane, int dstx, int dsty)
{
	uint8_t *src = src_data;
	uint8_t *dst = dst_bitplane->data + (dsty * dst_bitplane->stride) + (dstx / 8);

	int src_stride = src_width / 8;

	for(int y = 0; y < src_height; y++) {
		for(int x = 0; x < src_stride; x++) {
			dst[x] = src[x];
		}
		dst += dst_bitplane->stride;
		src += src_stride;
	}
}

static void set_palette_from_argb(uint16_t num_palette, uint32_t *palette_in)
{
	for(int i = 0; i < num_palette; i++) {
		pebble_palette[i] = GColorFromHEX(palette_in[i]).argb;
	}
}

void load_mbitmap(ResHandle handle) {
	/*
	 * Load an mbitmap, copy it to the bitplanes, and change the palette. 
	*/
	size_t size_in = resource_size(handle);

	struct multibit_compressed *compressed_resource = heap_alloc(size_in);
	resource_load(handle, (void *)compressed_resource, size_in);

	// Immediately after the fixed-length data we have palette entries.
	uint8_t *ptr = ((uint8_t *)(compressed_resource)) + (sizeof(struct multibit_compressed));
	set_palette_from_argb(compressed_resource->num_palette, (uint32_t *)ptr);

	ptr += (compressed_resource->num_palette * sizeof(uint32_t));
	// After the palette comes the lengths of each bitplane, and after these come the planes themselves.
	uint32_t *plane_lengths = (uint32_t *)ptr;
	ptr += (compressed_resource->num_planes * sizeof(uint32_t));

	// decompressor data structure
	void *tinf_internal_data = heap_alloc(tinf_data_size());
	uint8_t *decompressed = heap_alloc((compressed_resource->width * compressed_resource->height) / 8);

	//APP_LOG(APP_LOG_LEVEL_DEBUG, "bitmap width %d dest plane0 stride %d", compressed_resource->width, backend_bitplane[0].stride);

	for(int i = 0; i < compressed_resource->num_planes; i++) {
		unsigned int uncompressed_size_out;
		tinf_zlib_uncompress(decompressed, &uncompressed_size_out, ptr, plane_lengths[i], tinf_internal_data);

		draw_1bit(compressed_resource->width, compressed_resource->height, decompressed, &backend_bitplane[i], 0, 0);

		ptr += plane_lengths[i];
	}
	
	// pop decompressed data
	heap_pop();

	// pop decompressor data struct
	heap_pop();
}

/* Bitplane manipulation */
void backend_allocate_standard_bitplanes()
{
	int i;

	for(i = 0; i < NUM_BITPLANES; i++) {
		backend_bitplane[i].width = WIDTH;
		backend_bitplane[i].height = HEIGHT;
		backend_bitplane[i].stride = BITPLANE_STRIDE_BYTES;
		backend_bitplane[i].data = backend_bitplane[i].data_start = heap_alloc(HEIGHT * BITPLANE_STRIDE_BYTES);
	}

	for(i = NUM_BITPLANES; i < 5; i++) {
		backend_bitplane[i].width = backend_bitplane[i].height = backend_bitplane[i].stride = 0;
		backend_bitplane[i].data = backend_bitplane[i].data_start = 0;
	}
}

