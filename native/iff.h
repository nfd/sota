#include <stdbool.h>

struct BitmapHeader {
	uint16_t w, h;             /* raster width & height in pixels      */
	int16_t  x, y;             /* pixel position for this image        */
	uint8_t  nPlanes;          /* # source bitplanes                   */
	uint8_t  masking;
	uint8_t  compression;
	uint8_t  pad1;             /* unused; ignore on read, write as 0   */
	uint16_t transparentColor; /* transparent "color number" (sort of) */
	uint8_t  xAspect, yAspect; /* pixel aspect, a ratio width : height */
	int16_t  pageWidth, pageHeight; /* source "page" size in pixels    */
} __attribute__((__packed__));

struct LoadedIff {
	uint8_t *data;
	struct BitmapHeader *bmhd;
	int8_t *body;
	uint32_t cmap_count;
	uint8_t *cmap;
};

bool iff_load(int file_idx, struct LoadedIff *iff);
void iff_unload(struct LoadedIff *iff);
void iff_get_dimensions(struct LoadedIff *iff, uint16_t *w, uint16_t *h);
bool iff_display(struct LoadedIff *iff, int dst_x, int dst_y, int dst_w, int dst_h, uint32_t *num_colours, uint32_t *palette_out, struct Bitplane *planes, int start_plane);

