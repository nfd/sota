#ifndef BACKEND_H
#define BACKEND_H

#include <stdbool.h>
#include <stdint.h>

/* Blit modes */
#define BLIT_MODE_AND 1  /* overwrite destination, unused bitplanes treated as 0 */
#define BLIT_MODE_OR  2  /* OR against destination */

/* Blit command struct */
struct Bitplane {
	int width, height, stride;
	int offsetx, offsety;
	int idx; // bitplane number
	uint8_t *data;
};

/* Parts of the code (especially the background code) will make use of a bitmapped display if it is present. */
struct backend_interface_struct {
	/* Set by backend after startup */
	int width, height;
	int bitplane_stride; // e.g. width / 8 (but not necessarily)

	/* Backend-specific startup and shutdown */
	bool (*init)(struct backend_interface_struct *backend, bool fullscreen); 
	void (*shutdown)();

	/* Return a 64-bit time value representing milliseconds. This value should be
	 * increasing and ideally be monotonic -- but it's just a demo, so wall clock
	 * time is also fine.
	*/
	uint64_t (*get_time_ms)();

	/* A function which returns true when it is time to display the next frame, 
	 * or false if no more frames should be displayed
	*/
	bool (*should_display_next_frame)(int64_t time_remaining_this_frame);

	/* Graphics handling: line drawing */
	void (*planar_line_horizontal)(int bitplane_idx, int y, int start_x, int end_x, bool xor);
	void (*planar_line_vertical)(int bitplane_idx, int x, int start_y, int end_y, bool xor);

	/* Clear a bitplane (perhaps at the next draw operation) */
	void (*planar_clear)(int bitplane);

	/* Present the current frame and prepare to render the next. */
	void (*render)();

	/* Generic memory allocation#
	 * malloc_temporary may allocate from a separate temporary pool to avoid memory fragmentation
	 * Use malloc_temporary for allocations which will be freed before the caling function returns.
	*/
	void *(*malloc)(size_t amt);
	void *(*malloc_temporary)(size_t amt);
	void (*free)(void *);

	/* Blitting: blit a bitplane or bitplanes to the display.
	 * "mode" should be one of BLIT_MODE_AND or BLIT_MODE_OR.
	*/
	void (*blit)(int width, int height, int mode, int count, struct Bitplane *planes);
};

#endif // BACKEND_H
