#include <stdio.h>
#include <math.h>
#include <inttypes.h>
#include <stdbool.h>
#include <SDL2/SDL.h>

#include "graphics.h"

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

int plane_offset[NUM_BITPLANES];

/* Current palette */
uint32_t palette[32];

/* Data related to the polygon fill algorithm. Must be initialised because it depends on the window height. */
struct poly_elem {
	float xcurr;
	int ymax;
	float grad_recip;
	struct poly_elem *prev;
	// original vertex info
	int ymin;
	int xmin;
};

struct poly_elem **edge_table;

int graphics_width() {
	return window_width;
}

int graphics_height() {
	return window_height;
}

int graphics_bitplane_width() {
	return bitplane_width;
}

int graphics_bitplane_height() {
	return bitplane_height;
}

int graphics_bitplane_stride() {
	return bitplane_width;
}

void graphics_bitplane_set_offset(int bitplane_idx, int x, int y)
{
	int offset = (bitplane_width * y) + x;

	plane_offset[bitplane_idx] = offset;
}

int graphics_init(bool fullscreen) {
	SDL_DisplayMode currentMode;

	if(SDL_GetCurrentDisplayMode(0, &currentMode) != 0) {
		fprintf(stderr, "SDL_GetCurrentDisplayMode: %s\n", SDL_GetError());
		return -1;
	}

	if(fullscreen) {
		window = SDL_CreateWindow("State Of The Art", 0, 0, currentMode.w, currentMode.h, SDL_WINDOW_FULLSCREEN_DESKTOP | SDL_WINDOW_OPENGL);
	} else {
		window = SDL_CreateWindow("State Of The Art", 0, 0, 640, 512, SDL_WINDOW_OPENGL);
	}
	if(window == NULL) {
		fprintf(stderr, "SDL_CreateWindow: %s\n", SDL_GetError());
		return -1;
	}

	SDL_GetWindowSize(window, &window_width, &window_height);

	renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
	if(renderer == NULL) {
		fprintf(stderr, "SDL_CreateRenderer: %s\n", SDL_GetError());
		return -1;
	}

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
		plane_offset[i] = 0;
	}

	// no mouse
	SDL_ShowCursor(SDL_DISABLE);

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
	for(int i = num_elements; i < 32; i++) {
		palette[i] = 0xff000000;
	}
}

void graphics_get_palette(size_t num_elements, uint32_t *elements) {
	num_elements = min(num_elements, 32);

	memcpy(elements, palette, num_elements * sizeof(uint32_t));
}

static inline uint32_t lerp_byte(int idx, uint32_t byte_from, uint32_t byte_to, int current_step, int total_steps)
{
	if(current_step == 0) 
		return byte_from;

	byte_from >>= (idx * 8);
	byte_from &= 0xff;

	byte_to >>= (idx * 8);
	byte_to &= 0xff;

	int lerped = ((((int)byte_to) - ((int)byte_from)) * current_step) / total_steps;

	return min(0xff, max(byte_from + lerped, 0));
}

void graphics_lerp_palette(size_t num_elements, uint32_t *from, uint32_t *to, int current_step, int total_steps) {
	if(current_step < 0 || current_step > total_steps || num_elements > 32)
		return;

	for(int i = 0; i < num_elements; i++) {
		palette[i] = (lerp_byte(3, from[i], to[i], current_step, total_steps) << 24)
			| (lerp_byte(2, from[i], to[i], current_step, total_steps) << 16)
			| (lerp_byte(1, from[i], to[i], current_step, total_steps) << 8)
			| (lerp_byte(0, from[i], to[i], current_step, total_steps));
	}
}

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

static inline void planar_putpixel(int bitplane_idx, int x, int y)
{
	if(x >= 0 && x < bitplane_width && y >= 0 && y <= bitplane_height)
		plane[bitplane_idx][(y * bitplane_width) + x] = (1 << bitplane_idx);
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
	if((*pleft)->xcurr < (*pright)->xcurr)
		return -1;
	if((*pleft)->xcurr > (*pright)->xcurr)
		return 1;
	return 0;
}

