#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdbool.h>
#include <inttypes.h>
#include <assert.h>

#include "anim.h"
#include "graphics.h"
#include "files.h"
#include "backend.h"

#define ANIM_SOURCE_WIDTH 256
#define ANIM_SOURCE_HEIGHT 200
#define MAX_TWEENED_VERTICES 512

float anim_scale_x;
float anim_scale_y;
int anim_offset_x;
int anim_offset_y;

struct Bitplane *anim_bitplane;
bool anim_xor;

//#define MAX_SIMULTANEOUS_ANIM 2

struct animation current_anim, prev_anim;
uint8_t current_tween[MAX_TWEENED_VERTICES * 2];

void anim_init()
{
	/* Calculate scale and offset for animation.
	 * The original animations are nominally 256 x 204 (apparently), so find the best integer scale which matches that,
	 * then centre the result horizontally but ensure it touches the bottom of the display vertically.
	 *
	 * TODO we'll want to change this on a scene-by-scene basis I think: it's better to have the figure fullscreen
	 * and slightly cropped than uncropped but far away.
	*/
	anim_scale_x = ((float)window_width) / ANIM_SOURCE_WIDTH;
	anim_scale_y = ((float)window_height) / ANIM_SOURCE_HEIGHT;

	anim_offset_x = (window_width / 2) - ( (ANIM_SOURCE_WIDTH * anim_scale_x) / 2);
	anim_offset_y = window_height - (ANIM_SOURCE_HEIGHT * anim_scale_y);

	anim_bitplane = &backend_bitplane[0];

	current_anim.data_file = prev_anim.data_file = -1;
}

void anim_set_bitplane(struct Bitplane *bitplane) {
	anim_bitplane = bitplane;
}

void anim_set_xor(bool xor) {
	anim_xor = xor;
}

static uint8_t *anim_draw_object(uint8_t *data);
void anim_draw(struct animation *anim, int frame_idx)
{
	if(frame_idx > anim->num_frames)
		return;

	int index = ((anim->indices[(frame_idx * 2)] << 8) | anim->indices[(frame_idx * 2) + 1]);
	uint8_t *data = &(anim->data[index]);

	/* num_objects should be 1 through to 6, indicating the number of objects in the frame */
	uint8_t num_objects = *data++;
	if(num_objects < 1 || num_objects > 6) {
		fprintf(stderr, "anim_draw: num_objects: %d\n", num_objects);
		return;
	}

	planar_clear(anim_bitplane);

	for(uint8_t i = 0; i < num_objects; i++) {
		data = anim_draw_object(data);
	}
}

static uint8_t *anim_draw_object(uint8_t *data) {
	/* draw_cmd should be 0xdX for any X */
	uint8_t draw_cmd = *data++;
	//
	// It seems that the lower 4 bits of the draw command include extra
	// information about the animation, but I don't use it (or know what it
	// does)
	switch(draw_cmd & 0xf0) {
		case 0xd0:
		{
			int num_vertices = *data++;
			if(num_vertices > 0) {
				graphics_draw_filled_scaled_polygon_to_bitmap(num_vertices, data, anim_scale_x, anim_scale_y, anim_offset_x, anim_offset_y, anim_bitplane, anim_xor);
			}
			data += (num_vertices * 2);
			break;
		}
		case 0xe0: // e6 and e7 known
		case 0xf0: // f2 known
		{
			uint8_t *tween_from = data;
			uint8_t *tween_to   = data;

			tween_from     -= ((data[0] << 8) + (data[1])); data += 2;
			tween_to       += ((data[0] << 8) + (data[1])); data += 2;
			int tween_t     = *data++; // position in tween
			int tween_count = *data++;

			if(tween_from < current_anim.data) {
				// A tween referencing a previous animation. We boldly assume it is specifically referencing the previous
				// block (which always happens in SOTA). split_and_compress.py ensures that tweens never reference beyond
				// their current block, so this is the only other-block case.
				ptrdiff_t offset = (uintptr_t)current_anim.data - (uintptr_t)tween_from; // How far before the start of the block?
				tween_from = prev_anim.past_data_end - offset;
			}

			uint8_t *shape = lerp_tween(tween_from, tween_to, tween_t, tween_count);

			int num_vertices = *shape++;
			graphics_draw_filled_scaled_polygon_to_bitmap(num_vertices, shape, anim_scale_x, anim_scale_y, anim_offset_x, anim_offset_y, anim_bitplane, anim_xor);
			break;
		}
		default:
			fprintf(stderr, "Unknown command %x\n", draw_cmd);
			break;
	}
	return data;
}

uint8_t *lerp_tween(uint8_t *tween_from, uint8_t *tween_to, int tween_t, int tween_count)
{
	// We expect these to be draw commands (i.e. 0xd2) followed by lengths.
	tween_from++; // skip command
	tween_to++; 

	int tween_from_length = *tween_from++;
	int tween_to_length   = *tween_to++;

	uint8_t *pos = current_tween;

	int points = tween_from_length > tween_to_length ? tween_from_length: tween_to_length;
	*(pos++)= points;

	float point_multiplier_from = (float)tween_from_length / points;
	float point_multiplier_to   = (float)tween_to_length / points;

	float amt = (float)tween_t / tween_count;

	for(int i = 0; i < points; i++) {
		int point_from = i * point_multiplier_from;
		int point_to   = i * point_multiplier_to;

		uint8_t from_y = tween_from[point_from * 2];
		uint8_t from_x = tween_from[(point_from * 2) + 1];
		uint8_t to_y = tween_to[point_to * 2];
		uint8_t to_x = tween_to[(point_to * 2) + 1];

		*pos++ = from_y + (amt * (to_y - from_y));
		*pos++ = from_x + (amt * (to_x - from_x));
	}

	return current_tween;
}

struct animation *anim_load(int data_file, int anim_idx) {
	/*
	 * Format of an animation file
	 * 2 bytes: number of indices
	 * ...    : indices
	 * ...    : animation data
	*/
	if(current_anim.data_file != data_file) {
		prev_anim = current_anim;

		current_anim.indices = current_anim.data = NULL;

		ssize_t size;

		uint8_t *anim_file = file_get(data_file, &size);
		if(anim_file == NULL) {
			fprintf(stderr, "anim load fail\n");
			anim_destroy(&current_anim);
			return NULL;
		}

		current_anim.num_frames = anim_file[0] << 8 | anim_file[1];
		current_anim.indices = anim_file + sizeof(uint16_t);
		current_anim.data = current_anim.indices + (current_anim.num_frames * 2);
		current_anim.past_data_end = anim_file + size;
	}

	return &current_anim;
}

int anim_destroy(struct animation *anim) {
	return 0;
}

