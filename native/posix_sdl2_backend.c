/* A backend implementation for Unix-like systems with SDL2. */

// for real-time clock stuff
#define _POSIX_C_SOURCE 199309L
#define _DEFAULT_SOURCE

#include <SDL2/SDL.h>
#include <inttypes.h>
#include <stdbool.h>
#include <time.h>
#include <sys/time.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdarg.h>

#include "endian_compat.h"
#include "backend.h"
#include "minmax.h"
#include "iff-font.h"
#include "wad.h"
#include "choreography.h"
#include "choreography_commands.h"

SDL_Window *window;
SDL_Renderer *renderer;
SDL_Texture *texture; // which we will update every frame.
int window_width, window_height;

uint8_t *wad; // the entire wad

/* Frame rate the demo runs at. This doesn't affect the speed of the animations, 
 * which run at 25 fps */
#define MS_PER_FRAME 20

/* Current palette */
uint32_t palette[32];

uint32_t *framebuffer;

// The bitplanes
uint8_t *bitplane_pool_start, *bitplane_pool_next, *bitplane_pool_end;
struct Bitplane backend_bitplane[5];

// Set to -1 if no font loaded. If >= 0 a font
// is loaded and the bitplanes are valid.
int loaded_font_idx; 
// Bespoke artisanal bitplanes just for the font.
struct Bitplane font_bitplane[5];

uint64_t backend_get_time_ms()
{
#ifdef NO_POSIX_REALTIME_CLOCKS
	struct timeval tv;

	gettimeofday(&tv, NULL);

	return (uint64_t)(tv.tv_sec * 1000) + (tv.tv_usec / 1000);
#else
	struct timespec tp;

	clock_gettime(CLOCK_REALTIME, &tp);
	
	return (uint64_t)(tp.tv_sec * 1000) + (tp.tv_nsec / 1000000);
#endif
}


bool backend_should_display_next_frame(int64_t time_remaining_this_frame)
{
	if(time_remaining_this_frame > 0) {
		SDL_Event event;
		if(SDL_WaitEventTimeout(&event, time_remaining_this_frame) != 0) {
			switch(event.type) {
				case SDL_KEYUP:
					return false;
					break;
			}
		}
	}

	return true;
}

// NB not used at the moment because we want to do smooth scrolling with a pixel offset.
void backend_render_32bits()
{
	//SDL_RenderPresent(renderer);
	uint32_t *plane_idx_0 = (uint32_t *)backend_bitplane[0].data;
	uint32_t *plane_idx_1 = (uint32_t *)backend_bitplane[1].data;
	uint32_t *plane_idx_2 = (uint32_t *)backend_bitplane[2].data;
	uint32_t *plane_idx_3 = (uint32_t *)backend_bitplane[3].data;
	uint32_t *plane_idx_4 = (uint32_t *)backend_bitplane[4].data;

	int fb_idx = 0;

	for(int y = 0; y < window_height; y++) {
		int x = 0;
		while(x < window_width) {
			uint32_t plane0 = be32toh(*plane_idx_0++); // we always have at least 2 bitplanes (TODO?)
			uint32_t plane1 = be32toh(*plane_idx_1++); 
			uint32_t plane2 = plane_idx_2 ? be32toh(*plane_idx_2++) : 0;
			uint32_t plane3 = plane_idx_3 ? be32toh(*plane_idx_3++) : 0;
			uint32_t plane4 = plane_idx_4 ? be32toh(*plane_idx_4++) : 0;

			// Turn 32 bits from each plane into 32 pixels.
			unsigned bit;
			for(bit = 0x80000000UL; bit > 0x800000UL; bit >>= 1) {
				framebuffer[fb_idx + x] = palette[
					  ((plane0 & bit) ? 1 : 0)
					| ((plane1 & bit) ? 2 : 0)
					| ((plane2 & bit) ? 4 : 0)
					| ((plane3 & bit) ? 8 : 0)
					| ((plane4 & bit) ? 16 : 0)];

				x++;
			}
			for(; bit > 0x8000; bit >>= 1) {
				framebuffer[fb_idx + x] = palette[
					  ((plane0 & bit) ? 1 : 0)
					| ((plane1 & bit) ? 2 : 0)
					| ((plane2 & bit) ? 4 : 0)
					| ((plane3 & bit) ? 8 : 0)
					| ((plane4 & bit) ? 16 : 0)];

				x++;
			}
			for(; bit > 0x80; bit >>= 1) {
				framebuffer[fb_idx + x] = palette[
					  ((plane0 & bit) ? 1 : 0)
					| ((plane1 & bit) ? 2 : 0)
					| ((plane2 & bit) ? 4 : 0)
					| ((plane3 & bit) ? 8 : 0)
					| ((plane4 & bit) ? 16 : 0)];

				x++;
			}
			for(; bit; bit >>= 1) {
				framebuffer[fb_idx + x] = palette[
					  ((plane0 & bit) ? 1 : 0)
					| ((plane1 & bit) ? 2 : 0)
					| ((plane2 & bit) ? 4 : 0)
					| ((plane3 & bit) ? 8 : 0)
					| ((plane4 & bit) ? 16 : 0)];

				x++;
			}
		}

		fb_idx += window_width;
	}

	SDL_UpdateTexture(texture, NULL, framebuffer, window_width * 4);
	SDL_RenderCopy(renderer, texture, NULL, NULL);
	SDL_RenderPresent(renderer);
}

