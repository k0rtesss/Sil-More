#include "angband.h"

#include "app-scene-birth.h"

#include "blitz.h"
#include "log/log.h"

static bool birth_pending_compact_description_confirm = false;

static bool starting_artifact_is_eligible(int art_idx, int k_idx)
{
    artefact_type *a_ptr;
    object_type object_type_body;
    object_type *o_ptr = &object_type_body;

    if (art_idx <= 0 || art_idx >= z_info->art_max)
        return false;

    a_ptr = &a_info[art_idx];
    if (!a_ptr->name[0])
        return false;

    if (a_ptr->level > 10)
        return false;

    if (!k_idx)
        return false;

    object_prep(o_ptr, k_idx);
    o_ptr->name1 = art_idx;
    apply_magic(o_ptr, -1, true, true, true, true);

    return (object_smithing_difficulty(o_ptr) <= 45);
}

static int birth_curse_count(int id)
{
    return CURSE_GET(id);
}

static void give_start_items(const start_item *list)
{
    int i, slot, inven_slot;
    object_type object_type_body, *i_ptr, *o_ptr;

    for (i = 0; i < MAX_START_ITEMS && list[i].tval; i++)
    {
        const start_item *e_ptr = &list[i];

        s16b k_idx = lookup_kind(e_ptr->tval, e_ptr->sval);
        if (!k_idx)
            continue;

        object_kind *k_ptr = &k_info[k_idx];
        i_ptr = &object_type_body;

        object_prep(i_ptr, k_idx);
        i_ptr->number = (byte)rand_range(e_ptr->min, e_ptr->max);
        i_ptr->weight = k_ptr->weight;

        slot = wield_slot(i_ptr);

        if (slot == INVEN_LITE)
        {
            if (i_ptr->sval == SV_LIGHT_TORCH)
                i_ptr->timeout = 1000;
            else if (i_ptr->sval == SV_LIGHT_LANTERN)
                i_ptr->timeout = 3000;
            else if (i_ptr->sval == SV_LIGHT_MALLORN)
                i_ptr->timeout = 100;
        }

        bool start_known = true;
        if ((i_ptr->tval == TV_POTION)
            || (i_ptr->tval == TV_FOOD && i_ptr->sval <= SV_FOOD_SICKNESS)
            || (i_ptr->tval == TV_GEM))
        {
            if (!player_auto_identifies_object(i_ptr))
                start_known = false;
        }

        if (start_known)
            object_known(i_ptr);

        int carry_slot = inven_carry(i_ptr, true);

        if (carry_slot == SUPPLIES_INDEX)
        {
            object_type copy;
            char name[80];
            char label = supplies_label_char();

            object_copy(&copy, i_ptr);
            object_desc(name, sizeof(name), &copy, true, 3);
            if (!label)
                label = 'a';
            log_info("Starting item went to supplies: %s (%c)", name, label);

            if ((slot == INVEN_LITE) && (inventory[INVEN_LITE].tval == 0))
            {
                int supply_idx = supplies_first_entry_for_kind(i_ptr->k_idx);
                object_type equip_light;

                if ((supply_idx >= 0)
                    && supplies_take_one(supply_idx, &equip_light))
                {
                    object_copy(&inventory[INVEN_LITE], &equip_light);
                    if (inventory[INVEN_LITE].sval == SV_LIGHT_LANTERN)
                        inventory[INVEN_LITE].timeout = 0;
                    p_ptr->equip_cnt++;
                }
            }
            continue;
        }

        if (carry_slot < 0)
            continue;

        inven_slot = carry_slot;

        if (slot >= INVEN_WIELD && inventory[slot].tval == 0)
        {
            o_ptr = &inventory[slot];
            object_copy(o_ptr, i_ptr);

            if (o_ptr->tval != TV_ARROW)
                o_ptr->number = 1;

            inven_item_increase(inven_slot, -(o_ptr->number));
            inven_item_optimize(inven_slot);
            p_ptr->equip_cnt++;
        }

        object_wipe(i_ptr);
    }
}

static void copy_start_items(start_item dest[MAX_START_ITEMS],
    const start_item src[MAX_START_ITEMS])
{
    int item_idx;

    for (item_idx = 0; item_idx < MAX_START_ITEMS; item_idx++)
        dest[item_idx] = src[item_idx];
}

