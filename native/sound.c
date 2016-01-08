#include <stdbool.h>
#include <mikmod.h>
#include <stdio.h>
#include <pthread.h>
#include <unistd.h>

#include "files.h"
#include "sound.h"

#define MOD_INTRO_FN "data/stateldr.mod"
#define MOD_MAIN_FN "data/condom corruption.mod"

static MODULE *mod_intro, *mod_main;
static pthread_t audio_thread;

static MODULE *load(char *filename)
{
	FILE *fp = file_open(filename);
	if(fp == NULL)
		return NULL;

	MODULE *mod = Player_LoadFP(fp, 8, 0);

	fclose(fp);

	return mod;
}

static void *audio_thread_entry(void *arg)
{
	while(true) {
		MikMod_Update();
		usleep(10000);
	}

	return NULL;
}

bool sound_init()
{

	MikMod_RegisterAllDrivers();
	MikMod_RegisterLoader(&load_mod);

	md_mode |= DMODE_SOFT_MUSIC; // wtf else did you put in my namespace, mikmod?
	
	if(MikMod_Init("")) {
		fprintf(stderr, "sound_init: %s\n", MikMod_strerror(MikMod_errno));
		return false;
	}

	mod_intro = load(MOD_INTRO_FN);
	mod_main  = load(MOD_MAIN_FN);

	if(mod_intro == NULL || mod_main == NULL) {
		fprintf(stderr, "sound_init: couldn't load mods\n");
		return false;
	}

	if(pthread_create(&audio_thread, NULL, audio_thread_entry, NULL) != 0) {
		fprintf(stderr, "sound_init: pthread_create\n");
		return false;
	}

	pthread_detach(audio_thread);

	return true;
}

bool sound_mod_play(int modidx)
{
	MODULE *mod = modidx == MOD_INTRO ? mod_intro : mod_main;

	Player_Start(mod);
}

bool sound_mod_stop()
{
	Player_Stop();
}

bool sound_deinit()
{
	sound_mod_stop();

	pthread_cancel(audio_thread);

	// no join because detached
//	pthread_join(audio_thread, NULL);

	Player_Free(mod_intro);
	Player_Free(mod_main);
	MikMod_Exit();
}

