#include <inttypes.h>
#include <stddef.h>
#include <stdbool.h>
#include "backend.h"

// administration
int graphics_init();
int graphics_shutdown();

// the palette
void graphics_set_palette(size_t num_elements, uint32_t *elements);
void graphics_get_palette(size_t num_elements, uint32_t *elements);
void graphics_lerp_palette(size_t num_elements, uint32_t *from, uint32_t *to, int current_step, int total_steps);

// shapes
void graphics_draw_filled_scaled_polygon_to_bitmap(int num_vertices, uint8_t *data, float scalex, float scaley, int xofs, int yofs, int bitplane_idx, bool xor);
void draw_thick_circle(int xc, int yc, int radius, int thickness, int bitplane_idx);

// copper effects
typedef void(copper_effect)(int x, int y, uint32_t *palette);
void graphics_copper_effect_register(copper_effect handler); // call handler every 8 pixels
void graphics_copper_effect_unregister();

#ifndef max
#define max(x, y) ((x) < (y) ? (y) : (x))
#endif

#ifndef min
#define min(x, y) ((x) > (y) ? (y) : (x))
#endif

