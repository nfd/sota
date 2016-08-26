#include <inttypes.h>
#include <stdio.h>
#include <sys/types.h>

bool files_init(const char *filename);
void files_deinit();
uint8_t *file_get(int idx, ssize_t *size_out);
uint8_t *file_get_choreography();
FILE *file_open(int idx);

