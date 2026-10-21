#include "angband.h"
#include "achievements.h"
#include "achievements-lore.h"
#include "externs.h"

/* Character and race identities follow the stable template GUIDs. */
static bool hero(u32b hi, u32b lo)
{
    return p_ptr && z_info && c_info && p_ptr->pcharacter < z_info->c_max
        && c_info[p_ptr->pcharacter].guid.hi == hi
        && c_info[p_ptr->pcharacter].guid.lo == lo;
}

static bool race_is(u32b hi, u32b lo)
{
    return p_ptr && z_info && p_info && p_ptr->prace < z_info->p_max
        && p_info[p_ptr->prace].guid.hi == hi
        && p_info[p_ptr->prace].guid.lo == lo;
}

static bool is_artefact(const object_type* object, int artefact)
{
    return object && object->k_idx && object->number > 0
        && object->name1 == artefact;
}

static bool carrying(int artefact)
{
    for (int i = 0; i < INVEN_TOTAL; ++i)
        if (is_artefact(&inventory[i], artefact)) return true;
    for (int i = 0; i < player_carried_extra_entry_count(); ++i)
        if (is_artefact(player_carried_extra_entry_at(i), artefact)) return true;
    return false;
}

bool sil_achievement_lore_has_mairon_ring(void)
{
    return is_artefact(&inventory[INVEN_LEFT], ART_MAIRON)
        || is_artefact(&inventory[INVEN_RIGHT], ART_MAIRON);
}

static bool beside_chasm(int y, int x)
{
    for (int i = 0; i < 8; ++i)
    {
        int yy = y + ddy_ddd[i], xx = x + ddx_ddd[i];
        if (in_bounds(yy, xx) && cave_feat[yy][xx] == FEAT_CHASM) return true;
    }
    return false;
}

/* Only the synchronous weapon damage call owns this scope. Poison, spells,
 * traps, knockback falls and later damage cannot borrow an equipped weapon. */
static struct {
    int target, artefact, secondary;
    bool melee, unarmed;
} attack;

static bool attack_uses(int artefact)
{
    return attack.artefact == artefact || attack.secondary == artefact;
}

void sil_achievement_lore_attack_add_weapon(const object_type* weapon)
{
    if (attack.target && weapon && weapon->k_idx)
        attack.secondary = weapon->name1;
}

void sil_achievement_lore_attack_end(void)
{
    memset(&attack, 0, sizeof(attack));
}

void sil_achievement_lore_reset(void)
{
    p_ptr->achievement_lore_flags = 0;
    p_ptr->achievement_ringil_wounds = 0;
    p_ptr->achievement_glorfindel_depth = -1;
    memset(p_ptr->achievement_artefact_rings, 0,
        sizeof(p_ptr->achievement_artefact_rings));
    sil_achievement_lore_attack_end();
}

void sil_achievement_lore_attack_begin(int m_idx, const object_type* weapon,
    int damage, bool melee)
{
    sil_achievement_lore_attack_end();
    if (!sil_achievements_eligible() || damage <= 0) return;
    attack.target = m_idx;
    attack.melee = melee;
    attack.unarmed = melee && (!weapon || !weapon->k_idx);
    attack.artefact = weapon && weapon->k_idx ? weapon->name1 : 0;
}

void sil_achievement_lore_on_hit(int m_idx, int damage, int who)
{
    if (!sil_achievements_eligible() || who >= 0 || damage <= 0
        || attack.target != m_idx || m_idx <= 0 || m_idx >= mon_max) return;
    if (mon_list[m_idx].r_idx == R_IDX_MORGOTH
        && attack_uses(ART_RINGIL)
        && p_ptr->achievement_ringil_wounds < 7)
        ++p_ptr->achievement_ringil_wounds;
}

