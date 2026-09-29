#include "angband.h"
#include "sound-footsteps.h"
#include <SDL3/SDL.h>
#include <math.h>

float* sound_footstep_filter(const float* samples, int frames, int channels,
    int frequency, cave_atmosphere_kind atmosphere, int* output_frames)
{
    static const int reflection_ms[] = {19, 31, 43, 59, 73, 89, 113, 139, 167, 197, 233, 281};
    static const float reflection_gain[] = {.090f, .085f, .078f, .072f,
        .067f, .058f, .050f, .044f, .036f, .030f, .024f, .019f};
    bool hushed = atmosphere == CAVE_ATMOSPHERE_HUSHED;
    if (output_frames) *output_frames = 0;
    if (!samples || !output_frames || frames <= 0 || channels < 1 || channels > 2
        || frequency < 8000 || frequency > 192000 || frames > frequency * 4
        || (!hushed && atmosphere != CAVE_ATMOSPHERE_ECHOING))
        return NULL;

    /* Leave room for the decaying filter or reflections after the recording. */
    int count = frames + frequency * (hushed ? 30 : 320) / 1000;
    float* out = SDL_calloc((size_t)count * channels, sizeof(*out));
    if (!out) return NULL;
    float low[2] = {0}, low2[2] = {0};
    float cutoff = hushed ? 850.0f : 3200.0f;
    float alpha = 1.0f - expf(-6.28318530718f * cutoff / frequency);
    int delays[12];
    for (int i = 0; i < 12; i++)
        delays[i] = frequency * reflection_ms[i] / 1000;

    for (int frame = 0; frame < count; frame++)
        for (int channel = 0; channel < channels; channel++)
        {
            size_t index = (size_t)frame * channels + channel;
            float dry = frame < frames ? samples[index] : 0.0f;
            if (!isfinite(dry)) dry = 0.0f;
            low[channel] += alpha * (dry - low[channel]);
            if (hushed)
            {
                /* Two low-pass stages remove the sharp heel/clatter transient. */
                low2[channel] += alpha * (low[channel] - low2[channel]);
                out[index] = low2[channel] * 0.65f;
            }
            else
            {
                out[index] += dry * 0.82f;
                for (int tap = 0; tap < 12; tap++)
                    if (frame + delays[tap] < count)
                        out[(size_t)(frame + delays[tap]) * channels + channel]
                            += low[channel] * reflection_gain[tap];
            }
        }

    /* Preserve headroom for overlapping steps, including modded loud samples. */
    float peak = 0.0f;
    for (size_t i = 0; i < (size_t)count * channels; i++)
        peak = fmaxf(peak, fabsf(out[i]));
    if (peak > 0.95f)
        for (size_t i = 0; i < (size_t)count * channels; i++)
            out[i] *= 0.95f / peak;
    *output_frames = count;
    return out;
}
