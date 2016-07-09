/* A backend implementation for Unix-like systems with SDL2. */

#include <SDL2/SDL.h>
#include <inttypes.h>
#include <stdbool.h>
#include <time.h>
#include <sys/time.h>

#include "backend.h"

SDL_Window *window;
SDL_Renderer *renderer;
SDL_Texture *texture; // which we will update every frame.
int window_width, window_height;
uint32_t *framebuffer;  /* size window_width * window_height ( * 4) */

static uint64_t get_time_ms()
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


static bool should_display_next_frame(int64_t time_remaining_this_frame)
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

/* Graphics handling.
 *
 * In the SDL port, this is managed by writing to several "bit planes" and then combining them 
 * before display.
*/
static void planar_line_horizontal(int bitplane_idx, int y, int start_x, int end_x, bool xor)
{
	y = min(max(y, 0), bitplane_height - 1);
	start_x = max(min(start_x, bitplane_width - 1), 0);
	end_x = min(max(end_x, 0), bitplane_width - 1);

	if(start_x > end_x)
		return;

	uint8_t *bitplane = plane[bitplane_idx];
	uint8_t pen = 1 << bitplane_idx;
	
	bitplane += (y * bitplane_width) + start_x;

	if(xor) {
		for(int length = end_x - start_x; length; length--) {
			*bitplane++ ^= pen;
		}
	} else {
		for(int length = end_x - start_x; length; length--) {
			*bitplane++ = pen;
		}
	}
}

static void planar_line_vertical(int bitplane_idx, int x, int start_y, int end_y, bool xor)
{
	x = min(max(x, 0), bitplane_width - 1);
	start_y = max(min(start_y, bitplane_height - 1), 0);
	end_y = min(max(end_y, 0), bitplane_height - 1);

	if(start_y > end_y)
		return;

	uint8_t *bitplane = plane[bitplane_idx];
	uint8_t pen = 1 << bitplane_idx;
	
	bitplane += (start_y * bitplane_width) + x;

	if (xor) {
		for(int length = end_y - start_y; length; length--) {
			*bitplane ^= pen;
			bitplane += bitplane_width;
		}
	} else {
		for(int length = end_y - start_y; length; length--) {
			*bitplane = pen;
			bitplane += bitplane_width;
		}
	}
}

static void graphics_planar_render()
{
	//SDL_RenderPresent(renderer);
	uint8_t *plane_idx_0 = plane[0] + plane_offset[0];
	uint8_t *plane_idx_1 = plane[1] + plane_offset[1];
	uint8_t *plane_idx_2 = plane[2] + plane_offset[2];
	uint8_t *plane_idx_3 = plane[3] + plane_offset[3];
	uint8_t *plane_idx_4 = plane[4] + plane_offset[4];

	int fb_idx = 0;

	float copper_divisor = ((float)window_width) / 320.0 * 8.0;

	for(int y = 0; y < window_height; y++) {
		int this_copper, last_copper = -1;

		for(int x = 0; x < window_width; x++) {
			if(copper_effect_handler != NULL) {
				this_copper = x / copper_divisor;
				if(this_copper > last_copper) {
					int copper_y = y * 256 / window_height;

					(copper_effect_handler)(this_copper, copper_y, palette);
					last_copper = this_copper;
				}
			}

			// TODO this is pretty horrible, should go planar.
			uint8_t colour = (*(plane_idx_0 + x) ? 1 : 0)|
				(*(plane_idx_1 + x) ? 2 : 0) |
				(*(plane_idx_2 + x) ? 4 : 0) |
				(*(plane_idx_3 + x) ? 8 : 0) |
				(*(plane_idx_4 + x) ? 16: 0);
			framebuffer[fb_idx + x] = palette[colour];
		}

		plane_idx_0 += bitplane_width;
		plane_idx_1 += bitplane_width;
		plane_idx_2 += bitplane_width;
		plane_idx_3 += bitplane_width;
		plane_idx_4 += bitplane_width;
		fb_idx += window_width;
	}

	SDL_UpdateTexture(texture, NULL, framebuffer, window_width * 4);
	SDL_RenderCopy(renderer, texture, NULL, NULL);
	SDL_RenderPresent(renderer);
}

static bool init(struct backend_interface_struct *backend, bool fullscreen)
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

	backend->width = window_width;
	backend->height = window_height;
	backend->bitplane_stride = window_width / 8;

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
	if(framebuffer == NULL) {
		perror("framebuffer");
		return false;
	}

	// no mouse
	SDL_ShowCursor(SDL_DISABLE);

	SDL_RenderClear(renderer);
	SDL_RenderPresent(renderer);

	return true;
}

static void get_window_dimensions(int *width, int *height) {
	*width = window_width;
	*height = window_height;
}

static void shutdown()
{
	SDL_DestroyRenderer(renderer);
	SDL_DestroyWindow(window);
	SDL_Quit();
}

struct backend_interface_struct g_backend = {
	.init = init,
	.shutdown = shutdown,
	.get_time_ms = get_time_ms,
	.should_display_next_frame = should_display_next_frame,
	.planar_line_horizontal = planar_line_horizontal,
	.planar_line_vertical = planar_line_vertical,
	.render = graphics_planar_render,
};

struct backend_interface_struct *get_posix_backend() {
	return &posix_sdl2_backend;
}

