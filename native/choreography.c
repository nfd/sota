#define _POSIX_C_SOURCE 199309L
#include <stdbool.h>
#include <inttypes.h>
#include <stddef.h>
#include <string.h>

#include "graphics.h"
#include "anim.h"
#include "scene.h"
#include "mbit.h"

#ifdef BACKEND_SUPPORTS_SOUND
#include "sound.h"
#endif

#ifdef BACKEND_SUPPORTS_ILBM
#include "iff.h"
#endif

#include "backend.h"
#include "choreography.h"
#include "choreography_commands.h"

#define SOUND_START 1
#define SOUND_STOP 2

#define EFFECT_NOTHING 0
#define EFFECT_SPOTLIGHTS 1
#define EFFECT_VOTEVOTEVOTE 2
#define EFFECT_DELAYEDBLIT 3
#define EFFECT_COPPERPASTELS 4
#define EFFECT_STATIC 5
#define EFFECT_STATIC2 6

#define SCENE_ONION_SKIN 0x40

#define BITPLANE_OFF 0
#define BITPLANE_1X1 1
#define BITPLANE_2X1 2
#define BITPLANE_2X2 3

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
	uint32_t data_file;
	uint16_t frame_from;
	uint16_t frame_to;
	uint16_t bitplane;
	uint16_t xor;
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
	uint8_t x, y, w, h; /* 0 = smallest, 240 = largest */
	uint32_t fade_in_ms;
	uint32_t plane;
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

struct choreography_scene {
	struct choreography_header header;
	uint8_t bitplane_style[6];
	uint8_t msperframe;
	uint8_t onion_skin_cmd_high;
	uint8_t onion_skin_cmd_low;
	uint8_t post_anim_clear_plane_mask;
	uint8_t first_anim_frame_plane_copy_mask;
	uint8_t name_length;
	uint8_t name[];
};

#define SCENE_OPTION_DISTORT_MASK (1 << 15)
#define SCENE_OPTION_FLIP_VERTICAL_MASK (1 << 14)
#define SCENE_OPTION_FLIP_HORIZONTAL_MASK (1 << 13)
#define SCENE_OPTION_ANIM_OUTLINE_MASK (1 << 12)
#define SCENE_OPTION_ANIM_3_BEHIND_PLANE_1 (1 << 11)
#define SCENE_OPTION_EPILEPSY (1 << 10)
#define SCENE_OPTION_MULTIDRAW_3D (1 << 9)

struct choreography_scene_options {
	struct choreography_header header;
	uint16_t zoom;
	uint16_t flags;
};

struct choreography_mbit {
	struct choreography_header header;
	uint32_t file_idx;
};

struct choreography_end {
	struct choreography_header header;
	uint32_t end_of_scene;
};

static uint8_t *choreography;
static struct choreography_header *pos;

/* Things which may be happening. */
static struct {
	// animations
	struct animation *current_animation; // the anim data
	struct choreography_anim current_animation_info; // the choreography data (copied)
	bool animation_is_running;
	unsigned last_drawn_frame; // to avoid unnecessary redraws
	unsigned backwards_subtract; // A value here allows running the animation backwards
	uint16_t ms_per_anim_frame;
	uint8_t post_anim_clear_plane_mask; /* clear these planes when anim finishes */
	uint8_t first_anim_frame_plane_copy_mask; /* copy the first anim frame to these planes */
	bool onion_skin;
	bool anim_3_behind_plane_1;
	int onion_skin_low_frame;
	int onion_skin_high_frame;
	int anim_bitplane;
	bool epilepsy;
	bool epilepsy_last_frame;

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
	choreography = NULL;
	pos = NULL;

	state.current_animation = NULL;
	state.current_animation_info.data_file = 0xffffffff;
	state.animation_is_running = false;
	state.fade_count = 0;

	state.pause_end_ms = 0;

