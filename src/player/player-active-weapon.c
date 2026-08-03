#include "angband.h"
#include "externs.h"
#include "log/log.h"
#include "object/object-ui-select.h"
#include "player/player-upkeep-internal.h"
#include "ui/question.h"

static byte last_ranged_weapon_mode = PLAYER_ACTIVE_WEAPON_RANGED_1;
static byte pending_active_weapon_mode = PLAYER_ACTIVE_WEAPON_NONE;
static bool pending_ranged_quiver_only = false;
static bool free_active_weapon_change_used = false;

static bool active_weapon_mode_preview_text(int mode, char* buf,
    size_t buflen);

bool player_active_weapon_mode_is_ranged(int mode)
{
    return mode == PLAYER_ACTIVE_WEAPON_RANGED_1;
}

static int normalize_active_weapon_mode(int mode)
{
    switch (mode)
    {
    case PLAYER_ACTIVE_WEAPON_MELEE:
    case PLAYER_ACTIVE_WEAPON_RANGED_1:
        return mode;
    case PLAYER_ACTIVE_WEAPON_RANGED_2:
        /* Compatibility with saves/configuration written for two quivers. */
        return PLAYER_ACTIVE_WEAPON_RANGED_1;
    default:
        return PLAYER_ACTIVE_WEAPON_MELEE;
    }
}

int player_active_weapon_mode(void)
{
    int mode = normalize_active_weapon_mode(p_ptr->active_weapon_mode);

    if (player_active_weapon_mode_is_ranged(mode))
        last_ranged_weapon_mode = (byte)mode;

    return mode;
}

bool player_active_weapon_is_melee(void)
{
    return player_active_weapon_mode() == PLAYER_ACTIVE_WEAPON_MELEE;
}

bool player_active_weapon_is_ranged(void)
{
    return player_active_weapon_mode_is_ranged(player_active_weapon_mode());
}

int player_active_weapon_mode_for_quiver(int quiver)
{
    (void)quiver;
    return PLAYER_ACTIVE_WEAPON_RANGED_1;
}

int player_last_ranged_weapon_mode(void)
{
    int mode = player_active_weapon_mode();

    if (player_active_weapon_mode_is_ranged(mode))
        return mode;

    mode = normalize_active_weapon_mode(last_ranged_weapon_mode);
    return player_active_weapon_mode_is_ranged(mode)
        ? mode : PLAYER_ACTIVE_WEAPON_RANGED_1;
}

int player_selected_ranged_quiver_number(void)
{
    return 1;
}

int player_opposite_active_weapon_mode(void)
{
    return player_active_weapon_is_ranged()
        ? PLAYER_ACTIVE_WEAPON_MELEE : player_last_ranged_weapon_mode();
}

int player_active_weapon_quiver_slot(void)
{
    int slot;

    if (player_active_weapon_mode() != PLAYER_ACTIVE_WEAPON_RANGED_1)
        return -1;
    slot = player_active_throwing_weapon_slot();
    return slot >= 0 ? slot : player_quiver_selected_arrow_slot();
}

int player_active_weapon_quiver_number(void)
{
    return 1;
}

int player_active_throwing_weapon_slot(void)
{
    const object_type* o_ptr = &inventory[INVEN_WIELD];

    if (!o_ptr->k_idx
        || o_ptr->pickup_slot != PICKUP_SLOT_ACTIVE_THROWING
        || !player_can_treat_as_throwing(o_ptr))
    {
        return -1;
    }

    return INVEN_WIELD;
}

static int active_weapon_kind_for_mode(int mode)
{
    mode = normalize_active_weapon_mode(mode);
    if (mode == PLAYER_ACTIVE_WEAPON_MELEE)
        return PLAYER_ACTIVE_WEAPON_KIND_MELEE;

    if (player_active_throwing_weapon_slot() >= 0)
        return PLAYER_ACTIVE_WEAPON_KIND_THROWING;

    return PLAYER_ACTIVE_WEAPON_KIND_BOW;
}

int player_active_weapon_kind(void)
{
    return active_weapon_kind_for_mode(player_active_weapon_mode());
}

bool player_active_weapon_change_is_free(int old_kind, int new_kind)
{
    bool warden;
    bool versatility;
    bool throwing;
    bool rapid_attack;
    bool skirmishing;

    if (free_active_weapon_change_used)
        return false;

    warden = p_ptr->active_ability[S_MEL][MEL_WARDEN] != 0;
    versatility = p_ptr->active_ability[S_ARC][ARC_VERSATILITY] != 0;
    throwing = p_ptr->active_ability[S_MEL][MEL_THROWING] != 0;
    rapid_attack = p_ptr->active_ability[S_MEL][MEL_RAPID_ATTACK] != 0;
    skirmishing = p_ptr->active_ability[S_ARC][ARC_SKIRMISHING] != 0;

    /* Rows are current M/B/T; columns are destination M/B/T.
     * M: Rapid | W(T|V), W, WT
     * B: V, Skirmishing | V(T|W), VT
     * T: WT, VT, Throwing */

    switch (old_kind)
    {
    case PLAYER_ACTIVE_WEAPON_KIND_MELEE:
        if (new_kind == PLAYER_ACTIVE_WEAPON_KIND_MELEE)
            return rapid_attack || (warden && (throwing || versatility));
        if (new_kind == PLAYER_ACTIVE_WEAPON_KIND_BOW)
            return warden;
        if (new_kind == PLAYER_ACTIVE_WEAPON_KIND_THROWING)
            return warden && throwing;
        break;

    case PLAYER_ACTIVE_WEAPON_KIND_BOW:
        if (new_kind == PLAYER_ACTIVE_WEAPON_KIND_MELEE)
            return versatility;
        if (new_kind == PLAYER_ACTIVE_WEAPON_KIND_BOW)
            return skirmishing || (versatility && (throwing || warden));
        if (new_kind == PLAYER_ACTIVE_WEAPON_KIND_THROWING)
            return versatility && throwing;
        break;

    case PLAYER_ACTIVE_WEAPON_KIND_THROWING:
        if (new_kind == PLAYER_ACTIVE_WEAPON_KIND_MELEE)
            return warden && throwing;
        if (new_kind == PLAYER_ACTIVE_WEAPON_KIND_BOW)
            return versatility && throwing;
        if (new_kind == PLAYER_ACTIVE_WEAPON_KIND_THROWING)
            return throwing;
        break;
    }

    return false;
}

bool player_active_weapon_wield_change_is_free(int slot,
    const object_type* incoming, bool combine)
{
    int old_kind = player_active_weapon_kind();
    int new_kind = PLAYER_ACTIVE_WEAPON_KIND_NONE;

    if (!incoming || !incoming->k_idx || combine)
        return false;

    if (player_active_weapon_is_melee())
    {
        if (slot != INVEN_WIELD)
            return false;
        if (incoming->tval != TV_SWORD && incoming->tval != TV_POLEARM
            && incoming->tval != TV_HAFTED && incoming->tval != TV_DIGGING)
        {
            return false;
        }
        new_kind = PLAYER_ACTIVE_WEAPON_KIND_MELEE;
    }
    else if (slot == INVEN_BOW
        && old_kind == PLAYER_ACTIVE_WEAPON_KIND_BOW
        && incoming->tval == TV_BOW)
    {
        new_kind = PLAYER_ACTIVE_WEAPON_KIND_BOW;
    }
    return new_kind != PLAYER_ACTIVE_WEAPON_KIND_NONE
        && player_active_weapon_change_is_free(old_kind, new_kind);
}

void player_active_weapon_free_change_commit(void)
{
    free_active_weapon_change_used = true;
}

void player_active_weapon_begin_player_turn(void)
{
    free_active_weapon_change_used = false;
}

