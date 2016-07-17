#include <stdbool.h>
#include <inttypes.h>

struct Bitplane;

bool font_init();
bool font_load(int file_idx, uint32_t startchar, uint32_t numchars, uint16_t *positions);
void font_uninit();
int font_get_height();
void font_draw(int numchars, char *text, int x, int y);
void font_centre(int numchars, char *text, int y);

