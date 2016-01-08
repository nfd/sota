#include <inttypes.h>
#include <stddef.h>
#include <stdbool.h>

// administration
int graphics_init(bool fullscreen);
int graphics_width();
int graphics_height();

// bitplanes
int graphics_bitplane_width();
int graphics_bitplane_height();
void graphics_bitplane_set_offset(int bitplane_idx, int x, int y);
int graphics_shutdown();
void graphics_planar_render();
void graphics_planar_clear(int bitplane_idx);

// the palette
void graphics_set_palette(size_t num_elements, uint32_t *elements);
void graphics_get_palette(size_t num_elements, uint32_t *elements);
void graphics_lerp_palette(size_t num_elements, uint32_t *from, uint32_t *to, int current_step, int total_steps);

// shapes
int graphics_draw_filled_scaled_polygon_to_bitmap(int num_vertices, uint8_t *data, float scalex, float scaley, int xofs, int yofs, int bitplane_idx, bool xor);
void draw_thick_circle(int xc, int yc, int radius, int thickness, int bitplane_idx);

#ifndef max
#define max(x, y) ((x) < (y) ? (y) : (x))
#endif

#ifndef min
#define min(x, y) ((x) > (y) ? (y) : (x))
#endif