void player_active_weapon_sync_loaded_state(void)
{
    int saved_mode = p_ptr->active_weapon_mode;
    int mode = normalize_active_weapon_mode(saved_mode);
    object_type* old_quiver = &inventory[INVEN_QUIVER1];

    if (saved_mode == PLAYER_ACTIVE_WEAPON_BELT)
        mode = (player_quiver_arrow_count() > 0 || inventory[INVEN_BOW].k_idx)
            ? PLAYER_ACTIVE_WEAPON_RANGED_1 : PLAYER_ACTIVE_WEAPON_MELEE;

    if (old_quiver->k_idx && old_quiver->tval == TV_ARROW)
    {
        object_type moving;
        object_copy(&moving, old_quiver);
        moving.pickup = false;
        moving.pickup_slot = INVEN_QUIVER1;
        (void)player_quiver_absorb_arrow(&moving);
        object_wipe(old_quiver);
        if (p_ptr->equip_cnt > 0)
            p_ptr->equip_cnt--;
    }
    else if (old_quiver->k_idx && player_can_treat_as_throwing(old_quiver))
    {
        object_type moving;

        object_copy(&moving, old_quiver);
        object_wipe(old_quiver);
        if (p_ptr->equip_cnt > 0)
            p_ptr->equip_cnt--;

        if (player_active_weapon_mode_is_ranged(mode))
        {
            if (inventory[INVEN_WIELD].k_idx)
            {
                object_type displaced;

                object_copy(&displaced, &inventory[INVEN_WIELD]);
                displaced.pickup = false;
                displaced.pickup_slot = -1;
                object_wipe(&inventory[INVEN_WIELD]);
                if (p_ptr->equip_cnt > 0)
                    p_ptr->equip_cnt--;
                (void)inven_carry(&displaced, false);
                if (displaced.k_idx && displaced.number > 0)
                    drop_near(&displaced, 0, p_ptr->py, p_ptr->px);
            }
            object_copy(&inventory[INVEN_WIELD], &moving);
            inventory[INVEN_WIELD].pickup = false;
            inventory[INVEN_WIELD].pickup_slot
                = PICKUP_SLOT_ACTIVE_THROWING;
            p_ptr->equip_cnt++;
        }
        else
        {
            moving.pickup = false;
            moving.pickup_slot = -1;
            if (inven_carry(&moving, false) < 0 && moving.k_idx)
                drop_near(&moving, 0, p_ptr->py, p_ptr->px);
        }
    }

    /* Existing saves and fresh characters can arrive here with Harness
     * weapons that have not yet received a menu color. */
    for (int i = 0; i < INVEN_TOTAL; i++)
    {
        object_type* o_ptr = &inventory[i];

        if (inventory_limit_group_for_object(o_ptr) == INV_LIMIT_HARNESS)
            player_active_weapon_assign_harness_color(o_ptr);
    }
    for (int i = 0; i < player_pack_entry_count(); i++)
    {
        object_type* o_ptr = player_pack_entry_at(i);

        if (inventory_limit_group_for_object(o_ptr) == INV_LIMIT_HARNESS)
            player_active_weapon_assign_harness_color(o_ptr);
    }

    p_ptr->active_weapon_mode = (byte)mode;
    last_ranged_weapon_mode = player_active_weapon_mode_is_ranged(mode)
        ? (byte)mode : PLAYER_ACTIVE_WEAPON_RANGED_1;
    pending_active_weapon_mode = PLAYER_ACTIVE_WEAPON_NONE;
    pending_ranged_quiver_only = false;
    free_active_weapon_change_used = false;
}

static bool player_switch_needs_turn(int old_mode, int new_mode)
{
    return normalize_active_weapon_mode(old_mode)
        != normalize_active_weapon_mode(new_mode);
}

static const char* active_weapon_mode_log_name(int mode)
{
    switch (mode)
    {
    case PLAYER_ACTIVE_WEAPON_MELEE:
        return "melee";
    case PLAYER_ACTIVE_WEAPON_RANGED_1:
        return "ranged";
    default:
        return "unknown";
    }
}

static void active_weapon_mode_prompt_name(char* buf, size_t buflen, int mode)
{
    char weapon_name[80];
    char arrow_name[80] = "no arrows";
    object_type* weapon_ptr = NULL;

    if (!buf || buflen == 0)
        return;

    switch (normalize_active_weapon_mode(mode))
    {
    case PLAYER_ACTIVE_WEAPON_MELEE:
        weapon_ptr = &inventory[INVEN_WIELD];
        if (weapon_ptr->k_idx)
        {
            object_desc(weapon_name, sizeof(weapon_name), weapon_ptr, false,
                4);
            strnfmt(buf, buflen, "your melee weapon (%s)", weapon_name);
        }
        else
        {
            SDL_strlcpy(buf, "your melee weapon", buflen);
        }
        return;

    case PLAYER_ACTIVE_WEAPON_RANGED_1:
    default:
        break;
    }

    if (player_active_throwing_weapon_slot() >= 0)
    {
        weapon_ptr = &inventory[INVEN_WIELD];
        object_desc(weapon_name, sizeof(weapon_name), weapon_ptr, false, 4);
        strnfmt(buf, buflen, "your throwing weapon (%s)", weapon_name);
    }
    else
    {
        object_type* arrow_ptr = player_quiver_arrow_object(
            player_quiver_selected_arrow_slot());

        if (arrow_ptr)
            object_desc(arrow_name, sizeof(arrow_name), arrow_ptr, false, 4);
        weapon_ptr = &inventory[INVEN_BOW];
        if (weapon_ptr->k_idx)
        {
            object_desc(weapon_name, sizeof(weapon_name), weapon_ptr, false, 4);
            strnfmt(buf, buflen,
                "your bow (%s) with %s, quiver %d/%d", weapon_name,
                arrow_name, player_quiver_arrow_count(),
                QUIVER_ARROW_CAPACITY);
        }
        else
        {
            strnfmt(buf, buflen, "your bow with %s, quiver %d/%d",
                arrow_name, player_quiver_arrow_count(),
                QUIVER_ARROW_CAPACITY);
        }
    }
}

void player_active_weapon_name(char* buf, size_t buflen)
{
    int mode;
    int kind;
    const object_type* weapon_ptr = NULL;
    const object_type* arrow_ptr = NULL;
    char weapon_name[80] = "";
    char arrow_name[80] = "no arrows";

    if (!buf || buflen == 0)
        return;

    mode = player_active_weapon_mode();
    kind = active_weapon_kind_for_mode(mode);

    if (kind == PLAYER_ACTIVE_WEAPON_KIND_MELEE)
    {
        weapon_ptr = &inventory[INVEN_WIELD];
        if (weapon_ptr->k_idx)
        {
            object_desc(weapon_name, sizeof(weapon_name), weapon_ptr, false,
                4);
            strnfmt(buf, buflen, "Melee: %s", weapon_name);
        }
        else
        {
            SDL_strlcpy(buf, "Melee", buflen);
        }
        return;
    }

    if (kind == PLAYER_ACTIVE_WEAPON_KIND_THROWING)
    {
        weapon_ptr = &inventory[INVEN_WIELD];
        if (weapon_ptr->k_idx)
        {
            object_desc(weapon_name, sizeof(weapon_name), weapon_ptr, false,
                4);
            strnfmt(buf, buflen, "Throwing: %s", weapon_name);
        }
        else
        {
            SDL_strlcpy(buf, "Throwing", buflen);
        }
        return;
    }

    weapon_ptr = &inventory[INVEN_BOW];
    arrow_ptr = player_quiver_arrow_object(
        player_quiver_selected_arrow_slot());
    if (weapon_ptr->k_idx)
        object_desc(weapon_name, sizeof(weapon_name), weapon_ptr, false, 4);
    if (arrow_ptr)
        object_desc(arrow_name, sizeof(arrow_name), arrow_ptr, false, 4);
    if (weapon_name[0])
        strnfmt(buf, buflen, "Ranged: %s; %s; quiver %d/%d", weapon_name,
            arrow_name, player_quiver_arrow_count(), QUIVER_ARROW_CAPACITY);
    else
        strnfmt(buf, buflen, "Ranged: no bow; %s; quiver %d/%d",
            arrow_name, player_quiver_arrow_count(), QUIVER_ARROW_CAPACITY);
}

static bool confirm_active_weapon_switch(int new_mode)
{
    char target[160];
    char preview[64];
    char prompt[260];

    msg_print("Changing the active weapon takes one turn unless an ability makes it free; inactive weapon attack, damage, evasion, and armour bonuses do not apply.");
    msg_print("This confirmation can be removed in Gameplay Options.");
    active_weapon_mode_prompt_name(target, sizeof(target), new_mode);
    if (active_weapon_mode_preview_text(new_mode, preview, sizeof(preview)))
    {
        strnfmt(prompt, sizeof(prompt),
            "Change active weapon to %s? Expected %s. ", target, preview);
    }
    else
    {
        strnfmt(prompt, sizeof(prompt), "Change active weapon to %s? ",
            target);
    }
    return get_check(prompt);
}

static void ranged_slot_choice_name(char* buf, size_t buflen, int slot)
{
    object_type* arrow = player_quiver_arrow_object(slot);
    char arrow_name[120];

    if (!arrow)
    {
        strnfmt(buf, buflen, "Quiver: empty (0/%d)",
            QUIVER_ARROW_CAPACITY);
        return;
    }

    object_desc(arrow_name, sizeof(arrow_name), arrow, true, 3);
    strnfmt(buf, buflen, "Quiver: %s; %d/%d total", arrow_name,
        player_quiver_arrow_count(), QUIVER_ARROW_CAPACITY);
}

typedef struct active_weapon_choice
{
    int item;
    object_type* o_ptr;
    int mode;
    int kind;
    int target_slot;
    int arrow_item;
    char label[240];
} active_weapon_choice;

typedef struct active_weapon_preview
{
    int attack;
    int dd;
    int ds;
    bool throwing;
    bool has_offhand;
    int offhand_attack;
    int offhand_dd;
    int offhand_ds;
} active_weapon_preview;

static int active_weapon_preview_curse_adjusted_ds(int ds)
{
    int shift = curse_flag_delta_cur(CUR_MDS_SHIFT);

    if (ds > 0 && shift != 0)
        ds = MAX(1, ds - shift);

    return ds;
}

static void active_weapon_preview_stow(const object_type* o_ptr,
    int preferred_slot)
{
    if (!o_ptr || !o_ptr->k_idx)
        return;

    if (preferred_slot >= 0 && preferred_slot <= INVEN_PACK
        && !inventory[preferred_slot].k_idx)
    {
        object_copy(&inventory[preferred_slot], o_ptr);
        return;
    }

    for (int i = 0; i <= INVEN_PACK; i++)
    {
        if (!inventory[i].k_idx)
        {
            object_copy(&inventory[i], o_ptr);
            return;
        }
    }
}

