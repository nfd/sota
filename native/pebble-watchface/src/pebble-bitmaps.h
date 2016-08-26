#include <pebble.h>

struct Bitplane;
struct onebit_bitmap;

void render_bitplanes(Layer *root, GContext *ctx);
void load_mbitmap(ResHandle handle);
