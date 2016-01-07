#include <stdbool.h>
#include <inttypes.h>
#include <stddef.h>
#include <SDL2/SDL.h>
#include <time.h>

#include "files.h"
#include "graphics.h"
#include "anim.h"
#include "background.h"

#define FILENAME "data/choreography.bin"

#define CMD_END 0
#define CMD_CLEAR 1
#define CMD_PALETTE 2
#define CMD_FADETO 3
#define CMD_ANIM 4

#define MS_PER_TICK 20

struct choreography_header {
	uint32_t start_tick;
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
	uint32_t ticks;
	uint32_t palette_cmd;
	uint32_t count;
	uint32_t values[32];
};

struct choreography_anim {
	struct choreography_header header;
	uint32_t ticks_per_frame;
	uint32_t frame_from;
	uint32_t frame_to;
	uint32_t name;
};

static uint8_t *choreography;
static ssize_t choreography_size;
static struct choreography_header *pos;

/* Things which may be happening. */
static struct {
	struct animation *current_animation;
	struct choreography_anim *current_animation_info;
	int last_drawn_frame;
	// TODO palette fades
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
	// TODO
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
		default:
			/* This is bad */
			fprintf(stderr, "Unknown choreography command %x\n", pos->cmd);
			break;
	}
}

static void catch_up(int ticks)
{
	/* Advance 'pos' to match 'ticks' but don't go past it. */
	struct choreography_header *closest_older = pos;

	while(pos->start_tick < ticks) {
		closest_older = pos;

		if(pos->cmd == CMD_END)
			break;
		pos = next_header(pos);
	}

	if(pos->start_tick > ticks) {
		/* we went too far: we're somewhere in the middle of the previous sequence. */
		pos = closest_older;
	}

	/* Create the state */
	while(pos->start_tick <= ticks && pos->cmd != CMD_END) {
		create_state_item(pos);
		pos = next_header(pos);
	}
}

static void run(int tick) {
	/* Run everything in the queue. */

	// Animation.
	if(state.current_animation_info) {
		/* Work out what frame we're supposed to be on. */
		int anim_tick = tick - state.current_animation_info->header.start_tick;
		int anim_frame = (anim_tick / state.current_animation_info->ticks_per_frame) + state.current_animation_info->frame_from;

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

	// TODO palette fades
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

void choreography_run_demo(int tick)
{
	bool keepgoing = true;
	uint64_t starttime = gettime_ms();

	/* If 'tick' is initially >0, backdate startime */
	starttime -= (tick * MS_PER_TICK);

	while(keepgoing) {
		uint64_t frametime = gettime_ms();
		tick = (frametime - starttime) / MS_PER_TICK;

		catch_up(tick);
		run(tick);
		
		uint64_t frame_runtime = gettime_ms() - frametime;

		//anim_draw(anim, i);
		//background_concentric_circles_tick(i);
		graphics_planar_render();
		
		SDL_Event event;
		if(SDL_WaitEventTimeout(&event, MS_PER_TICK - frame_runtime) != 0) {
			switch(event.type) {
				case SDL_KEYUP:
					keepgoing = false;
					break;
			}
		}
	}
}

