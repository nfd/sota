/* heap management 
 * We use a separate heap for bump pointers, because why not
*/
#include <inttypes.h>
#include <stdbool.h>

#include "heap.h"

#include "align.h"
#include "backend.h"

#define HEAP_SIZE (HEAP_SIZE_KB * 1024)
#define MAX_ALLOC 16

static uint8_t heap[HEAP_SIZE];
static void *heap_allocs[MAX_ALLOC];
static int next_alloc_ptr; // index into heap_allocs

// Heap management 
void heap_reset() {
	next_alloc_ptr = 0;
	heap_allocs[0] = heap;
}

void *heap_alloc(size_t size)
{
	if(next_alloc_ptr == MAX_ALLOC) {
		backend_debug("heap_alloc: too many concurrent allocs");
		return NULL;
	}

	size = align(size, sizeof(uintptr_t));
	void *start = heap_allocs[next_alloc_ptr];

	next_alloc_ptr ++;
	heap_allocs[next_alloc_ptr] = start + size;

	if(((uint8_t *)heap_allocs[next_alloc_ptr]) >= heap + HEAP_SIZE) {
		backend_debug("heap_alloc: oom");
		next_alloc_ptr --;
		return NULL;
	} else {
		return start;
	}
}

void heap_pop()
{
	if(next_alloc_ptr > 0) {
		next_alloc_ptr--;
	}
}

int heap_get_location(void)
{
	return next_alloc_ptr;
}

void heap_set_location(int ptr)
{
	if(ptr >= 0 && ptr < MAX_ALLOC)
		next_alloc_ptr = ptr;
}

bool heap_free(void *data)
{
	if(next_alloc_ptr > 0 && heap_allocs[next_alloc_ptr - 1] == data) {
		next_alloc_ptr--;
		return true;
	} else {
		return false;
	}
}

/*
size_t heap_avail()
{
	return HEAP_SIZE - ( ((uint8_t *)heap_allocs[next_alloc_ptr]) - heap);
}
*/

