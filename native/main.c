#include <stdio.h>
#include <SDL2/SDL.h>

#include "graphics.h"
#include "palettes.h"
#include "anim.h"

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

	graphics_set_palette(palette_dancer_1_length, palette_dancer_1);

	anim_draw(anim, 0);
	graphics_planar_render();

	SDL_Delay(5000);

	graphics_shutdown();

	anim_destroy(anim);

	SDL_Quit();
}

