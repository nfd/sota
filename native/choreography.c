#define _POSIX_C_SOURCE 199309L
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
#include "iff.h"
#include "font.h"

#define FILENAME "data/choreography.bin"

#define CMD_END 0
#define CMD_CLEAR 1
#define CMD_PALETTE 2
#define CMD_FADETO 3
#define CMD_ANIM 4
#define CMD_PAUSE 5
#define CMD_MOD 6
#define CMD_ILBM 7
#define CMD_SOUND 8
#define CMD_STARTEFFECT 9
#define CMD_LOADFONT 10
#define CMD_ALTERNATE_PALETTE 11
#define CMD_USE_ALTERNATE_PALETTE 12

#define MOD_START 1
#define MOD_STOP 2

#define EFFECT_NOTHING 0
#define EFFECT_SPOTLIGHTS 1
#define EFFECT_VOTEVOTEVOTE 2
#define EFFECT_DELAYEDBLIT 3
#define EFFECT_COPPERPASTELS 4

#define ILBM_FULLSCREEN 0
#define ILBM_CENTRE 1

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

struct choreography_alternate_palette {
	struct choreography_header header;
	uint32_t alternate_idx;
	uint32_t values[32];
};

struct choreography_use_alternate_palette {
	struct choreography_header header;
	uint32_t alternate_idx;
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
	uint32_t index_file;
	uint32_t data_file;
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

struct choreography_ilbm {
	struct choreography_header header;
	uint32_t file_idx;
	uint32_t display_type;
	uint32_t fade_in_ms;
};

struct choreography_sound {
	struct choreography_header header;
	uint32_t file_idx;
};

struct choreography_starteffect {
	struct choreography_header header;
	uint32_t effect_num;
	uint8_t effect_data[];
};

struct choreography_loadfont {
	struct choreography_header header;
	uint32_t file_idx;
	uint32_t startchar;
	uint32_t numchars;
	uint16_t positions[];
};

static uint8_t *choreography;
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

	// effects
	void (*effect_tick)(int ms);
	void (*effect_deinit)();
} state;

static inline struct choreography_header *next_header(struct choreography_header *current_header)
{
	return (struct choreography_header *)(((uint8_t *)current_header) + current_header->length);
}

bool choreography_init()
{
	choreography = file_get_choreography();
	pos = (struct choreography_header *)choreography;

	state.current_animation = NULL;
	state.current_animation_info = NULL;
	state.fade_count = 0;

	state.pause_end_ms = 0;

	state.effect_deinit = state.effect_tick = NULL;

	return choreography != NULL;
}

static void cmd_clear(struct choreography_clear *clear)
{
	if(clear->plane == 0xff) {
		for(int i = 0; i < 5; i++) {
			graphics_planar_clear(i);
		}
	} else {
		graphics_planar_clear(clear->plane);
	}
}

static void cmd_palette(struct choreography_palette *palette) {
	graphics_set_palette(palette->count, palette->values);
}

static void cmd_alternate_palette(struct choreography_alternate_palette *alternate) {
	/* Store the palette in "fade_to" */
	int i;
	uint32_t *dest = alternate->alternate_idx == 0 ? state.fade_from : state.fade_to;

	for(i = 0; i < 32; i++) {
		dest[i] = alternate->values[i];
	}
}

static void cmd_use_alternate_palette(struct choreography_use_alternate_palette *alternate) {
	uint32_t *src = alternate->alternate_idx == 0 ? state.fade_from : state.fade_to;

	graphics_set_palette(32, src);
}

static void _fade_to(int start_ms, int end_ms, int count, uint32_t *palette)
{
	state.fade_start_ms = start_ms;
	state.fade_end_ms = end_ms;
	state.fade_count = count;

	graphics_get_palette(32, state.fade_from);
	memcpy(state.fade_to, palette, count * sizeof(uint32_t));

	if(count < 32) {
		memset(&state.fade_to[count], 0, (32 - count) * sizeof(uint32_t));
	}
}

static void cmd_fadeto(struct choreography_fadeto *fadeto) {
	_fade_to(fadeto->header.start_ms, fadeto->header.start_ms + fadeto->ms, fadeto->count, fadeto->values);
}