	state.effect_deinit = state.effect_tick = NULL;
	state.anim_3_behind_plane_1 = false;
	state.anim_bitplane = 0;
	state.epilepsy = false;

	return true;
}

static inline void clear_masked_planes(uint8_t mask)
{
	for(int i = 0; i < 6; i++) {
		if(mask & (1 << i))
			planar_clear(&backend_bitplane[i]);
	}
}

static void cmd_clear(struct choreography_clear *clear)
{
	/* TODO maybe just update the choreograph commands instead, so that they take a mask */
	if(clear->plane == 0xff) {
		for(int i = 0; i < 6; i++)
			planar_clear(&backend_bitplane[i]);
	} else 
		planar_clear(&backend_bitplane[clear->plane]);
}

static void cmd_palette(struct choreography_palette *palette) {
	/* Turn off any running fades */
	state.fade_count = 0;
	
	backend_set_palette(palette->count, palette->values);
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

	backend_set_palette(32, src);
}

static void _fade_to(int start_ms, int end_ms, int count, uint32_t *palette)
{
	state.fade_start_ms = start_ms;
	state.fade_end_ms = end_ms;
	state.fade_count = count;

	backend_get_palette(32, state.fade_from);
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
	if(state.current_animation_info.data_file != anim->data_file) {
		anim_destroy(state.current_animation);
	}

	state.current_animation = anim_load(anim->data_file, 0);
	state.current_animation_info = *anim;
	state.last_drawn_frame = 0xffff;
	state.animation_is_running = true;

	if(anim->frame_from > anim->frame_to) {
		/* Backwards; calculate backwards subtraction and flip */
		state.backwards_subtract = anim->frame_from + anim->frame_to;
		state.current_animation_info.frame_from = anim->frame_to;
		state.current_animation_info.frame_to = anim->frame_from;
	} else {
		state.backwards_subtract = 0;
	}

	if(state.current_animation == NULL) {
		backend_debug("Couldn't load animation %x\n", (unsigned int)(anim->data_file));
	}

	state.anim_bitplane = anim->bitplane;
	anim_set_xor(anim->xor);
}

static void cmd_pause(struct choreography_pause *pause) {
	state.pause_end_ms = pause->end_ms;
}

static void cmd_mod(struct choreography_mod *mod) {
#ifdef BACKEND_SUPPORTS_SOUND
	switch(mod->subcmd) {
		case SOUND_START:
			sound_mod_play(mod->arg);
			break;
		case SOUND_STOP:
			sound_mod_stop();
			break;
	}
#endif
}

static void cmd_mp3(struct choreography_mod *mod) {
#ifdef BACKEND_SUPPORTS_SOUND
	switch(mod->subcmd) {
		case SOUND_START:
			sound_mp3_play(mod->arg);
			break;
		case SOUND_STOP:
			sound_mp3_stop();
			break;
	}
#endif
}

static void cmd_ilbm(struct choreography_ilbm *ilbm) {
#ifdef BACKEND_SUPPORTS_ILBM
	// we re-use the fade-to palette for the image palette
	struct LoadedIff iff;

	/* Work out dimensions based on target bitplane width and height */
	struct Bitplane *target_bitplane = &backend_bitplane[ilbm->plane];
	int x = ilbm->x * target_bitplane->width / 240;
	int y = ilbm->y * target_bitplane->height / 240;
	int w = ilbm->w * target_bitplane->width / 240;
	int h = ilbm->h * target_bitplane->height / 240;

	iff_load(ilbm->file_idx, &iff);
	iff_display(&iff, x, y, w, h, &(state.fade_count), state.fade_to, backend_bitplane, ilbm->plane);
	iff_unload(&iff);

	if(ilbm->fade_in_ms == 0) {
		backend_set_palette(state.fade_count, state.fade_to);
		state.fade_count = 0; 
	} else {
		/* lerp to palette */
		backend_get_palette(32, state.fade_from);

		state.fade_start_ms = ilbm->header.start_ms;
		state.fade_end_ms = ilbm->header.start_ms + ilbm->fade_in_ms;

		// Everything else has been done for us above.
	}
#endif
}