static void active_weapon_preview_move_to_slot(int item, int target_slot)
{
    object_type candidate;
    object_type displaced;
    object_type* source;

    if (item == target_slot)
        return;

    source = player_inventory_object(item);
    if (!source || !source->k_idx)
        return;
    object_copy(&candidate, source);
    object_copy(&displaced, &inventory[target_slot]);
    object_copy(&inventory[target_slot], &candidate);

    if (player_inventory_handle_is_carried(item))
    {
        if (displaced.k_idx)
            object_copy(source, &displaced);
        else
            object_wipe(source);
    }
    else
    {
        object_wipe(source);
        active_weapon_preview_stow(&displaced, -1);
    }
}

static void active_weapon_preview_activate_item_abilities(
    const object_type* o_ptr)
{
    if (!o_ptr || !o_ptr->k_idx)
        return;

    /* do_cmd_wield() activates abilities granted by the newly wielded item
     * before its PU_BONUS calculation. */
    for (int i = 0; i < o_ptr->abilities; i++)
    {
        int skill = o_ptr->skilltype[i];
        int ability = o_ptr->abilitynum[i];

        if (skill < 0 || skill >= S_MAX || ability < 0
            || ability >= ABILITIES_MAX)
        {
            continue;
        }
        /* Match do_cmd_wield(): equipping only auto-activates an ability the
         * player did not already have.  Preserve an intentional off toggle. */
        if (!p_ptr->have_ability[skill][ability])
        {
            p_ptr->have_ability[skill][ability] = true;
            p_ptr->active_ability[skill][ability] = true;
        }
    }
}

static void active_weapon_preview_prepare_melee_equipment(void)
{
    object_type* weapon = &inventory[INVEN_WIELD];
    object_type* offhand = &inventory[INVEN_ARM];
    u32b f1 = 0, f2 = 0, f3 = 0;

    if (!weapon->k_idx)
        return;

    object_flags(weapon, &f1, &f2, &f3);
    if (!offhand->k_idx)
        return;

    if ((f3 & TR3_TWO_HANDED)
        || (offhand->tval != TV_SHIELD
            && !p_ptr->active_ability[S_MEL][MEL_TWO_WEAPON]))
    {
        object_type removed;

        object_copy(&removed, offhand);
        object_wipe(offhand);
        active_weapon_preview_stow(&removed, -1);
    }
}

static bool active_weapon_choice_preview(int mode, int item, int target_slot,
    int arrow_item, active_weapon_preview* preview)
{
    player_type player_snapshot;
    object_type inventory_snapshot[INVEN_TOTAL];
    object_type extra_source_snapshot;
    object_type* extra_source = player_carried_extra_handle_valid(item)
        ? player_inventory_object(item) : NULL;
    byte last_ranged_snapshot = last_ranged_weapon_mode;
    const object_type* weapon;
    const object_type* ammo;
    bool available = false;

    if (!preview || !p_ptr || !player_inventory_handle_valid(item)
        || target_slot < INVEN_WIELD || target_slot >= INVEN_TOTAL)
    {
        return false;
    }

    memset(preview, 0, sizeof(*preview));
    player_snapshot = *p_ptr;
    SDL_memcpy(inventory_snapshot, inventory, sizeof(inventory_snapshot));
    if (extra_source)
        object_copy(&extra_source_snapshot, extra_source);

    active_weapon_preview_move_to_slot(item, target_slot);

    if (mode == PLAYER_ACTIVE_WEAPON_RANGED_1
        && target_slot == INVEN_WIELD
        && player_can_treat_as_throwing(&inventory[INVEN_WIELD]))
    {
        inventory[INVEN_WIELD].pickup_slot = PICKUP_SLOT_ACTIVE_THROWING;
    }
    else if (inventory[INVEN_WIELD].pickup_slot
        == PICKUP_SLOT_ACTIVE_THROWING)
    {
        inventory[INVEN_WIELD].pickup_slot = -1;
    }

    p_ptr->active_weapon_mode = (byte)normalize_active_weapon_mode(mode);
    if (target_slot != INVEN_QUIVER1 && target_slot != INVEN_BELT)
        active_weapon_preview_activate_item_abilities(
            &inventory[target_slot]);
    if (mode == PLAYER_ACTIVE_WEAPON_MELEE)
        active_weapon_preview_prepare_melee_equipment();
    calc_bonuses_for_preview();

    if (mode == PLAYER_ACTIVE_WEAPON_MELEE)
    {
        weapon = &inventory[INVEN_WIELD];
        if (weapon->k_idx)
        {
            preview->attack = p_ptr->skill_use[S_MEL];
            preview->dd = p_ptr->mdd;
            preview->ds = p_ptr->mds;
            if (p_ptr->active_ability[S_MEL][MEL_TWO_WEAPON]
                && inventory[INVEN_ARM].k_idx
                && inventory[INVEN_ARM].tval != TV_SHIELD)
            {
                preview->has_offhand = true;
                preview->offhand_attack = p_ptr->skill_use[S_MEL]
                    + p_ptr->offhand_mel_mod;
                preview->offhand_dd = p_ptr->mdd2;
                preview->offhand_ds = p_ptr->mds2;
            }
            available = true;
        }
    }
    else if (normalize_active_weapon_mode(mode)
        == PLAYER_ACTIVE_WEAPON_RANGED_1)
    {
        object_type* bow = &inventory[INVEN_BOW];
        object_type* thrown = &inventory[INVEN_WIELD];
        int ammo_slot = arrow_item >= 0
            ? arrow_item : player_quiver_selected_arrow_slot();
        u32b f1 = 0, f2 = 0, f3 = 0, f4 = 0;

        ammo = player_quiver_arrow_object(ammo_slot);
        if (thrown->k_idx)
            object_flags4(thrown, &f1, &f2, &f3, &f4);

        if (target_slot == INVEN_WIELD && thrown->k_idx
            && player_can_treat_as_throwing_flags(thrown, f3))
        {
            preview->attack = p_ptr->skill_use[S_MEL] + thrown->att
                + axe_bonus(thrown) + polearm_bonus(thrown);
            if (p_ptr->active_ability[S_MEL][MEL_THROWING]
                || object_grants_ability(thrown, S_MEL, MEL_THROWING))
            {
                preview->attack++;
            }
            preview->dd = total_mdd(thrown);
            preview->ds = active_weapon_preview_curse_adjusted_ds(
                total_mds_for_weapon_mode(thrown, 0,
                    PLAYER_ACTIVE_WEAPON_RANGED_1));
            preview->throwing = true;
            available = true;
        }
        else if (bow->k_idx)
        {
            if (ammo)
                object_flags4(ammo, &f1, &f2, &f3, &f4);
            preview->attack = p_ptr->skill_use[S_ARC]
                + ((ammo && ammo->tval == TV_ARROW) ? ammo->att : 0);
            preview->dd = p_ptr->add;
            preview->ds = p_ptr->ads;
            available = true;
        }
    }

    *p_ptr = player_snapshot;
    SDL_memcpy(inventory, inventory_snapshot, sizeof(inventory_snapshot));
    if (extra_source)
        object_copy(extra_source, &extra_source_snapshot);
    last_ranged_weapon_mode = last_ranged_snapshot;
    return available;
}

bool player_active_weapon_stats_preview(int mode, int* attack, int* dd,
    int* ds, bool* throwing)
{
    active_weapon_preview preview;
    int item;
    int target_slot;

    mode = normalize_active_weapon_mode(mode);
    if (mode == PLAYER_ACTIVE_WEAPON_MELEE)
    {
        item = INVEN_WIELD;
        target_slot = INVEN_WIELD;
    }
    else if (mode == PLAYER_ACTIVE_WEAPON_RANGED_1)
    {
        if (player_active_weapon_kind()
            == PLAYER_ACTIVE_WEAPON_KIND_THROWING)
        {
            item = INVEN_WIELD;
            target_slot = INVEN_WIELD;
        }
        else
        {
            item = INVEN_BOW;
            target_slot = INVEN_BOW;
        }
    }
    else
    {
        return false;
    }

    if (!active_weapon_choice_preview(mode, item, target_slot,
            player_quiver_selected_arrow_slot(), &preview))
        return false;
    if (attack)
        *attack = preview.attack;
    if (dd)
        *dd = preview.dd;
    if (ds)
        *ds = preview.ds;
    if (throwing)
        *throwing = preview.throwing;
    return true;
}

bool player_active_weapon_offhand_stats_preview(int* attack, int* dd,
    int* ds)
{
    active_weapon_preview preview;

    if (!active_weapon_choice_preview(PLAYER_ACTIVE_WEAPON_MELEE,
            INVEN_WIELD, INVEN_WIELD, -1, &preview)
        || !preview.has_offhand)
    {
        return false;
    }

    if (attack)
        *attack = preview.offhand_attack;
    if (dd)
        *dd = preview.offhand_dd;
    if (ds)
        *ds = preview.offhand_ds;
    return true;
}

