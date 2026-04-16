#pragma once

#include <stdbool.h>
#include "app/app-movement.h"
#include "gamepad-config.h"
#include "pane-config.h"

#define SDL_MOVEMENT_BINDING_MAX 64

enum sdl_min_terminal_mode {
    SDL_MIN_TERMINAL_NORMAL = 0,
    SDL_MIN_TERMINAL_COMPACT = 1,
};

// SDL-specific configuration structure
struct sdl_config {
    int main_view_scale;
    // Default supporting-pane font size. Zero means auto from the main pane's
    // visible font/cell height.
    int aux_view_font_size;
    // Fixed-pixel menu and classic left-panel font size. Zero means auto.
    int menu_panel_font_size;
    int margin;
    bool fullscreen;
    bool tiles;
    bool enable_right_panes;
    bool enable_bottom_panes;
    bool hide_left_panel;
    int min_terminal_mode;
    char palette_preset[UI_COLOR_PRESET_ID_LEN];
    
    // Window position and size for windowed mode
    int window_x;
    int window_y;
    int window_width;
    int window_height;
    
    // Custom fonts
    char story_font[256];      // Story font path, relative to xtra/ or absolute
    char monospace_font[256];  // Monospace font path, relative to xtra/ or absolute
    
    // Monospace font rendering options
    bool mono_bold;            // Apply bold style to monospace font
    bool mono_italic;          // Apply italic style to monospace font
    bool mono_underline;       // Apply underline style to monospace font
    bool mono_strikethrough;   // Apply strikethrough style to monospace font
    int mono_hinting;          // TTF hinting mode: 0=normal, 1=light, 2=mono, 3=none, 4=light_subpixel
    bool mono_kerning;         // Enable kerning (default: true)
    int mono_outline;          // Outline width in pixels (0=none)
    
    // Story font rendering options
    bool story_bold;           // Apply bold style to story font
    bool story_italic;         // Apply italic style to story font
    bool story_underline;      // Apply underline style to story font
    bool story_strikethrough;  // Apply strikethrough style to story font
    int story_hinting;         // TTF hinting mode: 0=normal, 1=light, 2=mono, 3=none, 4=light_subpixel
    bool story_kerning;        // Enable kerning (default: true)
    int story_outline;         // Outline width in pixels (0=none)

    // Gamepad/controller settings
    bool gamepad_enabled;                 // Enable gamepad input
    bool gamepad_auto_mode;               // Auto-enable controller UI when gamepad is present/used
    bool steamdeck_mode;                  // Steam Deck UI mode setting
    bool gamepad_use_dpad;                // Use d-pad for movement
    bool gamepad_use_left_stick;          // Use left stick for movement
    int gamepad_deadzone;                 // Deadzone for analog sticks
    int gamepad_trigger_threshold;        // Threshold to treat triggers as pressed
    int gamepad_button_bindings[GAMEPAD_BUTTON_COUNT];
    int gamepad_trigger_bindings[GAMEPAD_TRIGGER_COUNT];
    int gamepad_left_stick_bindings[GAMEPAD_STICK_DIR_COUNT];
    int gamepad_right_stick_bindings[GAMEPAD_STICK_DIR_COUNT];
    int gamepad_shoulder_combo_binding;   // Binding for L1+R1 combo action
    int touch_pane_bindings[SDL_TOUCH_PANE_BUTTON_COUNT];
    char touch_pane_labels[SDL_TOUCH_PANE_BUTTON_COUNT][SDL_TOUCH_PANE_LABEL_LEN];
    int touch_pane_second_bindings[SDL_TOUCH_PANE_BUTTON_COUNT];
    char touch_pane_second_labels[SDL_TOUCH_PANE_BUTTON_COUNT][SDL_TOUCH_PANE_LABEL_LEN];
    char touch_pane_panel_names[SDL_TOUCH_PANE_PANEL_COUNT][SDL_TOUCH_PANE_LABEL_LEN];

    bool movement_keyboard_present;
    u16b movement_keyboard_preset;
    u16b movement_binding_count;
    app_movement_binding movement_bindings[SDL_MOVEMENT_BINDING_MAX];
};

extern struct sdl_config config;

// Load SDL configuration from JSON file
void sdl_config_load(const char* filename, struct sdl_config* config, 
                     struct pane_config* pane_configs, int* pane_count, int max_panes);

// Save SDL configuration to JSON file
void sdl_config_save(const char* filename, const struct sdl_config* config,
                     const struct pane_config* pane_configs, int pane_count);

// Set default configuration values
void sdl_config_set_defaults(struct sdl_config* config);

// Set default gamepad bindings (does not touch other fields)
void sdl_config_set_default_gamepad_bindings(struct sdl_config* config);

// Set default touch pane bindings (does not touch other fields)
void sdl_config_set_default_touch_pane_bindings(struct sdl_config* config);

// Clear custom touch pane labels (does not touch other fields)
void sdl_config_clear_touch_pane_labels(struct sdl_config* config);

// Clear keyboard movement bindings and reset the stored preset metadata.
void sdl_config_clear_movement_bindings(struct sdl_config* config);

// Apply a built-in keyboard movement preset.
void sdl_config_set_default_movement_bindings(struct sdl_config* config,
    u16b preset_id);

// Returns true when the config carries at least one stored movement binding.
bool sdl_config_has_movement_bindings(const struct sdl_config* config);

// Set default configuration values based on screen resolution
void sdl_config_set_defaults_for_resolution(struct sdl_config* config, 
                                            struct pane_config* pane_configs,
                                            int* pane_count,
                                            int max_panes,
                                            int screen_width,
                                            int screen_height);

// Apply command-line arguments to configuration
void sdl_config_apply_cmdline(struct sdl_config* config, int argc, char** argv);

// Load/save app-wide game options from/to the SDL JSON config file.
void sdl_config_load_app_options(const char* filename);
bool platform_intro_should_force_flame(void);
void platform_intro_mark_seen(void);
bool option_is_app_persistent(int opt);
