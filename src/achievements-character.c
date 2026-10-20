#include "angband.h"
#include "achievements.h"
#include "achievements-character.h"
#include "externs.h"

/* These events use the same character flags as the mechanics themselves. */
static bool has_unique(u32b flag)
{
    return p_ptr && c_info && (c_info[p_ptr->pcharacter].flags_u & flag);
}

void sil_achievement_character_on_forged(object_type* object)
{
    u32b f1, f2, f3;
    if (!sil_achievements_eligible() || !object || !object->k_idx) return;
    object_flags(object, &f1, &f2, &f3);
    if (has_unique(UNQ_SMT_TELCHAR) && (f1 & TR1_SHARPNESS2)
        && (object->tval == TV_SWORD || object->tval == TV_HAFTED
            || object->tval == TV_POLEARM))
        object->ident |= IDENT_ACH_TELCHAR_SHARPNESS;
    if (has_unique(UNQ_SMT_EOL) && object->tval == TV_SOFT_ARMOR
        && object->sval == SV_ARMOUR_OF_GALVORN)
        object->ident |= IDENT_ACH_EOL_GALVORN;
}

void sil_achievement_character_on_smithing_completed(
    const object_type* object, bool custom_artefact, int forge_uses_before,
    int forge_uses_paid, int mithril_paid)
{
    u32b f1, f2, f3;
    if (!sil_achievements_eligible() || !object || !object->k_idx) return;
    if (has_unique(UNQ_SMT_FEANOR) && custom_artefact
        && forge_uses_before == 1 && forge_uses_paid == 1)
        sil_achievements_on_event(SIL_ACH_A_SINGLE_SPARK);
    object_flags(object, &f1, &f2, &f3);
    if (has_unique(UNQ_SMT_GAMIL) && (f3 & TR3_MITHRIL)
        && (object->ident & IDENT_CANT_MELT)
        && forge_uses_paid >= 3 && mithril_paid == 0)
        sil_achievements_on_event(SIL_ACH_OUT_OF_THIN_ORE);
}

void sil_achievement_character_on_player_turn(void)
{
    const object_type* armour = &inventory[INVEN_BODY];
    if (!sil_achievements_eligible() || !has_unique(UNQ_SMT_EOL)) return;
    if (armour->k_idx && armour->tval == TV_SOFT_ARMOR
        && armour->sval == SV_ARMOUR_OF_GALVORN
        && (armour->ident & IDENT_ACH_EOL_GALVORN))
        sil_achievements_on_event(SIL_ACH_CLAD_IN_THE_NIGHT);
}

void sil_achievement_character_on_silmaril(const object_type* weapon)
{
    u32b f1, f2, f3;
    if (!sil_achievements_eligible() || !has_unique(UNQ_SMT_TELCHAR)
        || !weapon || !weapon->k_idx
        || !(weapon->ident & IDENT_ACH_TELCHAR_SHARPNESS)) return;
    object_flags(weapon, &f1, &f2, &f3);
    if (f1 & TR1_SHARPNESS2)
        sil_achievements_on_event(SIL_ACH_IRON_CLEAVER);
}

void sil_achievement_character_on_melee_kill(
    const object_type* weapon, int critical_dice)
{
    if (!sil_achievements_kill_eligible() || !has_unique(UNQ_MEL_MAEDHROS)
        || critical_dice <= 0 || weapon != &inventory[INVEN_WIELD]
        || !weapon->k_idx || inventory[INVEN_ARM].k_idx
        || !p_ptr->active_ability[S_MEL][MEL_CONTROL]
        || !(k_info[weapon->k_idx].flags3 & TR3_HAND_AND_A_HALF)) return;
    sil_achievements_on_event(SIL_ACH_ON_THE_OTHER_HAND);
}

void sil_achievement_character_on_rage_started(void)
{
    /* A new episode never reactivates a target from an earlier rage. */
    if (++p_ptr->achievement_rage_epoch == 0)
        ++p_ptr->achievement_rage_epoch;
}

void sil_achievement_character_on_entrancement_resisted(monster_type* attacker)
{
    if (!sil_achievements_eligible() || !has_unique(UNQ_WIL_TURIN)
        || !attacker || !attacker->r_idx || !p_ptr->rage) return;
    /* Old saves may already be raging without an episode identifier. A new
     * confirmed resistance establishes one without marking any other actor. */
    if (!p_ptr->achievement_rage_epoch)
        sil_achievement_character_on_rage_started();
    attacker->achievement_doom_rage_epoch = p_ptr->achievement_rage_epoch;
}