static bool active_weapon_mode_preview_text(int mode, char* buf,
    size_t buflen)
{
    active_weapon_preview preview;
    int item;
    int target_slot;

    if (!buf || buflen == 0)
        return false;
    buf[0] = '\0';

    mode = normalize_active_weapon_mode(mode);
    if (mode == PLAYER_ACTIVE_WEAPON_MELEE)
    {
        item = INVEN_WIELD;
        target_slot = INVEN_WIELD;
    }
    else if (mode == PLAYER_ACTIVE_WEAPON_RANGED_1)
    {
        if (player_active_weapon_kind()
            == PLAYER_ACTIVE_WEAPON_KIND_THROWING)
        {
            item = INVEN_WIELD;
            target_slot = INVEN_WIELD;
        }
        else
        {
            item = INVEN_BOW;
            target_slot = INVEN_BOW;
        }
    }
    else
    {
        return false;
    }

    if (!active_weapon_choice_preview(mode, item, target_slot,
            player_quiver_selected_arrow_slot(), &preview))
        return false;

    strnfmt(buf, buflen, "(%+d,%dd%d)", preview.attack, preview.dd,
        preview.ds);
    return true;
}

static bool object_is_melee_combat_weapon(const object_type* o_ptr)
{
    if (!o_ptr || !o_ptr->k_idx)
        return false;

    return o_ptr->tval == TV_SWORD || o_ptr->tval == TV_POLEARM
        || o_ptr->tval == TV_HAFTED || o_ptr->tval == TV_DIGGING;
}

static bool object_is_harness_weapon(const object_type* o_ptr)
{
    if (!o_ptr || !o_ptr->k_idx || object_has_broken_prefix(o_ptr))
        return false;
    if (inventory_limit_group_for_object(o_ptr) != INV_LIMIT_HARNESS)
        return false;

    return object_is_melee_combat_weapon(o_ptr) || o_ptr->tval == TV_BOW;
}

/* This is the color sequence for ordinary weapons.  Yellow is reserved for
 * artefacts, and light blue is reserved for the selected menu row. */
static const byte harness_weapon_colors[] = {
    TERM_L_RED, TERM_L_GREEN, TERM_L_UMBER, TERM_VIOLET,
    TERM_ORANGE, TERM_WHITE, TERM_RED, TERM_GREEN, TERM_BLUE, TERM_UMBER
};

static bool harness_weapon_colorable(const object_type* o_ptr)
{
    if (!o_ptr || !o_ptr->k_idx)
        return false;

    return object_is_melee_combat_weapon(o_ptr) || o_ptr->tval == TV_BOW;
}

static bool harness_weapon_color_is_reserved(byte color)
{
    byte base_color = color % TERM_SHADE;

    return base_color == TERM_YELLOW || base_color == TERM_L_BLUE;
}

static byte harness_weapon_saved_color(const object_type* o_ptr)
{
    byte color_id = object_runtime_harness_color_id(o_ptr);

    if (!color_id)
        return 0;
    if (!(color_id & OBJECT_RUNTIME_HARNESS_COLOR_MARKER))
        return 0;

    return color_id & OBJECT_RUNTIME_HARNESS_COLOR_VALUE_MASK;
}

static bool harness_weapon_color_in_use(byte color)
{
    if (!color || harness_weapon_color_is_reserved(color))
        return false;

    for (int i = 0; i < INVEN_TOTAL; i++)
    {
        const object_type* o_ptr = &inventory[i];

        if (!harness_weapon_colorable(o_ptr) || artefact_p(o_ptr))
            continue;
        if (harness_weapon_saved_color(o_ptr) == color)
            return true;
    }
    for (int i = 0; i < player_pack_entry_count(); i++)
    {
        const object_type* o_ptr = player_pack_entry_at(i);

        if (!harness_weapon_colorable(o_ptr) || artefact_p(o_ptr))
            continue;
        if (harness_weapon_saved_color(o_ptr) == color)
            return true;
    }

    return false;
}

static byte allocate_harness_weapon_color(void)
{
    for (int i = 0; i < (int)N_ELEMENTS(harness_weapon_colors); i++)
    {
        if (!harness_weapon_color_in_use(harness_weapon_colors[i]))
            return harness_weapon_colors[i];
    }

    /* All listed colors are already in use; reuse the first one. */
    return harness_weapon_colors[0];
}

void player_active_weapon_assign_harness_color(object_type* o_ptr)
{
    byte saved_color;

    if (!object_is_harness_weapon(o_ptr) || artefact_p(o_ptr))
    {
        return;
    }

    saved_color = harness_weapon_saved_color(o_ptr);
    if (saved_color && !harness_weapon_color_is_reserved(saved_color))
        return;

    object_set_runtime_harness_color_id(o_ptr,
        allocate_harness_weapon_color());
}

void player_active_weapon_forget_harness_color(object_type* o_ptr)
{
    if (!o_ptr || !harness_weapon_colorable(o_ptr))
        return;

    object_set_runtime_harness_color_id(o_ptr, 0);
}

byte player_active_weapon_harness_color(const object_type* o_ptr)
{
    byte color;

    if (!o_ptr || !o_ptr->k_idx)
        return TERM_L_WHITE;
    if (artefact_p(o_ptr))
        return TERM_YELLOW;

    color = harness_weapon_saved_color(o_ptr);
    return color && !harness_weapon_color_is_reserved(color)
        ? color : TERM_L_WHITE;
}

static char active_weapon_choice_key(int index)
{
    static cptr keys = "13579abcdefghilmnopqrstuvwyz";
    size_t key_count = strlen(keys);

    if (index >= 0 && (size_t)index < key_count)
        return keys[index];

    return 0;
}

static char active_weapon_menu_key(int index)
{
    static cptr keys = "abcdefghijklmnopqrstuvwxyz";
    size_t key_count = strlen(keys);

    if (index >= 0 && (size_t)index < key_count)
        return keys[index];

    return 0;
}

static void active_weapon_object_choice_name(char* buf, size_t buflen,
    const object_type* o_ptr)
{
    char object_name[120];

    if (!buf || buflen == 0)
        return;

    if (!o_ptr || !o_ptr->k_idx)
    {
        SDL_strlcpy(buf, "no bow", buflen);
        return;
    }

    object_desc(object_name, sizeof(object_name), o_ptr, true, 3);
    SDL_strlcpy(buf, object_name, buflen);
}

static bool add_active_weapon_choice(active_weapon_choice choices[],
    ui_question_option options[], int capacity, int* count, int item,
    object_type* o_ptr, int mode, int target_slot, int arrow_item, cptr role,
    cptr weapon_label)
{
    active_weapon_choice* choice;
    active_weapon_preview preview;
    char source_label[240];

    if (!choices || !count || *count < 0
        || *count >= capacity || !o_ptr)
    {
        return false;
    }

    choice = &choices[*count];
    choice->item = item;
    choice->o_ptr = o_ptr;
    choice->mode = mode;
    choice->kind = (mode == PLAYER_ACTIVE_WEAPON_MELEE)
        ? PLAYER_ACTIVE_WEAPON_KIND_MELEE
        : ((target_slot == INVEN_WIELD
                && player_can_treat_as_throwing(o_ptr))
            ? PLAYER_ACTIVE_WEAPON_KIND_THROWING
            : PLAYER_ACTIVE_WEAPON_KIND_BOW);
    choice->target_slot = target_slot;
    choice->arrow_item = arrow_item;
    strnfmt(source_label, sizeof(source_label), "%s%s",
        weapon_label ? weapon_label : "",
        item == INVEN_BELT ? " (belt)" : "");
    if (active_weapon_choice_preview(mode, item, target_slot, arrow_item,
            &preview))
    {
        strnfmt(choice->label, sizeof(choice->label),
            "%s\t%s\t(%+d,%dd%d)", role ? role : "Weapon",
            source_label, preview.attack, preview.dd, preview.ds);
    }
    else
    {
        strnfmt(choice->label, sizeof(choice->label), "%s\t%s",
            role ? role : "Weapon", source_label);
    }
    options[*count].key = active_weapon_menu_key(*count);
    options[*count].label = choice->label;
    options[*count].attr = TERM_L_WHITE;
    options[*count].disabled = false;
    (*count)++;
    return true;
}