void sil_achievement_lore_on_kill(int m_idx, int who)
{
    if (!sil_achievements_eligible() || who >= 0
        || m_idx <= 0 || m_idx >= mon_max) return;
    monster_type* monster = &mon_list[m_idx];
    int race = monster->r_idx;
    if (race <= 0 || race >= z_info->r_max
        || (r_info[race].flags1 & RF1_PEACEFUL)) return;

    if (r_info[race].flags1 & RF1_UNIQUE)
    {
        if (race == R_IDX_TEVILDO)
            p_ptr->achievement_lore_flags |= SIL_ACH_LORE_TEVILDO;
        if (race == R_IDX_GORTHAUR)
            p_ptr->achievement_lore_flags |= SIL_ACH_LORE_GORTHAUR;
        if ((p_ptr->achievement_lore_flags
                & (SIL_ACH_LORE_TEVILDO | SIL_ACH_LORE_GORTHAUR))
            == (SIL_ACH_LORE_TEVILDO | SIL_ACH_LORE_GORTHAUR))
            sil_achievements_on_event(SIL_ACH_THE_CAT_AND_THE_NECROMANCER);
        if ((r_info[race].flags3 & RF3_RAUKO)
            && strstr(r_name + r_info[race].name, "Balrog"))
        {
            sil_achievements_on_balrog_killed(race);
            if (hero(0x0ed53377, 0xc70064d5)
                && distance(p_ptr->py, p_ptr->px, monster->fy, monster->fx) <= 1
                && beside_chasm(p_ptr->py, p_ptr->px))
                p_ptr->achievement_glorfindel_depth = p_ptr->depth;
        }
    }

    if (attack.target != m_idx) return;
    if (attack.unarmed && hero(0x7165372d, 0xbc78071f)
        && (race == R_IDX_WEREWOLF || race == R_IDX_GREATER_WEREWOLF
            || race == R_IDX_DRAUGLUIN))
        sil_achievements_on_event(SIL_ACH_THE_KING_WHO_KEPT_HIS_OATH);
    if (race == R_IDX_MORGOTH && p_ptr->morgoth_second_wind
        && attack_uses(ART_GURTHANG) && hero(0xa1860c8b, 0x017a6daa))
        sil_achievements_on_event(SIL_ACH_HIS_DEATH_AND_FINAL_END);
    if (race == R_IDX_GLAURUNG && attack_uses(ART_AZAGHAL)
        && race_is(0xd64e250a, 0x46b743b9))
        sil_achievements_on_event(SIL_ACH_BELEGOST_REMEMBERS);
    if (race == R_IDX_SHELOB && attack_uses(ART_STING)
        && inventory[INVEN_LITE].k_idx
        && inventory[INVEN_LITE].tval == TV_LIGHT
        && inventory[INVEN_LITE].sval == SV_LIGHT_SILMARIL)
        sil_achievements_on_event(SIL_ACH_NO_PHIAL_REQUIRED);
    if (attack.artefact == ART_DAILIR && !attack.melee)
    {
        if (p_ptr->achievement_lore_flags & SIL_ACH_LORE_DAILIR_RECOVERED)
            sil_achievements_on_event(SIL_ACH_EVER_FOUND_AGAIN);
        p_ptr->achievement_lore_flags |= SIL_ACH_LORE_DAILIR_KILL;
    }
}

void sil_achievement_lore_on_recovered(int artefact)
{
    if (sil_achievements_eligible() && artefact == ART_DAILIR
        && (p_ptr->achievement_lore_flags & SIL_ACH_LORE_DAILIR_KILL))
        p_ptr->achievement_lore_flags |= SIL_ACH_LORE_DAILIR_RECOVERED;
}

void sil_achievement_lore_on_shatter(int silnum, const object_type* weapon)
{
    if (silnum == 2 && is_artefact(weapon, ART_ANGRIST))
        sil_achievements_on_event(SIL_ACH_NOT_THE_DOOM_OF_THE_SILMARILS);
}

void sil_achievement_lore_on_throne_entry(void)
{
    if (sil_achievements_eligible() && p_ptr->depth == MORGOTH_DEPTH
        && hero(0xada9da15, 0x7479fe7a)
        && is_artefact(&inventory[INVEN_OUTER], ART_THURINGWETHIL))
        sil_achievements_on_event(SIL_ACH_THURINGWETHIL_I_AM);
}

