#include <stdbool.h>
#include <stdint.h>

struct choreography_header {
	uint32_t start_ms;
	uint32_t length;
	uint32_t cmd;
};

struct choreography_scene_index_item {
	uint32_t ms;
	uint32_t offset;
	uint8_t name[8];
};

struct choreography_scene_index {
	struct choreography_header header;
	uint32_t count;
	struct choreography_scene_index_item items[];
};


bool choreography_init();
void choreography_run_demo(int ms);

bool choreography_prepare_to_run(uint8_t *choreography_in, int ms);
bool choreography_do_frame(int ms);
uint32_t choreography_find_offset_for_scene(uint8_t *choreography, unsigned ms, uint32_t *next_scene_offset);
uint32_t choreography_find_ms_for_scene_name(uint8_t *choreography, char *name);

