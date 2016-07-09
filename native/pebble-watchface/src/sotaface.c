#include <pebble.h>
#include <inttypes.h>

#include "../tinf/src/tinf.h"

/* data structures we use */
struct onebit_compressed {
	uint32_t signature; // "img1"
	uint16_t width;
	uint16_t height;
	// compressed data follows
};

/* Bitmaps: memory representation */
struct onebit_bitmap {
	int width, height;
	uint8_t data[]; // uncompressed data follows
};

static Window *sotaface_window;

/* heap management 
 * We use a separate heap for bump pointers, because why not
*/
#define HEAP_SIZE (8 * 1024)
#define MAX_ALLOC 8

static uint8_t heap[HEAP_SIZE];
static void *heap_allocs[MAX_ALLOC];
static int next_alloc_ptr; // index into heap_allocs

/* Bitplanes */
static uint8_t bitplane[4][180 * 180 / 8];

// Current bitmap being displayed. TODO?
struct onebit_bitmap *s_bmp;

// Heap management 
static void heap_reset() {
	next_alloc_ptr = 0;
	heap_allocs[0] = heap;
}

static size_t align(int amt, int align) {
	return amt + ((align - (amt % align)) % align);
}

static void *heap_alloc(size_t size)
{
	if(next_alloc_ptr == MAX_ALLOC)
		return NULL;

	size = align(size, sizeof(uintptr_t));
	void *start = heap_allocs[next_alloc_ptr];

	next_alloc_ptr ++;
	heap_allocs[next_alloc_ptr] = start + size;
	return start;
}

static void heap_pop()
{
	if(next_alloc_ptr > 0) {
		next_alloc_ptr--;
	}
}

static size_t heap_avail()
{
	return HEAP_SIZE - ( ((uint8_t *)heap_allocs[next_alloc_ptr]) - heap);
}

static int byte_get_bit(uint8_t byte, uint8_t bit) {
  return ((byte) >> bit) & 1;
}

static void draw_1bit(GBitmap *fb)
{
}

static void sotaface_root_layer_update(Layer *root, GContext *ctx)
{
	// This is called to redraw the window's root layer. This is where 
	// the SOTA demo graphics go.

	/*
	GRect rect = GRect(0, 0, 180, 180);
	graphics_draw_bitmap_in_rect(ctx, bmp, rect);
	*/

	GBitmap *fb = graphics_capture_frame_buffer(ctx);


	// Raw code to draw a 1bit bitmap. Ref https://developer.pebble.com/guides/graphics-and-animations/framebuffer-graphics/
	//

	GRect bounds = layer_get_bounds(root);
	int bmp_bytes_per_row = s_bmp->width / 8;

	for(int y = 0; y < bounds.size.h; y++) {
		GBitmapDataRowInfo info = gbitmap_get_data_row_info(fb, y);

		int bmp_y_offset = (y * bmp_bytes_per_row);

		for(int x = info.min_x; x < info.max_x; x++) {
			int px = byte_get_bit(s_bmp->data[bmp_y_offset + (x / 8)], 7 - (x % 8));

			GColor color = px ? GColorWhite : GColorBlack;

			//memset(&info.data[x], color.argb, 1);
			info.data[x] = color.argb;
		}
	}

	// end 1bit bitmap code
	
	graphics_release_frame_buffer(ctx, fb);
}

static void window_load(Window *window)
{
}

static void window_unload(Window *window)
{
}

static struct onebit_bitmap *load_1bit(ResHandle handle) {
	size_t size_in = resource_size(handle);

	struct onebit_compressed *compressed_resource = heap_alloc(size_in);
	resource_load(handle, (void *)compressed_resource, size_in);

	uint8_t *compressed_data = ((uint8_t *)compressed_resource) + sizeof(struct onebit_compressed);

	// TODO check header signature
	unsigned int decomp_size = (compressed_resource->width * compressed_resource->height) / 8;

	struct onebit_bitmap *bitmap = heap_alloc(decomp_size + sizeof(struct onebit_bitmap));
	bitmap->width = compressed_resource->width;
	bitmap->height = compressed_resource->height;

	// decompressor data structure
	void *tinf_internal_data = heap_alloc(tinf_data_size());

	tinf_zlib_uncompress(bitmap->data, &decomp_size, compressed_data,
			size_in - sizeof(struct onebit_compressed), tinf_internal_data);

	// pop decompressor data struct
	heap_pop();

	return bitmap;
}

static void sotaface_init(void)
{
	tinf_init();

	heap_reset();

	// Load PDCs (TODO?)
	//s_pdc = gdraw_command_image_create_with_resource(RESOURCE_ID_state_pdc);
	//bmp = gbitmap_create_with_resource(RESOURCE_ID_art_bmp);
	s_bmp = load_1bit(resource_get_handle(RESOURCE_ID_state_1bit));

	// create a window...
	sotaface_window = window_create();
	window_set_window_handlers(sotaface_window, (WindowHandlers) {
			.load = window_load,
			.unload = window_unload});

	// ... and show it.
	window_stack_push(sotaface_window, true);

	// Register for root layer updates -- this is where our main drawing happens.
	Layer *root_layer = window_get_root_layer(sotaface_window);
	layer_set_update_proc(root_layer, sotaface_root_layer_update);
}

static void sotaface_deinit(void)
{
	window_destroy(sotaface_window);
}

int main(void) {
  sotaface_init();

  APP_LOG(APP_LOG_LEVEL_DEBUG, "Done initializing, pushed window: %p", sotaface_window);

  app_event_loop();
  sotaface_deinit();
}
