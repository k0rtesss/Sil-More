#include "angband.h"
#include "monster/monster-abilities.h"
#include <stdio.h>

#if defined(ABILITY_SAVE_WRITER)
/* Keep the real serializer and byte encoding; expose only its private stream
 * setup. Compiled separately from the reader to retain their private names. */
#include "../../src/fs/save.c"
size_t ability_save_record(byte* data, size_t capacity, const monster_type* m)
{
    fff = SDL_IOFromMem(data, capacity);
    if (!fff) return 0;
    xor_byte = 0; v_stamp = x_stamp = 0; write_error = false;
    save_byte_offset = 0;
    wr_monster(m);
    wr_u16b(0xa53c);
    size_t size = (size_t)SDL_TellIO(fff);
    SDL_CloseIO(fff); fff = NULL;
    return write_error ? 0 : size;
}
size_t ability_save_lore(byte* data, size_t capacity, int r_idx)
{
    fff = SDL_IOFromMem(data, capacity);
    if (!fff) return 0;
    xor_byte = 0; v_stamp = x_stamp = 0; write_error = false;
    save_byte_offset = 0;
    wr_lore(r_idx);
    wr_u16b(0xb64d);
    size_t size = (size_t)SDL_TellIO(fff);
    SDL_CloseIO(fff); fff = NULL;
    return write_error ? 0 : size;
}
#elif defined(ABILITY_SAVE_READER)
#include "../../src/fs/load.c"
size_t ability_load_record(const byte* data, size_t size, byte extra,
    monster_type* m, u16b* sentinel)
{
    fff = SDL_IOFromConstMem(data, size);
    if (!fff) return 0;
    xor_byte = 0; v_check = x_check = 0; load_byte_offset = 0;
    sf_major = 0; sf_minor = 9; sf_patch = 8; sf_extra = extra;
    savefile_has_song_duels = true;
    savefile_has_monster_shatter = true;
    savefile_has_thrall_quest = true;
    savefile_has_thrall_quest_requested = true;
    rd_monster(m);
    rd_u16b(sentinel);
    size_t consumed = (size_t)SDL_TellIO(fff);
    SDL_CloseIO(fff); fff = NULL;
    return consumed;
}
size_t ability_load_lore(const byte* data, size_t size, byte extra,
    int r_idx, u16b* sentinel)
{
    fff = SDL_IOFromConstMem(data, size);
    if (!fff) return 0;
    xor_byte = 0; v_check = x_check = 0; load_byte_offset = 0;
    sf_major = 0; sf_minor = 9; sf_patch = 8; sf_extra = extra;
    rd_lore(r_idx);
    rd_u16b(sentinel);
    size_t consumed = (size_t)SDL_TellIO(fff);
    SDL_CloseIO(fff); fff = NULL;
    return consumed;
}
#else
#include "externs.h"
#include "log/log.h"

size_t ability_save_record(byte*, size_t, const monster_type*);
size_t ability_load_record(const byte*, size_t, byte, monster_type*, u16b*);
size_t ability_save_lore(byte*, size_t, int);
size_t ability_load_lore(const byte*, size_t, byte, int, u16b*);

/* Deterministic state-transition fixtures against the production ability
 * module. Hit rolls, AI path selection, rendering and save I/O are not mocked
 * into claims of end-to-end combat coverage. */
static monster_type monsters[3];
static monster_race races[3];
static monster_lore lore[3];
static int checks;

#define CHECK(test) do { checks++; if (!(test)) { \
    fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #test); exit(1); \
} } while (0)

void log_log(int level, const char* file, int line, const char* fmt, ...)
{
    if (level < LOG_WARN) return;
    va_list ap;
    fprintf(stderr, "%s:%d: ", file, line);
    va_start(ap, fmt); vfprintf(stderr, fmt, ap); va_end(ap);
    fputc('\n', stderr);
    CHECK(false);
}

static monster_type* reset_monster(u32b abilities)
{
    memset(monsters, 0, sizeof(monsters));
    memset(races, 0, sizeof(races));
    memset(lore, 0, sizeof(lore));
    mon_list = monsters;
    r_info = races;
    l_list = lore;
    monsters[1].r_idx = 1;
    monsters[1].fy = monsters[1].fx = 20;
    monsters[1].hp = monsters[1].maxhp = 100;
    monsters[1].alertness = ALERTNESS_ALERT;
    monsters[1].stance = STANCE_AGGRESSIVE;
    monsters[1].mana = 15;
    races[1].flags5 = abilities;
    races[1].per = 10;
    return &monsters[1];
}