void backend_render()
{
	//SDL_RenderPresent(renderer);
	uint8_t *plane_idx_0 = (uint8_t *)backend_bitplane[0].data;
	uint8_t *plane_idx_1 = (uint8_t *)backend_bitplane[1].data;
	uint8_t *plane_idx_2 = (uint8_t *)backend_bitplane[2].data;
	uint8_t *plane_idx_3 = (uint8_t *)backend_bitplane[3].data;
	uint8_t *plane_idx_4 = (uint8_t *)backend_bitplane[4].data;

	int fb_idx = 0;

	for(int y = 0; y < window_height; y++) {
		int bitx = 0, x = 0;
		while(x < window_width) {
			uint8_t plane0 = plane_idx_0[bitx]; // we always have at least 2 bitplanes (TODO?)
			uint8_t plane1 = plane_idx_1[bitx]; 
			uint8_t plane2 = plane_idx_2 ? plane_idx_2[bitx] : 0;
			uint8_t plane3 = plane_idx_3 ? plane_idx_3[bitx] : 0;
			uint8_t plane4 = plane_idx_4 ? plane_idx_4[bitx] : 0;

			unsigned bit;
			for(bit = 0x80; bit; bit >>= 1) {
				framebuffer[fb_idx + x] = palette[
					  ((plane0 & bit) ? 1 : 0)
					| ((plane1 & bit) ? 2 : 0)
					| ((plane2 & bit) ? 4 : 0)
					| ((plane3 & bit) ? 8 : 0)
					| ((plane4 & bit) ? 16 : 0)];

				x++;
			}

			bitx++;
		}

		plane_idx_0 += backend_bitplane[0].stride;
		plane_idx_1 += backend_bitplane[1].stride;
		plane_idx_2 += backend_bitplane[2].stride;
		plane_idx_3 += backend_bitplane[3].stride;
		plane_idx_4 += backend_bitplane[4].stride;

		fb_idx += window_width;
	}

	SDL_UpdateTexture(texture, NULL, framebuffer, window_width * 4);
	SDL_RenderCopy(renderer, texture, NULL, NULL);
	SDL_RenderPresent(renderer);
}

static uint8_t *read_entire_wad(const char *filename) {
	uint8_t *buf;
	
	int h = open(filename, O_RDONLY);
	if(h < 0) {
		perror(filename);
		return NULL;
	}

	struct stat statbuf;
	if(fstat(h, &statbuf) != 0)  {
		perror("fstat");
		close(h);
		return NULL;
	}

	buf = malloc(statbuf.st_size);
	if(buf == NULL) {
		perror("malloc");
		close(h);
		return NULL;
	}

	ssize_t to_read = statbuf.st_size;
	uint8_t *current = buf;
	while(to_read) {
		ssize_t amt_read = read(h, current, to_read);
		if(amt_read < 0) {
			fprintf(stderr, "read_entire_file: error reading\n");
			close(h);
			free(buf);
			return NULL;
		}
		to_read -= amt_read;
		current += amt_read;
	}

	close(h);

	return buf;
}


