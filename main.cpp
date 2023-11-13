#include <stdlib.h>
#include <iostream>
#include <math.h>
#include "SDL.h" // I think this is crucial. Emscripten is a buggy mess, so SDL2/SDL.h doesn't really work. I think emscripten's SDL implementation somewhat mixes SDL1 with SDL2 logic sometimes. The program thinks that we use SDL2 anyway, so don't know what's up with that. 
#include "emscripten.h"

static SDL_AudioSpec audioSpec;
static SDL_AudioDeviceID audioDeviceID;

// Our sound callback.
// Generates a tone/sinewave at 220 hz.
// Maybe a better approach would be to just memcpy from other pre-filled buffer, generated on a different (actually "audio" thread - since web audio is on the main thread), but it will result in additional latency.
// There are practically no distortions, but we can have slight amplitude modulation (who knows why). Doubles kinda make it better (precision problem with sin? M_PI? who knows).
// Note the time modulation, we do this to avoid precision errors. Could have used fmod, but decided not to to avoid platform-specific pitholes. There are other approaches, for example we can just count samples globally and use it like: currentSampleIndex/44100 * hz * TWO_PI to get our phase. In practice, this doesn't really work due to int->float conversion, the int becomes very large really fast, casuing a lot of distortions.
void mixAudio(void *userdata, Uint8 *stream, int len) {
	// Only works with F32 in this current state.
	if (audioSpec.format == AUDIO_F32) {
		float* buffer = (float*)stream;
		int numSamples = len / sizeof(float);
		
		static double currentTime = 0;
		double timeStep = 1.0f / (double)(audioSpec.freq) * M_PI * 2;
		double hz = 220.0f;
		
		// There are practically no reasons to have a mono playback for my use case (games).
		if (audioSpec.channels == 2) {
			for (int i = 0; i < numSamples; i += 2) {
				buffer[i] = sin(currentTime * hz);
				buffer[i + 1] = sin(currentTime * hz);
				
				currentTime += timeStep;
				if (currentTime > M_PI * 2) currentTime -= M_PI * 2;
			}
		}
		// But I include it anyway so that it doesn't kill my ears with noise if I request mono by mistake.
		if (audioSpec.channels == 1) {
			for (int i = 0; i < numSamples; i++) {
				buffer[i] = sin(currentTime * hz);
				currentTime += timeStep;
				if (currentTime > M_PI * 2) currentTime -= M_PI * 2;
			}
		}
	}
}

// This function's straight up stolen from Soloud. 
int InitAudioBackend(unsigned int aSamplerate, unsigned int aBuffer, unsigned int aChannels) {
	if (!SDL_WasInit(SDL_INIT_AUDIO))
		if (SDL_InitSubSystem(SDL_INIT_AUDIO) < 0) 
			return -1;

	// For code compactness, we can reuse global audiospec and prefill it.
	SDL_AudioSpec as;
	as.freq = aSamplerate;
	as.format = AUDIO_F32;
	as.channels = aChannels;
	as.samples = aBuffer;
	as.callback = mixAudio;
	as.userdata = NULL;

	// Try to force the web-browser to create F32 format. If failed, fall back to S16.
	// Web-browsers (chrome) usually work with F32 by default (even if it's not forced), but the sample rate can differ.
	audioDeviceID = SDL_OpenAudioDevice(NULL, 0, &as, &audioSpec, SDL_AUDIO_ALLOW_ANY_CHANGE & ~(SDL_AUDIO_ALLOW_FORMAT_CHANGE | SDL_AUDIO_ALLOW_CHANNELS_CHANGE));
	if (audioDeviceID == 0)
	{
		as.format = AUDIO_S16;
		audioDeviceID = SDL_OpenAudioDevice(NULL, 0, &as, &audioSpec, SDL_AUDIO_ALLOW_ANY_CHANGE & ~(SDL_AUDIO_ALLOW_FORMAT_CHANGE | SDL_AUDIO_ALLOW_CHANNELS_CHANGE));
		if (audioDeviceID == 0)
			return -1;
	}
	
	printf("Loaded SDL_Audio device\n");
	printf("format: %s\n", audioSpec.format == AUDIO_F32 ? "F32" : "S16");
	printf("freq: %i hz\n", audioSpec.freq);
	printf("samples: %i\n", audioSpec.samples);
	printf("channels: %i\n", audioSpec.channels);
	
	// SDL's naming as usual. This actually "unpauses" the newly opened device and allows playback.
	SDL_PauseAudioDevice(audioDeviceID, 0);
	return 0;
}	

// Dummy function, we need it for emscripten to work
void main_loop() {}

int main(int argc, char* argv[]) {
	InitAudioBackend(44100, 1024, 2);

    emscripten_set_main_loop(main_loop, 0, 1);

    return 0;
}