void sil_achievement_lore_on_flee(const monster_type* monster)
{
    if (!sil_achievements_eligible() || !monster || !monster->ml
        || player_active_weapon_mode() != PLAYER_ACTIVE_WEAPON_MELEE
        || !player_equipment_slot_is_active(INVEN_WIELD)
        || !player_equipment_slot_is_active(INVEN_ARM)
        || monster->r_idx <= 0 || monster->r_idx >= z_info->r_max
        || !(r_info[monster->r_idx].flags3 & RF3_ORC)
        || (r_info[monster->r_idx].flags1 & RF1_PEACEFUL)) return;
    if ((is_artefact(&inventory[INVEN_WIELD], ART_ORCRIST)
            && is_artefact(&inventory[INVEN_ARM], ART_GLAMDRING))
        || (is_artefact(&inventory[INVEN_WIELD], ART_GLAMDRING)
            && is_artefact(&inventory[INVEN_ARM], ART_ORCRIST)))
        sil_achievements_on_event(SIL_ACH_BITER_AND_BEATER);
}

void sil_achievement_lore_on_travel(int from_depth, int to_depth)
{
    if (from_depth != to_depth
        && p_ptr->achievement_glorfindel_depth == from_depth)
    {
        if (hero(0x0ed53377, 0xc70064d5))
            sil_achievements_on_event(SIL_ACH_THE_GOLDEN_FLOWER_STANDS);
        p_ptr->achievement_glorfindel_depth = -1;
    }
}

void sil_achievement_lore_on_escape(int silmarils)
{
    if (!sil_achievements_eligible() || p_ptr->depth != 0 || silmarils < 1) return;
    if (p_ptr->achievement_ringil_wounds >= 7 && !p_ptr->morgoth_slain)
        sil_achievements_on_event(SIL_ACH_SEVEN_TIMES_HE_CRIED);
    if (carrying(ART_MORGOTH_0))
        sil_achievements_on_event(SIL_ACH_HEAVY_IS_THE_STOLEN_CROWN);
    if (hero(0xb1902182, 0x0c53b215))
    {
        if (silmarils >= 3) sil_achievements_on_event(SIL_ACH_THESE_THINGS_ARE_MINE);
        if (p_ptr->oath_type == OATH_MERCY && !oath_invalid(OATH_MERCY))
            sil_achievements_on_event(SIL_ACH_THE_OATH_HE_SHOULD_HAVE_SWORN);
    }
    if (race_is(0xd28645bb, 0x6bf8409d)
        && carrying(ART_BARAHIR) && carrying(ART_NARSIL))
        sil_achievements_on_event(SIL_ACH_HEIRLOOMS_OF_THE_HIDDEN_KING);
}

void sil_achievement_lore_on_forged(const object_type* object, bool custom)
{
    if (!sil_achievements_eligible() || !custom
        || !hero(0x92088065, 0xa83f4ad1)
        || !object || !object->k_idx || object->tval != TV_RING
        || !object->name1) return;
    u32b f1, f2, f3, f4;
    object_flags4(object, &f1, &f2, &f3, &f4);
    if (cursed_p(object) || (f3 & (TR3_LIGHT_CURSE | TR3_HEAVY_CURSE | TR3_PERMA_CURSE))
        || (f4 & TR4_EVIL_ITEM)) return;
    for (int i = 0; i < 3; ++i)
        if (p_ptr->achievement_artefact_rings[i] == object->name1) return;
    for (int i = 0; i < 3; ++i)
        if (!p_ptr->achievement_artefact_rings[i])
        {
            p_ptr->achievement_artefact_rings[i] = object->name1;
            if (i == 2) sil_achievements_on_event(SIL_ACH_THREE_UNTOUCHED);
            break;
        }
}

void sil_achievement_lore_on_curse_lifted(const object_type* source,
    const object_type* equipment)
{
    if (hero(0xb1902182, 0x0c53b215)
        && is_artefact(source, ART_FINWE_LAMP)
        && is_artefact(equipment, ART_FEANOR))
        sil_achievements_on_event(SIL_ACH_HIS_FATHERS_LIGHT);
}
