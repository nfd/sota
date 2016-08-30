#include <stdbool.h>
#include <inttypes.h>

bool ifffont_init();
bool ifffont_load(int file_idx, uint32_t startchar, uint32_t numchars, uint16_t *positions, struct Bitplane *planes);
void ifffont_unload();
void ifffont_uninit();
int ifffont_get_height();
int ifffont_measure(int numchars, char *text);
void ifffont_draw(int numchars, char *text, int x, int y, struct Bitplane dest_planes[]);
void ifffont_centre(int numchars, char *text, int y, struct Bitplane dest_planes[]);

