#include <pebble.h>
#include <inttypes.h>

// Local includes
#include "pebble-bitmaps.h"
#include "lame_vsprintf.h"

#include "../heap.h"
#include "../tinf/src/tinf.h"
#include "../../backend.h"
#include "../../choreography.h"
#include "../../wad.h"
#include "../../anim.h"
#include "../../graphics.h"
#include "../../scene.h"

/* data structures we use */
static Window *sotaface_window;

/* Memory low-water mark prior to scene: this is stuff loaded prior to the actual scene animation which will
 * be used throughout the scene -- choreography stuff essentially.
 * When a new scene is started, we rewind memory back to this point.
*/
int heap_scene_low_water_mark = -1;

#define WAD_CENTRAL_DIRECTORY_SIZE 512
#define WAD_SCENE_INDEX_SIZE 512
uint8_t *wad; // just the central directory portion.
uint8_t *choreography; // the current scene.
bool scene_has_ended;
ResHandle wad_handle;

uint32_t ms; // current millisecond of demo to display.

static uint8_t *read_wad_portion(uint32_t start, size_t count)
{
	uint8_t *data = heap_alloc(count);
	resource_load_byte_range(wad_handle, start, data, count);

	return data;
}

void *backend_wad_load_choreography_for_scene_ms(int ms)
{
	uint32_t choreography_offset = wad_get_choreography_offset(wad);

	APP_LOG(APP_LOG_LEVEL_DEBUG, "Read scene index");
	uint8_t *scene_index = read_wad_portion(choreography_offset, WAD_SCENE_INDEX_SIZE);

	uint32_t next_scene_offset;
	uint32_t scene_offset = choreography_find_offset_for_scene(scene_index, ms, &next_scene_offset);

	bool freed = heap_free(scene_index);
	APP_LOG(APP_LOG_LEVEL_DEBUG, "Freed scene index %d, load choreography %lu:%lu", freed, scene_offset, next_scene_offset);
	uint8_t *data = read_wad_portion(choreography_offset + scene_offset, next_scene_offset - scene_offset);
	APP_LOG(APP_LOG_LEVEL_DEBUG, "choreography offset %lu scene offset %lu amt read %lu data %p",
			choreography_offset, scene_offset, next_scene_offset - scene_offset, data);

	return data;
}

static void sotaface_load_scene_choreography()
{
	/* Load the relevant choreography portion */
	choreography = backend_wad_load_choreography_for_scene_ms(ms);
	choreography_prepare_to_run(choreography, ms);
}

static void sotaface_root_layer_update(Layer *root, GContext *ctx)
{
	// This is called to redraw the window's root layer. This is where 
	// the SOTA demo graphics go.

	/*
	GRect rect = GRect(0, 0, 180, 180);
	graphics_draw_bitmap_in_rect(ctx, bmp, rect);
	*/
	// TODO calculate ms!
	//
	APP_LOG(APP_LOG_LEVEL_DEBUG, "root layer update"); // TODO
	if(scene_has_ended) {
		heap_set_location(heap_scene_low_water_mark);
		APP_LOG(APP_LOG_LEVEL_DEBUG, "scene ended; load choreography"); // TODO
		sotaface_load_scene_choreography();
	}

	APP_LOG(APP_LOG_LEVEL_DEBUG, "do frame"); // TODO
	scene_has_ended = choreography_do_frame(ms);
	APP_LOG(APP_LOG_LEVEL_DEBUG, "eof? %d. render...", scene_has_ended); // TODO
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
	heap_reset();

	tinf_init();

	anim_init();

	graphics_init();

	scene_init();

#if 0
	APP_LOG(APP_LOG_LEVEL_DEBUG, "allocate bitplanes");
	backend_allocate_standard_bitplanes();
//	pebble_clear_bitplanes();

	APP_LOG(APP_LOG_LEVEL_DEBUG, "write bitplane data");
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
#endif

	//load_mbitmap(resource_get_handle(RESOURCE_ID_art_mbit));

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

	/* Load the wad's central directory */
	wad_handle = resource_get_handle(RESOURCE_ID_wad);
	wad = read_wad_portion(0, WAD_CENTRAL_DIRECTORY_SIZE);

	/* Start at ms = 0 for now */
	//ms = 28000;
	ms = 34480;
	scene_has_ended = false;

	/* Setting this will ensure that the first scene choreography is loaded when the watchface is drawn. */
	choreography = NULL;
	scene_has_ended = true;

	/* Between scenes, all memory allocated after this point will be discarded */
	heap_scene_low_water_mark = heap_get_location();
}

static void sotaface_deinit(void)
{
	window_destroy(sotaface_window);
}

void backend_debug(const char *fmt, ...)
{
	char buf[256];
	va_list ap;
	va_start(ap, fmt);
	lame_vsprintf(buf, fmt, ap);
	va_end(ap);

	app_log(APP_LOG_LEVEL_DEBUG, "?", 0, "%s", buf);
}

int backend_random()
{
	return rand();
}

void backend_set_new_scene()
{
	// This is unused here because everything is deleted already when a new scene starts -- see render routines above.
}

void *backend_wad_load_file(int file_idx, size_t *size_out)
{
	return read_wad_portion(wad_get_file_offset(wad, file_idx), wad_get_file_size(wad, file_idx));
}

void backend_wad_unload_file(void *data)
{
	if(!heap_free(data)) {
		APP_LOG(APP_LOG_LEVEL_DEBUG, "backend_wad_unload_file: couldn't free");
	}
}

int main(void) {
	APP_LOG(APP_LOG_LEVEL_DEBUG, "sotaface main");
	sotaface_init();

	APP_LOG(APP_LOG_LEVEL_DEBUG, "Done initializing, pushed window: %p", sotaface_window);

	app_event_loop();
	sotaface_deinit();
}

