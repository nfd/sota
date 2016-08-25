#include <inttypes.h>
#include <stdbool.h>

#include "backend.h"

struct animation {
	int data_file;
	int num_frames;
	uint8_t *indices;
	uint8_t *data;
	uint8_t *past_data_end;
};

void anim_init();
void anim_set_bitplane(struct Bitplane *);
void anim_set_xor(bool xor);
struct animation *anim_load(int data_file, int anim_idx);
int anim_destroy(struct animation *anim);
void anim_draw(struct animation *anim, int frame);
uint8_t *lerp_tween(uint8_t *tween_from, uint8_t *tween_to, int tween_t, int tween_count);

