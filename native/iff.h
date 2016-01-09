#include <stdbool.h>

bool iff_display(int file_idx, int display_type, uint32_t *num_colours, uint32_t *palette_out);
uint32_t *iff_get_palette(int file_idx, uint16_t *count);