static void replace_start_food(start_item list[MAX_START_ITEMS], byte from_sval,
    byte to_sval)
{
    int item_idx;

    for (item_idx = 0; item_idx < MAX_START_ITEMS && list[item_idx].tval;
         item_idx++)
    {
        if (list[item_idx].tval == TV_FOOD && list[item_idx].sval == from_sval)
            list[item_idx].sval = to_sval;
    }
}

static int find_named_artifact_for_character(void)
{
    character_profile *character_profile_ptr = &c_info[p_ptr->pcharacter];
    const char *character_name = c_name + character_profile_ptr->name;
    char pattern[64];
    char art_lower[MAX_LEN_ART_NAME];
    char pattern_lower[64];

    strnfmt(pattern, sizeof(pattern), "of %s", character_name);

    for (int i = 0; pattern[i] && i < (int)sizeof(pattern_lower) - 1; i++)
        pattern_lower[i] = tolower((unsigned char)pattern[i]);
    pattern_lower[strlen(pattern)] = '\0';

    for (int i = 1; i < z_info->art_max; i++)
    {
        artefact_type *a_ptr = &a_info[i];

        if (!a_ptr->name[0])
            continue;
        if (a_ptr->cur_num > 0)
            continue;
        if (valar_reserved_artifacts && valar_reserved_artifacts[i])
            continue;

        for (int j = 0; a_ptr->name[j] && j < MAX_LEN_ART_NAME - 1; j++)
            art_lower[j] = tolower((unsigned char)a_ptr->name[j]);
        art_lower[strlen(a_ptr->name)] = '\0';

        if (strstr(art_lower, pattern_lower))
        {
            int k_idx = lookup_kind(a_ptr->tval, a_ptr->sval);

            if (starting_artifact_is_eligible(i, k_idx))
            {
                log_info("Found named artifact for %s: %s (idx=%d)",
                    character_name, a_ptr->name, i);
                return i;
            }
        }
    }

    log_debug("No named artifact found for character: %s", character_name);
    return 0;
}

static void grant_starting_artifact(void)
{
    int art_idx = 0;
    int k_idx = 0;

    art_idx = find_named_artifact_for_character();

    if (art_idx > 0)
    {
        artefact_type *a_ptr = &a_info[art_idx];

        k_idx = lookup_kind(a_ptr->tval, a_ptr->sval);

        if (!k_idx)
        {
            log_warn("Named artifact has invalid base kind (idx=%d)", art_idx);
            art_idx = 0;
        }
        else if (valar_reserved_artifacts && valar_reserved_artifacts[art_idx])
        {
            log_info("Named artifact already reserved (idx=%d)", art_idx);
            art_idx = 0;
        }
        else if (!starting_artifact_is_eligible(art_idx, k_idx))
        {
            log_info("Named artifact does not meet starting thresholds (idx=%d)",
                art_idx);
            art_idx = 0;
        }
    }

    if (art_idx == 0)
    {
        int candidates[512];
        int candidate_kinds[512];
        int count = 0;

        for (int i = 1; i < z_info->art_max
            && count < (int)N_ELEMENTS(candidates); i++)
        {
            artefact_type *a_ptr = &a_info[i];
            int k;

            if (!a_ptr->name[0])
                continue;
            if (a_ptr->cur_num > 0)
                continue;
            if (valar_reserved_artifacts && valar_reserved_artifacts[i])
                continue;

            k = lookup_kind(a_ptr->tval, a_ptr->sval);
            if (!starting_artifact_is_eligible(i, k))
                continue;

            candidates[count] = i;
            candidate_kinds[count] = k;
            count++;
        }

        if (count == 0)
        {
            log_warn("No artefacts available for starting blessing under the lvl<=10 and difficulty<=45 filter.");
            msg_print("No artefact could be granted.");
            return;
        }

        int pick = rand_int(count);
        art_idx = candidates[pick];
        k_idx = candidate_kinds[pick];
    }

    artefact_type *a_ptr = &a_info[art_idx];
    object_type object_type_body;
    object_type *o_ptr = &object_type_body;

    object_prep(o_ptr, k_idx);
    o_ptr->name1 = art_idx;
    apply_magic(o_ptr, -1, true, true, true, true);
    object_aware(o_ptr);
    object_known(o_ptr);
    if (inven_carry(o_ptr, true) < 0)
    {
        log_warn("Starting artefact could not be carried (idx=%d)", art_idx);
        msg_print("You have no room for a starting artefact.");
        return;
    }

    a_ptr->cur_num = 1;
    if (valar_reserved_artifacts)
        valar_reserved_artifacts[art_idx] = true;

    log_info("Starting artefact granted: %s (idx=%d)", a_ptr->name, art_idx);
}

