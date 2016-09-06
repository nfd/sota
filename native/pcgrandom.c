/* Minimal C implementation, Apache license. See pcg-random.org */
#include <inttypes.h>

#include "pcgrandom.h"

uint32_t pcg32_random_r(pcg32_random_t *rng)
{
	uint64_t oldstate = rng->state;
	// advance internal state
	rng->state = oldstate * 6364136223846793005ULL + (rng->inc|1);
	// calculate output function (XSH RR), uses old state for max ILP
	uint32_t xorshifted = ((oldstate >> 18u) ^ oldstate) >> 27u;
	uint32_t rot = oldstate >> 59u;
	return (xorshifted >> rot) | (xorshifted << ((-rot) & 31));
}

