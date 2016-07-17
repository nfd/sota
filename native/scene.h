#include <stdbool.h>

void scene_init();
bool scene_init_spotlights();
void scene_spotlights_tick(int cnt);
void scene_deinit_spotlights();

void scene_init_votevotevote(void *effect_data, uint32_t *palette_a, uint32_t *palette_b);
void scene_votevotevote_tick(int ms);
void scene_deinit_votevotevote();

void scene_init_delayedblit();
void scene_delayedblit_tick(int ms);
void scene_deinit_delayedblit();

void scene_init_copperpastels();
void scene_copperpastels_tick(int ms);
void scene_deinit_copperpastels();