void graphics_draw_filled_scaled_polygon_to_bitmap(int num_vertices, uint8_t *data, float scalex, float scaley, int xofs, int yofs, int bitplane_idx, bool xor)
{
	/* Polygon fill algorithm */
	// The following tutorial was most helpful:
	// https://www.cs.uic.edu/~jbell/CourseNotes/ComputerGraphics/PolygonFilling.html
	// This tutorial cleared up some corner cases:
	// http://web.cs.ucdavis.edu/~ma/ECS175_S00/Notes/0411_b.pdf

	// Line info entry is [xcurr, ymax, 1/m, previous-idx, ymin]
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

		y0 = data[i] * scaley;
		x0 = data[i+1] * scalex;

		if(i == (num_vertices * 2) - 2) {
			// close the shape
			y1 = data[0] * scaley;
			x1 = data[1] * scalex;
		} else {
			y1 = data[i+2] * scaley;
			x1 = data[i+3] * scalex;
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

		y0 = min(max(0, y0), window_height - 1);
		y1 = max(min(window_height - 1, y1), 0);

		x0 = min(max(0, x0), window_width - 1);
		x1 = max(min(window_width - 1, x1), 0);

		if(y0 < global_ymin)
			global_ymin = y0; // highest point of the polygon.

		if(y1 > global_ymax) 
			global_ymax = y1;

		struct poly_elem *elem = &(line_info[next_line_info++]);
		elem->xcurr = x0;
		elem->ymax = y1;
		elem->grad_recip = ((float)(x1 - x0)) / ((float)(y1 - y0));
		elem->prev = edge_table[y0];
		elem->ymin = y0;
		elem->xmin = x0;

		edge_table[y0] = elem;
	}

	// Active edge table: subset of the edge table which is currently being drawn.
	next_active_list = 0; // reset the active list

	// Scaline algorithm.
	for(int y = global_ymin; y <= global_ymax; y++) {
		struct poly_elem *active;

		// Move all edges with ymin == y into the active line table.
		active = edge_table[y];
		while(active) {
			add_active(active);
			active = active->prev;
		}

		// Sort the active edge table on xcurr 
		qsort(active_list, next_active_list, sizeof(struct poly_elem *), active_list_comparator);

		// Draw the lines.
		int is_drawing = 0;
		int prev_x = -1;
		for(i = 0; i < next_active_list; i++) {
			int next_x = is_drawing ? floorf(active_list[i]->xcurr) : ceilf(active_list[i]->xcurr);

			bool is_vertex = fabs(active_list[i]->xmin - active_list[i]->xcurr) < 0.5
				&& (active_list[i]->ymax == y || active_list[i]->ymin == y);

			if(is_drawing) {
				// TODO: this sometimes results in lines where prev_x = next_x - 2. Figure out why.
				planar_line_horizontal(bitplane_idx, y + yofs, xofs + prev_x, xofs + next_x, xor);
			}

			if((!is_vertex) || (active_list[i]->ymax == y)) {
				is_drawing = 1 - is_drawing;
			}

			prev_x = next_x;
		}

		// Remove active edges where ymax == y
		for(i = 0; i < next_active_list; /* empty */) {
			if(active_list[i]->ymax == y) {
				del_active(i);
			} else {
				i++;
			}
		}


		// Update the gradients in AET.
		for(i = 0; i < next_active_list; i++) {
			active_list[i]->xcurr += active_list[i]->grad_recip;
		}
	}
}

void draw_thick_circle(int xc, int yc, int radius, int thickness, int bitplane_idx) {
	if(radius <= 0)
		return;

	int xouter = radius;
    int xinner = radius - thickness;;
    int y = 0;
    int errouter = 1 - xouter;
    int errinner = 1 - xinner;

	while(xouter >= y) {
		planar_line_horizontal(bitplane_idx, yc + y, xc + xinner, xc + xouter, false);
		planar_line_vertical(bitplane_idx, xc + y,  yc + xinner, yc + xouter, false);
		planar_line_horizontal(bitplane_idx, yc + y, xc - xouter, xc - xinner, false);
		planar_line_vertical(bitplane_idx, xc - y,  yc + xinner, yc + xouter, false);
		planar_line_horizontal(bitplane_idx, yc - y, xc - xouter, xc - xinner, false);
		planar_line_vertical(bitplane_idx, xc - y,  yc - xouter, yc - xinner, false);
		planar_line_horizontal(bitplane_idx, yc - y, xc + xinner, xc + xouter, false);
		planar_line_vertical(bitplane_idx, xc + y,  yc - xouter, yc - xinner, false);

		y++;

		if (errouter < 0) {
			errouter += 2 * y + 1;
		} else {
			xouter --;
			errouter += 2 * (y - xouter + 1);
		}

		if (y > radius) {
			xinner = y;
		} else {
			if (errinner < 0) {
				errinner += 2 * y + 1;
			} else {
				xinner --;
				errinner += 2 * (y - xinner + 1);
			}
		}
	}
}

void graphics_planar_clear(int bitplane_idx)
{
	memset(plane[bitplane_idx], 0, bitplane_width * bitplane_height);
}

void graphics_clear_visible()
{
	for(int plane_idx = 0; plane_idx < 5; plane_idx++) {
		for(int y = 0; y < window_height; y++) {
			memset(plane[plane_idx] + (y * bitplane_width), 0, window_width);
		}
	}
}

uint8_t *graphics_bitplane_get(int idx)
{
	// NB returns the bitplane sans offset.
	return plane[idx];
}

/* Combine bitplanes */
void graphics_planar_render()
{
	//SDL_RenderPresent(renderer);
	uint8_t *plane_idx_0 = plane[0] + plane_offset[0];
	uint8_t *plane_idx_1 = plane[1] + plane_offset[1];
	uint8_t *plane_idx_2 = plane[2] + plane_offset[2];
	uint8_t *plane_idx_3 = plane[3] + plane_offset[3];
	uint8_t *plane_idx_4 = plane[4] + plane_offset[4];

	int fb_idx = 0;

	for(int y = 0; y < window_height; y++) {
		for(int x = 0; x < window_width; x++) {
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

void graphics_bitplane_blit(int plane_from, int plane_to, int sx, int sy, int w, int h, int dx, int dy)
{
	int src_offset = (bitplane_width * sy) + sx;
	int dst_offset = (bitplane_width * dy) + dx;

	uint8_t *src = plane[plane_from] + src_offset;
	uint8_t *dst = plane[plane_to]   + dst_offset;

	for(int y = 0; y < h; y++) {
		memcpy(dst, src, w);
		src += bitplane_width;
		dst += bitplane_width;
	}
}

void graphics_blit(int mask, int sx, int sy, int w, int h, int dx, int dy)
{
	/* copy all planes in 'mask' */
	for(int plane_idx = 0; plane_idx < 5; plane_idx++) {
		if((mask & (1 << plane_idx)) != 0) {
			graphics_bitplane_blit(plane_idx, plane_idx, sx, sy, w, h, dx, dy);
		}
	}
}