static void cmd_sound(struct choreography_sound *sound) {
#ifdef BACKEND_SUPPORTS_SOUND
	sound_sample_play(sound->file_idx);
#endif
}

static void cmd_mbit(struct choreography_mbit *mbit) {
	size_t size;

	uint8_t *data = backend_wad_load_file(mbit->file_idx, &size);
	if(data) {
		mbit_display(data, &backend_set_palette, backend_bitplane);
		backend_wad_unload_file(data);
	}
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
			scene_init_spotlights();
			state.effect_tick = scene_spotlights_tick;
			state.effect_deinit = scene_deinit_spotlights;
			break;
		case EFFECT_VOTEVOTEVOTE:
			scene_init_votevotevote(effect->effect_data, state.fade_from, state.fade_to);
			state.effect_tick = scene_votevotevote_tick;
			state.effect_deinit = scene_deinit_votevotevote;
			break;
		case EFFECT_DELAYEDBLIT:
			scene_init_delayedblit();
			state.effect_tick = scene_delayedblit_tick;
			state.effect_deinit = scene_deinit_delayedblit;
			break;
		case EFFECT_COPPERPASTELS:
			scene_init_copperpastels(ms, effect->effect_data);
			state.effect_tick = scene_copperpastels_tick;
			state.effect_deinit = scene_deinit_copperpastels;
			break;
		case EFFECT_STATIC:
			scene_init_static(ms, effect->effect_data);
			state.effect_tick = scene_static_tick;
			state.effect_deinit = scene_deinit_static;
			break;
		case EFFECT_STATIC2:
			scene_init_static2(ms, effect->effect_data);
			state.effect_tick = scene_static2_tick;
			state.effect_deinit = scene_deinit_static2;
			break;
		default:
			backend_debug("Unknown effect %u ignored\n", (unsigned int)(effect->effect_num));
			break;
	}
}

static void cmd_loadfont(int ms, struct choreography_loadfont *loadfont) {
	backend_font_load(loadfont->file_idx, loadfont->startchar, loadfont->numchars, loadfont->positions);
}

static void cmd_scene_options(int ms, struct choreography_scene_options *scene_options) {
	anim_set_zoom(scene_options->zoom);
	anim_set_distort(scene_options->flags & SCENE_OPTION_DISTORT_MASK);
	anim_set_flip(scene_options->flags & SCENE_OPTION_FLIP_HORIZONTAL_MASK,
			scene_options->flags & SCENE_OPTION_FLIP_VERTICAL_MASK);
	anim_set_outline(scene_options->flags & SCENE_OPTION_ANIM_OUTLINE_MASK);
	state.anim_3_behind_plane_1 = scene_options->flags & SCENE_OPTION_ANIM_3_BEHIND_PLANE_1;
	state.epilepsy = scene_options->flags & SCENE_OPTION_EPILEPSY;
	// TODO something more general?
	anim_set_multidraw_3d(scene_options->flags & SCENE_OPTION_MULTIDRAW_3D);
	state.epilepsy_last_frame = false;
}

