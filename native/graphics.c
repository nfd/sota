#include <stdio.h>
#include <math.h>
#include <inttypes.h>
#include <SDL2/SDL.h>

SDL_Window *window;
SDL_Renderer *renderer;
SDL_Texture *texture; // which we will update every frame.

int window_width, window_height;

uint32_t *framebuffer;  /* size window_width * window_height ( * 4) */

/* Bitplane-based rendering into 'chunky bitplanes'. */
#define NUM_BITPLANES 5
uint8_t *plane[NUM_BITPLANES];

/* For now, bitplanes are 2x the width and height of the window */
int bitplane_width, bitplane_height;

/* Current palette */
uint32_t palette[32];

/* Data related to the polygon fill algorithm. Must be initialised because it depends on the window height. */
struct poly_elem {
	float xmin;
	int ymax;
	float grad_recip;
	struct poly_elem *prev;
	int ymin;
};

struct poly_elem **edge_table;

int graphics_width() {
	return window_width;
}

int graphics_height() {
	return window_height;
}

int graphics_init() {
	SDL_DisplayMode currentMode;

	if(SDL_GetCurrentDisplayMode(0, &currentMode) != 0) {
		fprintf(stderr, "SDL_GetCurrentDisplayMode: %s\n", SDL_GetError());
		return -1;
	}

	window = SDL_CreateWindow("State Of The Art", 0, 0, currentMode.w, currentMode.h, SDL_WINDOW_FULLSCREEN_DESKTOP | SDL_WINDOW_OPENGL);
	if(window == NULL) {
		fprintf(stderr, "SDL_CreateWindow: %s\n", SDL_GetError());
		return -1;
	}

	renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
	if(renderer == NULL) {
		fprintf(stderr, "SDL_CreateRenderer: %s\n", SDL_GetError());
		return -1;
	}

	window_width = currentMode.w;
	window_height = currentMode.h;

	texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, window_width, window_height);
	if(texture == NULL) {
		fprintf(stderr, "SDL_CreateTexture: %s\n", SDL_GetError());
		return -1;
	}

	framebuffer = malloc(window_width * window_height * sizeof(uint32_t));
	if(framebuffer == NULL) {
		perror("framebuffer");
		return -1;
	}

	// edge_table is used by the scanline polygon rendering algorithm
	edge_table = malloc(window_height * sizeof(struct poly_elem *));
	if(edge_table == NULL) {
		perror("edge_table");
		return -1;
	}

	/* Create the bitplanes. */
	bitplane_width = window_width * 2;
	bitplane_height = window_height * 2;
	for(int i = 0; i < NUM_BITPLANES; i++) {
		plane[i] = malloc(bitplane_width * bitplane_height);
		if(plane[i] == NULL) {
			fprintf(stderr, "bitplane alloc\n");
			return -1; // we don't clean up but this is fatal anyway
		}
	}

	SDL_RenderClear(renderer);
	SDL_RenderPresent(renderer);

	return 0;
}

int graphics_shutdown() {
	SDL_DestroyRenderer(renderer);
	SDL_DestroyWindow(window);

	for(int i = 0; i < NUM_BITPLANES; i++) {
		free(plane[i]);
		plane[i] = NULL;
	}

	return 0;
}

void graphics_set_palette(size_t num_elements, uint32_t *elements) {
	memcpy(palette, elements, num_elements * sizeof(uint32_t));
	if(num_elements < 32) {
		memset(&palette[num_elements], 0, sizeof(uint32_t) * (32 - num_elements));
	}
}

static void planar_line_horiontal(int bitplane_idx, int y, int start_x, int end_x)
{
	if(start_x >= end_x)
		return;

	uint8_t *bitplane = plane[bitplane_idx];
	uint8_t pen = 1 << bitplane_idx;
	
	bitplane += (y * bitplane_width) + start_x;
	for(int length = end_x - start_x; length; length--) {
		*bitplane++ = pen;
	}
}

/* Heart of everything! */

#define MAX_LINES 1024
static struct poly_elem line_info[MAX_LINES];
static struct poly_elem *active_list[MAX_LINES];
static int next_active_list = 0;

static inline void add_active(struct poly_elem *new_elem)
{
	active_list[next_active_list ++] = new_elem;
}

static inline void del_active(int idx)
{
	for(int i = idx + 1; i < next_active_list; i++) {
		active_list[i - 1] = active_list[i];
	}
	next_active_list --;
}

static int active_list_comparator(const void *lhs, const void *rhs) {
	const struct poly_elem * const*pleft = lhs;
	const struct poly_elem * const*pright = rhs;
	if((*pleft)->xmin < (*pright)->xmin)
		return -1;
	if((*pleft)->xmin > (*pright)->xmin)
		return 1;
	return 0;
}