static const int birth_stat_costs[11]
    = { -4, -3, -2, -1, 0, 1, 3, 6, 10, 15, 21 };

int birth_get_start_xp(void)
{
    if (birth_fixed_exp)
        return PY_FIXED_EXP;

    return PY_START_EXP;
}

int birth_curses_stat_adj(int stat)
{
    int delta = 0;

    for (int bit = 0; bit < z_info->cu_max; bit++)
    {
        int cnt = birth_curse_count(bit);

        if (cnt)
            delta += cnt * cu_info[bit].cu_adj[stat];
    }

    return delta;
}

int birth_stat_cost(int stat_value)
{
    if (stat_value < -4 || stat_value > 6)
        return 0;

    return birth_stat_costs[stat_value + 4];
}

int birth_skill_cost(int base, int points)
{
    int total_cost = (points + base) * (points + base + 1) / 2;
    int prev_cost = base * (base + 1) / 2;

    return ((total_cost - prev_cost) * 100);
}

void birth_prepare_character_extra(void)
{
    int i, j;

    p_ptr->new_exp = p_ptr->exp = birth_get_start_xp();
    p_ptr->discovery_lore_flags = 0;
    log_debug("Set starting experience to %d", p_ptr->exp);

    p_ptr->song1 = SNG_NOTHING;
    p_ptr->song2 = SNG_NOTHING;
    p_ptr->song_target_idx = 0;
    p_ptr->song_target_song = SNG_NOTHING;
    p_ptr->song_lockout_timer = 0;
    p_ptr->song_contest_player_stacks = 0;
    p_ptr->song_duel_pad = 0;
    p_ptr->song_contest_last_turn = 0;

    for (i = 0; i < S_MAX; i++)
    {
        for (j = 0; j < ABILITIES_MAX; j++)
        {
            if (i == S_SPC && (j == SPC_OATH_MERCY
                || j == SPC_OATH_SILENCE || j == SPC_OATH_IRON
                || j == SPC_OATH_SMITH || j == SPC_OATH_VALOROUS
                || j == SPC_OATH_LIGHT))
            {
                continue;
            }
            p_ptr->innate_ability[i][j] = false;
            p_ptr->active_ability[i][j] = false;
        }
    }

    for (int slot = 0; slot < CHARACTER_ABILITY_MAX; slot++)
    {
        int stat = c_info[p_ptr->pcharacter].a_adj[slot][0];
        int ab;

        if (stat < 0)
            break;

        ab = c_info[p_ptr->pcharacter].a_adj[slot][1];
        if (stat < S_MAX && ab < ABILITIES_MAX)
        {
            p_ptr->innate_ability[stat][ab] = true;
            p_ptr->active_ability[stat][ab] = true;
            log_debug("Assigned character ability: stat=%d, ability=%d", stat,
                ab);
        }
    }
}

bool birth_assignment_review_pending(void)
{
    return birth_pending_compact_description_confirm;
}

void birth_set_assignment_review_pending(bool pending)
{
    birth_pending_compact_description_confirm = pending;
}