static void cmd_scene(int ms, struct choreography_scene *scene) {
	/* Initialise bitplanes */
	backend_set_new_scene();
	for(int i = 0; i < 6; i++) {
		switch(scene->bitplane_style[i]) {
			case BITPLANE_OFF:
				break;
			case BITPLANE_1X1:
				backend_allocate_bitplane(i, window_width, window_height);
				break;
			case BITPLANE_2X1:
				backend_allocate_bitplane(i, window_width * 2, window_height);
				break;
			case BITPLANE_2X2:
				backend_allocate_bitplane(i, window_width * 2, window_height * 2);
				break;
			default:
				backend_debug("Unknown bitplane style\n");
				break;
		}
	}

	state.ms_per_anim_frame = scene->msperframe;
	state.post_anim_clear_plane_mask = scene->post_anim_clear_plane_mask ;
	state.first_anim_frame_plane_copy_mask = scene->first_anim_frame_plane_copy_mask ;
	state.onion_skin = scene->onion_skin_cmd_high & SCENE_ONION_SKIN;
	state.onion_skin_low_frame = scene->onion_skin_cmd_high & 0x3f /*11 1111*/;
	state.onion_skin_high_frame = scene->onion_skin_cmd_low & 0xff;
	state.anim_bitplane = 0;

	struct choreography_scene_options null_scene_options;
	null_scene_options.zoom = 1;
	null_scene_options.flags = 0;

	cmd_scene_options(ms, &null_scene_options);
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
		case CMD_MP3:
			cmd_mp3((struct choreography_mod *)pos);
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
		case CMD_SCENE:
			cmd_scene(ms, (struct choreography_scene *)pos);
			break;
		case CMD_SCENE_INDEX:
			// Ignored during playback
			break;
		case CMD_MBIT:
			cmd_mbit((struct choreography_mbit *)pos);
			break;
		case CMD_SCENE_OPTIONS:
			cmd_scene_options(ms, (struct choreography_scene_options *)pos);
			break;
		default:
			/* This is bad */
			backend_debug("Unknown choreography command %x\n", (unsigned int)(pos->cmd));
			break;
	}
}

static inline bool end_of_demo()
{
	return pos->cmd == CMD_END && ((struct choreography_end *)pos)->end_of_scene == 0;
}

static void create_new_state(unsigned ms)
{
	/* Create the state */

	while(pos->start_ms <= ms && !end_of_demo()) {
		create_state_item(ms, pos);
		pos = next_header(pos);
	}
}

static void copy_planes_masked(int src_plane, uint8_t mask)
{
	struct Bitplane *src = &backend_bitplane[src_plane];

	/* First frame -- copy to other bitmaps if requested */
	for(int i = 0; i < 6; i++) {
		if(mask & (1 << i)) {
			struct Bitplane *dst = &backend_bitplane[i];

			graphics_copy_plane(src, dst);
		}
	}
}

static void run(int ms) {
	/* Run everything in the queue. */

	// Animation.
	if(state.animation_is_running) {
		/* Work out what frame we're supposed to be on. */
		int current_anim_frame_offset = (ms - state.current_animation_info.header.start_ms) / state.ms_per_anim_frame;

		unsigned anim_frame = current_anim_frame_offset + state.current_animation_info.frame_from;

		if(anim_frame > state.current_animation_info.frame_to) {
			/* We're done with this animation */
			anim_destroy(state.current_animation);
			state.animation_is_running = false;
			/* Clear any post-animation planes (a scene-specific flag) */
			clear_masked_planes(state.post_anim_clear_plane_mask);
		} else {
			if(state.last_drawn_frame != anim_frame) {
				/* Do an onion-skinning effect, where we show the three or four most recent frames overlaid on each other.
				 * This is done here by shuffling pointers to bitmaps around, rather than by copying.
				*/
				if(state.onion_skin) {
					/* Each participating bitplane moves up one*/
					struct Bitplane tmp = backend_bitplane[state.onion_skin_high_frame];
					for(int i = state.onion_skin_high_frame; i > state.onion_skin_low_frame; i--) {
						backend_bitplane[i] = backend_bitplane[i - 1];
					}
					backend_bitplane[state.onion_skin_low_frame] = tmp;
				}

				state.last_drawn_frame = anim_frame;

				unsigned real_anim_frame = state.backwards_subtract ? state.backwards_subtract - anim_frame : anim_frame;
				anim_draw(&backend_bitplane[state.anim_bitplane], state.current_animation, real_anim_frame);

				if(state.anim_3_behind_plane_1) {
					unsigned delayed_anim_frame = real_anim_frame < 3 ? 0 : real_anim_frame - 3;
					anim_draw(&backend_bitplane[1], state.current_animation, delayed_anim_frame);
				}

				if(anim_frame == state.current_animation_info.frame_from) {
					/* On the first frame, Copy the first frame to other
					 * bitplanes (scene-specific flag) This makes some of the
					 * onion-skinned scenes look better when they start. */
					copy_planes_masked(state.current_animation_info.bitplane, state.first_anim_frame_plane_copy_mask);
				}
			}

		}
	}

	// palette fades
	if(state.fade_count != 0) {
		if(ms > state.fade_end_ms) {
			// we're done with this palette fade
			backend_set_palette(state.fade_count, state.fade_to);
			state.fade_count = 0;
		} else {
			graphics_lerp_palette(state.fade_count, state.fade_from, state.fade_to, ms - state.fade_start_ms, state.fade_end_ms - state.fade_start_ms);
		}
	}

	// Backgrounds.
	if(state.effect_tick) {
		state.effect_tick(ms);
	}
	
	// epilepsy mode (the final dancing scene)
	// TODO slow this down to once per anim frame? 
	if(state.epilepsy) {
		if(state.epilepsy_last_frame == false) {
			backend_set_palette(0, NULL);
		} else if (state.fade_count == 0) {
			/* Don't reset the palette if we're doing a fade. */
			backend_set_palette(32, state.fade_to);
		}
		state.epilepsy_last_frame = !state.epilepsy_last_frame;
	}

	// pauses
	if(ms > state.pause_end_ms) {
		state.pause_end_ms = 0; // not even necessary to do this
	}
}

