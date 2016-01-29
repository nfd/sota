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
int graphics_bitplane_stride();
void graphics_bitplane_set_offset(int bitplane_idx, int x, int y);
int graphics_shutdown();
void graphics_planar_render();
void graphics_planar_clear(int bitplane_idx);
void graphics_clear_visible();
uint8_t *graphics_bitplane_get(int idx); 
void graphics_bitplane_blit(int plane_from, int plane_to, int sx, int sy, int w, int h, int dx, int dy);
void graphics_blit(int mask, int sx, int sy, int w, int h, int dx, int dy);

// the palette
void graphics_set_palette(size_t num_elements, uint32_t *elements);
void graphics_get_palette(size_t num_elements, uint32_t *elements);
void graphics_lerp_palette(size_t num_elements, uint32_t *from, uint32_t *to, int current_step, int total_steps);

// shapes
void graphics_draw_filled_scaled_polygon_to_bitmap(int num_vertices, uint8_t *data, float scalex, float scaley, int xofs, int yofs, int bitplane_idx, bool xor);
void draw_thick_circle(int xc, int yc, int radius, int thickness, int bitplane_idx);

#ifndef max
#define max(x, y) ((x) < (y) ? (y) : (x))
#endif

#ifndef min
#define min(x, y) ((x) > (y) ? (y) : (x))
#endif

