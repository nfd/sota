/*
 * SDL-mixer sound implementation: MP3s supported, MODs unsupported
 * (technically sdl mixer can pass mods and MP3s to the same API, but libmodplug, which is the
 * mp3 library of choice, misplays the mod!
*/

#define _DEFAULT_SOURCE
#define _BSD_SOURCE

#include <stdbool.h>
#include <stdio.h>
#include <unistd.h>
#include <SDL/SDL_mixer.h>

#include "wad.h"
#include "sound.h"
#include "backend.h"

#ifdef EMULATE_FMEMOPEN
#include "fmemopen.h"
#endif


static bool nosound;

static Mix_Chunk *current_sample;
static SDL_RWops *current_sample_rwops;
int snd_file_idx;

static Mix_Music *current_mus;
static SDL_RWops *current_mus_rwops;
int mus_file_idx;

static SDL_RWops *_load(int idx) {
	size_t size;

	uint8_t *data = backend_wad_load_file(idx, &size);

	return SDL_RWFromMem(data, size);
}

static void _unload(SDL_RWops **ops) {
	SDL_RWclose(*ops);
	*ops = NULL;
}

static void _unload_mus()
{
        if(current_mus_rwops) {
                _unload(&current_mus_rwops);
                Mix_FreeMusic(current_mus);
        }
}

static void _unload_sample()
{
	if(current_sample_rwops) {
		_unload(&current_sample_rwops);
		Mix_FreeChunk(current_sample);
	}
}

void sound_update(void)
{
	/* Nothing to do when using sdl mixer */
}


bool sound_mod_play(int new_mod_idx)
{
	// Unsupported using sdl mixer backend
	return true;
}

bool sound_mod_stop()
{
	// Unsupported using sdl mixer backend
	return true;
}

bool sound_mp3_play(int new_mp3_idx)
{
	if(nosound)
		return true;

	Mix_HaltMusic();

	if(new_mp3_idx != mus_file_idx) {
		if(current_mus_rwops) {
			_unload_mus();
		}

		current_mus_rwops = _load(new_mp3_idx);
		current_mus = Mix_LoadMUS_RW(current_mus_rwops);
		mus_file_idx = new_mp3_idx;
		Mix_PlayMusic(current_mus, -1);
	}

	return true;
}

bool sound_mp3_stop()
{
	if(!nosound) {
		Mix_HaltMusic();
	}
	return true;
}

bool sound_sample_play(int sample_idx)
{
	if(nosound)
		return true;

	if(snd_file_idx != sample_idx) {
		if(current_sample) {
			_unload_sample();
		}

		current_sample_rwops = _load(sample_idx);
		current_sample = Mix_LoadWAV_RW(current_sample_rwops, 0);
		snd_file_idx = sample_idx;
	}

	Mix_PlayChannel(-1, current_sample, 0);

	return true;
}

bool sound_init(bool arg_nosound)
{
	bool succeeded = true;

	if(arg_nosound) {
		nosound = true;
		printf("Sound disabled by request.\n");
	} else {
		nosound = false;
		bool succeeded;
#ifdef __EMSCRIPTEN__
		// Note: this requires a patched SDL_mixer currently
		//succeeded = Mix_OpenAudioDevice(44100, MIX_DEFAULT_FORMAT, MIX_DEFAULT_CHANNELS,
		//		4096, NULL, SDL_AUDIO_ALLOW_FREQUENCY_CHANGE) != -1;
		succeeded = Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, MIX_DEFAULT_CHANNELS, 4096) != -1;
#else
		succeeded = Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, MIX_DEFAULT_CHANNELS, 4096) != -1;
#endif
	}

	return succeeded;
}

bool sound_deinit()
{
	if(!nosound) {
		sound_mod_stop();
		sound_mp3_stop();

		_unload_sample();
		_unload_mus();
		Mix_CloseAudio();
	}

	return true;
}

