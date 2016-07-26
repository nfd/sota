#ifndef BACKEND_H
#define BACKEND_H

/* 
 * Graphics: the backend manages to real display. The graphics functions provide bitmaps
 * which are used to generate this display. These are each the same size as the display.
*/

#include <stdbool.h>
#include <stdint.h>

struct Bitplane {
	int width, height, stride;
	uint8_t *data; // potentially offset
	uint8_t *data_start; // unoffsetted data
};

extern int window_width, window_height;
// On memory-unconstrained systems, the bitplanes may be twice as wide and tall as the window. 
// On memory constrained systems, they will be the same size.
// This is bloody annoying.

/* New plan: remove 'background', replace with 'scene'. These are the basically the same except that
 * bitmap memory is allocated by the scene itself when it's instantiated, and deallocated when it's 
 * finished. This will let us have the moving-circles scene consisting of:
 *   - one bitplane 2x width and 2x height
 *   - one regular bitplane for dancers
 * and one scene consisting of
 *   - 5 bitplanes of the screen width & height
 * and possibly other things.
 *
 * Thus bitplane_width, bitplane_height and bitplane_stride exist.
*/
extern struct Bitplane backend_bitplane[5]; // the bitplanes

void backend_delete_bitplanes();
struct Bitplane *backend_allocate_bitplane(int idx, int width, int height);
void backend_allocate_standard_bitplanes();
void backend_copy_bitplane(struct Bitplane *dst, struct Bitplane *src);

/* Backend-specific startup and shutdown */
bool backend_init(int width, int height, bool fullscreen); 
void backend_shutdown();

/* Return a 64-bit time value representing milliseconds. This value should be
 * increasing and ideally be monotonic -- but it's just a demo, so wall clock
 * time is also fine.
*/
uint64_t backend_get_time_ms();

/* A function which returns true when it is time to display the next frame, 
 * or false if no more frames should be displayed
*/
bool backend_should_display_next_frame(int64_t time_remaining_this_frame);

/* Present the current frame and prepare to render the next. */
void backend_render();

/* Memory allocations are performed in the "init" portion of the module. They
 * are freed by the backend at shutdown and can't be freed by other modules.
 * This is to ensure that if the module can initialise, it will have enough
 * memory to run. */
void *backend_reserve_memory(size_t size);

/* Palette manipulation. The external palette is uin32_t. */
void backend_set_palette(size_t num_elements, uint32_t *elements);
void backend_get_palette(size_t num_elements, uint32_t *elements);
void backend_set_palette_element(int idx, uint32_t element);

/* Font drawing. This is backend-specific since some backends can't
 * load a whole-screen bitmapped font. */
bool backend_font_load(int file_idx, uint32_t startchar, uint32_t numchars, uint16_t *positions);
void backend_font_unload();
// font drawing: pass -1 as x co-ordinate to centre 
void backend_font_draw(int numchars, char *text, int x, int y);
int backend_font_get_height();

#endif // BACKEND_H