void player_wipe(void)
{
    int i;
    char history[550];
    int stat[A_MAX];
    byte prace = 0;
    byte pcharacter = 0;
    int age = 0;
    int height = 0;
    int weight = 0;

    character_generated = false;
    log_debug("birth.c: character_generated set to false - starting character wipe");
    log_debug("Wiping player data for new character creation");

    if (character_loaded_dead)
    {
        log_debug("Restoring previous character choices from dead character");
        prace = p_ptr->prace;
        pcharacter = p_ptr->pcharacter;
        age = p_ptr->age;
        height = p_ptr->ht;
        weight = p_ptr->wt;
        SDL_strlcpy(history, p_ptr->history, sizeof(history));

        for (i = 0; i < A_MAX; i++)
        {
            if (!(p_ptr->noscore & 0x0008))
            {
                stat[i] = p_ptr->stat_base[i]
                    - (rp_ptr->r_adj[i] + current_character_profile->h_adj[i]);
            }
            else
            {
                stat[i] = 0;
            }
        }
    }

    memset(p_ptr, 0, sizeof(player_type));

    supplies_reset_store();

    if (character_loaded_dead)
    {
        p_ptr->prace = prace;
        p_ptr->pcharacter = pcharacter;
        p_ptr->game_type = 0;
        p_ptr->age = age;
        p_ptr->ht = height;
        p_ptr->wt = weight;
        SDL_strlcpy(p_ptr->history, history, sizeof(p_ptr->history));
        for (i = 0; i < A_MAX; i++)
            p_ptr->stat_base[i] = stat[i];
    }
    else
    {
        p_ptr->prace = 0;
        p_ptr->pcharacter = 0;
        p_ptr->game_type = 0;
        p_ptr->age = 0;
        p_ptr->ht = 0;
        p_ptr->wt = 0;
        p_ptr->history[0] = '\0';
        for (i = 0; i < A_MAX; i++)
            p_ptr->stat_base[i] = 0;
    }

    for (i = 0; i < INVEN_TOTAL; i++)
        object_wipe(&inventory[i]);

    for (i = 0; i < z_info->art_max; i++)
    {
        artefact_type *a_ptr = &a_info[i];

        a_ptr->cur_num = 0;
        a_ptr->found_num = 0;
        a_ptr->seen = 0;
    }

    if (!valar_reserved_artifacts)
        valar_reserved_artifacts = mem_alloc_array(z_info->art_max, bool);
    for (i = 0; i < z_info->art_max; i++)
        valar_reserved_artifacts[i] = false;

    object_level = 0;

    for (i = 1; i < z_info->k_max; i++)
    {
        object_kind *k_ptr = &k_info[i];

        k_ptr->tried = false;
        k_ptr->aware = false;
    }

    for (i = 1; i < z_info->r_max; i++)
    {
        monster_race *r_ptr = &r_info[i];
        monster_lore *l_ptr = &l_list[i];

        r_ptr->cur_num = 0;
        r_ptr->max_num = 100;
        if (r_ptr->flags1 & RF1_UNIQUE)
            r_ptr->max_num = 1;

        l_ptr->psights = 0;
        l_ptr->pkills = 0;
    }

    bones_selector = 0;
    p_ptr->food = PY_FOOD_FULL - 1;
    p_ptr->stairs_taken = 0;
    p_ptr->staircasiness = 0;
    p_ptr->fixed_forge_count = 0;
    p_ptr->forge_count = 0;
    p_ptr->vengeance = 0;
    p_ptr->morgoth_state = 0;
    p_ptr->morgoth_second_wind = 0;
    p_ptr->killed_enemy_with_arrow = false;
    p_ptr->orome_bow_hit_streak = 0;
    p_ptr->orome_spear_ready = 0;
    p_ptr->oath_type = 0;
    p_ptr->oaths_broken = 0;

    p_ptr->tulkas_quest = TULKAS_QUEST_NOT_STARTED;
    p_ptr->tulkas_target_r_idx = 0;
    p_ptr->tulkas_prize_a_idx = 0;
    p_ptr->tulkas_quest_complete = 0;
    p_ptr->tulkas_stronghold_level = 0;
    p_ptr->tulkas_stronghold_placed = 0;
    p_ptr->tulkas_second_roll_done = 0;
    p_ptr->tulkas_orc_mask = 0;
    p_ptr->tulkas_orc_restricted = 0;
    p_ptr->tulkas_second_spawn_pending = 0;
    p_ptr->tulkas_morgoth_progress = 0;

    p_ptr->aule_quest = AULE_QUEST_NOT_STARTED;
    p_ptr->aule_forge_y = 0;
    p_ptr->aule_forge_x = 0;
    p_ptr->aule_reserved = 0;
    p_ptr->aule_level = 0;
    p_ptr->aule_last_object_diff = 0;

    p_ptr->mandos_quest = MANDOS_QUEST_NOT_STARTED;
    p_ptr->mandos_vault_y = 0;
    p_ptr->mandos_vault_x = 0;
    p_ptr->mandos_monsters_remaining = 0;
    p_ptr->mandos_level = 0;
    p_ptr->mandos_reserved = 0;
    p_ptr->mandos_resurrection_primed = 0;
    p_ptr->mandos_resurrection_used = 0;

    p_ptr->niena_quest = NIENA_QUEST_NOT_STARTED;
    p_ptr->niena_monsters_seen = 0;
    p_ptr->niena_monsters_killed = 0;
    p_ptr->niena_reserved = 0;
    p_ptr->niena_level = 0;
    p_ptr->niena_reserved2 = 0;

    p_ptr->orome_quest = OROME_QUEST_NOT_STARTED;
    p_ptr->orome_killed_count = 0;
    p_ptr->orome_target_type = 0;
    p_ptr->orome_target_count = 0;
    p_ptr->orome_wolves_killed = 0;
    p_ptr->orome_spiders_killed = 0;
    p_ptr->orome_serpents_killed = 0;
    p_ptr->orome_vampires_killed = 0;
    p_ptr->orome_dragons_killed = 0;
    p_ptr->orome_great_hunt_mask = 0;

    p_ptr->varda_quest = VARDA_QUEST_NOT_STARTED;
    p_ptr->varda_vault_ready = 0;
    p_ptr->varda_vault_placed = 0;
    p_ptr->varda_shadow_restricted = 0;
    p_ptr->varda_level = 0;
    p_ptr->varda_shadow_ready = 0;
    p_ptr->varda_shadow_placed = 0;
    p_ptr->varda_shadow_pad = 0;
    p_ptr->varda_shadow_level = 0;

    for (i = 0; i < VALA_MAX; i++)
    {
        p_ptr->vala_quest_stage2[i] = 0;
        p_ptr->vala_quest_stage3[i] = 0;
    }

    p_ptr->quest_vault_used = 0;
    log_trace("Birth: All quest states initialized to NOT_STARTED for new character");
    for (i = 0; i < (int)N_ELEMENTS(p_ptr->quest_reserved); i++)
        p_ptr->quest_reserved[i] = 0;

    p_ptr->unique_forge_made = false;
    p_ptr->unique_forge_seen = false;
    for (i = 0; i < MAX_GREATER_VAULTS; i++)
        p_ptr->greater_vaults[i] = 0;
}