static void add_ranged_active_weapon_choices(active_weapon_choice choices[],
    ui_question_option options[], int capacity, int* count, int item,
    object_type* bow, int* default_index)
{
    int active_mode = player_active_weapon_mode();
    int selected_arrow = player_quiver_selected_arrow_slot();
    int arrow_slots[QUIVER_ARROW_CAPACITY + 1];
    int arrow_count = player_quiver_arrow_slots(arrow_slots,
        (int)N_ELEMENTS(arrow_slots));
    char bow_name[160];
    char quiver_name[160];
    char label[240];
    bool active_bow = item == INVEN_BOW
        && active_mode == PLAYER_ACTIVE_WEAPON_RANGED_1
        && player_active_weapon_kind() == PLAYER_ACTIVE_WEAPON_KIND_BOW;

    if (bow && bow->k_idx)
    {
        active_weapon_object_choice_name(bow_name, sizeof(bow_name), bow);
        if (arrow_count <= 0)
        {
            int choice_index = *count;
            cptr role = active_bow ? "Ranged [active]" : "Ranged";

            ranged_slot_choice_name(quiver_name, sizeof(quiver_name), -1);
            strnfmt(label, sizeof(label), "%s; %s", bow_name, quiver_name);
            if (!add_active_weapon_choice(choices, options, capacity, count,
                    item, bow, PLAYER_ACTIVE_WEAPON_RANGED_1, INVEN_BOW, -1,
                    role, label))
            {
                return;
            }
            if (active_bow && default_index)
                *default_index = choice_index;
            return;
        }

        for (int i = 0; i < arrow_count; i++)
        {
            int arrow_item = arrow_slots[i];
            int choice_index = *count;
            bool selected = arrow_item == selected_arrow;
            cptr role = (active_bow && selected)
                ? "Ranged [active]" : "Ranged";

            ranged_slot_choice_name(quiver_name, sizeof(quiver_name),
                arrow_item);
            strnfmt(label, sizeof(label), "%s; %s", bow_name, quiver_name);
            if (!add_active_weapon_choice(choices, options, capacity, count,
                    item, bow, PLAYER_ACTIVE_WEAPON_RANGED_1, INVEN_BOW,
                    arrow_item, role, label))
            {
                return;
            }

            if (active_bow && selected && default_index)
                *default_index = choice_index;
        }
    }

}

static void add_melee_and_throwing_active_weapon_choices(
    active_weapon_choice choices[], ui_question_option options[], int capacity,
    int* count, int item, object_type* o_ptr, int current_mode,
    int* default_index)
{
    char label[200];
    cptr role;
    int choice_index;

    if (!object_is_melee_combat_weapon(o_ptr))
        return;

    choice_index = *count;
    role = (item == INVEN_WIELD
               && current_mode == PLAYER_ACTIVE_WEAPON_MELEE)
        ? "Melee [active]"
        : "Melee";
    active_weapon_object_choice_name(label, sizeof(label), o_ptr);
    if (add_active_weapon_choice(choices, options, capacity, count, item,
            o_ptr,
            PLAYER_ACTIVE_WEAPON_MELEE, INVEN_WIELD, -1, role, label)
        && item == INVEN_WIELD
        && current_mode == PLAYER_ACTIVE_WEAPON_MELEE && default_index)
    {
        *default_index = choice_index;
    }

    if (!player_can_treat_as_throwing(o_ptr))
        return;

    choice_index = *count;
    role = (item == INVEN_WIELD
               && current_mode == PLAYER_ACTIVE_WEAPON_RANGED_1
               && player_active_weapon_kind()
                    == PLAYER_ACTIVE_WEAPON_KIND_THROWING)
        ? "Throwing [active]"
        : "Throwing";
    active_weapon_object_choice_name(label, sizeof(label), o_ptr);
    if (add_active_weapon_choice(choices, options, capacity, count, item,
            o_ptr,
            PLAYER_ACTIVE_WEAPON_RANGED_1, INVEN_WIELD, -1, role, label)
        && item == INVEN_WIELD
        && current_mode == PLAYER_ACTIVE_WEAPON_RANGED_1
        && player_active_weapon_kind() == PLAYER_ACTIVE_WEAPON_KIND_THROWING
        && default_index)
    {
        *default_index = choice_index;
    }
}

static void add_empty_active_hand_choice(active_weapon_choice choices[],
    ui_question_option options[], int capacity, int* count,
    int current_mode, int* default_index)
{
    active_weapon_choice* choice;

    if (!choices || !options || !count || !default_index
        || *count < 0 || *count >= capacity
        || current_mode != PLAYER_ACTIVE_WEAPON_MELEE
        || inventory[INVEN_WIELD].k_idx)
    {
        return;
    }

    choice = &choices[*count];
    choice->item = INVEN_WIELD;
    choice->o_ptr = NULL;
    choice->mode = PLAYER_ACTIVE_WEAPON_MELEE;
    choice->kind = PLAYER_ACTIVE_WEAPON_KIND_MELEE;
    choice->target_slot = INVEN_WIELD;
    choice->arrow_item = -1;
    SDL_strlcpy(choice->label, "Melee [active]\tEmpty hand",
        sizeof(choice->label));
    options[*count].key = active_weapon_menu_key(*count);
    options[*count].label = choice->label;
    options[*count].attr = TERM_L_WHITE;
    options[*count].disabled = false;
    *default_index = *count;
    (*count)++;
}

static int active_weapon_choice_category(const active_weapon_choice* choice)
{
    if (!choice)
        return 3;

    switch (choice->kind)
    {
    case PLAYER_ACTIVE_WEAPON_KIND_MELEE:
        return 0;
    case PLAYER_ACTIVE_WEAPON_KIND_BOW:
        return 1;
    case PLAYER_ACTIVE_WEAPON_KIND_THROWING:
        return 2;
    default:
        return 3;
    }
}

static byte active_weapon_choice_color(
    const active_weapon_choice choices[], int index)
{
    if (!choices || index < 0 || !choices[index].o_ptr
        || !choices[index].o_ptr->k_idx)
    {
        return TERM_L_WHITE;
    }

    /* This is only a fallback for old objects or a missed acquisition path;
     * normal Harness entry assigns the ID before the menu is opened. */
    player_active_weapon_assign_harness_color(choices[index].o_ptr);
    return player_active_weapon_harness_color(choices[index].o_ptr);
}

static void prepare_active_weapon_menu_choices(
    active_weapon_choice choices[], ui_question_option options[], int count,
    int* default_index)
{
    active_weapon_choice active_choice;
    ui_question_option active_option;
    int active_index;

    if (!choices || !options || count <= 0 || !default_index)
        return;

    active_index = *default_index;

    /* Stable insertion sort groups all Melee, Ranged, and Throwing choices
     * while preserving their existing order within each category. */
    for (int i = 1; i < count; i++)
    {
        active_weapon_choice choice = choices[i];
        ui_question_option option = options[i];
        int category = active_weapon_choice_category(&choice);
        int insert_at = i;

        while (insert_at > 0
            && active_weapon_choice_category(&choices[insert_at - 1])
                > category)
        {
            choices[insert_at] = choices[insert_at - 1];
            options[insert_at] = options[insert_at - 1];
            if (active_index == insert_at - 1)
                active_index = insert_at;
            insert_at--;
        }

        choices[insert_at] = choice;
        options[insert_at] = option;
        if (active_index == i)
            active_index = insert_at;
    }

    if (active_index > 0 && active_index < count)
    {
        active_choice = choices[active_index];
        active_option = options[active_index];
        for (int i = active_index; i > 0; i--)
        {
            choices[i] = choices[i - 1];
            options[i] = options[i - 1];
        }
        choices[0] = active_choice;
        options[0] = active_option;
        active_index = 0;
    }

    *default_index = active_index;

    for (int i = 0; i < count; i++)
    {
        options[i].key = active_weapon_menu_key(i);
        options[i].label = choices[i].label;
        options[i].attr = active_weapon_choice_color(choices, i);
    }
}

static bool choose_active_weapon(active_weapon_choice* selected)
{
    int capacity = 2 * (player_pack_entry_count() + 5)
        * (player_quiver_store_entry_count() + 1);
    active_weapon_choice* choices;
    ui_question_option* options;
    const object_type** object_icons;
    int count = 0;
    int default_index = 0;
    int selected_index = -1;
    int current_mode = player_active_weapon_mode();

    if (!selected)
        return false;

    choices = mem_alloc_array(capacity, active_weapon_choice);
    options = mem_alloc_array(capacity, ui_question_option);
    object_icons = mem_alloc_array(capacity, const object_type*);

    add_empty_active_hand_choice(choices, options, capacity, &count,
        current_mode,
        &default_index);
    add_melee_and_throwing_active_weapon_choices(choices, options, capacity,
        &count, INVEN_WIELD, &inventory[INVEN_WIELD], current_mode,
        &default_index);
    add_melee_and_throwing_active_weapon_choices(choices, options, capacity,
        &count, INVEN_ARM, &inventory[INVEN_ARM], current_mode,
        &default_index);
    add_melee_and_throwing_active_weapon_choices(choices, options, capacity,
        &count, INVEN_BELT, &inventory[INVEN_BELT], current_mode,
        &default_index);

    for (int ordinal = 0; ordinal < player_pack_entry_count(); ordinal++)
    {
        int item = player_pack_entry_handle_at(ordinal);
        object_type* o_ptr = player_inventory_object(item);

        if (!object_is_harness_weapon(o_ptr)
            || !object_is_melee_combat_weapon(o_ptr))
        {
            continue;
        }

        add_melee_and_throwing_active_weapon_choices(choices, options,
            capacity, &count, item, o_ptr, current_mode, &default_index);
    }

    add_ranged_active_weapon_choices(choices, options, capacity, &count,
        INVEN_BOW, &inventory[INVEN_BOW], &default_index);

    for (int ordinal = 0; ordinal < player_pack_entry_count(); ordinal++)
    {
        int item = player_pack_entry_handle_at(ordinal);
        object_type* o_ptr = player_inventory_object(item);

        if (!object_is_harness_weapon(o_ptr) || o_ptr->tval != TV_BOW)
            continue;

        add_ranged_active_weapon_choices(choices, options, capacity, &count,
            item, o_ptr, &default_index);
    }

    if (count <= 0)
    {
        msg_print("You have no combat weapon to ready.");
        choices = mem_free(choices);
        options = mem_free(options);
        object_icons = mem_free(object_icons);
        return false;
    }

    prepare_active_weapon_menu_choices(choices, options, count,
        &default_index);

    for (int i = 0; i < count; i++)
    {
        object_type* arrow = player_quiver_arrow_object(choices[i].arrow_item);
        object_icons[i] = arrow ? arrow : choices[i].o_ptr;
    }

    selected_index = ui_question_ask_objects("Change active weapon",
            "Choose how to ready a weapon; the current choice is marked [active]. Each bow row selects one arrow type from the mixed Quiver, and changing only that arrow choice always takes no time. A throwing-capable weapon has separate Melee and Throwing choices. Expected attack and damage are shown at the end of each row. Other active-weapon changes take one turn unless an ability makes your first change before your next action free.",
            options, object_icons, count, UI_QUESTION_GLOBAL,
            UI_QUESTION_GLOBAL, default_index);

    if (selected_index < 0 || selected_index >= count)
    {
        choices = mem_free(choices);
        options = mem_free(options);
        object_icons = mem_free(object_icons);
        return false;
    }

    *selected = choices[selected_index];
    choices = mem_free(choices);
    options = mem_free(options);
    object_icons = mem_free(object_icons);
    return true;
}

