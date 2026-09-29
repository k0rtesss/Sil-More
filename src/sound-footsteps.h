#ifndef INCLUDED_SOUND_FOOTSTEPS_H
#define INCLUDED_SOUND_FOOTSTEPS_H

#include "h-basic.h"
#include "cave/cave-atmosphere.h"

/* Interleaved float PCM, mono/stereo. Returns SDL-allocated samples (caller
 * frees with SDL_free), or NULL for unsupported input/no effect. */
float* sound_footstep_filter(const float* samples, int frames, int channels,
    int frequency, cave_atmosphere_kind atmosphere, int* output_frames);

#endif
