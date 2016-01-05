#include <stdio.h>
#include <stdbool.h>
#include <SDL2/SDL.h>

#include "graphics.h"
#include "palettes.h"
#include "anim.h"
#include "background.h"

/* Thinking for the overall demo framework:
 *
 * static portion:
 * - just a list of (start frame, effect, args) sorted on start frame.
 *
 * dynamic portion:
 * - a background effect,
 * - an animation effect
 * - a palette fade effect
 * - possibly other things.
 *
 * - the list of effect handlers currently instantiated. These will be self-removing.
 *
 * whenever we draw a new frame we catch-up on the static portion, adding things to the dynamic
 * portion, until we've caught up. 
 *
 * we then run each dynamic handler once, passing it the current frame.
 *
 * when a handler is finished it removes itself from the dynamic list.
 *
 * Example for first scene:
 *
 * 0: start background 
 * 0: set palette to (whatever)
 * 0: fade palette to (whatever) over 10 frames
 * 0: start animation of square-morph-to-dancer
 * 15 (or whatever): start dancer animation
 *
 * So implement this as a linked list stored in a block. Perhaps create it in Python, e.g.:
 *    <length><frame number><command><args>
 *
 * where to walk the list you can just continuously add 'length', which should be 16 bit.
*/

int main(int argc, char **argv) {
	if(SDL_Init(SDL_INIT_VIDEO) != 0) {
		fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
		return 1;
	}


	struct animation *anim = anim_load("067230");
	if(anim == NULL) {
		fprintf(stderr, "anim_load\n");
		return 1;
	}

	graphics_init();
	anim_init(graphics_width(), graphics_height());
	background_init();

	background_init_concentric_circles();
	graphics_set_palette(palette_dancer_1_length, palette_dancer_1);

	bool getout = false;

	while(!getout) {
		for(int i = 0; i < 210; i++) {
			SDL_Event event;

			anim_draw(anim, i);
			background_concentric_circles_tick(i);
			graphics_planar_render();
			
			if(SDL_WaitEventTimeout(&event, 10) != 0) {
				switch(event.type) {
					case SDL_KEYUP:
						getout = true;
						break;
				}
			}

			if(getout)
				break;
		}
	}

	graphics_shutdown();

	anim_destroy(anim);

	SDL_Quit();
}