static void mark_active_weapon_changed(void)
{
    p_ptr->update |= PU_BONUS;
    p_ptr->redraw |= PR_BASIC | PR_MEL | PR_ARC | PR_ARMOR | PR_EQUIPPY
        | PR_QUIVER | PR_MAP;
    p_ptr->window |= PW_PLAYER_0 | PW_EQUIP;
}

static bool object_allows_quick_throw(const object_type* o_ptr)
{
    u32b f1 = 0, f2 = 0, f3 = 0;

    if (!o_ptr || !o_ptr->k_idx)
        return false;
    if (o_ptr->tval != TV_SWORD && o_ptr->tval != TV_POLEARM
        && o_ptr->tval != TV_HAFTED && o_ptr->tval != TV_DIGGING)
    {
        return false;
    }

    object_flags(o_ptr, &f1, &f2, &f3);
    if (f3 & TR3_TWO_HANDED)
        return false;

    return true;
}

static bool player_active_weapon_allows_quick_throw(void)
{
    const object_type* active_weapon;

    if (player_active_weapon_is_ranged())
    {
        if (player_active_weapon_kind()
            != PLAYER_ACTIVE_WEAPON_KIND_BOW)
        {
            return false;
        }
        active_weapon = &inventory[INVEN_BOW];
        return active_weapon->k_idx && active_weapon->tval == TV_BOW;
    }

    active_weapon = &inventory[INVEN_WIELD];
    return object_allows_quick_throw(active_weapon);
}

static bool object_is_dagger(const object_type* o_ptr)
{
    return o_ptr && o_ptr->k_idx && o_ptr->tval == TV_SWORD
        && (o_ptr->sval == SV_DAGGER || o_ptr->sval == SV_CHIPPED_DAGGER);
}

bool object_is_belt_weapon(const object_type* o_ptr)
{
    if (object_is_dagger(o_ptr))
        return true;

    return o_ptr && o_ptr->k_idx && o_ptr->tval == TV_POLEARM
        && o_ptr->sval == SV_HAND_AXE;
}

bool player_power_throw_weapon_eligible(const object_type* o_ptr)
{
    if (!o_ptr || !o_ptr->k_idx || o_ptr->tval != TV_POLEARM)
        return false;

    return o_ptr->sval == SV_SPEAR || o_ptr->sval == SV_HAND_AXE;
}

static bool player_has_melee_weapon_equipped(void)
{
    const object_type* o_ptr = &inventory[INVEN_WIELD];

    if (!o_ptr->k_idx)
        return false;

    return o_ptr->tval == TV_SWORD || o_ptr->tval == TV_POLEARM
        || o_ptr->tval == TV_HAFTED || o_ptr->tval == TV_DIGGING;
}

/* `have_ability` is a derived cache which may briefly retain an old equipment
 * grant.  Power Throw must be owned now: either learned by the player, or
 * granted by an item which currently contributes equipped abilities. */
static bool player_power_throw_ability_owned(void)
{
    if (!p_ptr)
        return false;
    if (p_ptr->innate_ability[S_MEL][MEL_POWER_THROW])
        return true;

    for (int slot = INVEN_WIELD; slot < INVEN_TOTAL; slot++)
    {
        const object_type* o_ptr = &inventory[slot];

        if (!o_ptr->k_idx || slot == INVEN_QUIVER1)
            continue;
        if (slot == INVEN_BELT && !player_can_treat_as_throwing(o_ptr))
            continue;
        if (object_grants_ability(o_ptr, S_MEL, MEL_POWER_THROW))
            return true;
    }

    return false;
}

bool player_power_throw_ready(void)
{
    int previous_action;

    if (!p_ptr)
        return false;
    if (!player_power_throw_ability_owned()
        || !p_ptr->active_ability[S_MEL][MEL_POWER_THROW])
        return false;
    if (!player_active_weapon_is_melee())
        return false;
    if (!player_has_melee_weapon_equipped())
        return false;

    previous_action = p_ptr->previous_action[1];
    return previous_action == 5 || previous_action == ACTION_READY_MELEE;
}

int player_power_throw_target_m_idx(void)
{
    if (!p_ptr)
        return 0;

    if (p_ptr->target_who > 0 && p_ptr->target_who < mon_max
        && target_able(p_ptr->target_who))
    {
        monster_type* m_ptr = &mon_list[p_ptr->target_who];

        if (distance(p_ptr->py, p_ptr->px, m_ptr->fy, m_ptr->fx) == 1)
            return p_ptr->target_who;
    }

    for (int i = 0; i < 8; i++)
    {
        int y = p_ptr->py + ddy_ddd[i];
        int x = p_ptr->px + ddx_ddd[i];
        int m_idx;

        if (!in_bounds(y, x))
            continue;

        m_idx = cave_m_idx[y][x];
        if (m_idx > 0 && target_able(m_idx))
            return m_idx;
    }

    return 0;
}

static bool harness_throw_source_slot(int slot)
{
    object_type* o_ptr = player_inventory_object(slot);

    if (!o_ptr || !o_ptr->k_idx || slot == INVEN_QUIVER1)
        return false;
    return inventory_limit_group_for_object(o_ptr)
        == INV_LIMIT_HARNESS;
}

bool player_can_power_throw_from_harness(int slot)
{
    if (!harness_throw_source_slot(slot)
        || (slot >= INVEN_WIELD && slot < INVEN_TOTAL
            && player_equipment_slot_is_active(slot)))
        return false;
    if (!player_power_throw_ready())
        return false;
    if (!player_power_throw_target_m_idx())
        return false;

    return player_power_throw_weapon_eligible(player_inventory_object(slot));
}

bool player_can_quick_throw_from_harness(int slot)
{
    object_type* o_ptr;

    if (!harness_throw_source_slot(slot))
        return false;
    if (slot >= INVEN_WIELD && slot < INVEN_TOTAL
        && player_equipment_slot_is_active(slot))
        return false;
    if (slot != INVEN_BELT
        && !p_ptr->active_ability[S_MEL][MEL_THROWING])
        return false;
    if (!player_active_weapon_allows_quick_throw())
        return false;

    o_ptr = player_inventory_object(slot);
    return object_is_dagger(o_ptr);
}

int player_quick_throw_harness_slot(void)
{
    for (int slot = 0; slot < INVEN_TOTAL; slot++)
    {
        if (player_can_quick_throw_from_harness(slot))
            return slot;
    }

    for (int i = 0; i < player_carried_extra_entry_count(); i++)
    {
        int slot = CARRIED_EXTRA_INDEX + i;

        if (player_can_quick_throw_from_harness(slot))
            return slot;
    }

    return -1;
}

/*
 * Alchemy lets the player hurl potions at any time, with no requirement to
 * be wielding a Quick Throw-compatible melee weapon (unlike daggers).
 */
bool player_can_throw_potions(void)
{
    return p_ptr->active_ability[S_PER][PER_ALCHEMY] != 0;
}

/* True if Alchemy is known and a potion with a thrown effect is carried. */
bool player_has_throwable_potion(void)
{
    int i;

    if (!player_can_throw_potions())
        return false;

    for (i = 0; i < supplies_entry_count(); i++)
    {
        object_type* o_ptr = supplies_entry_at(i);

        if (potion_has_thrown_effect(o_ptr))
            return true;
    }

    /* Keep compatibility with saves that have not ingested pack supplies. */
    for (int ordinal = 0; ordinal < player_pack_entry_count(); ordinal++)
    {
        object_type* o_ptr = player_pack_entry_at(ordinal);

        if (potion_has_thrown_effect(o_ptr))
            return true;
    }

    return false;
}

/*
 * Whether a quick-throw affordance should be offered at all: either a
 * quick-throwable dagger is available on the Harness, or Alchemy allows
 * throwing a carried potion.
 */
