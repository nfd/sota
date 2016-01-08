#include <stdbool.h>
#include <inttypes.h>
#include <stddef.h>
#include <SDL2/SDL.h>
#include <time.h>

#include "files.h"
#include "graphics.h"
#include "anim.h"
#include "background.h"
#include "sound.h"

#define FILENAME "data/choreography.bin"

#define CMD_END 0
#define CMD_CLEAR 1
#define CMD_PALETTE 2
#define CMD_FADETO 3
#define CMD_ANIM 4
#define CMD_PAUSE 5
#define CMD_MOD 6

#define MOD_START 1
#define MOD_STOP 2

/* Frame rate the demo runs at. This doesn't affect the speed of the animations, 
 * which run at 25 fps */
#define MS_PER_FRAME 20

struct choreography_header {
	uint32_t start_ms;
	uint32_t length;
	uint32_t cmd;
};

struct choreography_clear {
	struct choreography_header header;
	uint32_t plane;
};

struct choreography_palette {
	struct choreography_header header;
	uint32_t count;
	uint32_t values[32];
};

struct choreography_fadeto {
	struct choreography_header header;
	uint32_t ms;
	uint32_t palette_cmd;
	uint32_t count;
	uint32_t values[32];
};

struct choreography_anim {
	struct choreography_header header;
	uint32_t ms_per_frame;
	uint32_t frame_from;
	uint32_t frame_to;
	uint32_t name;
	uint32_t bitplane;
	uint32_t xor;
};

struct choreography_pause {
	struct choreography_header header;
	uint32_t end_ms;
};

struct choreography_mod {
	struct choreography_header header;
	uint32_t subcmd;
	uint32_t arg;
};

static uint8_t *choreography;
static ssize_t choreography_size;
static struct choreography_header *pos;

/* Things which may be happening. */
static struct {
	// animations
	struct animation *current_animation; // the anim data
	struct choreography_anim *current_animation_info; // the choreography data
	int last_drawn_frame; // to avoid unnecessary redraws

	// palette fades
	uint32_t fade_from[32]; // the constants feel so naughty
	uint32_t fade_to[32]; // but they directly model Amiga hardware (OCS / ECS)
	uint32_t fade_count;
	int fade_start_ms;
	int fade_end_ms;

	// pausing
	int pause_end_ms;
} state;

static inline struct choreography_header *next_header(struct choreography_header *current_header)
{
	return (struct choreography_header *)(((uint8_t *)current_header) + current_header->length);
}

bool choreography_init()
{
	choreography = read_entire_file(FILENAME, &choreography_size);
	pos = (struct choreography_header *)choreography;

	state.current_animation = NULL;
	state.current_animation_info = NULL;
	state.fade_count = 0;

	state.pause_end_ms = 0;

	return choreography != NULL;
}

static void cmd_clear(struct choreography_clear *clear)
{
	if(clear->plane == 0xff) {
		for(int i = 0; i < 4; i++) {
			graphics_planar_clear(i);
		}
	} else {
		graphics_planar_clear(clear->plane);
	}
}

static void cmd_palette(struct choreography_palette *palette) {
	graphics_set_palette(palette->count, palette->values);
}

static void cmd_fadeto(struct choreography_fadeto *fadeto) {
	state.fade_start_ms = fadeto->header.start_ms;
	state.fade_end_ms = state.fade_start_ms + fadeto->ms;
	state.fade_count = fadeto->count;

	graphics_get_palette(32, state.fade_from);
	memcpy(state.fade_to, fadeto->values, fadeto->count * sizeof(uint32_t));

	if(fadeto->count < 32) {
		memset(&state.fade_to[fadeto->count], 0, (32 - fadeto->count) * sizeof(uint32_t));
	}
}

static void cmd_anim(struct choreography_anim *anim) {
	// Unload the current animation if it's different.
	// TODO get rid of the mallocs and frees in this function! (And stop using the disk...)
	if(state.current_animation_info != NULL && state.current_animation_info->name != anim->name) {
		anim_destroy(state.current_animation);
	}

	// TODO get rid of this
	char basename[8];
	sprintf(basename, "%06x", anim->name);

	state.current_animation = anim_load(basename);
	state.current_animation_info = anim;

	if(state.current_animation == NULL) {
		fprintf(stderr, "Couldn't load animation %x (pos %p)\n", anim->name, (uint8_t *)anim - choreography);
	}

	anim_set_bitplane(anim->bitplane);
	anim_set_xor(anim->xor);
}

