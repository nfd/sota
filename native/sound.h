#include <stdbool.h>

bool sound_init(bool nosound);
bool sound_deinit();
bool sound_mod_play(int mod);
bool sound_mod_stop();
bool sound_mp3_play(int mp3);
bool sound_mp3_stop();
bool sound_sample_play(int sample_idx);
void sound_update(void);

