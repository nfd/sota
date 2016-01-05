#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>

#include "anim.h"
#include "graphics.h"

#define ANIM_SOURCE_WIDTH 256
#define ANIM_SOURCE_HEIGHT 204
#define MAX_TWEENED_VERTICES 512

int anim_scale;
int anim_offset_x;
int anim_offset_y;

uint8_t current_tween[MAX_TWEENED_VERTICES * 2];

void anim_init(int display_width, int display_height) {
	/* Calculate scale and offset for animation.
	 * The original animations are nominally 256 x 204 (apparently), so find the best integer scale which matches that,
	 * then centre the result horizontally but ensure it touches the bottom of the display vertically.
	 *
	 * TODO we'll want to change this on a scene-by-scene basis I think: it's better to have the figure fullscreen
	 * and slightly cropped than uncropped but far away.
	*/

	int scalex = display_width / ANIM_SOURCE_WIDTH;
	int scaley = display_height / ANIM_SOURCE_HEIGHT;

	anim_scale  = scalex < scaley ? scalex : scaley;
	if(anim_scale  == 0)
		anim_scale  = 1;

	anim_offset_x = (display_width / 2) - ( (ANIM_SOURCE_WIDTH * anim_scale) / 2);
	anim_offset_y = display_height - (ANIM_SOURCE_HEIGHT * anim_scale);
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

	graphics_planar_clear(1);

	for(uint8_t i = 0; i < num_objects; i++) {
		data = anim_draw_object(data);
	}
}

static uint8_t *anim_draw_object(uint8_t *data) {
	/* draw_cmd should be 0xdX for any X */
	uint8_t draw_cmd = *data++;
	switch(draw_cmd) {
		case 0xd2:
		case 0xd3:
		case 0xde:
		{
			int num_vertices = *data++;
			graphics_draw_filled_scaled_polygon_to_bitmap(num_vertices, data, anim_scale, anim_offset_x, anim_offset_y, 1);
			data += (num_vertices * 2);
			break;
		}
		case 0xe6:
		case 0xe7:
		case 0xe8:
		case 0xf2:
		{
			int bitplane_idx = 0; //cmd == 0xe6? 0: 2;
			uint8_t *tween_from = data;
			uint8_t *tween_to   = data;

			tween_from     -= ((data[0] << 8) + (data[1])); data += 2;
			tween_to       += ((data[0] << 8) + (data[1])); data += 2;
			int tween_t     = *data++; // position in tween
			int tween_count = *data++;

			uint8_t *shape = lerp_tween(tween_from, tween_to, tween_t, tween_count);

			int num_vertices = *shape++;
			graphics_draw_filled_scaled_polygon_to_bitmap(num_vertices, shape, anim_scale, anim_offset_x, anim_offset_y, 1);
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
	if(*tween_from++ & 0xd0 != 0xd0) {
		fprintf(stderr, "Unexpected tween idx (from)");
	}

	if(*tween_to++ & 0xd0 != 0xd0) {
		fprintf(stderr, "Unexpected tween idx (to)");
	}

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

static uint8_t *read_entire_file(char *filename, ssize_t *size_out) {
	uint8_t *buf;
	
	int h = open(filename, O_RDONLY);
	if(h < 0) {
		perror(filename);
		return NULL;
	}

	struct stat statbuf;
	if(fstat(h, &statbuf) != 0)  {
		close(h);
		return NULL;
	}

	buf = malloc(statbuf.st_size);
	if(buf == NULL) {
		close(h);
		return NULL;
	}

	ssize_t to_read = statbuf.st_size;
	uint8_t *current = buf;
	while(to_read) {
		ssize_t amt_read = read(h, current, to_read);
		if(amt_read < 0) {
			close(h);
			free(buf);
			return NULL;
		}
		to_read -= amt_read;
		current += amt_read;
	}

	close(h);

	if(size_out) {
		*size_out = statbuf.st_size;
	}

	return buf;
}

struct animation *anim_load(char *basename) {
	static char buf[256];

	struct animation *anim = malloc(sizeof(struct animation));
	if(anim == NULL)
		return NULL;

	anim->indices = anim->data = NULL;

	if(snprintf(buf, 256, "data/%s_index.bin", basename) >= 256) {
		anim_destroy(anim);
		return NULL;
	}

	size_t indices_size;

	anim->indices = read_entire_file(buf, &indices_size);
	if(anim->indices == NULL) {
		fprintf(stderr, "load fail: %s\n", buf);
		anim_destroy(anim);
		return NULL;
	}

	if(snprintf(buf, 256, "data/%s_anim.bin", basename) >= 256) {
		fprintf(stderr, "load fail: %s\n", buf);
		anim_destroy(anim);
		return NULL;
	}

	anim->data = read_entire_file(buf, NULL);
	if(anim->data == NULL) {
		anim_destroy(anim);
		return NULL;
	}

	anim->num_frames = indices_size / 2;

	return anim;
}

int anim_destroy(struct animation *anim) {
	if(anim->indices)
		free(anim->indices);

	if(anim->data)
		free(anim->data);

	free(anim);

	return 0;
}