bool backend_init(int width, int height, bool fullscreen, const void *wad_name)
{
	/* Read WAD */
	wad = read_entire_wad(wad_name ? (const char *)wad_name: "sota.wad");
	if(wad == NULL) {
		fprintf(stderr, "Couldn't read wad\n");
		return false;
	}

	/* Graphics initialisation */
	if(SDL_Init(SDL_INIT_AUDIO | SDL_INIT_VIDEO) != 0) {
		fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
		return false;
	}

	SDL_DisplayMode currentMode;

	if(SDL_GetCurrentDisplayMode(0, &currentMode) != 0) {
		fprintf(stderr, "SDL_GetCurrentDisplayMode: %s\n", SDL_GetError());
		return false;
	}

	if(fullscreen) {
		window = SDL_CreateWindow("State Of The Art", 0, 0, currentMode.w, currentMode.h, SDL_WINDOW_FULLSCREEN_DESKTOP | SDL_WINDOW_OPENGL);
	} else {
		window = SDL_CreateWindow("State Of The Art", 0, 0, width, height, SDL_WINDOW_OPENGL);
	}
	if(window == NULL) {
		fprintf(stderr, "SDL_CreateWindow: %s\n", SDL_GetError());
		return false;
	}

	SDL_GetWindowSize(window, &window_width, &window_height);

	renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
	if(renderer == NULL) {
		fprintf(stderr, "SDL_CreateRenderer: %s\n", SDL_GetError());
		return false;
	}

	texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, window_width, window_height);
	if(texture == NULL) {
		fprintf(stderr, "SDL_CreateTexture: %s\n", SDL_GetError());
		return false;
	}

	framebuffer = malloc(window_width * window_height * sizeof(uint32_t));

	// no mouse
	SDL_ShowCursor(SDL_DISABLE);

	SDL_RenderClear(renderer);
	SDL_RenderPresent(renderer);

	// reserve memory for a pool of bitplane allocations equal to 5 windows' worth of data.
	bitplane_pool_start = bitplane_pool_next = malloc(window_width * window_height * 5);
	if(bitplane_pool_start == NULL) {
		fprintf(stderr, "couldn't allocate bitplane memory\n");
		return false;
	}

	bitplane_pool_end = bitplane_pool_start + (window_width * window_height * 5);

	// Initially all bitplanes point to a single screen-wide display inside the pool.
	backend_allocate_standard_bitplanes();

	// Initially the font bitplanes are null.
	loaded_font_idx = -1;

	return true;
}

struct Bitplane *backend_allocate_bitplane(int idx, int width, int height)
{
	assert(backend_bitplane[idx].data_start == NULL);

	int stride = width / 8;

	backend_bitplane[idx].data_start = backend_bitplane[idx].data = bitplane_pool_next;
	bitplane_pool_next += (height * stride);
	
	if(bitplane_pool_next > bitplane_pool_end) {
		fprintf(stderr, "Bitplane alloc overflow\n");
		abort();
	}

	backend_bitplane[idx].width = width;
	backend_bitplane[idx].height = height;
	backend_bitplane[idx].stride = stride;

	return &backend_bitplane[idx];
}

void backend_allocate_standard_bitplanes() {
	backend_set_new_scene();

	for(int i = 0; i < 5; i++) {
		backend_allocate_bitplane(i, window_width, window_height);
	}
}

void backend_set_new_scene() {
	for(int i = 0; i < 5; i++) {
		backend_bitplane[i].data_start = backend_bitplane[i].data = NULL;
		backend_bitplane[i].width = backend_bitplane[i].height = backend_bitplane[i].stride = 0;
	}

	bitplane_pool_next = bitplane_pool_start;
}

void backend_copy_bitplane(struct Bitplane *dst, struct Bitplane *src)
{
	assert(dst->height == src->height && dst->stride == src->stride);

	memcpy(dst->data_start, src->data_start, src->stride * src->height);
}

void *backend_reserve_memory(size_t amt)
{
	void *mem = malloc(amt);
	if(mem == NULL) {
		perror("malloc");
		abort();
	}

	return mem;
}

