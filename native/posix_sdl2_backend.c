/* A backend implementation for Unix-like systems with SDL2. */

// for real-time clock stuff
#define _POSIX_C_SOURCE 199309L

#include <SDL2/SDL.h>
#include <inttypes.h>
#include <stdbool.h>
#include <time.h>
#include <sys/time.h>
#include <assert.h>

#include "backend.h"
#include "minmax.h"

SDL_Window *window;
SDL_Renderer *renderer;
SDL_Texture *texture; // which we will update every frame.
int window_width, window_height;

/* Current palette */
uint32_t palette[32];

uint32_t *framebuffer;

// The bitplanes
uint8_t *bitplane_pool_start, *bitplane_pool_next, *bitplane_pool_end;
struct Bitplane backend_bitplane[5];

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

void backend_render()
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
			uint32_t plane0 = *plane_idx_0++; // we always have at least 2 bitplanes (TODO?)
			uint32_t plane1 = *plane_idx_1++; 
			uint32_t plane2 = plane_idx_2 ? *plane_idx_2++ : 0;
			uint32_t plane3 = plane_idx_3 ? *plane_idx_3++ : 0;
			uint32_t plane4 = plane_idx_4 ? *plane_idx_4++ : 0;

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


bool backend_init(bool fullscreen)
{
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
		window = SDL_CreateWindow("State Of The Art", 0, 0, 640, 512, SDL_WINDOW_OPENGL);
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
	backend_delete_bitplanes();

	for(int i = 0; i < 5; i++) {
		backend_allocate_bitplane(i, window_width, window_height);
	}
}

void backend_delete_bitplanes() {
	for(int i = 0; i < 5; i++) {
		backend_bitplane[i].data_start = backend_bitplane[i].data = NULL;
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

