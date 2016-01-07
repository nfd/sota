#include <inttypes.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stddef.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

#include "files.h"

uint8_t *read_entire_file(char *filename, ssize_t *size_out) {
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