static void cmd_anim(struct choreography_anim *anim) {
	// Unload the current animation if it's different.
	// TODO get rid of the mallocs and frees in this function! (And stop using the disk...)
	if(state.current_animation_info != NULL && state.current_animation_info->data_file != anim->data_file) {
		anim_destroy(state.current_animation);
	}

	state.current_animation = anim_load(anim->index_file, anim->data_file);
	state.current_animation_info = anim;

	if(state.current_animation == NULL) {
		fprintf(stderr, "Couldn't load animation %x\n", anim->data_file);
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

static void cmd_ilbm(struct choreography_ilbm *ilbm) {
	// we re-use the fade-to palette for the image palette
	// TODO display_type is ignored
	iff_display(ilbm->file_idx, 0, 0, graphics_width(), graphics_height(), &(state.fade_count), state.fade_to);

	if(ilbm->fade_in_ms == 0) {
		graphics_set_palette(state.fade_count, state.fade_to);
		state.fade_count = 0; 
	} else {
		/* lerp to palette */
		graphics_get_palette(32, state.fade_from);

		state.fade_start_ms = ilbm->header.start_ms;
		state.fade_end_ms = ilbm->header.start_ms + ilbm->fade_in_ms;

		// Everything else has been done for us above.
	}
}

static void cmd_sound(struct choreography_sound *sound) {
	sound_sample_play(sound->file_idx);
}

static void cmd_starteffect(int ms, struct choreography_starteffect *effect) {
	//state->current_effect = effect->effect_num;

	if(state.effect_deinit) {
		state.effect_deinit();
		state.effect_tick = state.effect_deinit = NULL;
	}

	switch(effect->effect_num) {
		case EFFECT_NOTHING:
			break;
		case EFFECT_SPOTLIGHTS:
			background_init_spotlights();
			state.effect_tick = background_spotlights_tick;
			state.effect_deinit = background_deinit_spotlights;
			break;
		case EFFECT_VOTEVOTEVOTE:
			background_init_votevotevote(effect->effect_data, state.fade_from, state.fade_to);
			state.effect_tick = background_votevotevote_tick;
			state.effect_deinit = background_deinit_votevotevote;
			break;
		case EFFECT_DELAYEDBLIT:
			background_init_delayedblit();
			state.effect_tick = background_delayedblit_tick;
			state.effect_deinit = background_deinit_delayedblit;
			break;
		case EFFECT_COPPERPASTELS:
			background_init_copperpastels();
			state.effect_tick = background_copperpastels_tick;
			state.effect_deinit = background_deinit_copperpastels;
			break;
		default:
			fprintf(stderr, "Unknown effect %d ignored\n", effect->effect_num);
			break;
	}
}

static void cmd_loadfont(int ms, struct choreography_loadfont *loadfont) {
	font_load(loadfont->file_idx, loadfont->startchar, loadfont->numchars, loadfont->positions);
}

// Create the state item at 'pos' without advancing it.
static void create_state_item(int ms, struct choreography_header *pos)
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
		case CMD_ALTERNATE_PALETTE:
			cmd_alternate_palette((struct choreography_alternate_palette *)pos);
			break;
		case CMD_USE_ALTERNATE_PALETTE:
			cmd_use_alternate_palette((struct choreography_use_alternate_palette *)pos);
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
		case CMD_ILBM:
			cmd_ilbm((struct choreography_ilbm *)pos);
			break;
		case CMD_SOUND:
			cmd_sound((struct choreography_sound *)pos);
			break;
		case CMD_STARTEFFECT:
			cmd_starteffect(ms, (struct choreography_starteffect *)pos);
			break;
		case CMD_LOADFONT:
			cmd_loadfont(ms, (struct choreography_loadfont *)pos);
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
		create_state_item(ms, pos);
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

	// Backgrounds.
	if(state.effect_tick) {
		state.effect_tick(ms);
	}

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

static void skip_to_start_ms(int ms) {
	/* Advance our start, but apply palette changes. This is a hack. */
	pos = (struct choreography_header *)choreography;
	while(pos->start_ms < ms) {
		switch(pos->cmd) {
			case CMD_PALETTE:
				cmd_palette((struct choreography_palette *)pos);
				break;
			case CMD_FADETO: {
				struct choreography_fadeto *fadeto = (struct choreography_fadeto *)pos;
				graphics_set_palette(fadeto->count, fadeto->values);
				break;
			 }
		}
		pos = next_header(pos);
	}
}

void choreography_run_demo(int ms)
{
	skip_to_start_ms(ms);

	bool keepgoing = true;
	uint64_t starttime = gettime_ms();

	/* If 'ms' is initially >0, backdate startime */
	starttime -= ms;

	while(keepgoing) {
		uint64_t frametime = gettime_ms();
		ms = frametime - starttime;

		create_new_state(ms);
		run(ms);

		int64_t time_remaining_this_frame = MS_PER_FRAME - (gettime_ms() - frametime);

		//anim_draw(anim, i);
		//background_concentric_circles_tick(i);
		graphics_planar_render();
		
		if(time_remaining_this_frame > 0) {
			SDL_Event event;
			if(SDL_WaitEventTimeout(&event, time_remaining_this_frame) != 0) {
				switch(event.type) {
					case SDL_KEYUP:
						keepgoing = false;
						break;
				}
			}
		}
	}
}

