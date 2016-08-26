#include <stddef.h>

void heap_reset();
void *heap_alloc(size_t size);
void heap_pop();

int get_heap_location(void);
void set_heap_location(int ptr);
