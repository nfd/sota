#define _DEFAULT_SOURCE

#include <stdbool.h>
#include <mikmod.h>
#include <stdio.h>
#include <pthread.h>
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

static pthread_t audio_thread;

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

static void *audio_thread_entry(void *arg)
{
	while(true) {
		MikMod_Update();
		usleep(10000);
	}

	return NULL;
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

		//snd_heartbeat = wz_load_sample(SND_HEARTBEAT_FN);
		MikMod_SetNumVoices(-1, 1);

		MikMod_EnableOutput();

		if(pthread_create(&audio_thread, NULL, audio_thread_entry, NULL) != 0) {
			fprintf(stderr, "sound_init: pthread_create\n");
			return false;
		}

		pthread_detach(audio_thread);
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

	int8_t voice = Sample_Play(current_sample, 0, SFX_CRITICAL);
	Voice_SetVolume(voice, 255);
	Voice_SetPanning(voice, PAN_CENTER);

	return true;
}

bool sound_deinit()
{
	if(!nosound) {
		sound_mod_stop();

		pthread_cancel(audio_thread);

		if(current_sample)
			Sample_Free(current_sample);

		if(current_mod)
			Player_Free(current_mod);

		MikMod_Exit();
	}
	return true;
}