void backend_shutdown()
{
	free(wad);
	free(framebuffer);

	SDL_DestroyRenderer(renderer);
	SDL_DestroyWindow(window);
	SDL_Quit();
}

void backend_get_palette(size_t num_elements, uint32_t *elements) {
	num_elements = min(num_elements, 32);

	memcpy(elements, palette, num_elements * sizeof(uint32_t));
}

void backend_set_palette(size_t num_elements, uint32_t *elements) {
	memcpy(palette, elements, num_elements * sizeof(uint32_t));
	for(int i = num_elements; i < 32; i++) {
		palette[i] = 0xff000000;
	}
}

void backend_set_palette_element(int idx, uint32_t element) {
	palette[idx] = element;
}

/* Fonts, using the iff-font code.
 *
 * Kind of annoying -- creates a separate font bitmap
 * and blits from it.
*/
#define FONT_PLANES_WIDTH 320
#define FONT_PLANES_HEIGHT 256
bool backend_font_load(int file_idx, uint32_t startchar, uint32_t numchars, uint16_t *positions)
{
	if(loaded_font_idx >= 0) {
		backend_font_unload();
	}

	for(int i = 0; i < 5; i++) {
		font_bitplane[i].width = FONT_PLANES_WIDTH;
		font_bitplane[i].height = FONT_PLANES_HEIGHT;
		font_bitplane[i].stride = FONT_PLANES_WIDTH / 8;
		font_bitplane[i].data_start
			= font_bitplane[i].data
			= malloc(font_bitplane[i].height * font_bitplane[i].stride);
		if(font_bitplane[i].data_start == NULL) {
			fprintf(stderr, "font_load: malloc fail\n");
			abort();
		}
	}

	bool result = ifffont_load(file_idx, startchar, numchars,
			positions, font_bitplane);

	if(result) {
		loaded_font_idx = file_idx;
	}

	return result;
}

void backend_font_unload()
{
	for(int i = 0; i < 5; i++) {
		if(font_bitplane[i].data_start) {
			free(font_bitplane[i].data);
			font_bitplane[i].data = font_bitplane[i].data_start = NULL;
			font_bitplane[i].width
				= font_bitplane[i].height
				= font_bitplane[i].stride 
				= 0;
		}
	}

	ifffont_unload();
}

// font drawing: pass -1 as x co-ordinate to centre 
void backend_font_draw(int numchars, char *text, int x, int y)
{
	if(x == -1) {
		ifffont_centre(numchars, text, y, backend_bitplane);
	} else {
		ifffont_draw(numchars, text, x, y, backend_bitplane);
	}
}


int backend_font_get_height()
{
	return ifffont_get_height();
}

void *backend_wad_load_choreography_for_scene_ms(int ms)
{
	uint8_t *choreography = wad + wad_get_choreography_offset(wad);

	return choreography + choreography_find_offset_for_scene(choreography, ms, NULL);
}

/* Load a file */
void *backend_wad_load_file(int file_idx, size_t *size_out)
{
	// in this backend the entire wad is mapped so no copying or loading required.
	uint32_t offset = wad_get_file_offset(wad, file_idx);

	if(size_out != NULL) {
		*size_out = wad_get_file_size(wad, file_idx);
	}

	return (void *)(wad + offset);
}

void backend_wad_unload_file(void *data)
{
	/* wad is permanently loaded */
}

void backend_run(int ms)
{
	uint64_t starttime = backend_get_time_ms();

	/* If 'ms' is initially >0, backdate startime */
	starttime -= ms;

	uint8_t *choreography = backend_wad_load_choreography_for_scene_ms(ms);
	choreography_prepare_to_run(choreography, ms);

	bool keepgoing = true;
	while(keepgoing) {
		uint64_t frametime = backend_get_time_ms();
		ms = frametime - starttime;

		choreography_do_frame(ms);

		int64_t time_remaining_this_frame = MS_PER_FRAME - (backend_get_time_ms() - frametime);

		backend_render();

		keepgoing = backend_should_display_next_frame(time_remaining_this_frame);
	}

	backend_wad_unload_file(choreography);
}

void backend_debug(const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	putc('\n', stdout);

}

int backend_random()
{
	return rand();
}

