#ifndef INCLUDED_RUNTIME_RUNTIME_STATE_H
#define INCLUDED_RUNTIME_RUNTIME_STATE_H

#include "h-basic.h"

extern cptr copyright;

extern byte version_major;
extern byte version_minor;
extern byte version_patch;
extern byte version_extra;

extern byte sf_major;
extern byte sf_minor;
extern byte sf_patch;
extern byte sf_extra;
extern u32b sf_xtra;
extern u32b sf_when;
extern u16b sf_lives;
extern u16b sf_saves;

extern bool character_generated;
extern bool character_dungeon;
extern bool character_loaded;
extern bool character_loaded_dead;
extern bool character_saved;
extern s16b character_icky;
extern s16b character_xtra;

extern u32b seed_randart;
extern u32b seed_flavor;

extern s32b turn;
extern s32b playerturn;
extern s32b min_depth_counter;

extern byte feeling;
extern byte do_feeling;
extern s16b rating;
extern bool good_item_flag;
extern int closing_flag;
extern bool use_sound;

extern int player_uid;
extern int player_euid;
extern int player_egid;
extern char savefile[1024];

extern s16b signal_count;
extern bool msg_flag;
extern bool command_repeating;

extern char notes_buffer[NOTES_LENGTH];
extern byte bones_selector;
extern int r_ghost;
extern char ghost_name[80];
extern int ghost_string_type;
extern char ghost_string[80];
extern bool g_labyrinth_view_active;
extern bool stop_stealth_mode;

extern bool (*ang_sort_comp)(const void* u, const void* v, int a, int b);
extern void (*ang_sort_swap)(void* u, void* v, int a, int b);
extern bool (*get_mon_num_hook)(int r_idx);
extern bool (*get_obj_num_hook)(int k_idx);
extern void (*object_info_out_flags)(
    const object_type* o_ptr, u32b* f1, u32b* f2, u32b* f3);

#endif /* INCLUDED_RUNTIME_RUNTIME_STATE_H */