static void step(monster_type* m, int dy, int dx)
{
    int y = m->fy, x = m->fx;
    monster_abilities_begin_action(m);
    m->fy += dy;
    m->fx += dx;
    monster_abilities_end_action(m, y, x, false);
}

static void idle(monster_type* m, int action, bool skipped)
{
    int y = m->fy, x = m->fx;
    monster_abilities_begin_action(m);
    m->previous_action[0] = action;
    monster_abilities_end_action(m, y, x, skipped);
}

static void attack(monster_type* m, bool ordinary)
{
    int y = m->fy, x = m->fx;
    monster_abilities_begin_action(m);
    monster_abilities_mark_melee(m, ordinary);
    monster_abilities_end_action(m, y, x, false);
}

static void test_sprinting(void)
{
    monster_type* m = reset_monster(RF5_SPRINTING);
    for (int i = 0; i < 3; i++) {
        step(m, 0, 1);
        CHECK(!monster_sprinting(m));
    }
    step(m, 0, 1);
    CHECK(monster_sprinting(m));
    CHECK(monster_moved_last_action(m));
    step(m, -1, 0); /* A right-angle turn breaks the run. */
    CHECK(!monster_sprinting(m));

    m = reset_monster(RF5_SPRINTING);
    step(m, 0, 1); step(m, 0, 1); step(m, -1, 1); step(m, -1, 1);
    CHECK(monster_sprinting(m));
    /* Pairwise small turns cannot accumulate into a winding sprint. */
    m = reset_monster(RF5_SPRINTING);
    step(m, 0, 1); step(m, -1, 1); step(m, -1, 0); step(m, -1, -1);
    CHECK(!monster_sprinting(m));

    for (int mode = 0; mode < 4; mode++) {
        m = reset_monster(RF5_SPRINTING);
        for (int i = 0; i < 4; i++) step(m, 0, 1);
        if (mode == 0) idle(m, ACTION_MISC, false);
        if (mode == 1) idle(m, ACTION_ARCHERY, false);
        if (mode == 2) attack(m, true);
        if (mode == 3) idle(m, ACTION_MISC, true);
        CHECK(!monster_sprinting(m));
    }
    m = reset_monster(RF5_SPRINTING);
    for (int i = 0; i < 4; i++) idle(m, 6, false);
    CHECK(!monster_sprinting(m)); /* An attempted move is not movement. */
    CHECK(!monster_moved_last_action(m));
    m = reset_monster(0);
    for (int i = 0; i < 4; i++) step(m, 0, 1);
    CHECK(!monster_sprinting(m));
    for (int mode = 0; mode < 3; mode++) {
        m = reset_monster(RF5_SPRINTING);
        for (int i = 0; i < 4; i++) step(m, 0, 1);
        if (mode == 0) m->skip_next_turn = true;
        if (mode == 1) m->skip_this_turn = true;
        if (mode == 2) m->smite_recovery = 2;
        CHECK(!monster_sprinting(m));
    }
    m = reset_monster(RF5_SPRINTING | RF5_DODGING);
    for (int i = 0; i < 4; i++) step(m, 0, 1);
    step(m, 0, 2); /* Teleport/displacement is not a completed running step. */
    CHECK(!monster_sprinting(m));
    CHECK(monster_dodging_bonus(m) == 0);
}

