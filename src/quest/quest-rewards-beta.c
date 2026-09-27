#include "angband.h"
#include "externs.h"
#include "quest/quest-rewards-beta.h"
#include "quest/quest-runtime.h"
#include "metarun.h"
#include "blitz.h"

/* Slots 1/2 belong to the Great Hunt's persistent progress. */
enum { GIFT_MANDOS = 0, GIFT_NIENNA = 3, GIFT_VARDA = 4 };

bool quest_special_ability_active(int ability)
{
    int quest;
    if (!p_ptr || !quest_rewards_enabled()) return false;
    switch (ability) {
    case SPC_OROME_WRAITH: quest = QUEST_ID_OROME_DRAGONS; break;
    case SPC_HUNTSMAN_RHYTHM: quest = QUEST_ID_OROME_GREAT_HUNT; break;
    case SPC_TULKAS_WRATH: quest = QUEST_ID_TULKAS_MORGOTH; break;
    case SPC_QUEEN_STARS: quest = QUEST_ID_VARDA_SHADOW; break;
    default: return false;
    }
    return (quest_enabled(quest)
        || (ability == SPC_OROME_WRAITH && quest_enabled(QUEST_ID_OROME)))
        && p_ptr->active_ability[S_SPC][ability];
}

static bool lineage_active(int quest)
{
    return quest_lineage_enabled() && quest_enabled(quest)
        && !quest_debug_sandbox() && !run_mode_is_blitz()
        && metarun_current() != NULL;
}

static void store_gift(int slot, byte value)
{
    metarun *current = metarun_current_mutable();
    if (!current) return;
    metar.quest_reserved[slot] = value;
    current->quest_reserved[slot] = value;
    save_metaruns();
}

void quest_beta_apply_reward(int quest)
{
    if (!lineage_active(quest)) return;
    switch (quest) {
    case QUEST_ID_MANDOS_BETRAYER:
        store_gift(GIFT_MANDOS, 1);
        msg_print("Mandos grants this lineage one reprieve from death.");
        break;
    case QUEST_ID_NIENA_PACIFIST:
        store_gift(GIFT_NIENNA, 1);
        msg_print("Nienna grants this lineage one grace against a future curse.");
        break;
    case QUEST_ID_VARDA_UNGOLIANT:
        store_gift(GIFT_VARDA, 1);
        msg_print("Varda's light will accompany this lineage's future ventures.");
        break;
    }
}

bool quest_beta_resurrect(void)
{
    if (p_ptr->chp > 0 || !lineage_active(QUEST_ID_MANDOS_BETRAYER)
        || !metar.quest_reserved[GIFT_MANDOS]) return false;
    store_gift(GIFT_MANDOS, metar.quest_reserved[GIFT_MANDOS] - 1);
    p_ptr->chp = MAX(1, p_ptr->mhp);
    p_ptr->chp_frac = 0;
    p_ptr->energy += 100;
    set_blind(0); set_confused(0); set_poisoned(0); set_afraid(0);
    set_entranced(0); set_image(0); set_stun(0); set_cut(0); set_slow(0);
    p_ptr->redraw |= PR_HP;
    msg_print("Mandos recalls your spirit from the threshold of his halls!");
    return true;
}

bool quest_beta_cleanse_curse(void)
{
    if (!lineage_active(QUEST_ID_NIENA_PACIFIST)
        || !metar.quest_reserved[GIFT_NIENNA]) return false;
    store_gift(GIFT_NIENNA, metar.quest_reserved[GIFT_NIENNA] - 1);
    msg_print("Nienna's grace washes away the gathering curse.");
    return true;
}

void quest_beta_birth(void)
{
    int choices[3], count = 0;
    char descriptions[3][120];
    cptr labels[3];
    object_type gift;
    if (character_loaded || !lineage_active(QUEST_ID_VARDA_UNGOLIANT)
        || !metar.quest_reserved[GIFT_VARDA]) return;
    for (int i = 1; i < z_info->art_max; i++) {
        artefact_type *a = &a_info[i];
        if (!a->name[0] || !a->tval || a->cur_num
            || !(a->flags2 & (TR2_LIGHT | TR2_RADIANCE))
            || (a->flags2 & TR2_DARKNESS) || (a->flags4 & TR4_UNLIGHT)
            || (a->flags3 & TR3_LIGHT_CURSE)
            || (valar_reserved_artifacts && valar_reserved_artifacts[i])
            || !lookup_kind(a->tval, a->sval)) continue;
        int slot = count < 3 ? count : rand_int(count + 1);
        count++;
        if (slot < 3) choices[slot] = i;
    }
    if (!count) return;
    count = MIN(count, 3);
    for (int i = 0; i < count; i++) {
        artefact_type *a = &a_info[choices[i]];
        object_prep(&gift, lookup_kind(a->tval, a->sval));
        gift.name1 = choices[i];
        gift.ident |= IDENT_KNOWN;
        object_desc(descriptions[i], sizeof(descriptions[i]), &gift, true, 0);
        labels[i] = descriptions[i];
    }
    cptr words[] = {"Varda remembers your lineage's victory over Ungoliant.",
        "Choose a radiant relic to carry into your new journey."};
    int chosen = quest_reward_book_choice("Varda's Starlight", words, 2,
        "Choose your radiant gift:", choices, labels, NULL, count, 0, desc_art_fake);
    if (chosen <= 0) return;
    artefact_type *a = &a_info[chosen];
    object_prep(&gift, lookup_kind(a->tval, a->sval));
    gift.name1 = chosen;
    apply_magic(&gift, -1, true, true, true, true);
    object_aware(&gift);
    object_known(&gift);
    if (inven_carry(&gift, true) < 0) {
        a->cur_num = 0;
        return;
    }
    a->cur_num = 1;
    if (valar_reserved_artifacts) valar_reserved_artifacts[chosen] = true;
    msg_print("A radiant relic bears Varda's blessing into your new journey.");
}

static bool rhythm_active(void)
{
    if (quest_special_ability_active(SPC_HUNTSMAN_RHYTHM)) return true;
    return false;
}

void quest_beta_bow_hit(int damage)
{
    if (!rhythm_active()) return;
    if (p_ptr->orome_spear_ready) {
        p_ptr->orome_spear_ready = 0;
        p_ptr->orome_bow_hit_streak = 0;
    }
    if (damage > 0) {
        if (p_ptr->orome_bow_hit_streak < 2) p_ptr->orome_bow_hit_streak++;
        if (p_ptr->orome_bow_hit_streak == 2) p_ptr->orome_spear_ready = 1;
    } else p_ptr->orome_bow_hit_streak = 0;
}

void quest_beta_bow_miss(void)
{
    if (rhythm_active()) p_ptr->orome_bow_hit_streak = 0;
}

int quest_beta_melee_damage(const object_type *weapon, int damage)
{
    if (!rhythm_active()) return damage;
    if (p_ptr->orome_spear_ready && weapon->tval == TV_POLEARM
        && (weapon->sval == SV_SPEAR || weapon->sval == SV_GREAT_SPEAR)) {
        damage *= 2;
        msg_print("Your spear strike follows the rhythm of the hunt!");
    }
    p_ptr->orome_spear_ready = p_ptr->orome_bow_hit_streak = 0;
    return damage;
}

void quest_beta_melee_miss(void)
{
    if (rhythm_active()) p_ptr->orome_bow_hit_streak = 0;
}
