#include <stddef.h>

void heap_reset();
void *heap_alloc(size_t size);
void heap_pop();
bool heap_free(void *data); // will fail if this wasn't the most recent allocation

int heap_get_location(void);
void heap_set_location(int ptr);