static void test_dodging_and_blocking(void)
{
    monster_type* m = reset_monster(RF5_DODGING | RF5_BLOCKING);
    races[1].pd = 4; /* Three body dice and one actual shield die. */
    races[1].shield_dd = 1;
    CHECK(monster_dodging_bonus(m) == 0);
    CHECK(monster_blocking_bonus_dice(m) == 1);
    step(m, 0, 1);
    CHECK(monster_dodging_bonus(m) == 3);
    CHECK(monster_blocking_bonus_dice(m) == 0);
    /* Beginning the next action must still consult the completed move. */
    monster_abilities_begin_action(m);
    CHECK(monster_dodging_bonus(m) == 3);
    CHECK(monster_blocking_bonus_dice(m) == 0);
    monster_abilities_end_action(m, m->fy, m->fx, false);
    CHECK(monster_dodging_bonus(m) == 0);
    CHECK(monster_blocking_bonus_dice(m) == 1);
    idle(m, 6, false);
    CHECK(monster_dodging_bonus(m) == 0);
    CHECK(monster_blocking_bonus_dice(m) == 1);
    races[1].shield_dd = 0;
    CHECK(monster_blocking_bonus_dice(m) == 0);
    races[1].shield_dd = 2;
    m->song_armor_dice_penalty = 3;
    CHECK(monster_blocking_bonus_dice(m) == 1);
    m->song_armor_dice_penalty = 4;
    CHECK(monster_blocking_bonus_dice(m) == 0);
    m->song_armor_dice_penalty = 5;
    CHECK(monster_blocking_bonus_dice(m) == 0);
    m->song_armor_dice_penalty = 0;
    races[1].flags5 = 0;
    races[1].shield_dd = 1;
    step(m, 0, 1);
    CHECK(monster_dodging_bonus(m) == 0);
    idle(m, ACTION_MISC, false);
    CHECK(monster_blocking_bonus_dice(m) == 0);
}

static void test_vengeance(void)
{
    monster_type* m = reset_monster(RF5_VENGEANCE);
    CHECK(monster_vengeance_bonus_dice(m) == 0);
    monster_receive_melee_damage(m, 0);
    monster_receive_melee_damage(m, -3);
    CHECK(monster_vengeance_bonus_dice(m) == 0);
    monster_receive_melee_damage(m, 1);
    CHECK(monster_vengeance_bonus_dice(m) == 1);
    monster_receive_melee_damage(m, 20);
    CHECK(monster_vengeance_bonus_dice(m) == 1);
    attack(m, true); /* Missing does not call the landed-hit consumer. */
    CHECK(monster_vengeance_bonus_dice(m) == 1);
    idle(m, ACTION_ARCHERY, false);
    CHECK(monster_vengeance_bonus_dice(m) == 1);
    monster_consume_vengeance(m);
    CHECK(monster_vengeance_bonus_dice(m) == 0);
    monster_consume_vengeance(m);
    CHECK(monster_vengeance_bonus_dice(m) == 0);
    m = reset_monster(0);
    monster_receive_melee_damage(m, 10);
    CHECK(monster_vengeance_bonus_dice(m) == 0);
}

static void test_concentration(void)
{
    monster_type* m = reset_monster(RF5_CONCENTRATION);
    CHECK(monster_concentration_bonus(m, true) == 0);
    for (int i = 0; i < 8; i++) {
        int y = m->fy, x = m->fx;
        monster_abilities_begin_action(m);
        CHECK(monster_concentration_bonus(m, true) == (i < 5 ? i : 5));
        CHECK(monster_concentration_bonus(m, false) == 0);
        /* Ordinary attempts count; deliberately no landed-hit callback. */
        monster_abilities_mark_melee(m, true);
        monster_abilities_end_action(m, y, x, false);
    }
    int previous = m->consecutive_attacks;
    monster_abilities_mark_melee(m, false);
    CHECK(m->consecutive_attacks == previous);
    CHECK(monster_concentration_bonus(m, false) == 0);

    for (int mode = 0; mode < 6; mode++) {
        m = reset_monster(RF5_CONCENTRATION);
        attack(m, true); attack(m, true);
        CHECK(m->consecutive_attacks == 2);
        CHECK(monster_concentration_bonus(m, true) == 0);
        if (mode == 0) step(m, 0, 1);
        if (mode == 1) idle(m, ACTION_MISC, false);
        if (mode == 2) idle(m, ACTION_ARCHERY, false);
        if (mode == 3) idle(m, ACTION_MISC, true);
        if (mode == 4) attack(m, false);
        if (mode == 5) {
            monster_abilities_begin_action(m);
            monster_abilities_mark_melee(m, true);
            int y = m->fy, x = m->fx;
            m->fx++;
            monster_abilities_end_action(m, y, x, false);
        }
        CHECK(monster_concentration_bonus(m, true) == 0);
        CHECK(m->consecutive_attacks == 0);
    }
    m = reset_monster(RF5_CONCENTRATION);
    races[1].per = -2;
    attack(m, true);
    CHECK(monster_concentration_bonus(m, true) == 0);
    m = reset_monster(0);
    attack(m, true);
    CHECK(monster_concentration_bonus(m, true) == 0);
}

