#define _DEFAULT_SOURCE

#include <stdio.h>
#include <math.h>
#include <inttypes.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include "endian_compat.h"
#include "graphics.h"
#include "backend.h"
#include "minmax.h"
#include "heap.h"

/* Data related to the polygon fill algorithm. Must be initialised because it depends on the height. */
struct poly_elem {
	float xcurr;
	int16_t ymax;
	float grad_recip;
	struct poly_elem *prev;
	// original vertex info
	int16_t ymin;
	int16_t xmin;
};

struct poly_elem **edge_table;

int graphics_init() {
	// edge_table is used by the scanline polygon rendering algorithm
	edge_table = heap_alloc(window_height * sizeof(struct poly_elem *));
	if(edge_table == NULL) {
		backend_debug("edge_table: couldn't alloc");
		return -1;
	}

	return 0;
}

int graphics_shutdown() {
	// backend will free allocated memory automatically.
	return 0;
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

	return min(0xff, byte_from + lerped);
}

void graphics_lerp_palette(size_t num_elements, uint32_t *from, uint32_t *to, int current_step, int total_steps) {
	if(current_step < 0 || current_step > total_steps || num_elements > 32)
		return;

	for(unsigned i = 0; i < num_elements; i++) {
		backend_set_palette_element(i, (lerp_byte(3, from[i], to[i], current_step, total_steps) << 24)
			| (lerp_byte(2, from[i], to[i], current_step, total_steps) << 16)
			| (lerp_byte(1, from[i], to[i], current_step, total_steps) << 8)
			| (lerp_byte(0, from[i], to[i], current_step, total_steps)));
	}
}

/*
static inline void planar_putpixel(int bitplane_idx, int x, int y)
{
	if(x >= 0 && x < bitplane_width && y >= 0 && y <= bitplane_height)
		plane[bitplane_idx][(y * bitplane_width) + x] = (1 << bitplane_idx);
}
*/

/* Heart of everything! */

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

void graphics_draw_filled_scaled_polygon_to_bitmap(int num_vertices, uint8_t *data, float scalex, float scaley, int xofs, int yofs, struct Bitplane *bitplane, bool xor)
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
				planar_line_horizontal(bitplane, y + yofs, xofs + prev_x, xofs + next_x, xor);
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

static inline void planar_line_horizontal_xor(int start_x, int end_x, uint32_t *data, uint32_t *end_data)
{
	uint32_t start = htobe32(0xffffffffUL >> (start_x % 32));
	uint32_t end = htobe32(0xffffffffUL << (32 - (end_x % 32)));

	if(data == end_data) {
		/* The entire line fits in a word. */
		*data ^= (start & end);
	} else {
		/* First part: portion of a byte, drawing from LSB upwards. */
		*data++ ^= start;

		/* Second part: complete bytes */
		while(data < end_data)
			*data++ ^= 0xffffffffUL;

		/* Third part: portion of a byte, drawing from MSB downwards. */
		*data ^= end;
	}
}

static inline void planar_line_horizontal_or(int start_x, int end_x, uint32_t *data, uint32_t *end_data)
{
	uint32_t start = htobe32(0xffffffffUL >> (start_x % 32));
	uint32_t end = htobe32(0xffffffffUL << (32 - (end_x % 32)));

	if(data == end_data) {
		/* The entire line fits in a word. */
		*data |= (start & end);
	} else {
		/* First part: portion of a byte, drawing from LSB upwards. */
		*data++ |= start;

		/* Second part: complete bytes */
		while(data < end_data)
			*data++ = 0xffffffffUL;

		/* Third part: portion of a byte, drawing from MSB downwards. */
		*data |= end;
	}
}

void planar_line_horizontal(struct Bitplane *plane, int y, int start_x, int end_x, bool xor)
{
	y = min(max(y, 0), plane->height - 1);
	start_x = max(min(start_x, plane->width - 1), 0);
	end_x = min(max(end_x, 0), plane->width - 1);

	if(start_x > end_x)
		return;

	uint32_t *data = (uint32_t *)(plane->data + y * plane->stride) + (start_x / 32);
	uint32_t *end_data = (uint32_t *)(plane->data + y * plane->stride) + (end_x / 32);

	if(xor) {
		planar_line_horizontal_xor(start_x, end_x, data, end_data);
	} else {
		planar_line_horizontal_or(start_x, end_x, data, end_data);
	}
}

void planar_line_vertical(struct Bitplane *plane, int x, int start_y, int end_y, bool xor)
{
	x = min(max(x, 0), plane->width - 1);
	start_y = max(min(start_y, plane->height - 1), 0);
	end_y = min(max(end_y, 0), plane->height - 1);

	if(start_y > end_y)
		return;

	uint8_t *data = plane->data;
	uint8_t pen = 1 << (7 - (x % 8));
	
	data += (start_y * plane->stride) + (x / 8);

	if (xor) {
		for(int length = end_y - start_y; length; length--) {
			*data ^= pen;
			data += plane->stride;
		}
	} else {
		for(int length = end_y - start_y; length; length--) {
			*data |= pen;
			data += plane->stride;
		}
	}
}

void planar_draw_thick_circle(struct Bitplane *bitplane, int xc, int yc, int radius, int thickness) {
	if(radius <= 0)
		return;

	int xouter = radius;
    int xinner = radius - thickness;;
    int y = 0;
    int errouter = 1 - xouter;
    int errinner = 1 - xinner;

	while(xouter >= y) {
		planar_line_horizontal(bitplane, yc + y, xc + xinner, xc + xouter, false);
		planar_line_vertical(bitplane, xc + y,  yc + xinner, yc + xouter, false);
		planar_line_horizontal(bitplane, yc + y, xc - xouter, xc - xinner, false);
		planar_line_vertical(bitplane, xc - y,  yc + xinner, yc + xouter, false);
		planar_line_horizontal(bitplane, yc - y, xc - xouter, xc - xinner, false);
		planar_line_vertical(bitplane, xc - y,  yc - xouter, yc - xinner, false);
		planar_line_horizontal(bitplane, yc - y, xc + xinner, xc + xouter, false);
		planar_line_vertical(bitplane, xc + y,  yc - xouter, yc - xinner, false);

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

void planar_clear(struct Bitplane *plane)
{
	if(plane->data_start) {
		memset(plane->data_start, 0, plane->stride * plane->height);
	}
}

void graphics_bitplane_blit(struct Bitplane *from, struct Bitplane *to, int sx, int sy, int w, int h, int dx, int dy)
{
	int src_stride = from->stride;
	int dst_stride = to->stride;

	int src_offset = (src_stride * sy) + (sx / 8);
	int dst_offset = (dst_stride * dy) + (dx / 8);

	uint8_t *src = from->data + src_offset;
	uint8_t *dst = to->data   + dst_offset;

	int bytes_per_row = (w + 7) / 8;

	for(int y = 0; y < h; y++) {
		memcpy(dst, src, bytes_per_row);
		src += src_stride;
		dst += dst_stride;
	}
}

void graphics_blit(struct Bitplane from[], struct Bitplane to[], int mask, int sx, int sy, int w, int h, int dx, int dy)
{
	/* copy all planes in 'mask' */
	for(int plane_idx = 0; plane_idx < 5; plane_idx++) {
		if((mask & (1 << plane_idx)) != 0) {
			graphics_bitplane_blit(&from[plane_idx], &to[plane_idx], sx, sy, w, h, dx, dy);
		}
	}
}


