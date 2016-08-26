#include <pebble.h>
#include <inttypes.h>

// Local includes
#include "heap.h"
#include "pebble-bitmaps.h"

#include "../tinf/src/tinf.h"
#include "../../backend.h"

/* data structures we use */
static Window *sotaface_window;


static void sotaface_root_layer_update(Layer *root, GContext *ctx)
{
	// This is called to redraw the window's root layer. This is where 
	// the SOTA demo graphics go.

	/*
	GRect rect = GRect(0, 0, 180, 180);
	graphics_draw_bitmap_in_rect(ctx, bmp, rect);
	*/
	render_bitplanes(root, ctx);

	//draw_1bit(fb);

}

static void window_load(Window *window)
{
}

static void window_unload(Window *window)
{
}

static void sotaface_init(void)
{
	tinf_init();

	heap_reset();

	backend_allocate_standard_bitplanes();
//	pebble_clear_bitplanes();

	for(int y = 0; y < 180; y++) {
		for(int x = 0; x < (184 / 8); x++) {
			if(x % 2) {
				backend_bitplane[0].data[(y * (184 / 8)) + x] = 0b10101010;
				backend_bitplane[1].data[(y * (184 / 8)) + x] = 0;
			}
			else {
				backend_bitplane[0].data[(y * (184 / 8)) + x] = 0b00000000;
				backend_bitplane[1].data[(y * (184 / 8)) + x] = 0b10101010;
			}
			backend_bitplane[2].data[(y * (184 / 8)) + x] = 0;
			backend_bitplane[3].data[(y * (184 / 8)) + x] = 0;
		}
	}

	load_mbitmap(resource_get_handle(RESOURCE_ID_art_mbit));

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