static void test_smite(void)
{
    monster_type* m = reset_monster(RF5_SMITE);
    CHECK(!monster_try_smite(m, true)); /* No own action in progress. */
    monster_abilities_begin_action(m);
    CHECK(!monster_try_smite(m, false));
    CHECK(m->mana == 15 && !m->skip_next_turn);
    CHECK(monster_try_smite(m, true));
    CHECK(m->mana == 5);
    CHECK(m->skip_next_turn && m->smite_recovery);
    CHECK(!monster_abilities_can_react(m));
    CHECK(!monster_try_smite(m, true));
    CHECK(m->mana == 5);
    /* A miss still reaches action completion with recovery owed. */
    monster_abilities_mark_melee(m, true);
    monster_abilities_end_action(m, m->fy, m->fx, false);
    CHECK(m->skip_next_turn && m->smite_recovery);
    monster_abilities_begin_action(m);
    CHECK(!monster_try_smite(m, true));
    m->skip_next_turn = false; /* Scheduler consumes the missed action. */
    monster_abilities_end_action(m, m->fy, m->fx, true);
    CHECK(m->smite_recovery == 2);
    CHECK(!monster_abilities_can_react(m));
    m->mana = 10;
    monster_abilities_begin_action(m);
    CHECK(!m->smite_recovery);
    CHECK(monster_abilities_can_react(m));
    CHECK(monster_try_smite(m, true));
    CHECK(m->mana == 0);
    monster_abilities_end_action(m, m->fy, m->fx, false);

    for (int mode = 0; mode < 5; mode++) {
        m = reset_monster(mode == 4 ? 0 : RF5_SMITE);
        monster_abilities_begin_action(m);
        if (mode == 0) m->mana = 9;
        if (mode == 1) m->confused = 1;
        if (mode == 2) m->skip_this_turn = true;
        if (mode == 3) m->alertness = ALERTNESS_UNWARY;
        int mana = m->mana;
        CHECK(!monster_try_smite(m, true));
        CHECK(m->mana == mana);
    }
}

static void test_observation_and_reactions(void)
{
    monster_type* m = reset_monster(RF5_DODGING | RF5_BLOCKING | RF5_VENGEANCE);
    races[1].pd = 4;
    races[1].shield_dd = 1;
    step(m, 0, 1);
    CHECK(monster_dodging_bonus(m) == 3);
    CHECK(lore[1].flags5 == 0);
    m->ml = true;
    CHECK(monster_dodging_bonus(m) == 3);
    CHECK(lore[1].flags5 == RF5_DODGING);
    idle(m, ACTION_MISC, false);
    CHECK(monster_blocking_bonus_dice(m) == 1);
    CHECK(lore[1].flags5 == (RF5_DODGING | RF5_BLOCKING));
    monster_receive_melee_damage(m, 0);
    CHECK(!(lore[1].flags5 & RF5_VENGEANCE));
    monster_receive_melee_damage(m, 1);
    CHECK(lore[1].flags5 & RF5_VENGEANCE);
    CHECK(monster_abilities_can_react(m));
    m->confused = 1;
    CHECK(!monster_abilities_can_react(m));
    m->confused = 0;
    m->alertness = ALERTNESS_UNWARY;
    CHECK(!monster_abilities_can_react(m));
    m->alertness = ALERTNESS_ALERT;
    m->skip_this_turn = true;
    CHECK(!monster_abilities_can_react(m));
    CHECK(!monster_abilities_can_react(NULL));
}

