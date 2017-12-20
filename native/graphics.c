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
int outline_width;

int graphics_init() {
	// edge_table is used by the scanline polygon rendering algorithm
	edge_table = heap_alloc(window_height * sizeof(struct poly_elem *));
	if(edge_table == NULL) {
		backend_debug("edge_table: couldn't alloc");
		return -1;
	}

	/* width of lines when drawing polygons in outline mode */
	outline_width = (2 * (window_width < 320 ? 320 : window_width)) / 320; 

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

static inline void planar_putpixel(struct Bitplane *plane, int x, int y)
{
	if(x >= 0 && x < plane->width && y >= 0 && y <= plane->height)
		plane->data[(y * plane->stride) + x / 8] |= (1 << (7 - (x % 8)));
}

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

static void planar_line_thick(struct Bitplane *bitplane, int x0, int y0, int x1, int y1, int thickness)
{
	/* Modified from rosettacode's standard Bresenham algorithm. Could be
	 * improved -- lots of overdraw */
	int dx = abs(x1 - x0);
	int sx = x0 < x1 ? 1 : -1;
	int dy = abs(y1 - y0);
	int sy = y0<y1 ? 1 : -1; 
	int err = (dx > dy ? dx : -dy) / 2;
	int e2;

	int half_thickness = thickness / 2;

	for(;;){
		for(int ythick = -half_thickness; ythick < half_thickness; ythick++){
			for(int xthick= -half_thickness; xthick<= half_thickness; xthick++) {
				planar_putpixel(bitplane, x0 + xthick, y0 + ythick);
			}
		}
		if (x0 == x1 && y0 == y1)
			break;

		e2 = err;

		if (e2 >-dx) {
			err -= dy; x0 += sx;
		}

		if (e2 < dy) {
			err += dx; y0 += sy;
		}
	}
}

void graphics_draw_scaled_polygon_to_bitmap(int num_vertices, uint8_t *data, float scalex, float scaley, int xofs, int yofs, struct Bitplane *bitplane)
{
	int y0, x0, y1, x1;

	y0 = data[0];
	x0 = data[1];
	x0 = x0 * scalex + xofs;
	y0 = y0 * scaley + yofs;

	for(int i=2; i < (num_vertices * 2); i+= 2) {

		y1 = data[i];
		x1 = data[i+1];

		x1 = x1 * scalex + xofs;
		y1 = y1 * scaley + yofs;

		planar_line_thick(bitplane, x0, y0, x1, y1, outline_width);

		x0 = x1;
		y0 = y1;
	}

	/* Close the shape */
	y0 = data[0];
	x0 = data[1];
	x0 = x0 * scalex + xofs;
	y0 = y0 * scaley + yofs;

	planar_line_thick(bitplane, x0, y0, x1, y1, outline_width);
}


void graphics_draw_filled_scaled_polygon_to_bitmap(int num_vertices, uint8_t *data, float scalex, float scaley, int xofs, int yofs, struct Bitplane *bitplane, bool xor, bool distort, bool flip_horizontal, bool flip_vertical)
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

		y0 = data[i];
		x0 = data[i+1];

		if(i == (num_vertices * 2) - 2) {
			// close the shape
			y1 = data[0];
			x1 = data[1];
		} else {
			y1 = data[i+2];
			x1 = data[i+3];
		}

		if(distort)  {
			if(i & 2) {
				x0 += 8;
			} else {
				x1 += 8;
			}
		}

		y0 = y0 * scaley + yofs;
		y1 = y1 * scaley + yofs;
		x0 = x0 * scalex + xofs;
		x1 = x1 * scalex + xofs;

		if(y0 == y1) {
			// Horizontal edge; just ignore it.
			continue;
		}

		if(flip_horizontal) {
			x0 = window_width - x0;
			x1 = window_width - x1;
		}
		
		if(flip_vertical) {
			y0 = window_height - y0;
			y1 = window_height - y1;
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
		int prev_x = 0;
		for(i = 0; i < next_active_list; i++) {
			int next_x = is_drawing ? floorf(active_list[i]->xcurr) : ceilf(active_list[i]->xcurr);

			bool is_vertex = fabs(active_list[i]->xmin - active_list[i]->xcurr) < 0.5
				&& (active_list[i]->ymax == y || active_list[i]->ymin == y);

			if(is_drawing) {
				planar_line_horizontal(bitplane, y, prev_x, next_x, xor, 0xffff);
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

static inline void planar_line_horizontal_xor(int start_x, int end_x, uint32_t *data, uint32_t *end_data, uint32_t pattern)
{
	int end_shift_amt = 32 - (end_x % 32);

	uint32_t start = htobe32(pattern >> (start_x % 32));
	uint32_t end = end_shift_amt == 32 ? 0 : htobe32(pattern << (32 - (end_x % 32)));

	if(data == end_data) {
		/* The entire line fits in a word. */
		*data ^= (start & end);
	} else {
		/* First part: portion of a byte, drawing from LSB upwards. */
		*data++ ^= start;

		/* Second part: complete bytes */
		while(data < end_data)
			*data++ ^= pattern;

		/* Third part: portion of a byte, drawing from MSB downwards. */
		*data ^= end;
	}
}

static inline void planar_line_horizontal_or(int start_x, int end_x, uint32_t *data, uint32_t *end_data, uint32_t pattern)
{
	int end_shift_amt = 32 - (end_x % 32);
	uint32_t start = htobe32(pattern >> (start_x % 32));
	uint32_t end = end_shift_amt == 32 ? 0 : htobe32(pattern << (32 - (end_x % 32)));

	if(data == end_data) {
		/* The entire line fits in a word. */
		*data |= (start & end);
	} else {
		/* First part: portion of a byte, drawing from LSB upwards. */
		*data++ |= start;

		/* Second part: complete bytes */
		while(data < end_data)
			*data++ = pattern;

		/* Third part: portion of a byte, drawing from MSB downwards. */
		*data |= end;
	}
}

void planar_line_horizontal(struct Bitplane *plane, int y, int start_x, int end_x, bool xor, uint16_t pattern)
{
	y = min(max(y, 0), plane->height - 1);
	start_x = max(min(start_x, plane->width - 1), 0);
	end_x = min(max(end_x, 0), plane->width - 1);

	if(start_x > end_x)
		return;

	uint32_t *data = (uint32_t *)(plane->data + y * plane->stride) + (start_x / 32);
	uint32_t *end_data = (uint32_t *)(plane->data + y * plane->stride) + (end_x / 32);

	uint32_t pattern32 = (((uint32_t)pattern) << 16) | pattern;

	if(xor) {
		planar_line_horizontal_xor(start_x, end_x, data, end_data, pattern32);
	} else {
		planar_line_horizontal_or(start_x, end_x, data, end_data, pattern32);
	}
}

static inline uint16_t rol16 (uint16_t n, unsigned int c)
{
	// https://stackoverflow.com/questions/776508/best-practices-for-circular-shift-rotate-operations-in-c
	const unsigned int mask = (8 * sizeof(n) - 1);  // assumes width is a power of 2.

	c &= mask;
	return (n<<c) | (n>>( (-c)&mask ));
}

void planar_line_vertical(struct Bitplane *plane, int x, int start_y, int end_y, bool xor, uint16_t pattern)
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
			if(pattern & 0x8000) 
				*data ^= pen;
			data += plane->stride;

			pattern = rol16(pattern, 1);
		}
	} else {
		for(int length = end_y - start_y; length; length--) {
			if(pattern & 0x8000)
				*data |= pen;
			data += plane->stride;

			pattern = rol16(pattern, 1);
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
		planar_line_horizontal(bitplane, yc + y, xc + xinner, xc + xouter, false, 0xffff);
		planar_line_vertical(bitplane, xc + y,  yc + xinner, yc + xouter, false, 0xffff);
		planar_line_horizontal(bitplane, yc + y, xc - xouter, xc - xinner, false, 0xffff);
		planar_line_vertical(bitplane, xc - y,  yc + xinner, yc + xouter, false, 0xffff);
		planar_line_horizontal(bitplane, yc - y, xc - xouter, xc - xinner, false, 0xffff);
		planar_line_vertical(bitplane, xc - y,  yc - xouter, yc - xinner, false, 0xffff);
		planar_line_horizontal(bitplane, yc - y, xc + xinner, xc + xouter, false, 0xffff);
		planar_line_vertical(bitplane, xc + y,  yc - xouter, yc - xinner, false, 0xffff);

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

void planar_circle(struct Bitplane *plane, int x0, int y0, int radius)
{
	int x = radius;
	int y = 0;
	int err = 0;

	while(x >= y) {
		planar_putpixel(plane, x0 + x, y0 + y);
		planar_putpixel(plane, x0 + y, y0 + x);
		planar_putpixel(plane, x0 - y, y0 + x);
		planar_putpixel(plane, x0 - x, y0 + y);
		planar_putpixel(plane, x0 - x, y0 - y);
		planar_putpixel(plane, x0 - y, y0 - x);
		planar_putpixel(plane, x0 + y, y0 - x);
		planar_putpixel(plane, x0 + x, y0 - y);

		y += 1;
		err += 1 + 2*y;
		if (2 * (err - x) + 1 > 0) {
			x -= 1;
			err += 1 - 2 * x;
		}
	}
}

void planar_filled_rect(struct Bitplane *plane, int sx, int sy, int ex, int ey)
{
	sy = min(max(sy, 0), plane->height - 1);
	ey = min(max(ey, 0), plane->height - 1);
	sx = max(min(sx, plane->width - 1), 0);
	ex = min(max(ex, 0), plane->width - 1);

	if(sx > ex || sy > ey)
		return;

	uint32_t pattern32 = 0xffffffff; // (((uint32_t)pattern) << 16) | pattern;

	for(int y = sy; y <= ey; y++) {
		uint32_t *data = (uint32_t *)(plane->data + y * plane->stride) + (sx / 32);
		uint32_t *end_data = (uint32_t *)(plane->data + y * plane->stride) + (ex / 32);

		planar_line_horizontal_or(sx, ex, data, end_data, pattern32);
	}
}

void planar_clear(struct Bitplane *plane)
{
	if(plane->data_start) {
		memset(plane->data_start, 0, plane->stride * plane->height);
	}
}

static inline uint8_t update_masked_word(uint8_t orig, uint8_t value, int end_bit)
{
	uint8_t copied_mask = 0xff << (8 - (end_bit % 8));
	value &= copied_mask;
	uint8_t src_mask = 0xff >> (end_bit % 8);
	value |= (orig & src_mask);
	return value;
}

void graphics_bitplane_blit_line(uint8_t *fromdata, uint8_t *todata, int sx, int w, int dx)
{
	if(w <= 0)
		return;

	int src_offset = (sx / 8);
	int dst_offset = (dx / 8);

	uint8_t *src = fromdata + src_offset;
	uint8_t *dst = todata   + dst_offset;
	uint8_t *end_word = todata + ((dx + w) / 8);
	int end_bit = dx + w;

	if(((dx + w) % 8) == 0) {
		/* We want end_word to point to the final usable word, not to the word after. */
		end_word--;
	}

	uint8_t src_word = *src++;
	uint8_t dst_word;

	// Mask off the bits below dx in the initial word
	if(!(dx % 8)) {
		dst_word = 0;
	} else {
		dst_word = *dst & (0xff << (8 - (dx % 8)));
	}

	while(dx < end_bit) {
		int src_avail = 8 - (sx % 8);
		int dst_avail = 8 - (dx % 8);

		uint8_t bits;

		if(src_avail < 8) {
			bits = src_word & ((1 << src_avail) - 1); // mask off bits above src_avail..
		} else {
			bits = src_word;
		}

		if(src_avail > dst_avail) {
			// More src bits than dst bits: we will be right shifting to move to dst.

			// sx = 2, dx = 5
			// src_avail = 30, dst_avail = 27
			// dst |= bits >> 3
			dst_word |= bits >> ( (dx % 8) - (sx % 8) );

			// we wrote dst_avail bits
			sx += dst_avail;
			dx += dst_avail;
		} else {
			// More dst bits than src bits; we will be left shifting to move to dst.
			// sx = 5, dx = 2
			// src_avail = 27, dst_avail = 30
			// dst |= bits << 3;
			dst_word |= bits << ( (sx % 8) - (dx % 8) );

			// we wrote src_avail bits
			sx += src_avail;
			dx += src_avail;
		}

		if(!(sx % 8)) {
			src_word = *src++;
		}
		if(!(dx % 8)) {
			if(dx > end_bit) {
				/* Reload the parts of the final word which we shouldn't have
				 * touched.  NB end_bit % 8 will always be > 0 here -- only
				 * time it can't be is if we had a width of 0, which is
				 * special-cased at the start of the function. */
				dst_word = update_masked_word(*dst, dst_word, end_bit);
			}
			*dst++ = dst_word;
			dst_word = 0;
		}
	}
	if(dst <= end_word) {
		// dx advanced past length, but we didn't write the final word out.
		*dst = update_masked_word(*dst, dst_word, end_bit);
	}
}

void graphics_bitplane_blit(struct Bitplane *from, struct Bitplane *to, int sx, int sy, int w, int h, int dx, int dy)
{
	int src_offset = from->stride * sy;
	int dst_offset = to->stride * dy;

	uint8_t *src = from->data + src_offset;
	uint8_t *dst = to->data   + dst_offset;

	for( ; h; h--) {
		graphics_bitplane_blit_line(src, dst, sx, w, dx);

		src += from->stride;
		dst += to->stride;
	}
}

void graphics_blit(struct Bitplane from[], struct Bitplane to[], int mask, int sx, int sy, int w, int h, int dx, int dy)
{
	/* copy all planes in 'mask' */
	for(int plane_idx = 0; plane_idx < 6; plane_idx++) {
		if((mask & (1 << plane_idx)) != 0) {
			graphics_bitplane_blit(&from[plane_idx], &to[plane_idx], sx, sy, w, h, dx, dy);
		}
	}
}

/* Fast copy of an entire bitplane */
void graphics_copy_plane(struct Bitplane *from, struct Bitplane *to)
{
	if(from->width == to->width && from->height == to->height) {
		size_t amt = (from->width * from->height / 8);
		memcpy(to->data_start, from->data_start, amt);
	}
}



