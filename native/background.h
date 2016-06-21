void background_init();
void background_init_spotlights();
void background_spotlights_tick(int cnt);
void background_deinit_spotlights();

void background_init_votevotevote(void *effect_data, uint32_t *palette_a, uint32_t *palette_b);
void background_votevotevote_tick(int ms);
void background_deinit_votevotevote();

void background_init_delayedblit();
void background_delayedblit_tick(int ms);
void background_deinit_delayedblit();

void background_init_copperpastels();
void background_copperpastels_tick(int ms);
void background_deinit_copperpastels();