bool player_quick_throw_available(void)
{
    return player_quick_throw_harness_slot() >= 0
        || player_has_throwable_potion();
}

static bool player_shield_counts_for_weapon_mode(int mode,
    const object_type* o_ptr)
{
    if (!o_ptr || !o_ptr->k_idx || o_ptr->tval != TV_SHIELD)
        return false;
    if (mode == PLAYER_ACTIVE_WEAPON_MELEE)
        return true;

    return player_active_weapon_mode_is_ranged(mode)
        && o_ptr->sval == SV_ROUND_SHIELD
        && p_ptr->active_ability[S_ARC][ARC_POINT_BLANK];
}

bool player_shield_counts_for_active_weapon(const object_type* o_ptr)
{
    return player_shield_counts_for_weapon_mode(
        player_active_weapon_mode(), o_ptr);
}

bool player_weapon_slot_combat_bonuses_active_for_mode(int mode, int slot,
    const object_type* o_ptr)
{
    if (!o_ptr || !o_ptr->k_idx)
        return false;

    mode = normalize_active_weapon_mode(mode);

    switch (slot)
    {
    case INVEN_WIELD:
        return mode == PLAYER_ACTIVE_WEAPON_MELEE
            || (player_active_weapon_mode_is_ranged(mode)
                && o_ptr->pickup_slot == PICKUP_SLOT_ACTIVE_THROWING);
    case INVEN_BOW:
        return player_active_weapon_mode_is_ranged(mode)
            && player_active_weapon_kind() == PLAYER_ACTIVE_WEAPON_KIND_BOW;
    case INVEN_ARM:
        if (o_ptr->tval == TV_SHIELD)
            return player_shield_counts_for_weapon_mode(mode, o_ptr);
        return mode == PLAYER_ACTIVE_WEAPON_MELEE;
    case INVEN_QUIVER1:
        return false;
    case INVEN_BELT:
    case INVEN_STAFF:
    case INVEN_HORN:
        return false;
    default:
        return true;
    }
}

bool player_weapon_slot_combat_bonuses_active(int slot,
    const object_type* o_ptr)
{
    return player_weapon_slot_combat_bonuses_active_for_mode(
        player_active_weapon_mode(), slot, o_ptr);
}

bool player_equipment_slot_is_active(int slot)
{
    int mode = player_active_weapon_mode();

    if (slot < INVEN_WIELD || slot >= INVEN_TOTAL)
        return false;

    switch (slot)
    {
    case INVEN_WIELD:
        return mode == PLAYER_ACTIVE_WEAPON_MELEE
            || (mode == PLAYER_ACTIVE_WEAPON_RANGED_1
                && player_active_weapon_kind()
                    == PLAYER_ACTIVE_WEAPON_KIND_THROWING);
    case INVEN_BOW:
        return mode == PLAYER_ACTIVE_WEAPON_RANGED_1
            && player_active_weapon_kind() == PLAYER_ACTIVE_WEAPON_KIND_BOW;
    case INVEN_QUIVER1:
        return mode == PLAYER_ACTIVE_WEAPON_RANGED_1
            && player_active_weapon_kind() == PLAYER_ACTIVE_WEAPON_KIND_BOW
            && inventory_slot_is_quivered_arrow(slot);
    case INVEN_ARM:
        return inventory[slot].k_idx
            ? player_weapon_slot_combat_bonuses_active(slot, &inventory[slot])
            : mode == PLAYER_ACTIVE_WEAPON_MELEE;
    case INVEN_BELT:
    case INVEN_STAFF:
    case INVEN_HORN:
        return false;
    default:
        return true;
    }
}

static void player_polearm_switch_attack(void)
{
    object_type* o_ptr = &inventory[INVEN_WIELD];
    u32b f1 = 0, f2 = 0, f3 = 0;
    char o_name[80];

    if (!p_ptr->active_ability[S_MEL][MEL_POLEARMS])
        return;
    if (!o_ptr->k_idx)
        return;
    if (!p_ptr->focused || p_ptr->truce || p_ptr->confused || p_ptr->afraid
        || p_ptr->entranced || p_ptr->stun > 100)
    {
        return;
    }

    object_flags(o_ptr, &f1, &f2, &f3);
    if (!(f3 & TR3_POLEARM))
        return;

    object_desc(o_name, sizeof(o_name), o_ptr, false, 0);

    for (int d = 0; d < 8; d++)
    {
        int y = p_ptr->py + ddy_ddd[d];
        int x = p_ptr->px + ddx_ddd[d];
        int m_idx;
        monster_type* m_ptr;
        char m_name[80];

        if (!in_bounds(y, x))
            continue;

        m_idx = cave_m_idx[y][x];
        if (m_idx <= 0)
            continue;

        m_ptr = &mon_list[m_idx];
        if (!m_ptr->ml)
            continue;
        if (forgo_attacking_unwary && m_ptr->alertness < ALERTNESS_ALERT)
            continue;

        monster_desc(m_name, sizeof(m_name), m_ptr, 0);
        if (valorous_oath_auto_attack_safety && chosen_oath(OATH_VALOROUS)
            && !oath_invalid(OATH_VALOROUS)
            && (m_ptr->stance == STANCE_FLEEING))
        {
            msg_format("As you ready your %s, %s comes into reach, but you hold back.", o_name, m_name);
            return;
        }

        msg_format("As you ready your %s, %s comes into reach.", o_name, m_name);
        py_attack_aux(y, x, ATT_POLEARM);
        return;
    }
}

bool player_set_active_weapon_mode(int mode, bool confirm, bool take_turn)
{
    int old_mode = player_active_weapon_mode();
    int old_kind = player_active_weapon_kind();
    int new_kind;
    bool needs_turn;
    bool free_switch;

    mode = normalize_active_weapon_mode(mode);
    if (old_mode == mode)
        return true;

    new_kind = active_weapon_kind_for_mode(mode);
    needs_turn = player_switch_needs_turn(old_mode, mode);
    free_switch = player_active_weapon_change_is_free(old_kind, new_kind);

    if (confirm && needs_turn && !free_switch && active_weapon_switch_confirm
        && !confirm_active_weapon_switch(mode))
    {
        return false;
    }

    p_ptr->active_weapon_mode = (byte)mode;
    if (player_active_weapon_mode_is_ranged(mode))
        last_ranged_weapon_mode = (byte)mode;

    if (mode == PLAYER_ACTIVE_WEAPON_MELEE)
        msg_print("Your active weapon is now your melee weapon.");
    else
        msg_print("Your active weapon is now your ranged weapon with the quiver.");

    log_info("Active weapon changed: %s -> %s",
        active_weapon_mode_log_name(old_mode), active_weapon_mode_log_name(mode));

    mark_active_weapon_changed();
    update_stuff();

    if (mode == PLAYER_ACTIVE_WEAPON_MELEE
        && player_active_weapon_mode_is_ranged(old_mode))
    {
        player_polearm_switch_attack();
    }

    if (take_turn && needs_turn && !free_switch)
    {
        p_ptr->energy_use = 100;
        p_ptr->previous_action[0]
            = (mode == PLAYER_ACTIVE_WEAPON_MELEE)
            ? ACTION_READY_MELEE : ACTION_MISC;
    }
    else if (take_turn && free_switch)
    {
        p_ptr->energy_use = 0;
        p_ptr->previous_action[0] = ACTION_NOTHING;
        player_active_weapon_free_change_commit();
    }

    return true;
}

static void player_select_ranged_quiver_mode(int mode)
{
    int old_mode = player_active_weapon_mode();

    mode = normalize_active_weapon_mode(mode);
    if (!player_active_weapon_mode_is_ranged(mode))
        return;

    if (player_active_weapon_mode_is_ranged(old_mode))
    {
        (void)player_set_active_weapon_mode(mode, false, false);
        return;
    }

    if (last_ranged_weapon_mode == mode)
        return;

    last_ranged_weapon_mode = (byte)mode;
    msg_print("Your ranged weapon will use the quiver.");
    p_ptr->redraw |= PR_ARC | PR_QUIVER;
    p_ptr->window |= PW_PLAYER_0;
    handle_stuff();
}

static bool active_weapon_same_physical_item(const object_type* a,
    const object_type* b)
{
    if (!a || !b || !a->k_idx || !b->k_idx)
        return false;

    return a->k_idx == b->k_idx && a->name1 == b->name1
        && object_ego_prefix(a) == object_ego_prefix(b)
        && object_ego_suffix(a) == object_ego_suffix(b)
        && a->weight == b->weight && a->att == b->att && a->evn == b->evn
        && a->dd == b->dd && a->ds == b->ds && a->pd == b->pd
        && a->ps == b->ps && a->pval == b->pval
        && memcmp(a->stat_bonus, b->stat_bonus, sizeof(a->stat_bonus)) == 0
        && memcmp(a->skill_bonus, b->skill_bonus, sizeof(a->skill_bonus)) == 0;
}

static bool active_weapon_is_one_handed(const object_type* o_ptr)
{
    u32b f1 = 0, f2 = 0, f3 = 0;

    if (!object_is_melee_combat_weapon(o_ptr))
        return false;

    object_flags(o_ptr, &f1, &f2, &f3);
    return !(f3 & TR3_TWO_HANDED);
}

