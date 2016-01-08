#include <stdbool.h>

#define MOD_INTRO 1
#define MOD_MAIN 2

bool sound_init();
bool sound_deinit();
bool sound_mod_play(int mod);
bool sound_mod_stop();