int graphics_draw_filled_scaled_polygon_to_bitmap(int num_vertices, uint8_t *data, int scale, int xofs, int yofs, int bitplane_idx)
{
	/* Polygon fill algorithm */
	// The following tutorial was most helpful:
	// https://www.cs.uic.edu/~jbell/CourseNotes/ComputerGraphics/PolygonFilling.html
	// This tutorial cleared up some corner cases:
	// http://web.cs.ucdavis.edu/~ma/ECS175_S00/Notes/0411_b.pdf

	// Line info entry is [xmin, ymax, 1/m, previous-idx, ymin]
	// line info is indexed from the edge table: edge table for a given y co-ordinate (ymin)
	// is an index into the line_info table.
	int next_line_info = 0;
	int i;
	int global_ymin = window_height;
	int global_ymax = 0;

	/* Clear edge table */
	memset(edge_table, 0, window_height * sizeof(struct poly_elem *));

	/* Fill edge_table and line_info*/
	for(i=0; i < (num_vertices * 2); i+= 2) {
		int y0, x0, y1, x1;

		y0 = data[i] * scale;
		x0 = data[i+1] * scale;

		if(i == (num_vertices * 2) - 2) {
			// close the shape
			y1 = data[0] * scale;
			x1 = data[1] * scale;
		} else {
			y1 = data[i+2] * scale;
			x1 = data[i+3] * scale;
		}

		if(y0 == y1) {
			// Horizontal edge; just ignore it.
			continue;
		}

		// ensure y0 <= y1
		if(y0 > y1) {
			int tmp;
			tmp = y0; y0 = y1; y1 = tmp;
			tmp = x0; x0 = x1; x1 = tmp;
		}

		if(y0 < global_ymin)
			global_ymin = y0; // highest point of the polygon.

		if(y1 > global_ymax) 
			global_ymax = y1;

		struct poly_elem *elem = &(line_info[next_line_info++]);
		elem->xmin = x0;
		elem->ymax = y1;
		elem->grad_recip = ((float)(x1 - x0)) / ((float)(y1 - y0));
		elem->prev = edge_table[y0];
		elem->ymin = y0;

		edge_table[y0] = elem;
	}

	// Active edge table: subset of the edge table which is currently being drawn.
	next_active_list = 0; // reset the active list

	// Scaline algorithm.
	for(int y = global_ymin; y <= global_ymax; y++) {
		struct poly_elem *active;

		// Remove active edges where ymax == y
		for(i = 0; i < next_active_list; /* empty */) {
			if(active_list[i]->ymax == y) {
				del_active(i);
			} else {
				i++;
			}
		}

		// Move all edges with ymin == y into the active line table.
		active = edge_table[y];
		while(active) {
			add_active(active);
			active = active->prev;
		}

		// Sort the active edge table on xmin 
		qsort(active_list, next_active_list, sizeof(struct poly_elem *), active_list_comparator);

		// Draw the lines.
		int is_drawing = 0;
		int prev_x = -1;
		for(i = 0; i < next_active_list; i++) {
			int next_x = is_drawing ? floorf(active_list[i]->xmin) : ceilf(active_list[i]->xmin);

			if(is_drawing) {
				// TODO: this sometimes results in lines where prev_x = next_x - 2. Figure out why.
				planar_line_horiontal(bitplane_idx, y + yofs, xofs + prev_x, xofs + next_x);
			}

			if(prev_x != next_x
					|| ((active_list[i - 1]->ymin == y) || (active_list[i]->ymin == y)) ) {
				is_drawing = 1 - is_drawing;
			} 

			prev_x = next_x;
		}

		// Update the gradients in AET.
		for(i = 0; i < next_active_list; i++) {
			active_list[i]->xmin += active_list[i]->grad_recip;
		}
	}
}

/* Combine bitplanes */
void graphics_planar_render()
{
	//SDL_RenderPresent(renderer);
	for(int y = 0; y < window_height; y++) {
		for(int x = 0; x < window_width; x++) {
			int idx = (y * bitplane_width) + x;

			uint8_t colour = plane[0][idx] | plane[1][idx] | plane[2][idx] | plane[3][idx];
			framebuffer[ (y * window_width) + x] = palette[colour];
		}
	}

	SDL_UpdateTexture(texture, NULL, framebuffer, window_width * 4);
	SDL_RenderCopy(renderer, texture, NULL, NULL);
	SDL_RenderPresent(renderer);
}