static void test_save_records(void)
{
    byte encoded[1024], legacy[1024], plain[1024];
    monster_type restored;
    u16b sentinel = 0;
    for (int recovery = 0; recovery <= 2; recovery++) {
        monster_type* m = reset_monster(RF5_SMITE | RF5_VENGEANCE
            | RF5_CONCENTRATION | RF5_SPRINTING);
        m->vengeance = 1;
        m->smite_recovery = recovery;
        m->skip_next_turn = recovery == 1;
        m->energy = -120;
        m->poisoned = 37;
        m->consecutive_attacks = 3;
        m->turns_stationary = 4;
        m->previous_action[0] = 6;
        m->previous_action[1] = 6;
        m->previous_action[2] = 9;
        m->previous_action[3] = 9;
        m->thrall_quest_item = 2;
        m->thrall_quest_requested = 1;
        m->thrall_quest_completed = 1;
        /* Transient flags must not survive even from a dirty destination. */
        m->ability_in_action = m->ability_melee = m->ability_displaced = true;
        size_t size = ability_save_record(encoded, sizeof(encoded), m);
        CHECK(size > 4);
        memset(&restored, 0xff, sizeof(restored));
        CHECK(ability_load_record(encoded, size, 5, &restored, &sentinel) == size);
        CHECK(sentinel == 0xa53c);
        CHECK(restored.r_idx == m->r_idx && restored.hp == m->hp);
        CHECK(restored.energy == -120 && restored.poisoned == 37);
        CHECK(restored.vengeance == 1 && restored.smite_recovery == recovery);
        CHECK(restored.skip_next_turn == (recovery == 1));
        CHECK(restored.consecutive_attacks == 3);
        CHECK(restored.turns_stationary == 4);
        CHECK(!restored.ability_in_action && !restored.ability_melee);
        CHECK(!restored.ability_displaced);
        CHECK(!memcmp(restored.previous_action, m->previous_action, ACTION_MAX));
        CHECK(restored.thrall_quest_item == 2 && restored.thrall_quest_requested == 1
            && restored.thrall_quest_completed == 1);
        if (recovery) CHECK(!monster_abilities_can_react(&restored));

        /* 0.9.8.4 ends the monster record after poison. Remove the new two
         * trailing fields from decoded bytes, retain the following record's
         * sentinel, then re-encode. This checks the legacy read boundary. */
        byte previous = 0;
        for (size_t i = 0; i < size; i++) {
            plain[i] = encoded[i] ^ previous;
            previous = encoded[i];
        }
        plain[size - 4] = plain[size - 2];
        plain[size - 3] = plain[size - 1];
        previous = 0;
        for (size_t i = 0; i < size - 2; i++) {
            previous ^= plain[i];
            legacy[i] = previous;
        }
        memset(&restored, 0xff, sizeof(restored));
        CHECK(ability_load_record(legacy, size - 2, 4, &restored, &sentinel)
            == size - 2);
        CHECK(sentinel == 0xa53c);
        CHECK(restored.energy == -120 && restored.poisoned == 37);
        CHECK(restored.vengeance == 0 && restored.smite_recovery == 0);
        CHECK(restored.consecutive_attacks == 0);
        CHECK(!restored.ability_in_action && !restored.ability_melee);
        CHECK(!restored.ability_displaced);
        CHECK(restored.skip_next_turn == (recovery == 1));
        CHECK(!memcmp(restored.previous_action, m->previous_action, ACTION_MAX));
    }
    monster_type* m = reset_monster(RF5_SMITE | RF5_VENGEANCE);
    m->smite_recovery = 1;
    m->skip_next_turn = false; /* Reader repairs pending recovery's skip bit. */
    size_t size = ability_save_record(encoded, sizeof(encoded), m);
    memset(&restored, 0, sizeof(restored));
    CHECK(ability_load_record(encoded, size, 5, &restored, &sentinel) == size);
    CHECK(restored.smite_recovery == 1 && restored.skip_next_turn);
    CHECK(!monster_abilities_can_react(&restored));
    m->smite_recovery = 255;
    m->vengeance = 255;
    size = ability_save_record(encoded, sizeof(encoded), m);
    CHECK(ability_load_record(encoded, size, 5, &restored, &sentinel) == size);
    CHECK(sentinel == 0xa53c);
    CHECK(restored.smite_recovery == 2 && restored.vengeance == 1);
}

static void test_forced_movement(void)
{
    monster_type* m = reset_monster(RF5_SPRINTING | RF5_CONCENTRATION
        | RF5_DODGING | RF5_SMITE);
    for (int i = 0; i < 4; i++) step(m, 0, 1);
    CHECK(monster_sprinting(m));
    monster_abilities_forced_movement(m);
    CHECK(!monster_sprinting(m));
    CHECK(monster_dodging_bonus(m) == 0);
    attack(m, true); attack(m, true);
    CHECK(m->consecutive_attacks == 2);
    monster_abilities_begin_action(m);
    int y = m->fy, x = m->fx;
    monster_abilities_mark_melee(m, true);
    monster_abilities_forced_movement(m);
    CHECK(monster_concentration_bonus(m, true) == 0);
    CHECK(!monster_try_smite(m, true));
    monster_abilities_mark_melee(m, true);
    /* A displacement and return cannot masquerade as standing still. */
    monster_abilities_end_action(m, y, x, false);
    CHECK(m->consecutive_attacks == 0);
    CHECK(monster_dodging_bonus(m) == 0);
    CHECK(!m->ability_displaced);

    m = reset_monster(0);
    monster_abilities_begin_action(m);
    y = m->fy; x = m->fx; m->fx++;
    m->previous_action[0] = ACTION_MISC; /* Existing charge/swap exclusion. */
    monster_abilities_end_action(m, y, x, false);
    CHECK(m->previous_action[0] == ACTION_MISC);
}