void sil_achievement_character_on_monster_killed(int m_idx, int who)
{
    if (!sil_achievements_eligible() || who >= 0
        || m_idx <= 0 || m_idx >= mon_max) return;
    /* Match the selected template's persistent GUID, never the editable player
     * name or a shared racial/ability flag. These are character.txt Q: values. */
    static const struct {
        guid64 hero;
        int victim;
        sil_achievement_id achievement;
    } deeds[] = {
        { { 0x40a8c0d5, 0xcdd81345 }, R_IDX_GOTHMOG, SIL_ACH_LORD_OF_THE_FOUNTAIN },
        { { 0xa1860c8b, 0x017a6daa }, R_IDX_GLAURUNG, SIL_ACH_MASTER_OF_DOOM },
        { { 0x372282fe, 0x5bf0b106 }, R_IDX_MAEGLIN, SIL_ACH_FOR_IDRIL_AND_EARENDIL },
        { { 0x2bca19ef, 0x728150b9 }, R_IDX_ANCALAGON, SIL_ACH_BEFORE_THE_RISING_OF_THE_SUN }
    };
    int race = mon_list[m_idx].r_idx;
    if (c_info && z_info && p_ptr->pcharacter < z_info->c_max
        && race > 0 && race < z_info->r_max
        && (r_info[race].flags1 & RF1_UNIQUE)
        && !(r_info[race].flags1 & RF1_PEACEFUL))
    {
        guid64 hero = c_info[p_ptr->pcharacter].guid;
        for (size_t i = 0; i < N_ELEMENTS(deeds); ++i)
            if (hero.hi == deeds[i].hero.hi && hero.lo == deeds[i].hero.lo
                && race == deeds[i].victim)
                sil_achievements_on_event(deeds[i].achievement);
    }
    if (!has_unique(UNQ_WIL_TURIN) || !p_ptr->rage
        || !p_ptr->achievement_rage_epoch) return;
    if (mon_list[m_idx].achievement_doom_rage_epoch
        == p_ptr->achievement_rage_epoch)
        sil_achievements_on_event(SIL_ACH_DOOM_CAN_WAIT);
}

static struct {
    bool active, paid, luthien_sleep, both_awarded;
    int first, second, voice;
    unsigned int affected;
} singing_scope;

static bool singing_scope_complete(void)
{
    bool both = singing_scope.active && singing_scope.paid
        && !singing_scope.both_awarded
        && singing_scope.affected == 3u
        && singing_scope.first != SNG_NOTHING
        && singing_scope.second != SNG_NOTHING
        && singing_scope.first != singing_scope.second
        && p_ptr->active_ability[S_SNG][SNG_WOVEN_THEMES]
        && sil_achievements_eligible();
    if (singing_scope.active && singing_scope.paid && singing_scope.luthien_sleep)
    {
        sil_achievements_on_event(SIL_ACH_A_DREAM_UPON_THE_DARK_LORD);
        singing_scope.luthien_sleep = false;
    }
    if (both && has_unique(UNQ_WOVEN_MASTER))
        sil_achievements_on_event(SIL_ACH_NEITHER_VOICE_DIMINISHED);
    if (both) singing_scope.both_awarded = true;
    return both;
}

void sil_achievement_character_song_begin(void)
{
    memset(&singing_scope, 0, sizeof(singing_scope));
    if (!sil_achievements_eligible() || p_ptr->energy_use <= 0
        || p_ptr->csp < 1 || p_ptr->song1 == SNG_NOTHING
        || p_ptr->song1 >= ABILITIES_MAX
        || !p_ptr->active_ability[S_SNG][p_ptr->song1]) return;
    singing_scope.active = true;
    singing_scope.first = p_ptr->song1;
    singing_scope.second = p_ptr->song2;
    singing_scope.voice = p_ptr->csp;
}

bool sil_achievement_character_song_effect(
    int song, int m_idx, int previous_alertness)
{
    monster_type* monster;
    if (!sil_achievements_eligible() || !singing_scope.active
        || m_idx <= 0 || m_idx >= mon_max) return false;
    monster = &mon_list[m_idx];
    if (!monster->r_idx || (r_info[monster->r_idx].flags1 & RF1_PEACEFUL))
        return false;
    if (song == SNG_LORIEN && has_unique(UNQ_SNG_LUT)
        && monster->r_idx == R_IDX_MORGOTH
        && previous_alertness >= ALERTNESS_UNWARY
        && monster->alertness < ALERTNESS_UNWARY)
        singing_scope.luthien_sleep = true;
    if (song == singing_scope.first) singing_scope.affected |= 1u;
    if (song == singing_scope.second) singing_scope.affected |= 2u;
    return singing_scope_complete();
}

bool sil_achievement_character_song_end(int cost)
{
    singing_scope.paid = cost >= 0 && singing_scope.voice >= 1
        && singing_scope.voice >= cost;
    return singing_scope_complete();
}