static void cmd_pause(struct choreography_pause *pause) {
	state.pause_end_ms = pause->end_ms;
}

static void cmd_mod(struct choreography_mod *mod) {
	switch(mod->subcmd) {
		case MOD_START:
			sound_mod_play(mod->arg);
			break;
		case MOD_STOP:
			sound_mod_stop();
			break;
	}
}

// Create the state item at 'pos' without advancing it.
static void create_state_item()
{
	switch(pos->cmd) {
		case CMD_END:
			/* Nothing to do and don't advance */
			break;
		case CMD_CLEAR:
			cmd_clear((struct choreography_clear *)pos);
			break;
		case CMD_PALETTE:
			cmd_palette((struct choreography_palette *)pos);
			break;
		case CMD_FADETO:
			cmd_fadeto((struct choreography_fadeto *)pos);
			break;
		case CMD_ANIM:
			cmd_anim((struct choreography_anim *)pos);
			break;
		case CMD_PAUSE:
			cmd_pause((struct choreography_pause *)pos);
			break;
		case CMD_MOD:
			cmd_mod((struct choreography_mod *)pos);
			break;
		default:
			/* This is bad */
			fprintf(stderr, "Unknown choreography command %x\n", pos->cmd);
			break;
	}
}

static void create_new_state(int ms)
{
	/* Create the state */
	while(pos->start_ms <= ms && pos->cmd != CMD_END) {
		create_state_item(pos);
		pos = next_header(pos);
	}
}

static void run(int ms) {
	/* Run everything in the queue. */

	// Animation.
	if(state.current_animation_info) {
		/* Work out what frame we're supposed to be on. */
		int anim_ms = ms - state.current_animation_info->header.start_ms;
		int anim_frame = (anim_ms / state.current_animation_info->ms_per_frame) + state.current_animation_info->frame_from;

		if(anim_frame > state.current_animation_info->frame_to) {
			/* We're done with this animation */
			anim_destroy(state.current_animation);
			state.current_animation_info = NULL;
		} else {
			if(state.last_drawn_frame != anim_frame) {
				state.last_drawn_frame = anim_frame;
				anim_draw(state.current_animation, anim_frame);
			}
		}
	}

	// TODO backgrounds

	// palette fades
	if(state.fade_count != 0) {
		if(ms > state.fade_end_ms) {
			// we're done with this palette fade
			graphics_set_palette(state.fade_count, state.fade_to);
			state.fade_count = 0;
		} else {
			graphics_lerp_palette(state.fade_count, state.fade_from, state.fade_to, ms - state.fade_start_ms, state.fade_end_ms - state.fade_start_ms);
		}
	}

	// pauses
	if(ms > state.pause_end_ms) {
		state.pause_end_ms = 0; // not even necessary to do this
	}
}

static uint64_t gettime_ms()
{
	struct timespec tp;

	clock_gettime(CLOCK_REALTIME, &tp);
	
	return (uint64_t)(tp.tv_sec * 1000) + (tp.tv_nsec / 1000000);
}

/* This is looking nice! */
// TODO: include commands to switch pen between overwrite and XOR. It's important in the first sequence,
// may already be encoded but don't care anymore. Or maybe I do. Argh.

void choreography_run_demo(int ms)
{
	bool keepgoing = true;
	uint64_t starttime = gettime_ms();

	/* If 'ms' is initially >0, backdate startime */
	starttime -= ms;

	while(keepgoing) {
		uint64_t frametime = gettime_ms();
		ms = frametime - starttime;

		create_new_state(ms);
		run(ms);

		uint64_t frame_runtime = gettime_ms() - frametime;

		//anim_draw(anim, i);
		//background_concentric_circles_tick(i);
		graphics_planar_render();
		
		SDL_Event event;
		if(SDL_WaitEventTimeout(&event, MS_PER_FRAME - frame_runtime) != 0) {
			switch(event.type) {
				case SDL_KEYUP:
					keepgoing = false;
					break;
			}
		}
	}
}

