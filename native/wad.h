#include <inttypes.h>
#include <stdio.h>
#include <sys/types.h>

uint32_t wad_get_file_offset(uint8_t *wad, int file_idx);
size_t wad_get_file_size(uint8_t *wad, int file_idx);
uint32_t wad_get_choreography_offset(uint8_t *wad);

