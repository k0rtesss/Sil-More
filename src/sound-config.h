#pragma once

#include <stdbool.h>

/* Keep this equal to MSG_MAX in defines.h; the config header is included
 * before angband.h in some translation units, so it cannot include defines.h. */
#define SOUND_CONFIG_EVENT_MAX 87

// Sound configuration structure
struct sound_config {
    bool enabled;              // Enable/disable game sounds (default: false)
    bool enable_combat;        // Enable combat sounds
    bool enable_inventory;     // Enable inventory sounds
    bool enable_walk;          // Enable walk sounds
    bool enable_doors;         // Enable door sounds
    bool enable_monster_hits;  // Master switch for monster sounds
    bool enable_other;         // Enable sounds outside the named groups
    bool enable_attack; // Enable attack sounds
    bool enable_damage; // Enable damage sounds
    bool enable_death; // Enable death sounds
    bool enable_idle; // Enable idle sounds
    bool enable_traps;         // Enable trap sounds
    bool enable_river;         // Enable running and still water/acid loops
    bool enable_torches;       // Enable torch and brazier loops
    bool enable_lava;          // Enable lava loops
    bool enable_forge;         // Enable forge loop and hammering sounds
    bool enable_bridge;        // Enable bridge construction and repair loops
    float volume_master;       // Master volume (0.0-1.0, default: 1.0)
    float volume_combat;       // Combat sounds volume (0.0-1.0, default: 1.0)
    float volume_inventory;    // Inventory sounds volume (0.0-1.0, default: 1.0)
    float volume_walk;         // Walk sounds volume (0.0-1.0, default: 1.0)
    float volume_doors;        // Door sounds volume (0.0-1.0, default: 1.0)
    float volume_monster_hits; // Monster hit sounds volume (0.0-1.0, default: 1.0)
    float volume_traps;        // Trap sounds volume (0.0-1.0, default: 1.0)
    float volume_other;        // Other sounds volume (0.0-1.0, default: 1.0)
    float volume_river;         // River loop volume (0.0-1.0, default: 1.0)
    float volume_torches;       // Torch loop volume (0.0-1.0, default: 1.0)
    float volume_lava;          // Lava loop volume (0.0-1.0, default: 1.0)
    float volume_forge;         // Forge loop and hammering volume (0.0-1.0, default: 1.0)
    float volume_bridge;        // Bridge work loop volume (0.0-1.0, default: 1.0)
    bool music_main_enabled;
    bool music_ambient_enabled;
    float music_main_volume;
    float music_ambient_volume;
    char music_main_path[256];
    char music_main_full_path[256];
    char music_ambient_path[256];
    char music_death_path[256];
    int sample_rate;           // Audio sample rate (default: 22050)
    int channels;              // Audio channels: 1=mono, 2=stereo (default: 2)
    char format[16];           // Audio format: "s8", "u8", "s16", "s32", "f32" (default: "s16")
    char events[SOUND_CONFIG_EVENT_MAX][256]; // Folder paths for each sound event (MSG_MAX entries)
};

// Load sound configuration from JSON file
void sound_config_load(const char* filename, struct sound_config* config);

// Save sound configuration to JSON file
void sound_config_save(const char* filename, const struct sound_config* config);

// Set default sound configuration values
void sound_config_set_defaults(struct sound_config* config);
