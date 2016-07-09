#include <stdio.h>
#include <math.h>
#include <inttypes.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>

#include "graphics.h"
#include "backend.h"

extern struct backend_interface_struct g_backend;

/* Current palette */
uint32_t palette[32];

/* Data related to the polygon fill algorithm. Must be initialised because it depends on the height. */
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

int graphics_init() {
	// edge_table is used by the scanline polygon rendering algorithm
	edge_table = g_backend.malloc(g_backend.height * sizeof(struct poly_elem *));
	if(edge_table == NULL) {
		perror("edge_table");
		return -1;
	}

	return 0;
}

int graphics_shutdown() {
	g_backend.free(edge_table);
	edge_table = NULL;

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

/*
static inline void planar_putpixel(int bitplane_idx, int x, int y)
{
	if(x >= 0 && x < bitplane_width && y >= 0 && y <= bitplane_height)
		plane[bitplane_idx][(y * bitplane_width) + x] = (1 << bitplane_idx);
}
*/

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
	int global_ymin = g_backend.height;
	int global_ymax = 0;

	/* Clear edge table */
	memset(edge_table, 0, g_backend.height * sizeof(struct poly_elem *));

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

		y0 = min(max(0, y0), g_backend.height - 1);
		y1 = max(min(g_backend.height - 1, y1), 0);

		x0 = min(max(0, x0), g_backend.width - 1);
		x1 = max(min(g_backend.width - 1, x1), 0);

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
				g_backend.planar_line_horizontal(bitplane_idx, y + yofs, xofs + prev_x, xofs + next_x, xor);
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

/* TODO: Backend draw cmds are now window-only, so the horizontal and vertical line commands
 * should move back into here...
*/

void draw_thick_circle(int xc, int yc, int radius, int thickness, int bitplane_idx) {
	if(radius <= 0)
		return;

	int xouter = radius;
    int xinner = radius - thickness;;
    int y = 0;
    int errouter = 1 - xouter;
    int errinner = 1 - xinner;

	while(xouter >= y) {
		g_backend.planar_line_horizontal(bitplane_idx, yc + y, xc + xinner, xc + xouter, false);
		g_backend.planar_line_vertical(bitplane_idx, xc + y,  yc + xinner, yc + xouter, false);
		g_backend.planar_line_horizontal(bitplane_idx, yc + y, xc - xouter, xc - xinner, false);
		g_backend.planar_line_vertical(bitplane_idx, xc - y,  yc + xinner, yc + xouter, false);
		g_backend.planar_line_horizontal(bitplane_idx, yc - y, xc - xouter, xc - xinner, false);
		g_backend.planar_line_vertical(bitplane_idx, xc - y,  yc - xouter, yc - xinner, false);
		g_backend.planar_line_horizontal(bitplane_idx, yc - y, xc + xinner, xc + xouter, false);
		g_backend.planar_line_vertical(bitplane_idx, xc + y,  yc - xouter, yc - xinner, false);

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

#if 0
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
#endif


