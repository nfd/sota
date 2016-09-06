#include <inttypes.h>

typedef struct { uint64_t state; uint64_t inc; } pcg32_random_t;

uint32_t pcg32_random_r(pcg32_random_t *rng);
