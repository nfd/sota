#include <stdint.h>
#include <sys/types.h>

uint8_t *read_entire_file(char *filename, ssize_t *size_out);
FILE *file_open(char *filename);