u32b curse_flag_mask(void)
{
    u32b mask = 0;

    for (int id = 0; id < z_info->cu_max; id++)
    {
        if (CURSE_CURSE_STACK(id) > 0)
            mask |= cu_info[id].flags;
    }

    return mask;
}

int curse_flag_count_rhf(u32b rhf_flag)
{
    int count = 0;

    for (int i = 0; i < z_info->cu_max; i++)
    {
        int stacks = CURSE_GET(i);

        if (stacks > 0)
        {
            if (cu_info[i].flags & rhf_flag)
                count += stacks;
        }
        else if (stacks < 0)
        {
            if (cu_info[i].blessing_flags & rhf_flag)
                count += -stacks;
        }
    }

    return count;
}

int curse_flag_count_cur(u32b cur_flag)
{
    int count = 0;

    for (int i = 0; i < z_info->cu_max; i++)
    {
        int stacks = CURSE_GET(i);

        if (stacks > 0)
        {
            if (cu_info[i].flags_u & cur_flag)
                count += stacks;
        }
        else if (stacks < 0)
        {
            if (cu_info[i].blessing_flags_u & cur_flag)
                count += -stacks;
        }
    }

    return count;
}

int curse_flag_delta_cur(u32b cur_flag)
{
    int delta = 0;

    for (int i = 0; i < z_info->cu_max; i++)
    {
        int stacks = CURSE_GET(i);

        if (stacks > 0)
        {
            if (cu_info[i].flags_u & cur_flag)
                delta += stacks;
        }
        else if (stacks < 0)
        {
            if (cu_info[i].blessing_flags_u & cur_flag)
                delta -= (-stacks);
        }
    }

    return delta;
}

