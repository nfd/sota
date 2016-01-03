#include <inttypes.h>

struct animation {
	int num_frames;
	uint8_t *indices;
	uint8_t *data;
};

void anim_init(int display_width, int display_height);
struct animation *anim_load(char *basename);
int anim_destroy(struct animation *anim);
void anim_draw(struct animation *anim, int frame);

