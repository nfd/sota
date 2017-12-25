#include <stdio.h>
#include <stdbool.h>
#include <unistd.h>
#include <getopt.h>
#include <stdlib.h>
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

#include "graphics.h"
#include "anim.h"
#include "scene.h"
#include "choreography.h"
#include "sound.h"
#include "iff-font.h"
#include "posix_sdl2_backend.h"
#include "heap.h"
#include "tinf/src/tinf.h"

#define DEFAULT_WIDTH 640
#define DEFAULT_HEIGHT 512

#define OPT_FULLSCREEN 1
#define OPT_HELP 2
#define OPT_MS 3
#define OPT_SCENE 4
#define OPT_NOSOUND 5
#define OPT_WIDTH 6
#define OPT_HEIGHT 7
#define OPT_WAD 8

struct option options[] = {
	{"fullscreen", no_argument, NULL, OPT_FULLSCREEN},
	{"ms", required_argument, NULL, OPT_MS},
	{"scene", required_argument, NULL, OPT_SCENE},
	{"help", no_argument, NULL, OPT_HELP},
	{"nosound", no_argument, NULL, OPT_NOSOUND},
	{"width", required_argument, NULL, OPT_WIDTH},
	{"height", required_argument, NULL, OPT_HEIGHT},
	{"wad", required_argument, NULL, OPT_WAD},
	{0, 0, 0, 0}
};

static void help() {
	printf("State Of The Art\n");
	printf("A demo by Spaceballs, revived by WzDD\n");
	printf("Options:\n\n");
	printf("  --ms <x>         : start at millisecond x rather than the beginning\n");
	printf("  --scene <scene>  : start at scene <scene>\n");
	printf("  --nosound        : don't make any noise\n");
	printf("  --fullscreen     : run in fullscreen rather than a 640x480 window\n");
	printf("  --width <x>      : set display width\n");
	printf("  --height <x>     : set display height\n");
	printf("  --wad            : use alternative wad file (sota.wad)\n");
}

int main(int argc, char **argv) {
	bool fullscreen = false;
	bool nosound = false;
	int start_ms = 0;
	char *wad_filename = 0;
	char *start_scene_name = NULL;

	int width = DEFAULT_WIDTH;
	int height = DEFAULT_HEIGHT;

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
			case OPT_SCENE:
				start_scene_name = optarg;
				break;
			case OPT_NOSOUND:
				nosound = true;
				break;
			case OPT_WIDTH:
				width = atoi(optarg);
				break;
			case OPT_HEIGHT:
				height = atoi(optarg);
				break;
			case OPT_WAD:
				wad_filename = optarg;
				break;
			case -1:
				break;
		}
	}

	heap_reset();
	tinf_init();

	if(backend_init(width, height, fullscreen, wad_filename) == false) {
		fprintf(stderr, "couldn't init backend\n");
	}

	if(ifffont_init() == false) {
		fprintf(stderr, "couldn't init fonts\n");
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

	graphics_init();
	anim_init();
	scene_init();

	backend_run(start_ms, start_scene_name);

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

#ifdef __EMSCRIPTEN__
	emscripten_exit_with_live_runtime();
#else
	graphics_shutdown();
	sound_deinit();
	backend_shutdown();
#endif
}

