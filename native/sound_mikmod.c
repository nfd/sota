#define _DEFAULT_SOURCE
#define _BSD_SOURCE

#include <stdbool.h>
#include <mikmod.h>
#include <stdio.h>
#include <unistd.h>

#include "wad.h"
#include "sound.h"
#include "backend.h"

#ifdef EMULATE_FMEMOPEN
#include "fmemopen.h"
#endif


static bool nosound;

static MODULE *current_mod;
int mod_file_idx;

static SAMPLE *current_sample;
int snd_file_idx;

FILE *file_open(int idx)
{
	size_t size;

	uint8_t *data = backend_wad_load_file(idx, &size);

	if(data == NULL) {
		return NULL;
	} else {
		return fmemopen(data, size, "r");
	}
}

static MODULE *wz_load_mod(int idx)
{
	FILE *fp = file_open(idx);
	if(fp == NULL)
		return NULL;

	MODULE *mod = Player_LoadFP(fp, 8, 0);

	fclose(fp);

	return mod;
}

static SAMPLE *wz_load_sample(int idx)
{
	FILE *fp = file_open(idx);
	if(fp == NULL)
		return NULL;

	SAMPLE *sample = Sample_LoadFP(fp);

	fclose(fp);

	return sample;
}

void sound_update(void)
{
	if(!nosound)
		MikMod_Update();
}

bool sound_init(bool arg_nosound)
{
	if(arg_nosound) {
		nosound = true;
		printf("Sound disabled by request.\n");
	} else {
		nosound = false;

		MikMod_RegisterAllDrivers();
		MikMod_RegisterLoader(&load_mod);

		md_mode |= DMODE_SOFT_MUSIC; // wtf else did you put in my namespace, mikmod?
		
		if(MikMod_Init("")) {
			fprintf(stderr, "sound_init: %s\n", MikMod_strerror(MikMod_errno));
			return false;
		}

		current_mod  = NULL;
		mod_file_idx = -1;

		current_sample = NULL;
		snd_file_idx = -1;

		MikMod_SetNumVoices(-1, 1);
		MikMod_EnableOutput();
	}

	return true;
}

bool sound_mod_play(int new_mod_idx)
{
	if(nosound)
		return true;

	if(new_mod_idx != mod_file_idx) {
		if(current_mod)
			Player_Free(current_mod);

		current_mod = wz_load_mod(new_mod_idx);
		mod_file_idx = new_mod_idx;
	}

	Player_Start(current_mod);
	return true;
}

bool sound_mod_stop()
{
	if(nosound)
		return true;

	Player_Stop();
	return true;
}

bool sound_mp3_play(int new_mp3_idx)
{
	//  Unsupported under mikmod
	return true;
}

bool sound_mp3_stop()
{
	return true;
}

bool sound_sample_play(int sample_idx)
{
	if(nosound)
		return true;

	if(snd_file_idx != sample_idx) {
		if(current_sample)
			Sample_Free(current_sample);

		current_sample = wz_load_sample(sample_idx);
		snd_file_idx = sample_idx;
	}

	int8_t voice = Sample_Play(current_sample, 0, 0);
	Voice_SetVolume(voice, 255);
	Voice_SetPanning(voice, PAN_CENTER);

	return true;
}

bool sound_deinit()
{
	if(!nosound) {
		sound_mod_stop();

		if(current_sample)
			Sample_Free(current_sample);

		if(current_mod)
			Player_Free(current_mod);

		MikMod_Exit();
	}
	return true;
}

