#include <inttypes.h>
#include <stddef.h>
#include <stdbool.h>
#include "backend.h"

// Maximum number of lines in a polygon.
#define MAX_LINES 128

// administration
int graphics_init();
int graphics_shutdown();

// the palette
void graphics_lerp_palette(size_t num_elements, uint32_t *from, uint32_t *to, int current_step, int total_steps);

// shapes
void graphics_draw_filled_scaled_polygon_to_bitmap(int num_vertices, uint8_t *data, float scalex, float scaley, int xofs, int yofs, struct Bitplane *bitplane, bool xor);
void planar_draw_thick_circle(struct Bitplane *bitplane, int xc, int yc, int radius, int thickness);

// planar stuff
void planar_line_vertical(struct Bitplane *plane, int x, int start_y, int end_y, bool xor);
void planar_line_horizontal(struct Bitplane *plane, int y, int start_x, int end_x, bool xor);
void planar_clear(struct Bitplane *plane);
void graphics_blit(int mask, int sx, int sy, int w, int h, int dx, int dy);

// copper effects
typedef void(copper_effect)(int x, int y, uint32_t *palette);
void graphics_copper_effect_register(copper_effect handler); // call handler every 8 pixels
void graphics_copper_effect_unregister();