static void test_save_lore(void)
{
    byte encoded[256], legacy[256], plain[256];
    u16b sentinel = 0;
    reset_monster(RF5_SMITE | RF5_DODGING);
    races[1].max_num = 7;
    lore[1].flags5 = RF5_SMITE | RF5_DODGING | RF5_BLOCKING;
    lore[1].psights = 123;
    lore[1].blows[0] = 17;
    lore[1].song_lore_flags = MONSTER_LORE_SONG_CONTEST;
    size_t size = ability_save_lore(encoded, sizeof(encoded), 1);
    CHECK(size > 4);
    memset(&lore[1], 0xff, sizeof(lore[1]));
    races[1].max_num = 0;
    CHECK(ability_load_lore(encoded, size, 5, 1, &sentinel) == size);
    CHECK(sentinel == 0xb64d);
    CHECK(lore[1].flags5 == (RF5_SMITE | RF5_DODGING));
    CHECK(lore[1].psights == 123 && lore[1].blows[0] == 17);
    CHECK(races[1].max_num == 7);
    CHECK(lore[1].song_lore_flags == MONSTER_LORE_SONG_CONTEST);
    /* Old lore: five 16-bit counters, four byte counters, observed blows,
     * then four 32-bit flag words. flags5 did not precede max_num in .4. */
    size_t flags5_offset = 10 + 4 + MONSTER_BLOW_MAX + 16;
    byte previous = 0;
    for (size_t i = 0; i < size; i++) {
        plain[i] = encoded[i] ^ previous;
        previous = encoded[i];
    }
    memmove(plain + flags5_offset, plain + flags5_offset + 4,
        size - flags5_offset - 4);
    previous = 0;
    for (size_t i = 0; i < size - 4; i++) {
        previous ^= plain[i];
        legacy[i] = previous;
    }
    memset(&lore[1], 0xff, sizeof(lore[1]));
    races[1].max_num = 0;
    CHECK(ability_load_lore(legacy, size - 4, 4, 1, &sentinel) == size - 4);
    CHECK(sentinel == 0xb64d);
    CHECK(lore[1].flags5 == 0);
    CHECK(lore[1].psights == 123 && lore[1].blows[0] == 17);
    CHECK(races[1].max_num == 7);
    CHECK(lore[1].song_lore_flags == MONSTER_LORE_SONG_CONTEST);
    /* An old unused race slot allowed 100 spawns. Once the template assigns
     * a unique to that slot, loading must permit only one, while a defeated
     * unique's saved zero must remain zero. Use the actual .4 lore bytes. */
    races[1].flags1 = RF1_UNIQUE;
    for (int saved_limit = 100; saved_limit >= 0; saved_limit -= 100) {
        plain[flags5_offset] = (byte)saved_limit;
        previous = 0;
        for (size_t i = 0; i < size - 4; i++) {
            previous ^= plain[i];
            legacy[i] = previous;
        }
        races[1].max_num = 7;
        lore[1].flags5 = RF5_SMITE;
        CHECK(ability_load_lore(legacy, size - 4, 4, 1, &sentinel) == size - 4);
        CHECK(sentinel == 0xb64d);
        CHECK(races[1].max_num == (saved_limit ? 1 : 0));
        CHECK(lore[1].flags5 == 0);
    }
}

int main(void)
{
    test_sprinting();
    test_dodging_and_blocking();
    test_vengeance();
    test_concentration();
    test_smite();
    test_observation_and_reactions();
    test_save_records();
    test_forced_movement();
    test_save_lore();
    printf("Monster ability regression checks passed: %d\n", checks);
    return 0;
}
#endif