static bool active_weapon_shield_candidate(const object_type* o_ptr)
{
    if (!o_ptr || !o_ptr->k_idx || o_ptr->tval != TV_SHIELD)
        return false;
    if (object_has_broken_prefix(o_ptr))
        return false;

    return inventory_limit_group_for_object(o_ptr) == INV_LIMIT_HARNESS;
}

static bool active_weapon_wield_to_slot(const object_type* source, int item,
    int slot, bool full_stack)
{
    object_type wanted;
    bool wielded;

    if (!source || !source->k_idx || slot < INVEN_WIELD
        || slot >= INVEN_TOTAL)
    {
        return false;
    }

    object_copy(&wanted, source);
    wielded = full_stack
        ? do_cmd_wield_stack_to_slot((object_type*)source, item, slot)
        : do_cmd_wield_to_slot((object_type*)source, item, slot);

    return wielded
        && active_weapon_same_physical_item(&wanted, &inventory[slot])
        && (!full_stack || wanted.number == inventory[slot].number);
}

static bool active_weapon_offer_shield(void)
{
    int capacity = MAX(player_pack_entry_count(), 1);
    object_choice_entry* entries = mem_alloc_array(capacity,
        object_choice_entry);
    int* items = mem_alloc_array(capacity, int);
    int count = 0;
    int selected = -1;
    char shield_name[120];
    char prompt[180];

    if (!active_weapon_is_one_handed(&inventory[INVEN_WIELD]))
        goto no_shield;
    if (inventory[INVEN_ARM].k_idx
        && inventory[INVEN_ARM].tval == TV_SHIELD)
    {
        goto no_shield;
    }
    if (inventory[INVEN_ARM].k_idx && cursed_p(&inventory[INVEN_ARM]))
        goto no_shield;
    if (c_info[p_ptr->pcharacter].flags_u & UNQ_MEL_MAEDHROS)
        goto no_shield;

    for (int ordinal = 0; ordinal < player_pack_entry_count(); ordinal++)
    {
        char label[OBJECT_CHOICE_LABEL_LEN];
        char key;
        int item = player_pack_entry_handle_at(ordinal);
        object_type* o_ptr = player_inventory_object(item);

        if (!active_weapon_shield_candidate(o_ptr))
            continue;

        key = active_weapon_choice_key(count);
        if (key)
            strnfmt(label, sizeof(label), "%c)", key);
        else
            label[0] = '\0';
        object_choice_entry_make(&entries[count], item, o_ptr, label,
            NULL);
        items[count++] = item;
    }

    if (count <= 0)
        goto no_shield;

    if (count == 1)
    {
        object_desc(shield_name, sizeof(shield_name),
            player_inventory_object(items[0]), true, 3);
        strnfmt(prompt, sizeof(prompt), "Add %s to your active weapon? ",
            shield_name);
        if (!get_check(prompt))
            goto no_shield;
        selected = 0;
    }
    else if (!object_choice_overlay("Add a shield?",
                 "Choose a shield to equip, or cancel to leave your off hand unchanged.",
                 entries, count, 0, &selected))
    {
        goto no_shield;
    }

    if (selected < 0 || selected >= count)
        goto no_shield;

    {
        bool wielded = active_weapon_wield_to_slot(
            player_inventory_object(items[selected]), items[selected],
            INVEN_ARM, false);
        entries = mem_free(entries);
        items = mem_free(items);
        return wielded;
    }

no_shield:
    entries = mem_free(entries);
    items = mem_free(items);
    return false;
}

static void apply_active_weapon_choice(const active_weapon_choice* choice)
{
    int old_mode;
    int old_kind;
    bool free_choice;
    bool wielded_item = false;
    bool wielded_shield = false;
    bool role_changed;
    bool cleared_throwing_marker = false;
    bool arrow_changed = false;

    if (!choice || !choice->o_ptr)
        return;

    old_mode = player_active_weapon_mode();
    old_kind = player_active_weapon_kind();
    free_choice = player_active_weapon_change_is_free(old_kind, choice->kind);
    role_changed = old_kind != choice->kind;

    /* Slot 24 is the active hand for both melee and throwing.  Clear the old
     * role marker before a replacement is stowed so it cannot follow an
     * inactive weapon back into the Harness. */
    if (inventory[INVEN_WIELD].pickup_slot
        == PICKUP_SLOT_ACTIVE_THROWING)
    {
        inventory[INVEN_WIELD].pickup_slot = -1;
        cleared_throwing_marker = true;
    }

    if (choice->item != choice->target_slot)
    {
        wielded_item = active_weapon_wield_to_slot(choice->o_ptr,
            choice->item, choice->target_slot,
            choice->kind == PLAYER_ACTIVE_WEAPON_KIND_THROWING);
        if (!wielded_item)
        {
            if (cleared_throwing_marker && inventory[INVEN_WIELD].k_idx)
                inventory[INVEN_WIELD].pickup_slot
                    = PICKUP_SLOT_ACTIVE_THROWING;
            return;
        }
    }

    if (choice->kind == PLAYER_ACTIVE_WEAPON_KIND_THROWING)
        inventory[INVEN_WIELD].pickup_slot = PICKUP_SLOT_ACTIVE_THROWING;
    else if (inventory[INVEN_WIELD].pickup_slot
        == PICKUP_SLOT_ACTIVE_THROWING)
    {
        inventory[INVEN_WIELD].pickup_slot = -1;
    }

    if (choice->kind == PLAYER_ACTIVE_WEAPON_KIND_BOW
        && choice->arrow_item >= 0)
    {
        int old_arrow = player_quiver_selected_arrow_slot();

        if (!player_quiver_select_arrow(choice->arrow_item))
            return;
        arrow_changed = old_arrow != player_quiver_selected_arrow_slot();
        if (arrow_changed)
        {
            object_type* arrow = player_quiver_arrow_object(
                player_quiver_selected_arrow_slot());
            char arrow_name[120];

            object_desc(arrow_name, sizeof(arrow_name), arrow, true, 3);
            msg_format("Your active arrows are now %s.", arrow_name);
        }
    }

    if (role_changed && !wielded_item)
    {
        p_ptr->energy_use = 100;
        p_ptr->previous_action[0] = ACTION_MISC;
        mark_active_weapon_changed();
    }

    if (choice->mode == PLAYER_ACTIVE_WEAPON_MELEE)
        wielded_shield = active_weapon_offer_shield();

    if (old_mode == choice->mode)
    {
        if (arrow_changed && !wielded_item && !role_changed
            && !wielded_shield)
        {
            p_ptr->energy_use = 0;
            p_ptr->previous_action[0] = ACTION_NOTHING;
            handle_stuff();
            return;
        }
        if ((wielded_item || role_changed) && !wielded_shield && free_choice)
        {
            p_ptr->energy_use = 0;
            p_ptr->previous_action[0] = ACTION_NOTHING;
            player_active_weapon_free_change_commit();
        }
        return;
    }

    if (wielded_item || wielded_shield)
    {
        int wield_energy = p_ptr->energy_use;

        (void)player_set_active_weapon_mode(choice->mode, false, false);
        if (free_choice && !wielded_shield)
        {
            p_ptr->energy_use = 0;
            p_ptr->previous_action[0] = ACTION_NOTHING;
            player_active_weapon_free_change_commit();
        }
        else
        {
            p_ptr->energy_use = wield_energy;
            p_ptr->previous_action[0]
                = (choice->mode == PLAYER_ACTIVE_WEAPON_MELEE
                    && player_active_weapon_mode_is_ranged(old_mode))
                ? ACTION_READY_MELEE : ACTION_MISC;
        }
        return;
    }

    (void)player_set_active_weapon_mode(choice->mode, false, true);
}

void do_cmd_toggle_active_weapon(void)
{
    active_weapon_choice choice;

    if (!choose_active_weapon(&choice))
        return;

    apply_active_weapon_choice(&choice);
}

void player_queue_active_weapon_mode(int mode)
{
    mode = normalize_active_weapon_mode(mode);
    pending_active_weapon_mode = (byte)mode;
    pending_ranged_quiver_only = false;
    if (player_active_weapon_mode_is_ranged(mode))
        last_ranged_weapon_mode = (byte)mode;
}

void player_queue_ranged_quiver_mode(int mode)
{
    mode = normalize_active_weapon_mode(mode);
    if (!player_active_weapon_mode_is_ranged(mode))
        return;

    pending_active_weapon_mode = (byte)mode;
    pending_ranged_quiver_only = true;
}

void do_cmd_pending_active_weapon_mode(void)
{
    int mode;
    bool quiver_only;

    if (pending_active_weapon_mode == PLAYER_ACTIVE_WEAPON_NONE)
        return;

    mode = normalize_active_weapon_mode(pending_active_weapon_mode);
    quiver_only = pending_ranged_quiver_only;
    pending_active_weapon_mode = PLAYER_ACTIVE_WEAPON_NONE;
    pending_ranged_quiver_only = false;

    if (quiver_only)
        player_select_ranged_quiver_mode(mode);
    else
        (void)player_set_active_weapon_mode(mode, true, true);
}
