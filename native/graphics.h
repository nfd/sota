#include <inttypes.h>

int graphics_init();
int graphics_width();
int graphics_height();
int graphics_shutdown();
int graphics_draw_filled_scaled_polygon_to_bitmap(int num_vertices, uint8_t *data, int scale, int xofs, int yofs, int bitplane_idx);
void graphics_planar_render();
void graphics_set_palette(size_t num_elements, uint32_t *elements);
void graphics_planar_clear(int bitplane_idx);