static void skip_to_start_ms(unsigned ms) {
	/* Advance to the requested position. */
	pos = (struct choreography_header *)choreography;
	/* Linear search to get to the exact point */
	while(pos->start_ms < ms && pos->cmd != CMD_END) {
		create_state_item(ms, pos);
		pos = next_header(pos);
	}
}

bool choreography_prepare_to_run(uint8_t *choreography_in, int ms)
{
	choreography = choreography_in;

	skip_to_start_ms(ms);
	//backend_wad_unload_file(choreography);
	return true;
}

// Return false if the next frame is end of scene or end of demo, true otherwise.
bool choreography_do_frame(int ms)
{
	create_new_state(ms);
	run(ms);

	return pos->cmd == CMD_END;
}

// Find a scene offset using the choreography scene index.
uint32_t choreography_find_offset_for_scene(uint8_t *choreography, unsigned ms, uint32_t *next_scene_offset)
{
	/* Use the scene index to find the right part. */
	if(((struct choreography_header* )choreography)->cmd != CMD_SCENE_INDEX) {
		backend_debug("no scene index!\n");
		return 0;
	}

	struct choreography_scene_index *idx = (struct choreography_scene_index *)choreography;

	for(int scene_num = idx->count - 1; scene_num >= 0; scene_num --) {
		if(idx->items[scene_num].ms <= ms) {
			if(next_scene_offset)
				*next_scene_offset = idx->items[scene_num + 1].offset;

			return idx->items[scene_num].offset;
		} 
	}

	return 0;
}

uint32_t choreography_find_ms_for_scene_name(uint8_t *choreography, char *name)
{
	if(((struct choreography_header* )choreography)->cmd != CMD_SCENE_INDEX) {
		backend_debug("no scene index!\n");
		return 0;
	}

	// strnlen()
	size_t name_len = 0;
	while(name_len < sizeof(((struct choreography_scene_index_item *)0)->name)) {
		if(!name[name_len])
			break;
		name_len++;
	}

	/* Use the scene index to find the right part. */
	struct choreography_scene_index *idx = (struct choreography_scene_index *)choreography;

	for(int scene_num = idx->count - 1; scene_num >= 0; scene_num --) {
		if(!memcmp(idx->items[scene_num].name, name, name_len)) {
			return idx->items[scene_num].ms;
		} 
	}

	return 0;
}