void birth_finalize_character_creation_selection(void)
{
    int i, j;

    for (i = 0; i < S_MAX; i++)
        p_ptr->skill_base[i] = 0;

    for (i = 0; i < S_MAX; i++)
    {
        for (j = 0; j < ABILITIES_MAX; j++)
        {
            p_ptr->innate_ability[i][j] = false;
            p_ptr->active_ability[i][j] = false;
        }
    }

    for (int slot = 0; slot < CHARACTER_ABILITY_MAX; slot++)
    {
        int stat = c_info[p_ptr->pcharacter].a_adj[slot][0];
        int ab;

        if (stat < 0)
            break;
        ab = c_info[p_ptr->pcharacter].a_adj[slot][1];
        if (stat < S_MAX && ab < ABILITIES_MAX)
        {
            p_ptr->innate_ability[stat][ab] = true;
            p_ptr->active_ability[stat][ab] = true;
        }
    }

    for (i = OPT_BIRTH; i < OPT_CHEAT; i++)
        op_ptr->opt[OPT_ADULT + (i - OPT_BIRTH)] = op_ptr->opt[i];

    for (i = OPT_CHEAT; i < OPT_ADULT; i++)
        op_ptr->opt[OPT_SCORE + (i - OPT_CHEAT)] = op_ptr->opt[i];

    if (strlen(op_ptr->full_name) == 0)
    {
        op_ptr->vault_drop_frequency = VDF_NORMAL;
        op_ptr->noble_item_spawn_mode = NOBLE_ITEM_SPAWN_RESTRICTED;
    }

    if (op_ptr->main_combat_rolls > 4)
        op_ptr->main_combat_rolls = 0;
    if (op_ptr->narrative_banner_seconds > NARRATIVE_BANNER_SECONDS_MAX)
        op_ptr->narrative_banner_seconds = NARRATIVE_BANNER_SECONDS_DEFAULT;
    if (op_ptr->ability_desc_mode > 2)
        op_ptr->ability_desc_mode = 0;
    if (op_ptr->vault_drop_frequency > VDF_PLENTIFUL)
        op_ptr->vault_drop_frequency = VDF_NORMAL;
    if (op_ptr->level_entry_narrative_mode > LEVEL_ENTRY_NARRATIVE_OFF)
        op_ptr->level_entry_narrative_mode = LEVEL_ENTRY_NARRATIVE_BANNER_DELAY;
    if (op_ptr->partition_narrative_mode > PARTITION_NARRATIVE_OFF)
        op_ptr->partition_narrative_mode = PARTITION_NARRATIVE_BANNER;
    if (op_ptr->intro_style > INTRO_STYLE_RANDOM)
        op_ptr->intro_style = INTRO_STYLE_RANDOM;
    if (op_ptr->noble_item_spawn_mode > NOBLE_ITEM_SPAWN_INCLUDE_VAULTS)
        op_ptr->noble_item_spawn_mode = NOBLE_ITEM_SPAWN_RESTRICTED;

    for (i = 0; i < z_info->e_max; i++)
        e_info[i].aware = false;

    log_debug("Character creation step completed: %s %s",
        p_name + p_info[p_ptr->prace].name,
        c_name + c_info[p_ptr->pcharacter].name);
}

void birth_player_outfit(void)
{
    time_t c;
    struct tm *tp;

    log_debug("Starting player equipment setup");

    if (character_loaded)
        return;

    if (curse_flag_count_cur(CUR_NOSTART))
        return;

    player_race *birth_race_ptr = &p_info[p_ptr->prace];
    character_profile *birth_character_profile = &c_info[p_ptr->pcharacter];
    start_item race_start_items[MAX_START_ITEMS];

    copy_start_items(race_start_items, birth_race_ptr->start_items);

    if (birth_character_profile->flags_u & UNQ_SMT_EOL)
        replace_start_food(race_start_items, SV_FOOD_LEMBAS, SV_FOOD_BREAD);

    log_debug("Giving starting items for race: %s",
        p_name + birth_race_ptr->name);
    give_start_items(race_start_items);
    log_debug("Giving starting items for character: %s",
        c_name + birth_character_profile->name);
    give_start_items(birth_character_profile->start_items);

    if (!run_mode_is_blitz()
        && metarun_has_major_blessing_effect(METARUN_MAJOR_EFFECT_START_ARTIFACT))
    {
        grant_starting_artifact();
    }

    c = time((time_t*)0);
    tp = localtime(&c);
    if ((tp->tm_mon == 11) && (tp->tm_mday >= 25))
    {
        object_type object_type_body, *i_ptr = &object_type_body;
        s16b k_idx = lookup_kind(TV_CHEST, SV_CHEST_PRESENT);

        object_prep(i_ptr, k_idx);
        i_ptr->number = 1;
        i_ptr->pval = -20;

        (void)inven_carry(i_ptr, true);
    }

    p_ptr->update |= (PU_BONUS | PU_MANA);
    p_ptr->window |= (PW_INVEN | PW_EQUIP | PW_PLAYER_0);
    p_ptr->redraw |= (PR_EQUIPPY | PR_RESIST);

    log_debug("Player equipment setup completed");
}
