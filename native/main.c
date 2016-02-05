#include <stdio.h>
#include <stdbool.h>
#include <SDL2/SDL.h>
#include <unistd.h>
#include <getopt.h>
#include <stdlib.h>

#include "graphics.h"
#include "anim.h"
#include "background.h"
#include "choreography.h"
#include "sound.h"
#include "files.h"
#include "font.h"

#define OPT_FULLSCREEN 1
#define OPT_HELP 2
#define OPT_MS 3
#define OPT_NOSOUND 4

struct option options[] = {
	{"fullscreen", no_argument, NULL, OPT_FULLSCREEN},
	{"ms", required_argument, NULL, OPT_MS},
	{"help", no_argument, NULL, OPT_HELP},
	{"nosound", no_argument, NULL, OPT_NOSOUND},
	{0, 0, 0, 0}
};

static void help() {
	printf("State Of The Art\n");
	printf("A demo by Spaceballs, revived by WzDD\n");
	printf("Options:\n\n");
	printf("  --fullscreen     : run in fullscreen rather than a 640x480 window\n");
	printf("  --ms <x>         : start at millisecond x rather than the beginning\n");
	printf("  --nosound        : don't make any noise\n");
}

int main(int argc, char **argv) {
	bool fullscreen = false;
	bool nosound = false;
	int start_ms = 0;

	while(true) {
		int opt = getopt_long(argc, argv, "", options, NULL);

		if(opt == -1)
			break;

		switch(opt) {
			default:
			case '?':
				return 1;
			case OPT_FULLSCREEN:
				fullscreen = true;
				break;
			case OPT_HELP:
				help();
				return 1;
			case OPT_MS:
				start_ms = atoi(optarg);
				break;
			case OPT_NOSOUND:
				nosound = true;
				break;
			case -1:
				break;
		}
	}

	if(files_init() == false) {
		fprintf(stderr, "couldn't read wad\n");
		return 1;
	}

	if(font_init() == false) {
		fprintf(stderr, "couldn't init fonts\n");
		return 1;
	}


	if(SDL_Init(SDL_INIT_AUDIO | SDL_INIT_VIDEO) != 0) {
		fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
		return 1;
	}


	/*
	struct animation *anim = anim_load("067230");
	if(anim == NULL) {
		fprintf(stderr, "anim_load\n");
		return 1;
	}
	*/

	if(sound_init(nosound) == false) {
		fprintf(stderr, "couldn't init sound\n");
		return 1;
	}

	if(choreography_init() == false) {
		fprintf(stderr, "couldn't load choreography\n");
		return 1;
	}

	graphics_init(fullscreen);
	anim_init(graphics_width(), graphics_height());
	background_init();

	choreography_run_demo(start_ms);

#if 0
	background_init_concentric_circles();
	graphics_set_palette(palette_dancer_1_length, palette_dancer_1);

	bool getout = false;


	// TODO: It seems like the demo is running at 25 FPS, or 40 ms / frame.

	while(!getout) {
		for(int i = 0; i < 210; i++) {
			SDL_Event event;

			anim_draw(anim, i);
			background_concentric_circles_tick(i);
			graphics_planar_render();
			
			if(SDL_WaitEventTimeout(&event, 35) != 0) {
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

	anim_destroy(anim);
#endif

	graphics_shutdown();
	SDL_Quit();
	sound_deinit();
	files_deinit();
}

