#define _XOPEN_SOURCE 700

#include <inttypes.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#include "wad.h"

struct wad_header {
	uint32_t magic;
	uint32_t file_count;
	uint32_t choreography_file_idx;
};

struct wad_idx {
	uint32_t offset;
	uint32_t size;
};

static inline struct wad_idx *wad_get_file_idx(uint8_t *wad, unsigned file_idx)
{
	struct wad_header *header = (struct wad_header*)wad;

	if(file_idx >= header->file_count)
		return NULL;

	struct wad_idx *idx_array = (struct wad_idx *)(wad + sizeof(struct wad_header));

	return &idx_array[file_idx];
}

uint32_t wad_get_file_offset(uint8_t *wad, int file_idx)
{
	struct wad_idx *idx = wad_get_file_idx(wad, file_idx);
	return idx ? idx->offset: 0;
}

size_t wad_get_file_size(uint8_t *wad, int file_idx)
{
	struct wad_idx *idx = wad_get_file_idx(wad, file_idx);
	return idx ? idx->size: 0;
}

uint32_t wad_get_choreography_offset(uint8_t *wad)
{
	struct wad_header *header = (struct wad_header*)wad;
	return wad_get_file_offset(wad, header->choreography_file_idx);
}

