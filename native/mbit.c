#include <inttypes.h>
#include <stddef.h>

#include "tinf/src/tinf.h"

#include "backend.h"
#include "heap.h"
#include "mbit.h"

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

void mbit_display(void *mbit_source, void(*set_palette_from_argb)(int num, uint32_t *in), struct Bitplane *dest_planes) {
	/*
	 * Load an mbitmap, copy it to the bitplanes, and change the palette. 
	*/
	struct multibit_compressed *compressed_resource = mbit_source;

	// TODO we need a backend_allocate_temporary which allocates memory which is freed on the next frame. Or something -- we now have three types of memory allocation: for bitplanes, for long-running stuff (backend_reserve_memory), plus this new thing, so that's potentially bad.

	// Immediately after the fixed-length data we have palette entries.
	uint8_t *ptr = ((uint8_t *)(compressed_resource)) + (sizeof(struct multibit_compressed));
	set_palette_from_argb(compressed_resource->num_palette, (uint32_t *)ptr);

	ptr += (compressed_resource->num_palette * sizeof(uint32_t));
	// After the palette comes the lengths of each bitplane, and after these come the planes themselves.
	uint32_t *plane_lengths = (uint32_t *)ptr;
	ptr += (compressed_resource->num_planes * sizeof(uint32_t));

	// decompressor data structure
	int loc = heap_get_location();
	void *tinf_internal_data = heap_alloc(tinf_data_size());
	uint8_t *decompressed = heap_alloc((compressed_resource->width * compressed_resource->height) / 8);

	//APP_LOG(APP_LOG_LEVEL_DEBUG, "bitmap width %d dest plane0 stride %d", compressed_resource->width, backend_bitplane[0].stride);

	for(int i = 0; i < compressed_resource->num_planes; i++) {
		unsigned int uncompressed_size_out;
		tinf_zlib_uncompress(decompressed, &uncompressed_size_out, ptr, plane_lengths[i], tinf_internal_data);

		draw_1bit(compressed_resource->width, compressed_resource->height, decompressed, &dest_planes[i], 0, 0);

		ptr += plane_lengths[i];
	}
	
	heap_set_location(loc);
}
