// Pebble endian support -- Pebble is little endian.
//
#include <inttypes.h>

#define bswap_32(x) ((((x) & 0x000000ff) << 24) | (((x) & 0x0000ff00) << 8) | (((x) & 0x00ff0000) >> 8) | (((x) & 0xff000000) >> 24))

#define htobe32(x) (bswap_32(x))


