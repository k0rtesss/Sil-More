#ifndef INCLUDED_CAVE_CATASTROPHE_H
#define INCLUDED_CAVE_CATASTROPHE_H

/* Include after types.h. Persist these values; never reorder them. */
enum catastrophe_kind {
    CATA_AUTO, CATA_WATER, CATA_ACID, CATA_LAVA, CATA_ICE, CATA_CHASM,
    CATA_KIND_MAX
};
enum catastrophe_event {
    CATA_UNIQUE, CATA_TIMER, CATA_VALAR, CATA_SMITH, CATA_FIND,
    CATA_THRALL, CATA_SONG, CATA_ABILITY, CATA_EVENT_MAX
};
typedef struct catastrophe_state {
    byte chance, smith_milestones, find_milestones, kind, y, x;
    s32b last_stage, craft_difficulty, timer_step;
    u32b random, step, pending[CATA_EVENT_MAX];
    bool ready;
} catastrophe_state;

void catastrophe_reset_run(void);
void catastrophe_reset_level(void);
void catastrophe_prepare_level(void);
void catastrophe_resume_level(void);
void catastrophe_begin_action(void);
void catastrophe_end_action(void);
void catastrophe_flush_events(void);
void catastrophe_note(enum catastrophe_event event);
void catastrophe_depth_tick(int stage);
void catastrophe_sync_depth(int stage);
void catastrophe_accept_craft(int difficulty);
void catastrophe_crafted(const object_type* obj);
void catastrophe_acquired(const object_type* obj);
void catastrophe_scan_inventory(bool baseline);
void catastrophe_ability(int skill, int ability);
bool catastrophe_active(void);
int catastrophe_chance(void);
bool catastrophe_owns(int y, int x);
bool catastrophe_protected(int y, int x);
bool catastrophe_can_start(int kind);
bool catastrophe_start(int kind);
cptr catastrophe_name(int kind);
void catastrophe_format(char* buf, size_t size);

/* Save lane accessors; restoration never runs gameplay. */
catastrophe_state catastrophe_get_state(void);
bool catastrophe_restore_state(catastrophe_state state);
u32b catastrophe_cell_step(int y, int x);
bool catastrophe_restore_cell(int y, int x, u32b step);
int catastrophe_acquired_count(void);
guid64 catastrophe_acquired_guid(int index);
bool catastrophe_restore_guid(guid64 guid);
void catastrophe_legacy_loaded(void);

#endif
