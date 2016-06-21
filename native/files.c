#define _XOPEN_SOURCE 700

#include <inttypes.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stddef.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdbool.h>

#include "files.h"

#ifdef EMULATE_FMEMOPEN
#include "fmemopen.h"
#endif

uint8_t *wad;

static uint8_t *read_entire_file(char *filename, ssize_t *size_out) {
	uint8_t *buf;
	
	int h = open(filename, O_RDONLY);
	if(h < 0) {
		perror(filename);
		return NULL;
	}

	struct stat statbuf;
	if(fstat(h, &statbuf) != 0)  {
		perror("fstat");
		close(h);
		return NULL;
	}

	buf = malloc(statbuf.st_size);
	if(buf == NULL) {
		perror("malloc");
		close(h);
		return NULL;
	}

	ssize_t to_read = statbuf.st_size;
	uint8_t *current = buf;
	while(to_read) {
		ssize_t amt_read = read(h, current, to_read);
		if(amt_read < 0) {
			fprintf(stderr, "read_entire_file: error reading\n");
			close(h);
			free(buf);
			return NULL;
		}
		to_read -= amt_read;
		current += amt_read;
	}

	close(h);

	if(size_out) {
		*size_out = statbuf.st_size;
	}

	return buf;
}

struct wad_header {
	uint32_t magic;
	uint32_t file_count;
	uint32_t choreography_file_idx;
};

struct wad_idx {
	uint32_t offset;
	uint32_t size;
};

uint8_t *file_get(int idx, ssize_t *size_out)
{
	struct wad_header *header = (struct wad_header*)wad;

	if(idx >= header->file_count)
		return NULL;

	struct wad_idx *idx_array = (struct wad_idx *)(wad + sizeof(struct wad_header));

	if(size_out)
		*size_out = idx_array[idx].size;

	return &wad[idx_array[idx].offset];
}

uint8_t *file_get_choreography()
{
	struct wad_header *header = (struct wad_header*)wad;
	return file_get(header->choreography_file_idx, NULL);
}

FILE *file_open(int idx)
{
	ssize_t size;

	uint8_t *data = file_get(idx, &size);

	if(data == NULL) {
		return NULL;
	} else {
		return fmemopen(data, size, "r");
	}
}

bool files_init()
{
	wad = read_entire_file("sota.wad", NULL);
	return wad != NULL;
}

void files_deinit()
{
	if(wad)
		free(wad);
}

