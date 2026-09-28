#ifndef INCLUDED_SOUND_SPATIAL_H
#define INCLUDED_SOUND_SPATIAL_H

/* Audible path radii, including the quiet outer tile. These affect playback
 * only: monster perception and the gameplay noise fields are unchanged. */
#define SOUND_RADIUS_LOCAL 6
#define SOUND_RADIUS_IDLE 11
#define SOUND_RADIUS_COMBAT 21
#define SOUND_RADIUS_TORCH 3
#define SOUND_RADIUS_BRAZIER 4

/* A subtle, bounded response centred on ordinary Stealth bonuses: +/-5 gives
 * -/+10% gain, and +20 gives 84%. The limits remain 80% and 120%, with no hard knee.
 * Convert before taking the magnitude so INT_MIN cannot overflow. */
static inline float sound_footstep_stealth_gain(int stealth)
{
    float score = (float)stealth;
    float magnitude = score < 0.0f ? -score : score;
    return 1.0f - 0.2f * (score / (5.0f + magnitude));
}

/* Same curve for every located sound. Full gain at the source and its first
 * neighbour preserves adjacent actions. A squared taper has no volume floor:
 * at the last audible tile it is 1 / radius^2, then zero one tile farther. */
static inline float sound_distance_gain(int path_distance, int radius)
{
    float remaining;
    if (path_distance < 0 || radius <= 0 || path_distance > radius)
        return 0.0f;
    if (path_distance <= 1)
        return 1.0f;
    remaining = (float)(radius + 1 - path_distance) / (float)radius;
    return remaining * remaining;
}

static inline float sound_level_gain(int level, int maximum)
{
    if (level <= 0 || maximum <= 1)
        return 0.0f;
    return sound_distance_gain(level >= maximum ? 0 : maximum - level,
        maximum - 1);
}

#endif
