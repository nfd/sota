/* Multi-bit bitmaps: memory representation */
struct multibit_compressed {
	uint32_t signature; // "img1"
	uint16_t width;
	uint16_t height;
	uint16_t num_planes;
	uint16_t num_palette;
	// palette entries follow (4 bytes each)
	// compressed bitplane lengths follow (4 bytes each)
	// compressed bitplanes follow
};


void mbit_display(void *mbit_source, void(*set_palette_from_argb)(int num, uint32_t *in), struct Bitplane *dest_planes);

