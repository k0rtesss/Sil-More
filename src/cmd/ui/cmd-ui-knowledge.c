/* File: cmd-ui-knowledge.c */

/*
 * Copyright (c) 1997 Ben Harrison, James E. Wilson, Robert A. Koeneke
 *
 * This software may be copied and distributed for educational, research,
 * and not for profit purposes provided that this copyright and statement
 * are included in all such copies.  Other copyrights may also apply.
 */
#include "angband.h"
#include "platform-input.h"
#include "object/object-ui-select.h"
#include "player/player-abilities.h"
#include "player/player-bane.h"
#include "player/player-oaths.h"
#include "sound-config.h"
#include "platform-audio.h"

extern struct sound_config g_sound_config;
#include "fs/file.h"
#include "fs/path.h"
#include "log/log.h"
#include <ctype.h>
#include "h-define.h"
#include "metarun.h"
#include "score/score_artefact.h"
#include "score/score_guid.h"
#include "app/app-ui.h"
#include "ui/ui-information-scene.h"
#include "cmd-ui.h"

typedef struct knowledge_browser_state knowledge_browser_state;

#define BROWSER_ROWS 16

typedef struct monster_list_entry monster_list_entry;
/*
 * Structure for building monster "lists"
 */
struct monster_list_entry
{
    s16b r_idx; /* Monster race index */

    byte amount;
};

typedef struct object_list_entry object_list_entry;
struct object_list_entry
{
    enum
    {
        OBJ_NONE,
        OBJ_NORMAL,
        OBJ_SPECIAL
    } type;
    int idx;
    int e_idx;
    int tval, sval;
};

typedef struct supply_list_entry supply_list_entry;

struct supply_list_entry
{
    int item_idx;   /* First inventory slot containing this kind */
    int k_idx;      /* Object kind index */
    int total;      /* Total quantity across the pack */
    int supply_idx; /* Index inside the supply cache (-1 if not present) */
};

struct knowledge_browser_state
{
    int column[4];
    int group_cur[4];
    int group_top[4];
    int entry_cur[4];
    int entry_top[4];
    bool tabs_focus;
};

static int g_knowledge_last_page = KNOWLEDGE_PAGE_ARTEFACTS;

static bool knowledge_pause_information_scene(
    ui_information_scene_scope* scope)
{
    if (!scope || !scope->active)
        return false;

    ui_information_scene_leave(scope);
    return true;
}

static bool knowledge_resume_information_scene(
    ui_information_scene_scope* scope)
{
    if (!scope)
        return false;

    return ui_information_scene_enter(scope);
}

static bool knowledge_enter_information_scene_or_report(
    ui_information_scene_scope* scope, cptr log_name, cptr unavailable_message)
{
    if (ui_information_scene_enter(scope))
        return true;

    log_warn("%s: semantic scene unavailable on the snapshot renderer path",
        log_name ? log_name : "knowledge");
    if (unavailable_message && unavailable_message[0])
        msg_print(unavailable_message);
    return false;
}

static bool knowledge_present_ui_scene_or_abort(
    ui_information_scene_scope* scope, bool build_ok, app_ui_scene* scene,
    cptr scene_name, cptr user_message)
{
    if (build_ok && scene && ui_information_scene_present_ui(scene))
        return true;

    log_error("knowledge: failed to present SDL semantic scene for %s",
        scene_name ? scene_name : "unknown knowledge screen");
    if (scope && scope->active)
        ui_information_scene_leave(scope);
    bell(user_message ? user_message : "Knowledge screen unavailable.");
    if (user_message && user_message[0])
        msg_print(user_message);
    return false;
}

static cptr supply_group_text[SUPPLY_GROUP_MAX + 1] = {
    "Herbs",
    "Food",
    "Potions",
    "Gems",
    "Lights",
    NULL
};

int cmd_ui_knowledge_last_page(void)
{
    return g_knowledge_last_page;
}


static bool supplies_menu_use_entry(supply_list_entry* entry)
{
    if (!entry || entry->supply_idx < 0)
        return false;

    object_type* o_ptr = supplies_entry_at(entry->supply_idx);
    if (!o_ptr || !o_ptr->k_idx)
        return false;

    supplies_begin_action(entry->supply_idx);

    switch (o_ptr->tval)
    {
    case TV_FOOD:
        do_cmd_eat_food(o_ptr, SUPPLIES_INDEX);
        break;
    case TV_POTION:
        do_cmd_quaff_potion(o_ptr, SUPPLIES_INDEX);
        break;
    case TV_STAFF:
        do_cmd_activate_staff(o_ptr, SUPPLIES_INDEX);
        break;
    case TV_GEM:
        do_cmd_use_gem(o_ptr, SUPPLIES_INDEX);
        break;
    default:
        supplies_end_action();
        bell("Cannot use that item here!");
        msg_print("Cannot use that item here.");
        return false;
    }

    supplies_end_action();
    return true;
}

static bool supplies_menu_drop_entry(supply_list_entry* entry)
{
    if (!entry || entry->supply_idx < 0)
        return false;

    object_type* o_ptr = supplies_entry_at(entry->supply_idx);
    if (!o_ptr || !o_ptr->k_idx)
        return false;

    int max_amt = o_ptr->number;
    if (max_amt <= 0)
        return false;

    int actual_amt = get_quantity(NULL, max_amt);
    if (actual_amt <= 0)
        return false;
    supplies_begin_action(entry->supply_idx);
    bool dropped = supplies_drop_amount(entry->supply_idx, actual_amt);
    supplies_end_action();

    if (dropped)
        handle_stuff();

    return dropped;
}

/*display the notes file*/
void do_cmd_knowledge_notes(void) { show_buffer(notes_buffer, 0); }

/*
 * Display oath status information
 */
void do_cmd_knowledge_oaths(void)
{
    ang_file* fff;
    char file_name[1024];
    
    /* Temporary file */
    if (!path_temp(file_name, sizeof(file_name)))
        return;

    /* Open a new file */
    fff = ang_file_open(file_name, "w");

    /* File type is "TEXT" */
    FILE_TYPE(FILE_TYPE_TEXT);

    /* Scan the oaths */
    ang_file_printf(fff, "Oath Status\n\n");
    
    /* Check current character oath */
    if (p_ptr->have_ability[S_SPC][SPC_OATH_MERCY])
    {
        if (p_ptr->active_ability[S_SPC][SPC_OATH_MERCY])
            ang_file_printf(fff, "Current Oath: Oath of Mercy (Active)\n\n");
        else
            ang_file_printf(fff, "Current Oath: Oath of Mercy (Broken)\n\n");
    }
    else if (p_ptr->have_ability[S_SPC][SPC_OATH_SILENCE])
    {
        if (p_ptr->active_ability[S_SPC][SPC_OATH_SILENCE])
            ang_file_printf(fff, "Current Oath: Oath of Silence (Active)\n\n");
        else
            ang_file_printf(fff, "Current Oath: Oath of Silence (Broken)\n\n");
    }
    else if (p_ptr->have_ability[S_SPC][SPC_OATH_IRON])
    {
        if (p_ptr->active_ability[S_SPC][SPC_OATH_IRON])
            ang_file_printf(fff, "Current Oath: Oath of Iron (Active)\n\n");
        else
            ang_file_printf(fff, "Current Oath: Oath of Iron (Broken)\n\n");
    }
    else if (p_ptr->have_ability[S_SPC][SPC_OATH_SMITH])
    {
        if (p_ptr->active_ability[S_SPC][SPC_OATH_SMITH])
            ang_file_printf(fff, "Current Oath: Oath of the Smith (Active)\n\n");
        else
            ang_file_printf(fff, "Current Oath: Oath of the Smith (Broken)\n\n");
    }
    else if (p_ptr->have_ability[S_SPC][SPC_OATH_VALOROUS])
    {
        if (p_ptr->active_ability[S_SPC][SPC_OATH_VALOROUS])
            ang_file_printf(fff, "Current Oath: Oath of Valorous Heart (Active)\n\n");
        else
            ang_file_printf(fff, "Current Oath: Oath of Valorous Heart (Broken)\n\n");
    }
    else
    {
        ang_file_printf(fff, "Current Oath: None\n\n");
    }
    
    /* Display metarun oath status */
    ang_file_printf(fff, "Metarun Oath Status:\n");
    
    /* Check unlocked oaths */
    bool has_unlocked = false;
    if (oath_unlocked(OATH_MERCY)) 
    {
        ang_file_printf(fff, "  Oath of Mercy: Unlocked");
        if (oath_banned(OATH_MERCY))
            ang_file_printf(fff, " (Banned this run)");
        ang_file_printf(fff, "\n");
        has_unlocked = true;
    }
    
    if (oath_unlocked(OATH_SILENCE)) 
    {
        ang_file_printf(fff, "  Oath of Silence: Unlocked");
        if (oath_banned(OATH_SILENCE))
            ang_file_printf(fff, " (Banned this run)");
        ang_file_printf(fff, "\n");
        has_unlocked = true;
    }
    
    if (oath_unlocked(OATH_IRON)) 
    {
        ang_file_printf(fff, "  Oath of Iron: Unlocked");
        if (oath_banned(OATH_IRON))
            ang_file_printf(fff, " (Banned this run)");
        ang_file_printf(fff, "\n");
        has_unlocked = true;
    }
    
    if (oath_unlocked(OATH_SMITH)) 
    {
        ang_file_printf(fff, "  Oath of the Smith: Unlocked");
        if (oath_banned(OATH_SMITH))
            ang_file_printf(fff, " (Banned this run)");
        ang_file_printf(fff, "\n");
        has_unlocked = true;
    }
    
    if (oath_unlocked(OATH_VALOROUS)) 
    {
        ang_file_printf(fff, "  Oath of Valorous Heart: Unlocked");
        if (oath_banned(OATH_VALOROUS))
            ang_file_printf(fff, " (Banned this run)");
        ang_file_printf(fff, "\n");
        has_unlocked = true;
    }
    
    if (!has_unlocked)
    {
        ang_file_printf(fff, "  No oaths unlocked yet.\n");
        ang_file_printf(fff, "  Complete Valar quests to unlock new oaths.\n");
    }
    
    /* Close the file */
    ang_file_close(fff);

    /* Display the file contents */
    show_file(file_name, "Oath Status", 0);

    /* Remove the file */
    fd_kill(file_name);
}

/*
 * Description of each object group.
 */
static cptr object_group_text[]
    = { "Herbs", "Potions", "Rings", "Amulets", "Staves", "Horns", "Swords",
          "Axes & Polearms", "Blunt Weapons", "Diggers", "Bows",
          //	"Arrows",
          "Light Sources", "Soft Armour", "Mail", "Shields", "Cloaks", "Gloves",
          "Helms", "Crowns", "Boots", "Chests", NULL };

/*
 * TVALs of items in each group
 */
static byte object_group_tval[] = { TV_FOOD, TV_POTION, TV_RING, TV_AMULET,
    TV_STAFF, TV_HORN, TV_SWORD, TV_POLEARM, TV_HAFTED, TV_DIGGING, TV_BOW,
    //	TV_ARROW,
    TV_LIGHT, TV_SOFT_ARMOR, TV_MAIL, TV_SHIELD, TV_CLOAK, TV_GLOVES, TV_HELM,
    TV_CROWN, TV_BOOTS, TV_CHEST, 0 };

/*
 * Build a list of objects indexes in the given group. Return the number
 * of objects in the group. object_idx[] must be one element larger than the
 * largest number of objects that will be collected.
 *  (Incorporates some code from jdh)
 */
static int collect_objects(int grp_cur, object_list_entry object_idx[])
{
    int i, j, k, object_cnt = 0;
    int max_sval = -1;

    /* Get a list of x_char in this group */
    byte group_tval = object_group_tval[grp_cur];

    /* Check every object */
    for (i = 0; i < z_info->k_max; i++)
    {
        /* Access the object type */
        object_kind* k_ptr = &k_info[i];

        /*used to check for allocation*/
        k = 0;

        /* Skip empty objects */
        if (!k_ptr->name)
            continue;

        /* Skip items with no distribution (including special artefacts) */
        /* Scan allocation pairs */
        for (j = 0; j < 4; j++)
        {
            /*add the rarity, if there is one*/
            k += k_ptr->chance[j];
        }
        /*not in allocation table*/
        if (!(k))
            continue;

        /* Require objects ever seen*/
        // if (!(k_ptr->aware && k_ptr->everseen)) continue;
        if (!(k_ptr->everseen))
            continue;

        /* Check for object in the group */
        if (k_ptr->tval == group_tval)
        {
            /* Save the highest sval in the group for later */
            if (k_ptr->sval > max_sval)
            {
                max_sval = k_ptr->sval;
            }

            /* Add the object type */
            if (object_idx)
            {
                object_idx[object_cnt].type = OBJ_NORMAL;
                object_idx[object_cnt].idx = i;
            }

            object_cnt++;
        }
    }

    /* Add special items to the list */
    /* Skip this part if we don't know any normal items */
    for (i = 0; object_cnt > 0 && i < z_info->e_max; i++)
    {
        /* Access the object type */
        ego_item_type* e_ptr = &e_info[i];

        /* Skip empty objects */
        if (!e_ptr->name)
            continue;

        /* Require objects ever seen*/
        if (!(e_ptr->everseen))
            continue;

        /* Check for object in the group */
        for (j = 0; j < EGO_TVALS_MAX; j++)
        {
            if (e_ptr->tval[j] == group_tval)
            {
                if (object_idx)
                {
                    object_idx[object_cnt].type = OBJ_SPECIAL;
                    object_idx[object_cnt].idx = -1;
                    object_idx[object_cnt].e_idx = i;
                    object_idx[object_cnt].tval = group_tval;
                    object_idx[object_cnt].sval = -1;
                }
                object_cnt++;

                break;
            }
        }
    }

    /* Terminate the list */
    if (object_idx)
        object_idx[object_cnt].type = OBJ_NONE;

    /* Return the number of object types */
    return object_cnt;
}

/*
 * Build a list of artefact indexes in the given group. Return the number
 * of eligible artefacts in that group.
 */
static int collect_artefacts(int grp_cur, int object_idx[])
{
    int i, object_cnt = 0;
    bool* okay;
    bool know_all = cheat_know;

    /* Get a list of x_char in this group */
    byte group_tval = object_group_tval[grp_cur];

    /*make a list of artefacts not found*/
    /* Allocate the "object_idx" array */
    okay = mem_alloc_array(z_info->art_max, bool);

    /* Default first,  */
    for (i = 0; i < z_info->art_max; i++)
    {
        artefact_type* a_ptr = &a_info[i];
        bool revealed = (a_ptr->seen & ART_SEEN_REVEALED) != 0;

        /*start with false*/
        okay[i] = false;

        /* Skip "empty" artefacts */
        if (a_ptr->tval + a_ptr->sval == 0)
            continue;

        /* Skip "unfound" artefacts, unless in wizard mode, cheating,
         * or revealed via quests/lore. */
        if (!know_all && !p_ptr->wizard && !a_ptr->found_num && !revealed)
            continue;

        /* Skip "ungenerated" artefacts, unless cheating or quest-revealed. */
        if (!know_all && !revealed && !a_ptr->cur_num)
            continue;

        /* Skip the later versions of the Iron Crown */
        if ((i == ART_MORGOTH_0) || (i == ART_MORGOTH_1)
            || (i == ART_MORGOTH_2))
            continue;

        /* Skip the special smithing template artefacts */
        if ((i >= ART_ULTIMATE) && (i <= z_info->art_norm_max))
            continue;

        /*assume all created artefacts are good at this point*/
        okay[i] = true;
    }

    /* Finally, go through the list of artefacts and categorize the good ones */
    for (i = 0; i < z_info->art_max; i++)
    {
        /* Access the artefact */
        artefact_type* a_ptr = &a_info[i];

        /* Skip empty artefacts */
        if (a_ptr->tval + a_ptr->sval == 0)
            continue;

        /* Require artefacts ever seen*/
        if (okay[i] == false)
            continue;

        /* Check for race in the group */
        if (a_ptr->tval == group_tval)
        {
            /* Add the race */
            object_idx[object_cnt++] = i;
        }
    }

    /* Terminate the list */
    object_idx[object_cnt] = 0;

    /*clear the array*/
    mem_free_null(okay);

    /* Return the number of races */
    return object_cnt;
}

static bool supply_kind_matches(int group, int tval, int sval)
{
    switch (group)
    {
    case SUPPLY_GROUP_HERBS:
        return (tval == TV_FOOD) && (sval < SV_FOOD_MIN_FOOD);
    case SUPPLY_GROUP_FOOD:
        return (tval == TV_FOOD) && (sval >= SV_FOOD_MIN_FOOD);
    case SUPPLY_GROUP_POTIONS:
        return (tval == TV_POTION);
    case SUPPLY_GROUP_GEMS:
        return (tval == TV_GEM);
    case SUPPLY_GROUP_LIGHTS:
        return (tval == TV_LIGHT)
            && (sval == SV_LIGHT_TORCH || sval == SV_LIGHT_MALLORN
                || sval == SV_LIGHT_LANTERN
                || sval == SV_LIGHT_LESSER_JEWEL);
    default:
        return false;
    }
}

static bool supply_item_matches(int group, const object_type* o_ptr)
{
    if (!o_ptr)
        return false;

    return supply_kind_matches(group, o_ptr->tval, o_ptr->sval);
}

static void append_supply_item_weight(char* buf, size_t len,
    const object_type* o_ptr, bool each)
{
    char weight_buf[32];

    if (!buf || len == 0 || !o_ptr || o_ptr->weight <= 0)
        return;

    strnfmt(weight_buf, sizeof(weight_buf), " [%d.%1d lb%s]",
        o_ptr->weight / 10, o_ptr->weight % 10, each ? " each" : "");
    SDL_strlcat(buf, weight_buf, len);
}

static int supply_group_uniform_weight(int group_idx)
{
    int weight = -1;

    for (int i = 0; i < z_info->k_max; i++)
    {
        object_kind* k_ptr = &k_info[i];

        if (!k_ptr->name)
            continue;
        if (!supply_kind_matches(group_idx, k_ptr->tval, k_ptr->sval))
            continue;

        if (weight < 0)
            weight = k_ptr->weight;
        else if (weight != k_ptr->weight)
            return -1;
    }

    return weight;
}

static void describe_supply_group_status(int group_idx, char* buf, size_t len)
{
    int weight;

    if (!buf || len == 0)
        return;

    buf[0] = '\0';

    switch (group_idx)
    {
    case SUPPLY_GROUP_HERBS:
        weight = supply_group_uniform_weight(group_idx);
        if (weight >= 0)
            strnfmt(buf, len, "All herbs weigh %d.%1d lb each.",
                weight / 10, weight % 10);
        break;
    case SUPPLY_GROUP_FOOD:
        SDL_strlcpy(buf, "Food weight varies; each row shows per-item weight.",
            len);
        break;
    case SUPPLY_GROUP_POTIONS:
        weight = supply_group_uniform_weight(group_idx);
        if (weight >= 0)
            strnfmt(buf, len, "All potions weigh %d.%1d lb each.",
                weight / 10, weight % 10);
        break;
    case SUPPLY_GROUP_GEMS:
        weight = supply_group_uniform_weight(group_idx);
        if (weight >= 0)
            strnfmt(buf, len, "All gems weigh %d.%1d lb each.",
                weight / 10, weight % 10);
        break;
    case SUPPLY_GROUP_LIGHTS:
        SDL_strlcpy(buf,
            "Each light row shows item weight; light total above includes oil.",
            len);
        break;
    default:
        break;
    }
}

static void compute_supply_group_totals(int totals[SUPPLY_GROUP_MAX])
{
    int i;

    for (i = 0; i < SUPPLY_GROUP_MAX; i++)
        totals[i] = 0;

    for (i = 0; i < INVEN_PACK; i++)
    {
        object_type* o_ptr = &inventory[i];

        if (!o_ptr->k_idx)
            continue;

        if (supply_kind_matches(SUPPLY_GROUP_HERBS, o_ptr->tval, o_ptr->sval))
            totals[SUPPLY_GROUP_HERBS] += o_ptr->number;
        else if (supply_kind_matches(SUPPLY_GROUP_FOOD, o_ptr->tval, o_ptr->sval))
            totals[SUPPLY_GROUP_FOOD] += o_ptr->number;
        else if (o_ptr->tval == TV_POTION)
            totals[SUPPLY_GROUP_POTIONS] += o_ptr->number;
        else if (o_ptr->tval == TV_GEM)
            totals[SUPPLY_GROUP_GEMS] += o_ptr->number;
    }

    for (i = 0; i < supplies_entry_count(); i++)
    {
        object_type* s_ptr = supplies_entry_at(i);
        if (!s_ptr || !s_ptr->k_idx)
            continue;

        if (supply_kind_matches(SUPPLY_GROUP_HERBS, s_ptr->tval, s_ptr->sval))
            totals[SUPPLY_GROUP_HERBS] += s_ptr->number;
        else if (supply_kind_matches(SUPPLY_GROUP_FOOD, s_ptr->tval, s_ptr->sval))
            totals[SUPPLY_GROUP_FOOD] += s_ptr->number;
        else if (s_ptr->tval == TV_POTION)
            totals[SUPPLY_GROUP_POTIONS] += s_ptr->number;
        else if (s_ptr->tval == TV_GEM)
            totals[SUPPLY_GROUP_GEMS] += s_ptr->number;
        else if (supplies_is_light_object(s_ptr))
            totals[SUPPLY_GROUP_LIGHTS] += s_ptr->number;
    }

    {
        object_type* light_ptr = &inventory[INVEN_LITE];
        if (supplies_is_light_object(light_ptr))
            totals[SUPPLY_GROUP_LIGHTS] += MAX(light_ptr->number, 1);
    }
}

static bool supply_kind_is_known(const object_kind* k_ptr)
{
    if (!k_ptr)
        return false;

    if (cheat_know || p_ptr->wizard)
        return true;

    return k_ptr->aware || k_ptr->everseen || k_ptr->tried;
}

static int collect_supply_entries(int group_idx, supply_list_entry entries[])
{
    int count = 0;
    int capacity = z_info->k_max;
    int i;

    if (!entries)
        return 0;

    memset(entries, 0, sizeof(supply_list_entry) * capacity);

    /* Aggregate carried items first */
    for (i = 0; i < INVEN_PACK; i++)
    {
        object_type* o_ptr = &inventory[i];
        int j;

        if (!o_ptr->k_idx)
            continue;

        if (!supply_item_matches(group_idx, o_ptr))
            continue;

        int value = o_ptr->number;

        for (j = 0; j < count; j++)
        {
            if (entries[j].k_idx == o_ptr->k_idx)
            {
                entries[j].total += value;
                if (entries[j].item_idx < 0)
                    entries[j].item_idx = i;
                break;
            }
        }

        if (j == count)
        {
            if (count >= capacity)
                break;

            entries[count].k_idx = o_ptr->k_idx;
            entries[count].item_idx = i;
            entries[count].total = value;
            entries[count].supply_idx = -1;
            count++;
        }
    }

    if (group_idx == SUPPLY_GROUP_LIGHTS)
    {
        object_type* light_ptr = &inventory[INVEN_LITE];
        int j;

        if (light_ptr->k_idx && supply_item_matches(group_idx, light_ptr))
        {
            int value = MAX(light_ptr->number, 1);

            for (j = 0; j < count; j++)
            {
                if (entries[j].k_idx == light_ptr->k_idx)
                {
                    entries[j].total += value;
                    if (entries[j].item_idx < 0)
                        entries[j].item_idx = INVEN_LITE;
                    break;
                }
            }

            if ((j == count) && (count < capacity))
            {
                entries[count].k_idx = light_ptr->k_idx;
                entries[count].item_idx = INVEN_LITE;
                entries[count].total = value;
                entries[count].supply_idx = -1;
                count++;
            }
        }
    }

    /* Aggregate supplies from the cache */
    for (i = 0; i < supplies_entry_count(); i++)
    {
        object_type* s_ptr = supplies_entry_at(i);
        int j;

        if (!s_ptr || !s_ptr->k_idx)
            continue;

        if (!supply_item_matches(group_idx, s_ptr))
            continue;

        int value = s_ptr->number;

        for (j = 0; j < count; j++)
        {
            if (entries[j].k_idx == s_ptr->k_idx)
            {
                entries[j].total += value;
                if (entries[j].item_idx < 0)
                    entries[j].item_idx = SUPPLIES_INDEX;
                entries[j].supply_idx = i;
                break;
            }
        }

        if (j == count)
        {
            if (count >= capacity)
                break;

            entries[count].k_idx = s_ptr->k_idx;
            entries[count].item_idx = SUPPLIES_INDEX;
            entries[count].total = value;
            entries[count].supply_idx = i;
            count++;
        }
    }

    /* Add known kinds even when none are carried */
    for (i = 0; i < z_info->k_max; i++)
    {
        object_kind* k_ptr = &k_info[i];
        int j;

        if (!k_ptr->name)
            continue;

        if (!supply_kind_matches(group_idx, k_ptr->tval, k_ptr->sval))
            continue;

        if (!supply_kind_is_known(k_ptr))
            continue;

        for (j = 0; j < count; j++)
        {
            if (entries[j].k_idx == i)
                break;
        }

        if (j == count)
        {
            if (count >= capacity)
                break;

            entries[count].k_idx = i;
            entries[count].item_idx = -1;
            entries[count].total = 0;
            entries[count].supply_idx = -1;
            count++;
        }
    }

    if (count < capacity)
    {
        entries[count].k_idx = -1;
        entries[count].item_idx = -1;
        entries[count].total = 0;
        entries[count].supply_idx = -1;
    }

    return count;
}

static byte get_supply_item_color(int k_idx, bool aware)
{
    object_kind* k_ptr;

    if (k_idx < 0 || k_idx >= z_info->k_max)
        return TERM_WHITE;

    k_ptr = &k_info[k_idx];

    /* Unidentified items all use slate color */
    if (!aware)
        return TERM_SLATE;

    /* Color by specific item type */
    switch (k_ptr->tval)
    {
        case TV_FOOD: /* Herbs */
            switch (k_ptr->sval)
            {
                case SV_FOOD_RAGE:         return TERM_RED;    /* Red for rage */
                case SV_FOOD_SUSTENANCE:   return TERM_GREEN;    /* Green for sustenance */
                case SV_FOOD_TERROR:       return TERM_VIOLET;   /* Violet for fear */
                case SV_FOOD_HEALING:      return TERM_L_GREEN;  /* Light green for healing */
                case SV_FOOD_RESTORATION:  return TERM_BLUE;     /* Blue for restoration */
                case SV_FOOD_HUNGER:       return TERM_UMBER;    /* Brown for hunger */
                case SV_FOOD_VISIONS:      return TERM_L_UMBER;  /* Light brown for visions */
                case SV_FOOD_ENTRANCEMENT: return TERM_VIOLET;   /* Violet for entrancement */
                case SV_FOOD_WEAKNESS:     return TERM_SLATE;    /* Grey for weakness */
                case SV_FOOD_SICKNESS:     return TERM_L_DARK;   /* Dark grey for sickness */
                default:                   return TERM_WHITE;
            }

        case TV_POTION:
            switch (k_ptr->sval)
            {
                case SV_POTION_MIRUVOR:          return TERM_WHITE;  /* White for Miruvor */
                case SV_POTION_ORCISH_LIQUOR:    return TERM_UMBER;    /* Brown for liquor */
                case SV_POTION_ESGALDUIN:        return TERM_VIOLET;   /* Violet for Esgalduin */
                case SV_POTION_CLARITY:          return TERM_L_UMBER;  /* Light brown for clarity */
                case SV_POTION_HEALING:          return TERM_L_GREEN;  /* Light green for healing */
                case SV_POTION_VOICE:            return TERM_L_BLUE;  /* White for voice */
                case SV_POTION_true_SIGHT:       return TERM_BLUE;     /* Blue for true sight */
                case SV_POTION_ANTIDOTE:         return TERM_GREEN;    /* Green for antidote */
                case SV_POTION_QUICKNESS:        return TERM_ORANGE;  /* Light brown for speed */
                case SV_POTION_ELEM_RESISTANCE:  return TERM_L_BLUE;   /* Orange for resistance */
                case SV_POTION_STR:              return TERM_RED;      /* Red for strength */
                case SV_POTION_DEX:              return TERM_GREEN;    /* Green for dexterity */
                case SV_POTION_CON:              return TERM_L_RED;     /* Blue for constitution */
                case SV_POTION_GRA:              return TERM_BLUE;   /* Violet for grace */
                case SV_POTION_SLOWNESS:         return TERM_SLATE;    /* Grey for slowness */
                case SV_POTION_POISON:           return TERM_L_DARK;   /* Dark for poison */
                case SV_POTION_BLINDNESS:        return TERM_L_DARK;   /* Dark for blindness */
                case SV_POTION_CONFUSION:        return TERM_SLATE;    /* Grey for confusion */
                case SV_POTION_DEC_DEX:          return TERM_SLATE;    /* Grey for decrease dex */
                case SV_POTION_DEC_GRA:          return TERM_SLATE;    /* Grey for decrease grace */
                default:                         return TERM_WHITE;
            }

        case TV_GEM:
            switch (k_ptr->sval)
            {
                case SV_GEM_FREEDOM:         return TERM_WHITE;  /* White for freedom */
                case SV_GEM_LIGHT:           return TERM_ORANGE;   /* Orange for light */
                case SV_GEM_SANCTITY:        return TERM_L_UMBER;  /* Light brown for sanctity */
                case SV_GEM_UNDERSTANDING:   return TERM_BLUE;     /* Blue for understanding */
                case SV_GEM_REVELATIONS:     return TERM_L_BLUE;   /* Violet for revelations */
                case SV_GEM_TREASURES:       return TERM_ORANGE;   /* Orange for treasures */
                case SV_GEM_FOES:            return TERM_RED;      /* Red for foes */
                case SV_GEM_SELF_KNOWLEDGE:  return TERM_GREEN;  /* Light green for self-knowledge */
                case SV_GEM_WARDING:         return TERM_VIOLET;  /* Light brown for warding */
                case SV_GEM_RECHARGING:      return TERM_BLUE;     /* Blue for recharging */
                case SV_GEM_SHADOWS:         return TERM_L_DARK;   /* Dark for shadows */
                default:                     return TERM_WHITE;
            }

        case TV_LIGHT:
            switch (k_ptr->sval)
            {
                case SV_LIGHT_TORCH:        return TERM_YELLOW;
                case SV_LIGHT_MALLORN:      return TERM_L_GREEN;
                case SV_LIGHT_LANTERN:      return TERM_UMBER;
                case SV_LIGHT_LESSER_JEWEL: return TERM_L_BLUE;
                case SV_LIGHT_FEANORIAN:    return TERM_WHITE;
                default:                    return TERM_WHITE;
            }

        default:
            return TERM_WHITE;
    }
}

/*
 * Move the cursor in a browser window
 */
static void browser_cursor_with_rows(char ch, int* column, int* grp_cur,
    int grp_cnt, int* list_cur, int list_cnt, int page_rows)
{
    int d;
    int col = *column;
    int grp = *grp_cur;
    int list = *list_cur;
    int page_jump = (page_rows > 0) ? page_rows : BROWSER_ROWS;

    /* Extract direction */
    d = target_dir(ch);

    if (!d)
        return;

    /* Diagonals - hack */
    if ((ddx[d] > 0) && ddy[d])
    {
        /* Browse group list */
        if (!col)
        {
            int old_grp = grp;

            /* Move up or down */
            grp += ddy[d] * page_jump;

            /* Verify */
            if (grp >= grp_cnt)
                grp = grp_cnt - 1;
            if (grp < 0)
                grp = 0;
            if (grp != old_grp)
                list = 0;
        }

        /* Browse sub-list list */
        else
        {
            /* Move up or down */
            list += ddy[d] * page_jump;

            /* Verify */
            if (list >= list_cnt)
                list = list_cnt - 1;
            if (list < 0)
                list = 0;
        }

        (*grp_cur) = grp;
        (*list_cur) = list;

        return;
    }

    if (ddx[d])
    {
        col += ddx[d];
        if (col < 0)
            col = 0;
        if (col > 1)
            col = 1;

        (*column) = col;

        return;
    }

    /* Browse group list */
    if (!col)
    {
        int old_grp = grp;

        /* Move up or down */
        grp += ddy[d];

        /* Verify */
        if (grp >= grp_cnt)
            grp = grp_cnt - 1;
        if (grp < 0)
            grp = 0;
        if (grp != old_grp)
            list = 0;
    }

    /* Browse sub-list list */
    else
    {
        /* Move up or down */
        list += ddy[d];

        /* Verify */
        if (list >= list_cnt)
            list = list_cnt - 1;
        if (list < 0)
            list = 0;
    }

    (*grp_cur) = grp;
    (*list_cur) = list;
}

/*
 * Hack -- Create a "forged" artefact
 */
static bool prepare_fake_artefact(object_type* o_ptr, byte name1)
{
    s16b i;

    artefact_type* a_ptr = &a_info[name1];

    /* Ignore "empty" artefacts */
    if (a_ptr->tval + a_ptr->sval == 0)
        return false;

    /* Get the "kind" index */
    i = lookup_kind(a_ptr->tval, a_ptr->sval);

    /* Oops */
    if (!i)
        return (false);

    /* Create the artefact */
    object_prep(o_ptr, i);

    /* Save the name */
    o_ptr->name1 = name1;

    /* Extract the fields */
    o_ptr->pval = a_ptr->pval;
    o_ptr->att = a_ptr->att;
    o_ptr->dd = a_ptr->dd;
    o_ptr->ds = a_ptr->ds;
    o_ptr->evn = a_ptr->evn;
    o_ptr->pd = a_ptr->pd;
    o_ptr->ps = a_ptr->ps;
    o_ptr->weight = a_ptr->weight;

    memcpy(o_ptr->stat_bonus, a_ptr->stat_bonus, sizeof(o_ptr->stat_bonus));
    memcpy(o_ptr->skill_bonus, a_ptr->skill_bonus, sizeof(o_ptr->skill_bonus));

    // add the abilities
    for (i = 0; i < a_ptr->abilities; i++)
    {
        o_ptr->skilltype[i + o_ptr->abilities] = a_ptr->skilltype[i];
        o_ptr->abilitynum[i + o_ptr->abilities] = a_ptr->abilitynum[i];
        o_ptr->bane_type[i + o_ptr->abilities] = a_ptr->bane_type[i];
    }
    o_ptr->abilities += a_ptr->abilities;

    /*identify it*/
    object_known(o_ptr);

    /*make it a spoiler item*/
    o_ptr->ident |= IDENT_SPOIL;

    /* Hack -- extract the "cursed" flag */
    if (a_ptr->flags3 & (TR3_LIGHT_CURSE | TR3_HEAVY_CURSE | TR3_PERMA_CURSE))
        o_ptr->ident |= (IDENT_CURSED);

    /* Success */
    return (true);
}

/*
 * Describe fake artefact
 */
void desc_art_fake(int a_idx)
{
    object_type* i_ptr;
    object_type object_type_body;

    /* Get local object */
    i_ptr = &object_type_body;

    /* Wipe the object */
    object_wipe(i_ptr);

    /* Make fake artefact */
    prepare_fake_artefact(i_ptr, a_idx);

    /* Hack -- Handle stuff */
    handle_stuff();

    object_info_screen(i_ptr);
}

/*
 * Display known artefacts
 */
void do_cmd_knowledge_artefacts(void)
{
    log_debug("Player opened artifacts knowledge screen");
    do_cmd_knowledge_browser_page(KNOWLEDGE_PAGE_ARTEFACTS);
}

/*
 * Description of each monster group.
 */
static cptr monster_group_text[] = { "Uniques", /*All uniques, all letters*/
    /*Unused*/ /*'a'*/
    /*Unused*/ /*'A'*/
    "Bats & Birds", /*'b'*/
    /*Unused*/ /*'B'*/
    /*Unused*/ /*'c'*/
    "Canines", /*'C'*/
    "Young Dragons", /*'d'*/
    "Great Dragons", /*'D'*/
    /*Unused*/ /*'e'*/
    /*Unused*/ /*'E'*/
    "Felines", /*'f'*/
    /*Unused*/ /*'F'*/
    /*Unused*/ /*'g'*/
    "Giants", /*'G'*/
    /*Unused*/ /*'h'*/
    "Horrors", /*'H'*/
    /*Unused*/ /*'i'*/
    "Insects", /*'I'*/
    /*Unused*/ /*'j'*/
    /*Unused*/ /*'J'*/
    /*Unused*/ /*'k'*/
    /*Unused*/ /*'K'*/
    /*Unused*/ /*'l'*/
    /*Unused*/ /*'L'*/
    "Young Spiders", /*'m'*/
    "Spiders", /*'M'*/
    /*Unused*/ /*'n'*/
    "Nameless Things", /*'N'*/
    "Orcs", /*'o'*/
    /*Unused*/ /*'O'*/
    /*Unused*/ /*'p'*/
    /*Unused*/ /*'P'*/
    /*Unused*/ /*'q'*/
    /*Unused*/ /*'Q'*/
    /*Unused*/ /*'r'*/
    "Raukar", /*'R'*/
    "Serpents", /*'s'*/
    "Ancient Serpents", /*'S'*/
    /*Unused*/ /*'t'*/
    "Trolls", /*'T'*/
    /*Unused*/ /*'u'*/
    /*Unused*/ /*'U'*/
    "Vampires", /*'v'*/
    "Valar", /*'V'*/
    "Creeping Shadows", /*'w'*/
    "Wights and Wraiths", /*'W'*/
    /*Unused*/ /*'x'*/
    /*Unused*/ /*'X'*/
    /*Unused*/ /*'y'*/
    /*Unused*/ /*'Y'*/
    /*Unused*/ /*'Z'*/
    /*Unused*/ /*'Z'*/
    "Plants", /*'&'*/
    "People", /*'@'*/
    NULL };

/*
 * Symbols of monsters in each group. Note the "Uniques" group
 * is handled differently.
 */
static cptr monster_group_char[] = { (char*)-1L,
    /*"a", Unused*/
    /*"A", Unused*/
    "b",
    /*"B", Unused*/
    /*"c", Unused*/
    "C", "d", "D",
    /*"e", Unused*/
    /*"E", Unused*/
    "f",
    /*"F", Unused*/
    /*"g", Unused*/
    "G",
    /*"h", Unused*/
    "H",
    /*"i", Unused*/
    "I",
    /*"j", Unused*/
    /*"J", Unused*/
    /*"k", Unused*/
    /*"K", Unused*/
    /*"l", Unused*/
    /*"L", Unused*/
    "m", "M",
    /*"n", Unused*/
    "N", "o",
    /*"O", Unused*/
    /*"p", Unused*/
    /*"P", Unused*/
    /*"q", Unused*/
    /*"Q", Unused*/
    /*"r", Unused*/
    "R", "s", "S",
    /*"t", Unused*/
    "T",
    /*"u", Unused*/
    /*"U", Unused*/
    "v", "V", "w", "W",
    /*"x", Unused*/
    /*"X", Unused*/
    /*"y", Unused*/
    /*"Y", Unused*/
    /*"z", Unused*/
    /*"Z", Unused*/
    "&", // plants
    "@", // human/elf/dwarf
    NULL };

/*
 * Build a list of monster indexes in the given group. Return the number
 * of monsters in the group.
 */
static int collect_monsters(int grp_cur, monster_list_entry* mon_idx, int mode)
{
    int i, mon_count = 0;

    /* Get a list of x_char in this group */
    cptr group_char = monster_group_char[grp_cur];

    /* XXX Hack -- Check if this is the "Uniques" group */
    bool grp_unique = (monster_group_char[grp_cur] == (char*)-1L);

    /* Check every race */
    for (i = 1; i < z_info->r_max; i++)
    {
        /* Access the race */
        monster_race* r_ptr = &r_info[i];
        monster_lore* l_ptr = &l_list[i];

        /* Is this a unique? */
        bool unique = (r_ptr->flags1 & (RF1_UNIQUE));

        /* Skip empty race */
        if (!r_ptr->name)
            continue;

        if (grp_unique && !(unique))
            continue;

        /* Require known monsters */
        if (!(mode & 0x02) && (!cheat_know) && (!know_monster_info)
            && (!(l_ptr->tsights)))
            continue;

        // Ignore monsters that can't be generated
        if (r_ptr->level > 25)
            continue;

        /* Check for race in the group */
        if ((grp_unique) || (strchr(group_char, r_ptr->d_char)))
        {
            /* Add the race */
            mon_idx[mon_count++].r_idx = i;

            /* XXX Hack -- Just checking for non-empty group */
            if (mode & 0x01)
                break;
        }
    }

    /* Terminate the list */
    mon_idx[mon_count].r_idx = 0;

    /* Return the number of races */
    return (mon_count);
}

/*
 * Display known monsters.
 */
void do_cmd_knowledge_monsters(void)
{
    do_cmd_knowledge_browser_page(KNOWLEDGE_PAGE_MONSTERS);
}

/*
 * Add a pval so the object descriptions don't look strange*
 */
void apply_magic_fake(object_type* o_ptr)
{
    s16b old_pval = o_ptr->pval;

    /* Analyze type */
    switch (o_ptr->tval)
    {
    case TV_DIGGING:
    {
        if (o_ptr->pval < 1)
            o_ptr->pval = 1;
        break;
    }

    /*many rings need a pval*/
    case TV_RING:
    {
        /* Analyze */
        switch (o_ptr->sval)
        {
        /* Strength, Dexterity */
        case SV_RING_STR:
        case SV_RING_DEX:
        {
            if (o_ptr->pval < 1)
                o_ptr->pval = 1;

            break;
        }

        /* Ring of Accuracy */
        case SV_RING_ACCURACY:
        {
            /* Bonus to hit */
            if (o_ptr->att < 1)
                o_ptr->att = 1;

            break;
        }

        /* Ring of Evasion */
        case SV_RING_EVASION:
        {
            /* Bonus to evasion */
            if (o_ptr->evn < 1)
                o_ptr->evn = 1;

            break;
        }

        /* Ring of Secrets */
        case SV_RING_SECRETS:
        {
            /* Bonus to perception */
            if (o_ptr->pval < 1)
                o_ptr->pval = 1;

            break;
        }

        /* Ring of Ered Luin */
        case SV_RING_ERED_LUIN:
        {
            /* Bonus to will */
            if (o_ptr->pval < 1)
                o_ptr->pval = 1;
            break;
        }

        /* Ring of the Laiquendi */
        case SV_RING_LAIQUENDI:
        {
            /* Bonus to stealth and archery */
            if (o_ptr->pval < 1)
                o_ptr->pval = 1;
            break;
        }
        }

        /*break for TVAL-Rings*/
        break;
    }

    case TV_AMULET:
    {
        /* Analyze */
        switch (o_ptr->sval)
        {
        /* Various amulets */
        case SV_AMULET_CON:
        case SV_AMULET_GRA:
        {
            if (o_ptr->pval < 1)
                o_ptr->pval = 1;
            break;
        }

        /* Amulet of Protection */
        case SV_AMULET_PROTECTION:
        {
            if (o_ptr->pd < 1)
                o_ptr->pd = 1;
            if (o_ptr->ps < 1)
                o_ptr->ps = 1;
            break;
        }

        /* Amulet of the Blessed Realm */
        case SV_AMULET_BLESSED_REALM:
        {
            if (o_ptr->pval < 1)
                o_ptr->pval = 1;
            break;
        }

        /* Amulet of the Vigilant Eye */
        case SV_AMULET_VIGILANT_EYE:
        {
            if (o_ptr->pval < 1)
                o_ptr->pval = 1;
            break;
        }

        default:
            break;
        }
        /*break for TVAL-Amulets*/
        break;
    }

    case TV_LIGHT:
    {
        /* Analyze */
        switch (o_ptr->sval)
        {
        case SV_LIGHT_TORCH:
        case SV_LIGHT_MALLORN:
        case SV_LIGHT_LANTERN:
        {
            o_ptr->timeout = 0;

            break;
        }
        }
        /*break for TVAL-Lights*/
        break;
    }

    /*give them one charge*/
    case TV_STAFF:
    {
        if (o_ptr->pval < 1)
            o_ptr->pval = 1;

        break;
    }
    }

    int pval_delta = (int)o_ptr->pval - (int)old_pval;
    if (pval_delta != 0)
        object_apply_pval_delta_with_mask(o_ptr, object_pval_flags1(o_ptr), pval_delta);
}

/*
 * Describe fake object
 */
static void desc_obj_fake(int k_idx)
{
    object_type* i_ptr;
    object_type object_type_body;

    /* Get local object */
    i_ptr = &object_type_body;

    /* Wipe the object */
    object_wipe(i_ptr);

    /* Create the object */
    object_prep(i_ptr, k_idx);

    /*add minimum bonuses so the descriptions don't look strange*/
    apply_magic_fake(i_ptr);

    /* It's fully known */
    i_ptr->ident |= IDENT_KNOWN;

    /* Hack -- Handle stuff */
    handle_stuff();

    object_info_screen(i_ptr);
}

static int knowledge_normalize_page(int page)
{
    if (page < KNOWLEDGE_PAGE_ARTEFACTS || page > KNOWLEDGE_PAGE_CURSES)
        return g_knowledge_last_page;

    return page;
}

static cptr knowledge_tab_label(int page)
{
    static const cptr labels[] = {
        "Arts",
        "Objs",
        "Mons",
        "Curses"
    };

    if (page < KNOWLEDGE_PAGE_ARTEFACTS || page > KNOWLEDGE_PAGE_CURSES)
        return "";

    return labels[page];
}

static void knowledge_clamp_group_state(int* column, int* grp_cur, int* grp_top,
    int grp_cnt, int* entry_cur, int* entry_top, int entry_cnt, int per_page)
{
    if (grp_cnt <= 0)
    {
        *column = 0;
        *grp_cur = 0;
        *grp_top = 0;
        *entry_cur = 0;
        *entry_top = 0;
        return;
    }

    if (*grp_cur >= grp_cnt)
        *grp_cur = grp_cnt - 1;
    if (*grp_cur < 0)
        *grp_cur = 0;
    if (*grp_top > *grp_cur)
        *grp_top = *grp_cur;
    if (*grp_cur >= *grp_top + per_page)
        *grp_top = *grp_cur - per_page + 1;
    if (*grp_top < 0)
        *grp_top = 0;

    if (entry_cnt <= 0)
    {
        *column = 0;
        *entry_cur = 0;
        *entry_top = 0;
    }
    else
    {
        if (*entry_cur >= entry_cnt)
            *entry_cur = entry_cnt - 1;
        if (*entry_cur < 0)
            *entry_cur = 0;
        if (*entry_top > *entry_cur)
            *entry_top = *entry_cur;
        if (*entry_cur >= *entry_top + per_page)
            *entry_top = *entry_cur - per_page + 1;
        if (*entry_top < 0)
            *entry_top = 0;
    }

    if (*column < 0)
        *column = 0;
    if (*column > 1)
        *column = 1;
    if (entry_cnt <= 0)
        *column = 0;
}

static void knowledge_clamp_list_state(int* cur, int* top, int count, int per_page)
{
    if (count <= 0)
    {
        *cur = 0;
        *top = 0;
        return;
    }

    if (*cur >= count)
        *cur = count - 1;
    if (*cur < 0)
        *cur = 0;
    if (*top > *cur)
        *top = *cur;
    if (*cur >= *top + per_page)
        *top = *cur - per_page + 1;
    if (*top < 0)
        *top = 0;
}

static void knowledge_monster_summary(char* buf, size_t buflen, int grp_cur)
{
    int i;
    u32b known_uniques = 0;
    u32b dead_uniques = 0;
    u32b slay_count = 0;

    for (i = 1; i < z_info->r_max - 1; i++)
    {
        monster_race* r_ptr = &r_info[i];
        monster_lore* l_ptr = &l_list[i];

        if ((r_ptr->rarity == 0) || (r_ptr->level > 25))
            continue;

        if (r_ptr->flags1 & RF1_UNIQUE)
        {
            if (l_ptr->tsights)
            {
                known_uniques++;
                if (r_ptr->max_num == 0)
                {
                    dead_uniques++;
                    slay_count++;
                }
            }
            else if (know_monster_info || cheat_know)
            {
                known_uniques++;
            }
        }
        else
        {
            slay_count += l_ptr->pkills;
        }
    }

    if (monster_group_char[grp_cur] != (char*)-1L)
    {
        strnfmt(buf, buflen, "Total creatures slain: %u.", (unsigned)slay_count);
    }
    else
    {
        strnfmt(buf, buflen, "Known uniques: %u, slain uniques: %u.",
            (unsigned)known_uniques, (unsigned)dead_uniques);
    }
}

static int knowledge_collect_curses(int curse_idx[])
{
    int id;
    int count = 0;

    for (id = 0; id < (int)z_info->cu_max; id++)
    {
        if (CURSE_SEEN(id))
            curse_idx[count++] = id;
    }

    return count;
}

static cptr knowledge_curse_display_name(int idx)
{
    cptr raw = cu_name + cu_info[idx].name;

    if (strncmp(raw, "Curse of ", 9) == 0)
        raw += 9;
    else if (strncmp(raw, "Burden of ", 10) == 0)
        raw += 10;
    else if (strncmp(raw, "Sorrow of ", 10) == 0)
        raw += 10;
    else if (strncmp(raw, "Doom of ", 8) == 0)
        raw += 8;

    return raw;
}

static cptr knowledge_blessing_display_name(int idx)
{
    if (cu_info[idx].blessing_name)
    {
        cptr raw = cu_name + cu_info[idx].blessing_name;

        if (strncmp(raw, "Blessing of ", 12) == 0)
            raw += 12;

        return raw;
    }

    return knowledge_curse_display_name(idx);
}
static bool knowledge_scene_add_rich_paragraph(app_ui_scene* scene,
    app_ui_panel* panel, byte attr, cptr text)
{
    if (!scene || !panel || !text || !text[0])
        return true;

    return app_ui_panel_begin_rich_paragraph(scene, panel)
        && app_ui_panel_add_rich_text(scene, panel, attr, text);
}

static app_ui_panel* knowledge_begin_curse_detail_scene(app_ui_scene* scene,
    cptr cname, cptr subtitle)
{
    app_ui_panel* panel;

    if (!scene)
        return NULL;

    app_ui_scene_init(scene);
    scene->flags |= APP_UI_SCENE_FLAG_DIM_BACKDROP;
    panel = app_ui_scene_append_panel(scene, APP_UI_LAYER_MODAL);
    if (!panel)
        return NULL;

    panel->style = APP_UI_PANEL_STYLE_PLAIN;
    panel->accent_attr = TERM_L_BLUE;
    app_ui_panel_set_widths(panel, 720, 1180);
    app_ui_panel_set_title(panel, TERM_L_RED, cname ? cname : "");
    if (subtitle && subtitle[0])
        app_ui_panel_set_subtitle(panel, TERM_L_GREEN, subtitle);

    return panel;
}

static bool knowledge_present_curse_detail_scene(app_ui_scene* scene,
    app_ui_panel* panel, bool steamdeck, cptr accept_label)
{
    if (!scene || !panel)
        return false;

    if (steamdeck)
    {
        (void)app_ui_panel_add_footer_action(panel, 1, TERM_L_BLUE, true,
            accept_label, "Continue");
    }
    else
    {
        (void)app_ui_panel_add_footer_action(panel, 1, TERM_L_BLUE, true,
            "Any", "Continue");
    }

    if (!ui_information_scene_present_ui(scene))
        return false;

    (void)ui_information_scene_wait_key_nonrepeat();
    return true;
}

static bool knowledge_show_curse_detail_ui(int curse_id)
{
    curse_type* c;
    cptr cname;
    cptr cdesc;
    cptr cpower;
    cptr bname;
    cptr bdesc;
    cptr bpower;
    bool steamdeck = steamdeck_controls_active();
    bool has_blessing_text;
    bool has_blessing_effect;
    bool has_blessing_info;
    bool show_blessing_name;
    char accept_label[16] = "";
    char effect_line[256];
    char blessing_line[256];
    char subtitle[APP_UI_TEXT_MAX];
    app_ui_scene scene;
    app_ui_panel* panel = NULL;

    if (curse_id < 0 || curse_id >= z_info->cu_max)
        return false;

    c = &cu_info[curse_id];
    cname = cu_name + c->name;
    cdesc = cu_text + c->text;
    cpower = cu_text + c->power;
    bname = knowledge_blessing_display_name(curse_id);
    bdesc = (c->blessing_text) ? (cu_text + c->blessing_text) : "";
    bpower = (c->blessing_power) ? (cu_text + c->blessing_power) : "";
    has_blessing_text = bdesc && *bdesc;
    has_blessing_effect = bpower && *bpower;
    has_blessing_info = has_blessing_text || has_blessing_effect
        || (c->blessing_name != 0);
    show_blessing_name = has_blessing_info && bname && bname[0]
        && strcmp(bname, cname) != 0;

    if (steamdeck)
    {
        controller_prompt_label(steamdeck_confirm_key(), "A", accept_label,
            sizeof(accept_label));
    }

    subtitle[0] = '\0';
    if (show_blessing_name)
        strnfmt(subtitle, sizeof(subtitle), "Blessing: %s", bname);

    panel = knowledge_begin_curse_detail_scene(&scene, cname, subtitle);
    if (!panel)
        return false;

    if (cdesc && cdesc[0]
        && !knowledge_scene_add_rich_paragraph(&scene, panel, TERM_WHITE, cdesc))
    {
        return false;
    }

    strnfmt(effect_line, sizeof(effect_line), "Effect: %s",
        (*cpower) ? cpower : "[no additional effect listed]");
    if (!knowledge_scene_add_rich_paragraph(&scene, panel, TERM_RED, effect_line))
        return false;

    if (has_blessing_info)
    {
        if (has_blessing_text
            && !knowledge_scene_add_rich_paragraph(&scene, panel, TERM_WHITE,
                bdesc))
        {
            return false;
        }

        strnfmt(blessing_line, sizeof(blessing_line), "Blessing effect: %s",
            has_blessing_effect ? bpower : "[no additional effect listed]");
        if (!knowledge_scene_add_rich_paragraph(&scene, panel, TERM_L_GREEN,
                blessing_line))
        {
            return false;
        }
    }

    return knowledge_present_curse_detail_scene(&scene, panel, steamdeck,
        accept_label);
}

static bool knowledge_handle_page_input(char ch, int* page)
{
    int next_page = *page;

    switch (ch)
    {
    case 'A':
    case 'a':
        next_page = KNOWLEDGE_PAGE_ARTEFACTS;
        break;
    case 'B':
    case 'b':
        next_page = KNOWLEDGE_PAGE_OBJECTS;
        break;
    case 'N':
    case 'n':
        next_page = KNOWLEDGE_PAGE_MONSTERS;
        break;
    case 'U':
    case 'u':
        next_page = KNOWLEDGE_PAGE_CURSES;
        break;
    case '\t':
    case ']':
    case 'I':
    case 'i':
        next_page = (*page + 1) % 4;
        break;
    case '[':
    case 'E':
    case 'e':
        next_page = (*page + 3) % 4;
        break;
    default:
        return false;
    }

    *page = next_page;
    g_knowledge_last_page = next_page;
    return true;
}

static bool knowledge_handle_tab_navigation(char ch, int* page, bool* tabs_focus,
    bool can_focus_tabs)
{
    int d = target_dir(ch);

    if (!*tabs_focus)
    {
        if (can_focus_tabs && d && !ddx[d] && (ddy[d] < 0))
        {
            *tabs_focus = true;
            return true;
        }

        return false;
    }

    if (d)
    {
        if (ddx[d] > 0)
        {
            *page = (*page + 1) % 4;
            g_knowledge_last_page = *page;
            return true;
        }
        if (ddx[d] < 0)
        {
            *page = (*page + 3) % 4;
            g_knowledge_last_page = *page;
            return true;
        }
        if (ddy[d] > 0)
        {
            *tabs_focus = false;
            return true;
        }
        if (ddy[d] < 0)
        {
            return true;
        }
    }

    return false;
}

static bool knowledge_is_recall_input(int ch)
{
    int confirm_key = steamdeck_confirm_key();

    if (ch == ' ' || ch == 'R' || ch == 'r' || ch == 'X' || ch == 'x'
        || ch == INPUT_BIND_CONFIRM)
    {
        return true;
    }

    if (confirm_key != GAMEPAD_BIND_NONE && ch == confirm_key)
        return true;

    return false;
}

static void knowledge_scene_add_tabs(app_ui_panel* panel, int page,
    bool tabs_focus)
{
    int i;

    if (!panel)
        return;

    for (i = KNOWLEDGE_PAGE_ARTEFACTS; i <= KNOWLEDGE_PAGE_CURSES; i++)
    {
        byte attr = TERM_SLATE;

        if (i == page)
            attr = tabs_focus ? TERM_YELLOW : TERM_L_BLUE;
        (void)app_ui_panel_add_tab(panel, (s16b)i, attr, i == page,
            knowledge_tab_label(i));
    }
}

static void knowledge_scene_add_footer_actions(app_ui_panel* panel,
    bool has_groups, bool can_recall)
{
    if (!panel)
        return;

    (void)app_ui_panel_add_footer_action(panel, 1, TERM_L_BLUE, true,
        "e", "Prev page");
    (void)app_ui_panel_add_footer_action(panel, 2, TERM_L_BLUE, true,
        "i", "Next page");
    if (has_groups)
    {
        (void)app_ui_panel_add_footer_action(panel, 3, TERM_WHITE, true,
            "4/6", "Group");
    }
    (void)app_ui_panel_add_footer_action(panel, 4, TERM_WHITE, true,
        "8/2", "Move");
    (void)app_ui_panel_add_footer_action(panel, 5, TERM_WHITE, can_recall,
        "r", "Recall");
    (void)app_ui_panel_add_footer_action(panel, 6, TERM_WHITE, true,
        "Esc", "Back");
}

static app_ui_panel* knowledge_scene_begin(app_ui_scene* scene, int page,
    bool tabs_focus, cptr status)
{
    app_ui_panel* panel;

    if (!scene)
        return NULL;

    app_ui_scene_init(scene);
    panel = app_ui_scene_append_panel(scene, APP_UI_LAYER_BROWSER);
    if (!panel)
        return NULL;

    panel->style = APP_UI_PANEL_STYLE_BROWSER;
    panel->flags |= APP_UI_PANEL_FLAG_TOP_ANCHORED
        | APP_UI_PANEL_FLAG_LEFT_ANCHORED
        | APP_UI_PANEL_FLAG_SCROLL_ROWS;
    panel->accent_attr = TERM_L_BLUE;
    app_ui_panel_set_widths(panel, 980, 2048);
    app_ui_panel_set_title(panel, TERM_L_WHITE, "Known lore");
    app_ui_panel_set_subtitle(panel, TERM_SLATE, "");
    knowledge_scene_add_tabs(panel, page, tabs_focus);
    if (status && status[0])
        (void)app_ui_panel_add_body_line(panel, TERM_L_BLUE, status);

    return panel;
}

static void knowledge_scene_add_group_detail_lines(app_ui_panel* panel,
    int grp_idx[], cptr group_text[], int grp_cnt, int grp_cur, int grp_top)
{
    int start;
    int i;

    if (!panel || !grp_idx || !group_text || grp_cnt <= 0)
        return;

    panel->flags |= APP_UI_PANEL_FLAG_DETAIL_LEADING;
    app_ui_panel_set_detail_title(panel, TERM_SLATE, "Groups");
    start = grp_top;
    if (start < 0)
        start = 0;
    if (start >= grp_cnt)
        start = grp_cnt - 1;
    if (start < 0)
        start = 0;
    if ((start + (int)APP_UI_DETAIL_LINE_MAX) > grp_cnt)
        start = MAX(0, grp_cnt - (int)APP_UI_DETAIL_LINE_MAX);

    for (i = start; i < grp_cnt && panel->detail_line_count < APP_UI_DETAIL_LINE_MAX;
        i++)
    {
        char buf[APP_UI_TEXT_MAX];
        bool selected = (i == grp_cur);
        byte attr = selected ? TERM_L_BLUE : TERM_WHITE;

        strnfmt(buf, sizeof(buf), "%c %s", selected ? '>' : ' ',
            group_text[grp_idx[i]]);
        (void)app_ui_panel_add_detail_line(panel, attr, buf);
    }
}

static void knowledge_scene_set_focus(app_ui_panel* panel, bool tabs_focus)
{
    if (!panel)
        return;

    if (tabs_focus && panel->tab_count > 0)
    {
        panel->focus_area = APP_UI_FOCUS_TABS;
        return;
    }
    if (panel->row_count > 0)
    {
        panel->focus_area = APP_UI_FOCUS_ROWS;
        panel->focus_id = (panel->selected_row >= 0)
            ? panel->rows[panel->selected_row].id
            : panel->rows[0].id;
    }
}

static void knowledge_scene_append_artefact_rows(app_ui_panel* panel,
    int artefact_idx[], int artefact_cnt, int artefact_cur)
{
    int i;

    if (!panel || !artefact_idx || artefact_cnt <= 0)
        return;

    for (i = 0; i < artefact_cnt; i++)
    {
        object_type object_type_body;
        object_type* o_ptr = &object_type_body;
        artefact_type* a_ptr;
        char label[APP_UI_LABEL_MAX];
        char meta[APP_UI_META_MAX];

        object_wipe(o_ptr);
        if (!prepare_fake_artefact(o_ptr, (byte)artefact_idx[i]))
            continue;

        a_ptr = &a_info[artefact_idx[i]];
        object_desc(label, sizeof(label), o_ptr, true, 0);
        meta[0] = '\0';
        if (cheat_know)
        {
            strnfmt(meta, sizeof(meta), "#%d L%d R%d", artefact_idx[i],
                a_ptr->level, a_ptr->rarity);
        }

        if (!app_ui_panel_add_row_ex(panel, (s16b)i, TERM_WHITE, TERM_SLATE,
                object_attr(o_ptr), object_char(o_ptr), true,
                i == artefact_cur, "", label, meta))
        {
            break;
        }
    }
}

static void knowledge_scene_append_object_rows(app_ui_panel* panel,
    object_list_entry object_idx[], int object_cnt, int object_cur)
{
    int i;

    if (!panel || !object_idx || object_cnt <= 0)
        return;

    for (i = 0; i < object_cnt; i++)
    {
        object_list_entry* obj = &object_idx[i];
        char label[APP_UI_LABEL_MAX];
        char meta[APP_UI_META_MAX];
        byte attr = TERM_WHITE;
        byte icon_attr = 0;
        char icon_char = '\0';

        label[0] = '\0';
        meta[0] = '\0';

        switch (obj->type)
        {
        case OBJ_NORMAL:
        {
            object_kind* k_ptr = &k_info[obj->idx];

            attr = k_ptr->aware ? TERM_WHITE : TERM_SLATE;
            strip_name(label, obj->idx);
            if (k_ptr->aware)
            {
                object_type object_type_body;
                object_type* o_ptr = &object_type_body;

                object_wipe(o_ptr);
                object_prep(o_ptr, obj->idx);
                o_ptr->ident |= IDENT_KNOWN;
                icon_attr = object_attr(o_ptr);
                icon_char = object_char(o_ptr);
            }
            if (cheat_know)
                strnfmt(meta, sizeof(meta), "#%d", obj->idx);
            break;
        }

        case OBJ_SPECIAL:
        {
            ego_item_type* e_ptr = &e_info[obj->e_idx];

            attr = e_ptr->aware ? TERM_WHITE : TERM_SLATE;
            if (obj->sval == -1)
            {
                strnfmt(label, sizeof(label), "%s", &e_name[e_ptr->name]);
            }
            else
            {
                int j;
                char base_name[80];

                base_name[0] = '\0';
                for (j = 0; j < z_info->k_max; ++j)
                {
                    if ((k_info[j].tval == obj->tval)
                        && (k_info[j].sval == obj->sval))
                    {
                        strip_name(base_name, j);
                        break;
                    }
                }

                strnfmt(label, sizeof(label), "%s %s", base_name,
                    &e_name[e_ptr->name]);
            }
            if (cheat_know)
                SDL_strlcpy(meta, "ego", sizeof(meta));
            break;
        }

        case OBJ_NONE:
        default:
            continue;
        }

        if (!app_ui_panel_add_row_ex(panel, (s16b)i, attr, TERM_SLATE,
                icon_attr, icon_char, true, i == object_cur, "", label, meta))
        {
            break;
        }
    }
}

static void knowledge_scene_append_monster_rows(app_ui_panel* panel,
    monster_list_entry mon_idx[], int mon_cnt, int mon_cur)
{
    int i;

    if (!panel || !mon_idx || mon_cnt <= 0)
        return;

    for (i = 0; i < mon_cnt; i++)
    {
        int r_idx = mon_idx[i].r_idx;
        monster_race* r_ptr = &r_info[r_idx];
        monster_lore* l_ptr = &l_list[r_idx];
        char label[APP_UI_LABEL_MAX];
        char meta[APP_UI_META_MAX];
        byte meta_attr = TERM_SLATE;

        monster_desc_race(label, sizeof(label), r_idx);
        if (r_ptr->flags1 & RF1_UNIQUE)
        {
            SDL_strlcpy(meta, (r_ptr->max_num == 0) ? "dead" : "alive",
                sizeof(meta));
            meta_attr = (r_ptr->max_num == 0) ? TERM_L_RED : TERM_L_GREEN;
        }
        else
        {
            strnfmt(meta, sizeof(meta), "%d", l_ptr->pkills);
        }

        if (!app_ui_panel_add_row_ex(panel, (s16b)i, TERM_WHITE, meta_attr,
                r_ptr->x_attr, r_ptr->x_char, true, i == mon_cur, "",
                label, meta))
        {
            break;
        }
    }
}

static void knowledge_scene_append_curse_rows(app_ui_panel* panel,
    int curse_idx[], int curse_cnt, int curse_cur)
{
    int i;

    if (!panel || !curse_idx || curse_cnt <= 0)
        return;

    for (i = 0; i < curse_cnt; i++)
    {
        char meta[APP_UI_META_MAX];
        cptr blessing = knowledge_blessing_display_name(curse_idx[i]);

        meta[0] = '\0';
        if (blessing && blessing[0]
            && strcmp(blessing, knowledge_curse_display_name(curse_idx[i])) != 0)
        {
            SDL_strlcpy(meta, blessing, sizeof(meta));
        }

        if (!app_ui_panel_add_row_ex(panel, (s16b)i, TERM_L_RED,
                meta[0] ? TERM_L_GREEN : TERM_SLATE, 0, '\0', true,
                i == curse_cur, "", knowledge_curse_display_name(curse_idx[i]),
                meta))
        {
            break;
        }
    }
}

static void knowledge_scene_add_curse_detail(app_ui_panel* panel, int curse_id)
{
    curse_type* curse;
    cptr blessing;
    cptr power;
    char buf[APP_UI_TEXT_MAX];

    if (!panel || curse_id < 0)
        return;

    curse = &cu_info[curse_id];
    blessing = knowledge_blessing_display_name(curse_id);
    power = cu_text + curse->power;

    app_ui_panel_set_detail_title(panel, TERM_L_RED,
        knowledge_curse_display_name(curse_id));
    if (blessing && blessing[0]
        && strcmp(blessing, knowledge_curse_display_name(curse_id)) != 0)
    {
        strnfmt(buf, sizeof(buf), "Blessing: %s", blessing);
        (void)app_ui_panel_add_detail_line(panel, TERM_L_GREEN, buf);
    }

    strnfmt(buf, sizeof(buf), "Effect: %s",
        (power && power[0]) ? power : "[no additional effect listed]");
    (void)app_ui_panel_add_detail_line(panel, TERM_WHITE, buf);
}

static bool knowledge_build_artefact_browser_scene(app_ui_scene* scene, int page,
    bool tabs_focus, int grp_idx[], int grp_cnt, int grp_cur, int grp_top,
    int artefact_idx[], int artefact_cnt, int artefact_top, int artefact_cur)
{
    app_ui_panel* panel;
    char status[APP_UI_TEXT_MAX];

    if (artefact_cnt > 0)
    {
        strnfmt(status, sizeof(status), "%d artefact%s in %s.",
            artefact_cnt, (artefact_cnt == 1) ? "" : "s",
            object_group_text[grp_idx[grp_cur]]);
    }
    else
    {
        SDL_strlcpy(status, "No known artefacts yet.", sizeof(status));
    }

    panel = knowledge_scene_begin(scene, page, tabs_focus, status);
    if (!panel)
        return false;

    app_ui_panel_set_title(panel, TERM_L_WHITE, "Known lore - Artefacts");
    app_ui_panel_set_subtitle(panel, TERM_SLATE, "Artefact");
    knowledge_scene_add_group_detail_lines(panel, grp_idx, object_group_text,
        grp_cnt, grp_cur, grp_top);
    knowledge_scene_append_artefact_rows(panel, artefact_idx, artefact_cnt,
        artefact_cur);
    app_ui_panel_set_row_offset(panel, (s16b)artefact_top);
    knowledge_scene_add_footer_actions(panel, true, artefact_cnt > 0);
    knowledge_scene_set_focus(panel, tabs_focus);
    return true;
}

static bool knowledge_build_object_browser_scene(app_ui_scene* scene, int page,
    bool tabs_focus, int grp_idx[], int grp_cnt, int grp_cur, int grp_top,
    object_list_entry object_idx[], int object_cnt, int object_top,
    int object_cur)
{
    app_ui_panel* panel;
    char status[APP_UI_TEXT_MAX];

    if (object_cnt > 0)
    {
        object_list_entry* obj = &object_idx[object_cur];

        if ((obj->type == OBJ_NORMAL) && k_info[obj->idx].aware)
        {
            strnfmt(status, sizeof(status),
                "%d object%s in %s. Recall available.", object_cnt,
                (object_cnt == 1) ? "" : "s",
                object_group_text[grp_idx[grp_cur]]);
        }
        else
        {
            strnfmt(status, sizeof(status),
                "%d object%s in %s. Recall works for identified base items.",
                object_cnt, (object_cnt == 1) ? "" : "s",
                object_group_text[grp_idx[grp_cur]]);
        }
    }
    else
    {
        SDL_strlcpy(status, "No known objects yet.", sizeof(status));
    }

    panel = knowledge_scene_begin(scene, page, tabs_focus, status);
    if (!panel)
        return false;

    app_ui_panel_set_title(panel, TERM_L_WHITE, "Known lore - Objects");
    app_ui_panel_set_subtitle(panel, TERM_SLATE, "Object");
    knowledge_scene_add_group_detail_lines(panel, grp_idx, object_group_text,
        grp_cnt, grp_cur, grp_top);
    knowledge_scene_append_object_rows(panel, object_idx, object_cnt,
        object_cur);
    app_ui_panel_set_row_offset(panel, (s16b)object_top);
    knowledge_scene_add_footer_actions(panel, true,
        (object_cnt > 0)
        && (object_idx[object_cur].type == OBJ_NORMAL)
        && k_info[object_idx[object_cur].idx].aware);
    knowledge_scene_set_focus(panel, tabs_focus);
    return true;
}

static bool knowledge_build_monster_browser_scene(app_ui_scene* scene, int page,
    bool tabs_focus, int grp_idx[], int grp_cnt, int grp_cur, int grp_top,
    monster_list_entry mon_idx[], int mon_cnt, int mon_top, int mon_cur)
{
    app_ui_panel* panel;
    char status[APP_UI_TEXT_MAX];

    if (mon_cnt > 0)
        knowledge_monster_summary(status, sizeof(status), grp_idx[grp_cur]);
    else
        SDL_strlcpy(status, "No known monsters in this group yet.",
            sizeof(status));

    panel = knowledge_scene_begin(scene, page, tabs_focus, status);
    if (!panel)
        return false;

    app_ui_panel_set_title(panel, TERM_L_WHITE, "Known lore - Monsters");
    app_ui_panel_set_subtitle(panel, TERM_SLATE, "Monster");
    knowledge_scene_add_group_detail_lines(panel, grp_idx, monster_group_text,
        grp_cnt, grp_cur, grp_top);
    knowledge_scene_append_monster_rows(panel, mon_idx, mon_cnt, mon_cur);
    app_ui_panel_set_row_offset(panel, (s16b)mon_top);
    knowledge_scene_add_footer_actions(panel, true, mon_cnt > 0);
    knowledge_scene_set_focus(panel, tabs_focus);
    return true;
}

static bool knowledge_build_curse_browser_scene(app_ui_scene* scene, int page,
    bool tabs_focus, int curse_idx[], int curse_cnt, int curse_top,
    int curse_cur)
{
    app_ui_panel* panel;
    char status[APP_UI_TEXT_MAX];

    if (curse_cnt > 0)
    {
        curse_type* c = &cu_info[curse_idx[curse_cur]];
        cptr power = cu_text + c->power;

        strnfmt(status, sizeof(status), "Effect: %s",
            (power && power[0]) ? power : "[no additional effect listed]");
    }
    else
    {
        SDL_strlcpy(status, "No known curses yet.", sizeof(status));
    }

    panel = knowledge_scene_begin(scene, page, tabs_focus, status);
    if (!panel)
        return false;

    app_ui_panel_set_title(panel, TERM_L_WHITE, "Known lore - Curses");
    app_ui_panel_set_subtitle(panel, TERM_SLATE, "Known curses");
    knowledge_scene_append_curse_rows(panel, curse_idx, curse_cnt, curse_cur);
    if (curse_cnt > 0)
        knowledge_scene_add_curse_detail(panel, curse_idx[curse_cur]);
    app_ui_panel_set_row_offset(panel, (s16b)curse_top);
    knowledge_scene_add_footer_actions(panel, false, curse_cnt > 0);
    knowledge_scene_set_focus(panel, tabs_focus);
    return true;
}

static bool knowledge_build_root_menu_scene(app_ui_scene* scene, int selected)
{
    static const struct {
        int id;
        cptr key;
        cptr label;
    } entries[] = {
        { 1, "1", "Display known lore browser" },
        { 2, "2", "Display supplies overview" },
        { 3, "3", "Display names of the fallen" },
        { 4, "4", "Display kill counts" },
        { 5, "5", "Display character notes file" },
        { 6, "6", "Display oath status" }
    };
    app_ui_panel* panel;
    size_t i;

    panel = knowledge_scene_begin(scene, KNOWLEDGE_PAGE_ARTEFACTS, false,
        "Browser, history, and oath records.");
    if (!panel)
        return false;

    app_ui_panel_set_title(panel, TERM_L_WHITE, "Display current knowledge");
    app_ui_panel_set_subtitle(panel, TERM_SLATE, "Choose a screen");
    panel->tab_count = 0;
    panel->body_line_count = 0;

    for (i = 0; i < N_ELEMENTS(entries); i++)
    {
        if (!app_ui_panel_add_row(panel, (s16b)entries[i].id, TERM_WHITE, true,
                (int)i == selected, entries[i].key, entries[i].label, ""))
        {
            return false;
        }
    }

    (void)app_ui_panel_add_footer_action(panel, 1, TERM_L_BLUE, true,
        "Enter", "Select");
    (void)app_ui_panel_add_footer_action(panel, 2, TERM_WHITE, true,
        "Esc", "Back");
    knowledge_scene_set_focus(panel, false);
    return true;
}

static void knowledge_scene_add_supply_footer_actions(app_ui_panel* panel)
{
    char recall_key[APP_UI_KEY_MAX];
    char use_key[APP_UI_KEY_MAX];
    char confirm_key[APP_UI_KEY_MAX];
    char drop_key[APP_UI_KEY_MAX];
    char back_key[APP_UI_KEY_MAX];
    char use_confirm_key[APP_UI_KEY_MAX];

    if (!panel)
        return;

    if (steamdeck_controls_active())
    {
        controller_prompt_label(steamdeck_info_key(), "RS", recall_key,
            sizeof(recall_key));
        controller_prompt_label(steamdeck_alt_action_key(), "X", use_key,
            sizeof(use_key));
        controller_prompt_label(steamdeck_confirm_key(), "A", confirm_key,
            sizeof(confirm_key));
        controller_prompt_label('d', "d", drop_key, sizeof(drop_key));
        controller_prompt_label(steamdeck_back_key(), "B", back_key,
            sizeof(back_key));
        strnfmt(use_confirm_key, sizeof(use_confirm_key), "%s/%s", use_key,
            confirm_key);

        (void)app_ui_panel_add_footer_action(panel, 1, TERM_WHITE, true,
            "D-pad", "Move");
        (void)app_ui_panel_add_footer_action(panel, 2, TERM_WHITE, true,
            recall_key, "Recall");
        (void)app_ui_panel_add_footer_action(panel, 3, TERM_WHITE, true,
            use_confirm_key, "Use");
        (void)app_ui_panel_add_footer_action(panel, 4, TERM_WHITE, true,
            drop_key, "Drop");
        (void)app_ui_panel_add_footer_action(panel, 5, TERM_WHITE, true,
            back_key, "Back");
        return;
    }

    (void)app_ui_panel_add_footer_action(panel, 1, TERM_WHITE, true,
        "4/6", "Group");
    (void)app_ui_panel_add_footer_action(panel, 2, TERM_WHITE, true,
        "8/2", "Move");
    (void)app_ui_panel_add_footer_action(panel, 3, TERM_WHITE, true,
        "r", "Recall");
    (void)app_ui_panel_add_footer_action(panel, 4, TERM_WHITE, true,
        "u/Space", "Use");
    (void)app_ui_panel_add_footer_action(panel, 5, TERM_WHITE, true,
        "d", "Drop");
    (void)app_ui_panel_add_footer_action(panel, 6, TERM_WHITE, true,
        "Esc", "Back");
}

static void knowledge_scene_add_supply_group_detail_lines(app_ui_panel* panel,
    int grp_idx[], int grp_cnt, int grp_cur, int grp_top, int group_totals[])
{
    int start;
    int i;

    if (!panel || !grp_idx || !group_totals || grp_cnt <= 0)
        return;

    panel->flags |= APP_UI_PANEL_FLAG_DETAIL_LEADING;
    app_ui_panel_set_detail_title(panel, TERM_SLATE, "Group");

    start = grp_top;
    if (start < 0)
        start = 0;
    if (start >= grp_cnt)
        start = grp_cnt - 1;
    if ((start + (int)APP_UI_DETAIL_LINE_MAX) > grp_cnt)
        start = MAX(0, grp_cnt - (int)APP_UI_DETAIL_LINE_MAX);

    for (i = start; i < grp_cnt && panel->detail_line_count < APP_UI_DETAIL_LINE_MAX;
        i++)
    {
        int grp = grp_idx[i];
        byte attr;
        char buf[APP_UI_TEXT_MAX];

        switch (grp)
        {
        case SUPPLY_GROUP_HERBS:
            attr = TERM_GREEN;
            break;
        case SUPPLY_GROUP_FOOD:
            attr = TERM_L_GREEN;
            break;
        case SUPPLY_GROUP_POTIONS:
            attr = TERM_VIOLET;
            break;
        case SUPPLY_GROUP_GEMS:
            attr = TERM_BLUE;
            break;
        case SUPPLY_GROUP_LIGHTS:
            attr = TERM_YELLOW;
            break;
        default:
            attr = TERM_WHITE;
            break;
        }

        if (i == grp_cur)
            attr = TERM_L_WHITE;
        else if (group_totals[grp] == 0)
            attr = TERM_L_DARK;

        strnfmt(buf, sizeof(buf), "%-8s %3d", supply_group_text[grp],
            group_totals[grp]);
        (void)app_ui_panel_add_detail_line(panel, attr, buf);
    }
}

static void knowledge_scene_append_supply_rows(app_ui_panel* panel,
    supply_list_entry entries[], int entry_cnt, int entry_cur, bool focus_rows,
    int current_group)
{
    int i;

    if (!panel || !entries || entry_cnt <= 0)
        return;

    for (i = 0; i < entry_cnt; i++)
    {
        supply_list_entry* entry = &entries[i];
        object_type* o_ptr;
        object_type fake;
        object_kind* k_ptr;
        bool aware;
        byte base_attr;
        byte cursor_attr;
        byte attr;
        char name[APP_UI_LABEL_MAX];
        char meta[APP_UI_META_MAX];

        if (entry->k_idx < 0 || entry->k_idx >= z_info->k_max)
            continue;

        k_ptr = &k_info[entry->k_idx];
        aware = k_ptr->aware;

        if (entry->total == 0)
        {
            base_attr = TERM_L_DARK;
            cursor_attr = TERM_SLATE;
        }
        else
        {
            base_attr = get_supply_item_color(entry->k_idx, aware);
            cursor_attr = aware ? TERM_L_WHITE : TERM_WHITE;
        }

        attr = (focus_rows && i == entry_cur) ? cursor_attr : base_attr;

        if ((entry->item_idx >= 0) && (entry->item_idx < INVEN_PACK))
        {
            o_ptr = &inventory[entry->item_idx];
        }
        else if (entry->item_idx == INVEN_LITE)
        {
            o_ptr = &inventory[INVEN_LITE];
        }
        else
        {
            object_wipe(&fake);
            object_prep(&fake, entry->k_idx);
            if (aware)
                fake.ident |= IDENT_KNOWN;
            fake.number = (entry->total > 0) ? entry->total : 1;
            o_ptr = &fake;
        }

        object_desc(name, sizeof(name), o_ptr, true, 3);
        if (current_group == SUPPLY_GROUP_FOOD)
            append_supply_item_weight(name, sizeof(name), o_ptr,
                entry->total > 1);
        else if (current_group == SUPPLY_GROUP_LIGHTS)
            append_supply_item_weight(name, sizeof(name), o_ptr, false);
        if ((current_group == SUPPLY_GROUP_LIGHTS) && (entry->item_idx == INVEN_LITE))
            SDL_strlcat(name, " [equipped]", sizeof(name));
        strnfmt(meta, sizeof(meta), "x%-3d", entry->total);

        if (!app_ui_panel_add_row_ex(panel, (s16b)i, attr, attr,
                object_attr(o_ptr), object_char(o_ptr), true, false, "",
                name, meta))
        {
            break;
        }
    }
}

static bool knowledge_build_supplies_browser_scene(app_ui_scene* scene,
    int grp_idx[], int grp_cnt, int grp_cur, int grp_top, int group_totals[],
    supply_list_entry entries[], int entry_cnt, int entry_top, int entry_cur,
    int column, cptr weight_text)
{
    app_ui_panel* panel;

    if (!scene)
        return false;

    app_ui_scene_init(scene);
    panel = app_ui_scene_append_panel(scene, APP_UI_LAYER_BROWSER);
    if (!panel)
        return false;

    panel->style = APP_UI_PANEL_STYLE_BROWSER;
    panel->flags |= APP_UI_PANEL_FLAG_TOP_ANCHORED
        | APP_UI_PANEL_FLAG_LEFT_ANCHORED
        | APP_UI_PANEL_FLAG_SCROLL_ROWS;
    panel->accent_attr = TERM_L_WHITE;
    app_ui_panel_set_widths(panel, 980, 2048);
    app_ui_panel_set_title(panel, TERM_L_WHITE,
        "Supplies - Herbs, Food, Potions, Gems, Lights");
    app_ui_panel_set_subtitle(panel, TERM_SLATE, "Name / Qty / Sym");
    if (weight_text && weight_text[0])
        (void)app_ui_panel_add_body_line(panel, TERM_SLATE, weight_text);
    {
        char status_buf[96];
        describe_supply_group_status(grp_idx[grp_cur], status_buf,
            sizeof(status_buf));
        if (status_buf[0])
            (void)app_ui_panel_add_body_line(panel, TERM_L_BLUE, status_buf);
    }

    knowledge_scene_add_supply_group_detail_lines(panel, grp_idx, grp_cnt,
        grp_cur, grp_top, group_totals);
    knowledge_scene_append_supply_rows(panel, entries, entry_cnt, entry_cur,
        column == 1, grp_idx[grp_cur]);
    app_ui_panel_set_row_offset(panel, (s16b)entry_top);
    knowledge_scene_add_supply_footer_actions(panel);

    if (column == 0 && panel->detail_line_count > 0)
    {
        panel->focus_area = APP_UI_FOCUS_DETAIL;
    }
    else if (panel->row_count > 0)
    {
        panel->focus_area = APP_UI_FOCUS_ROWS;
        panel->focus_id = panel->rows[MIN(entry_cur, (int)panel->row_count - 1)].id;
    }

    return true;
}

void do_cmd_knowledge_browser_page(int page)
{
    ui_information_scene_scope info_scope;
    int page_rows = BROWSER_ROWS;
    int i;
    int artefact_grp_idx[100];
    int object_grp_idx[100];
    int monster_grp_idx[100];
    int* artefact_idx = NULL;
    object_list_entry* object_idx = NULL;
    monster_list_entry* mon_idx = NULL;
    int* curse_idx = NULL;
    int artefact_grp_cnt = 0;
    int object_grp_cnt = 0;
    int monster_grp_cnt = 0;
    int curse_cnt = 0;
    int artefact_old = -1;
    int object_old = -1;
    int monster_old = -1;
    knowledge_browser_state state = { 0 };
    bool done = false;

    page = knowledge_normalize_page(page);
    g_knowledge_last_page = page;

    FILE_TYPE(FILE_TYPE_TEXT);

    if (!knowledge_enter_information_scene_or_report(&info_scope,
            "knowledge browser",
            "Known lore browser unavailable."))
    {
        return;
    }

    artefact_idx = mem_alloc_array(z_info->art_max, int);
    object_idx = mem_alloc_array(z_info->k_max + z_info->e_max + 1,
        object_list_entry);
    mon_idx = mem_alloc_array(z_info->r_max, monster_list_entry);
    curse_idx = mem_alloc_array(z_info->cu_max, int);

    for (i = 0; object_group_text[i] != NULL; i++)
    {
        if (collect_artefacts(i, artefact_idx))
            artefact_grp_idx[artefact_grp_cnt++] = i;
        if (collect_objects(i, NULL))
            object_grp_idx[object_grp_cnt++] = i;
    }

    for (i = 0; monster_group_text[i] != NULL; i++)
    {
        if ((monster_group_char[i] == (char*)-1L)
            || collect_monsters(i, mon_idx, 0x01))
        {
            monster_grp_idx[monster_grp_cnt++] = i;
        }
    }

    curse_cnt = knowledge_collect_curses(curse_idx);

    if (p_ptr && p_ptr->playing)
        sdl_music_play_menu_theme();

    while (!done)
    {
        int ch;

        switch (page)
        {
        case KNOWLEDGE_PAGE_ARTEFACTS:
        {
            int artefact_cnt = 0;
            int selected_artefact = -1;

            if (artefact_grp_cnt > 0)
                artefact_cnt = collect_artefacts(
                    artefact_grp_idx[state.group_cur[page]], artefact_idx);
            knowledge_clamp_group_state(&state.column[page], &state.group_cur[page],
                &state.group_top[page], artefact_grp_cnt, &state.entry_cur[page],
                &state.entry_top[page], artefact_cnt, page_rows);
            if (artefact_grp_cnt > 0)
                artefact_cnt = collect_artefacts(
                    artefact_grp_idx[state.group_cur[page]], artefact_idx);

            if (artefact_cnt > 0)
            {
                selected_artefact = artefact_idx[state.entry_cur[page]];
            }

            if (selected_artefact != artefact_old)
            {
                handle_stuff();
                artefact_old = selected_artefact;
            }

            {
                app_ui_scene scene;

                if (!knowledge_present_ui_scene_or_abort(&info_scope,
                        knowledge_build_artefact_browser_scene(&scene, page,
                            state.tabs_focus, artefact_grp_idx,
                            artefact_grp_cnt, state.group_cur[page],
                            state.group_top[page], artefact_idx, artefact_cnt,
                            state.entry_top[page], state.entry_cur[page]),
                        &scene, "knowledge artefact browser",
                        "Artefact knowledge screen unavailable."))
                {
                    goto cleanup;
                }
            }

            ch = ui_information_scene_wait_key();
            if (steamdeck_controls_active() && ch == steamdeck_back_key())
                ch = ESCAPE;

            if (knowledge_handle_tab_navigation((char)ch, &page,
                &state.tabs_focus,
                (artefact_grp_cnt <= 0) || ((state.column[page] == 0)
                    ? (state.group_cur[page] == 0)
                    : (state.entry_cur[page] == 0))))
            {
                break;
            }

            if (knowledge_handle_page_input((char)ch, &page))
                break;

            if (knowledge_is_recall_input(ch))
            {
                if (artefact_cnt > 0)
                {
                    if (!knowledge_pause_information_scene(&info_scope))
                        goto cleanup;
                    desc_art_fake(artefact_idx[state.entry_cur[page]]);
                    if (!knowledge_resume_information_scene(&info_scope))
                        goto cleanup;
                }
                else
                    bell("Nothing to recall.");
                break;
            }

            switch (ch)
            {
            case ESCAPE:
                done = true;
                break;

            default:
                browser_cursor_with_rows((char)ch, &state.column[page],
                    &state.group_cur[page], artefact_grp_cnt,
                    &state.entry_cur[page], artefact_cnt, page_rows);
                break;
            }
            break;
        }

        case KNOWLEDGE_PAGE_OBJECTS:
        {
            int object_cnt = 0;
            int tracked_kind = 0;

            if (object_grp_cnt > 0)
                object_cnt = collect_objects(
                    object_grp_idx[state.group_cur[page]], object_idx);
            knowledge_clamp_group_state(&state.column[page], &state.group_cur[page],
                &state.group_top[page], object_grp_cnt, &state.entry_cur[page],
                &state.entry_top[page], object_cnt, page_rows);
            if (object_grp_cnt > 0)
                object_cnt = collect_objects(
                    object_grp_idx[state.group_cur[page]], object_idx);

            if ((object_cnt > 0)
                && (object_idx[state.entry_cur[page]].type == OBJ_NORMAL))
            {
                tracked_kind = object_idx[state.entry_cur[page]].idx;
            }

            if (tracked_kind != object_old)
            {
                object_kind_track(tracked_kind);
                handle_stuff();
                object_old = tracked_kind;
            }

            {
                app_ui_scene scene;

                if (!knowledge_present_ui_scene_or_abort(&info_scope,
                        knowledge_build_object_browser_scene(&scene, page,
                            state.tabs_focus, object_grp_idx, object_grp_cnt,
                            state.group_cur[page], state.group_top[page],
                            object_idx, object_cnt, state.entry_top[page],
                            state.entry_cur[page]),
                        &scene, "knowledge object browser",
                        "Object knowledge screen unavailable."))
                {
                    goto cleanup;
                }
            }

            ch = ui_information_scene_wait_key();
            if (steamdeck_controls_active() && ch == steamdeck_back_key())
                ch = ESCAPE;

            if (knowledge_handle_tab_navigation((char)ch, &page,
                &state.tabs_focus,
                (object_grp_cnt <= 0) || ((state.column[page] == 0)
                    ? (state.group_cur[page] == 0)
                    : (state.entry_cur[page] == 0))))
            {
                break;
            }

            if (knowledge_handle_page_input((char)ch, &page))
                break;

            if (knowledge_is_recall_input(ch))
            {
                if ((object_cnt > 0)
                    && (object_idx[state.entry_cur[page]].type == OBJ_NORMAL)
                    && k_info[object_idx[state.entry_cur[page]].idx].aware)
                {
                    if (!knowledge_pause_information_scene(&info_scope))
                        goto cleanup;
                    desc_obj_fake(object_idx[state.entry_cur[page]].idx);
                    if (!knowledge_resume_information_scene(&info_scope))
                        goto cleanup;
                }
                else
                {
                    bell("Nothing to recall.");
                }
                break;
            }

            switch (ch)
            {
            case ESCAPE:
                done = true;
                break;

            default:
                browser_cursor_with_rows((char)ch, &state.column[page],
                    &state.group_cur[page], object_grp_cnt,
                    &state.entry_cur[page], object_cnt, page_rows);
                break;
            }
            break;
        }

        case KNOWLEDGE_PAGE_MONSTERS:
        {
            int monster_cnt = 0;
            int selected_r_idx = 0;

            if (monster_grp_cnt > 0)
                monster_cnt = collect_monsters(
                    monster_grp_idx[state.group_cur[page]], mon_idx, 0x00);
            knowledge_clamp_group_state(&state.column[page], &state.group_cur[page],
                &state.group_top[page], monster_grp_cnt, &state.entry_cur[page],
                &state.entry_top[page], monster_cnt, page_rows);
            if (monster_grp_cnt > 0)
                monster_cnt = collect_monsters(
                    monster_grp_idx[state.group_cur[page]], mon_idx, 0x00);

            if (monster_cnt > 0)
                selected_r_idx = mon_idx[state.entry_cur[page]].r_idx;

            if (selected_r_idx != monster_old)
            {
                monster_race_track(selected_r_idx);
                handle_stuff();
                monster_old = selected_r_idx;
            }

            {
                app_ui_scene scene;

                if (!knowledge_present_ui_scene_or_abort(&info_scope,
                        knowledge_build_monster_browser_scene(&scene, page,
                            state.tabs_focus, monster_grp_idx,
                            monster_grp_cnt, state.group_cur[page],
                            state.group_top[page], mon_idx, monster_cnt,
                            state.entry_top[page], state.entry_cur[page]),
                        &scene, "knowledge monster browser",
                        "Monster knowledge screen unavailable."))
                {
                    goto cleanup;
                }
            }

            ch = ui_information_scene_wait_key();
            if (steamdeck_controls_active() && ch == steamdeck_back_key())
                ch = ESCAPE;

            if (knowledge_handle_tab_navigation((char)ch, &page,
                &state.tabs_focus,
                (monster_grp_cnt <= 0) || ((state.column[page] == 0)
                    ? (state.group_cur[page] == 0)
                    : (state.entry_cur[page] == 0))))
            {
                break;
            }

            if (knowledge_handle_page_input((char)ch, &page))
                break;

            if (knowledge_is_recall_input(ch))
            {
                if (monster_cnt > 0)
                {
                    if (!knowledge_pause_information_scene(&info_scope))
                        goto cleanup;
                    if (!ui_information_scene_show_monster_recall(
                            mon_idx[state.entry_cur[page]].r_idx, NULL, NULL,
                            false, NULL))
                    {
                        bell("Monster recall screen unavailable.");
                    }
                    if (!knowledge_resume_information_scene(&info_scope))
                        goto cleanup;
                }
                else
                {
                    bell("Nothing to recall.");
                }
                break;
            }

            switch (ch)
            {
            case ESCAPE:
                done = true;
                break;

            default:
                browser_cursor_with_rows((char)ch, &state.column[page],
                    &state.group_cur[page], monster_grp_cnt,
                    &state.entry_cur[page], monster_cnt, page_rows);
                break;
            }
            break;
        }

        case KNOWLEDGE_PAGE_CURSES:
        default:
        {
            knowledge_clamp_list_state(&state.entry_cur[page], &state.entry_top[page],
                curse_cnt, page_rows);
            {
                app_ui_scene scene;

                if (!knowledge_present_ui_scene_or_abort(&info_scope,
                        knowledge_build_curse_browser_scene(&scene, page,
                            state.tabs_focus, curse_idx, curse_cnt,
                            state.entry_top[page], state.entry_cur[page]),
                        &scene, "knowledge curse browser",
                        "Curse knowledge screen unavailable."))
                {
                    goto cleanup;
                }
            }

            ch = ui_information_scene_wait_key();
            if (steamdeck_controls_active() && ch == steamdeck_back_key())
                ch = ESCAPE;

            if (knowledge_handle_tab_navigation((char)ch, &page,
                &state.tabs_focus, (curse_cnt <= 0) || (state.entry_cur[page] == 0)))
            {
                break;
            }

            if (knowledge_handle_page_input((char)ch, &page))
                break;

            if (knowledge_is_recall_input(ch))
            {
                if (curse_cnt > 0)
                {
                    if (!knowledge_show_curse_detail_ui(
                            curse_idx[state.entry_cur[page]]))
                    {
                        bell("Curse detail unavailable.");
                        msg_print("Curse detail unavailable.");
                    }
                }
                else
                    bell("Nothing to recall.");
                break;
            }

            switch (ch)
            {
            case ESCAPE:
                done = true;
                break;

            default:
            {
                int d = target_dir(ch);
                int page_jump = page_rows;

                if (curse_cnt <= 0)
                {
                    state.entry_cur[page] = 0;
                    break;
                }

                if (!d)
                    break;

                if (ddx[d] && ddy[d])
                    state.entry_cur[page] += ddy[d] * page_jump;
                else if (ddy[d])
                    state.entry_cur[page] += ddy[d];

                if (state.entry_cur[page] < 0)
                    state.entry_cur[page] = 0;
                if (state.entry_cur[page] >= curse_cnt)
                    state.entry_cur[page] = curse_cnt - 1;
                break;
            }
            }
            break;
        }
        }
    }

cleanup:
    mem_free_null(curse_idx);
    mem_free_null(mon_idx);
    mem_free_null(object_idx);
    mem_free_null(artefact_idx);

    if (info_scope.active)
        ui_information_scene_leave(&info_scope);
    if (p_ptr && p_ptr->playing)
        sdl_music_stop_main();
}

/*
 * Display known objects
 */
bool do_cmd_knowledge_supplies(const supply_menu_request* request)
{
    ui_information_scene_scope info_scope;
    int page_rows = BROWSER_ROWS;
    int i;
    int grp_cnt = SUPPLY_GROUP_MAX;
    int grp_idx[SUPPLY_GROUP_MAX + 1];
    int group_totals[SUPPLY_GROUP_MAX];
    supply_list_entry* entries;
    int grp_cur = 0;
    int grp_top = 0;
    int entry_cur = 0;
    int entry_top = 0;
    int column = 0;
    bool flag = false;
    supply_menu_action forced_action = SUPPLY_MENU_ACTION_NONE;
    bool hotkey_mode = false;
    bool acted = false;
    bool refresh_after_close = false;

    if (request)
    {
        forced_action = request->action;
        hotkey_mode = request->hotkey_mode;
        if (request->focus_group && request->group >= 0 && request->group < SUPPLY_GROUP_MAX)
            grp_cur = request->group;
        if (forced_action != SUPPLY_MENU_ACTION_NONE)
            column = 1;
    }

    for (i = 0; i < SUPPLY_GROUP_MAX; i++)
    {
        grp_idx[i] = i;
    }
    grp_idx[grp_cnt] = -1;

    if (!knowledge_enter_information_scene_or_report(&info_scope,
            "knowledge supplies",
            "Supplies screen unavailable."))
    {
        return false;
    }

    entries = mem_alloc_array(z_info->k_max, supply_list_entry);

    while (!flag)
    {
        int entry_cnt;
        int used_weight;
        int light_item_weight;
        int light_oil_weight;
        int light_weight;
        int lamp_oil;
        int max_weight;
        char weight_buf[128];
        char ch;

        compute_supply_group_totals(group_totals);
        used_weight = supplies_limit_weight();
        light_item_weight = supplies_carried_light_item_weight();
        light_oil_weight = player_lamp_oil_weight();
        light_weight = light_item_weight + light_oil_weight;
        lamp_oil = player_lamp_oil();
        max_weight = supplies_current_weight_cap();
        strnfmt(weight_buf, sizeof(weight_buf),
            "Supply: %d.%1d/%d.%1d lb  Light: %d.%1d lb (%d.%1d items + %d.%1d oil)  Oil: %d/%d",
            used_weight / 10, used_weight % 10,
            max_weight / 10, max_weight % 10,
            light_weight / 10, light_weight % 10,
            light_item_weight / 10, light_item_weight % 10,
            light_oil_weight / 10, light_oil_weight % 10,
            lamp_oil, PLAYER_LAMP_OIL_MAX);

        if (grp_cur >= grp_cnt)
            grp_cur = grp_cnt - 1;
        if (grp_cur < 0)
            grp_cur = 0;

        entry_cnt = collect_supply_entries(grp_idx[grp_cur], entries);

        if (entry_cnt == 0)
        {
            entry_cur = 0;
            entry_top = 0;
            if (column)
                column = 0;
        }
        else
        {
            if (entry_cur >= entry_cnt)
                entry_cur = entry_cnt - 1;
            if (entry_cur < 0)
                entry_cur = 0;

            if (entry_cur < entry_top)
                entry_top = entry_cur;
            if (entry_cur >= entry_top + page_rows)
                entry_top = entry_cur - page_rows + 1;
            if (entry_top < 0)
                entry_top = 0;
        }

        if (grp_cur < grp_top)
            grp_top = grp_cur;
        if (grp_cur >= grp_top + page_rows)
            grp_top = grp_cur - page_rows + 1;
        if (grp_top < 0)
            grp_top = 0;

        {
            app_ui_scene scene;

            if (!knowledge_present_ui_scene_or_abort(&info_scope,
                    knowledge_build_supplies_browser_scene(&scene, grp_idx,
                        grp_cnt, grp_cur, grp_top, group_totals, entries,
                        entry_cnt, entry_top, entry_cur, column, weight_buf),
                    &scene, "knowledge supplies browser",
                    "Supplies screen unavailable."))
            {
                goto cleanup;
            }
        }

        ch = (char)ui_information_scene_wait_key();
        if (steamdeck_controls_active() && ch == steamdeck_back_key())
            ch = ESCAPE;

        if ((ch == '\r' || ch == '\n' || (steamdeck_controls_active() && ch == steamdeck_confirm_key())) && column && entry_cnt)
        {
            if (forced_action == SUPPLY_MENU_ACTION_USE)
                ch = 'u';
            else if (forced_action == SUPPLY_MENU_ACTION_DROP)
                ch = 'd';
        }

        switch (ch)
        {
        case ESCAPE:
            flag = true;
            break;

        case 'R':
        case 'r':
        case 'X':
        case 'x':
            if (!column && entry_cnt)
            {
                column = 1;
            }
            else if (column && entry_cnt)
            {
                supply_list_entry* entry = &entries[entry_cur];
                if (entry->item_idx >= 0 && entry->item_idx < INVEN_PACK)
                {
                    if (!knowledge_pause_information_scene(&info_scope))
                        goto cleanup;
                    (void)player_try_identify_smithing_object_on_examine(
                        &inventory[entry->item_idx], false);
                    object_info_screen(&inventory[entry->item_idx]);
                    if (!knowledge_resume_information_scene(&info_scope))
                        goto cleanup;
                }
                else if (entry->k_idx >= 0)
                {
                    object_kind* k_ptr = &k_info[entry->k_idx];
                    if (k_ptr->aware)
                    {
                        if (!knowledge_pause_information_scene(&info_scope))
                            goto cleanup;
                        desc_obj_fake(entry->k_idx);
                        if (!knowledge_resume_information_scene(&info_scope))
                            goto cleanup;
                    }
                    else
                    {
                        bell("You have not identified that yet.");
                        msg_print("You have not identified that yet.");
                    }
                }
            }
            break;

        case 'u':
        case 'U':
        case ' ':
            if (!column && entry_cnt)
            {
                column = 1;
            }
            else if (column && entry_cnt)
            {
                supply_list_entry* entry = &entries[entry_cur];
                bool handled = false;

                if (death_spectator_active())
                {
                    msg_print("You can no longer take that action.");
                    break;
                }

                if (entry->item_idx == SUPPLIES_INDEX && entry->supply_idx >= 0)
                {
                    handled = supplies_menu_use_entry(entry);
                }
                else if (entry->item_idx >= 0 && entry->item_idx < INVEN_PACK)
                {
                    object_type* o_ptr = &inventory[entry->item_idx];

                    switch (o_ptr->tval)
                    {
                    case TV_FOOD:
                        do_cmd_eat_food(o_ptr, entry->item_idx);
                        handled = true;
                        break;
                    case TV_POTION:
                        do_cmd_quaff_potion(o_ptr, entry->item_idx);
                        handled = true;
                        break;
                    case TV_STAFF:
                        do_cmd_activate_staff(o_ptr, entry->item_idx);
                        handled = true;
                        break;
                    case TV_GEM:
                        do_cmd_use_gem(o_ptr, entry->item_idx);
                        handled = true;
                        break;
                    default:
                        bell("Cannot use that item here!");
                        break;
                    }

                    if (handled)
                        handle_stuff();
                }
                else
                {
                    bell("You do not have any of that item.");
                    msg_print("You do not have any of that item.");
                }

                if (handled)
                {
                    acted = true;
                    refresh_after_close = true;
                    if (hotkey_mode || forced_action == SUPPLY_MENU_ACTION_USE)
                        flag = true;
                }
            }
            break;

        case 'd':
        case 'D':
            if (!column && entry_cnt)
            {
                column = 1;
            }
            else if (column && entry_cnt)
            {
                supply_list_entry* entry = &entries[entry_cur];
                bool dropped = false;

                if (death_spectator_active())
                {
                    msg_print("You can no longer take that action.");
                    break;
                }

                if (entry->item_idx == SUPPLIES_INDEX && entry->supply_idx >= 0)
                {
                    dropped = supplies_menu_drop_entry(entry);
                }
                else if (entry->item_idx >= 0 && entry->item_idx < INVEN_PACK)
                {
                    do_cmd_drop_item_by_index(entry->item_idx);
                    dropped = true;
                }
                else
                {
                    bell("Nothing to drop here.");
                    msg_print("Nothing to drop here.");
                }

                if (dropped)
                {
                    acted = true;
                    handle_stuff();
                    refresh_after_close = true;
                    if (hotkey_mode || forced_action == SUPPLY_MENU_ACTION_DROP)
                        flag = true;
                }
            }
            break;

        default:
            browser_cursor_with_rows(ch, &column, &grp_cur, grp_cnt, &entry_cur,
                entry_cnt, page_rows);
            break;
        }
    }

cleanup:
    mem_free_null(entries);
    if (info_scope.active)
        ui_information_scene_leave(&info_scope);

    if (refresh_after_close)
    {
        p_ptr->redraw |= (PR_MAP);
        p_ptr->window |= (PW_MESSAGE);
        handle_stuff();
    }

    return acted;
}

void do_cmd_knowledge_objects(void)
{
    do_cmd_knowledge_browser_page(KNOWLEDGE_PAGE_OBJECTS);
}

/*
 * Display kill counts
 */
void do_cmd_knowledge_kills(void)
{
    int n, i;

    ang_file* fff;

    char file_name[1024];

    u16b* who;
    //	u16b why = 4;

    /* Temporary file */
    fff = ang_file_open_temp(file_name, sizeof(file_name));

    /* Failure */
    if (!fff)
        return;

    /* Allocate the "who" array */
    who = mem_alloc_array(z_info->r_max, u16b);

    /* Collect matching monsters */
    for (n = 0, i = 1; i < z_info->r_max - 1; i++)
    {
        // monster_race *r_ptr = &r_info[i];
        monster_lore* l_ptr = &l_list[i];

        /* Require non-unique monsters */
        // if (r_ptr->flags1 & RF1_UNIQUE) continue;

        /* Collect "appropriate" monsters */
        if (l_ptr->pkills > 0)
            who[n++] = i;
    }

    /* Select the sort method */
    // ang_sort_comp = ang_sort_comp_hook;
    // ang_sort_swap = ang_sort_swap_hook;

    /* Sort by kills (and level) */
    // ang_sort(who, &why, n);

    /* Print the monsters (highest kill counts first) */
    for (i = n - 1; i >= 0; i--)
    {
        monster_race* r_ptr = &r_info[who[i]];
        monster_lore* l_ptr = &l_list[who[i]];

        if (r_ptr->flags1 & (RF1_UNIQUE))
        {
            /* Print a message */
            ang_file_printf(fff, "         %-40s\n", (r_name + r_ptr->name));
        }
        else
        {
            /* Print a message */
            ang_file_printf(
                fff, "  %5d  %-40s\n", l_ptr->pkills, (r_name + r_ptr->name));
        }
    }

    /* Free the "who" array */
    mem_free_null(who);

    /* Close the file */
    ang_file_close(fff);

    /* Display the file contents */
    show_file(file_name, "Kill counts", 0);

    /* Remove the file */
    fd_kill(file_name);
}

/*
 * Interact with "knowledge"
 */
void do_cmd_knowledge(void)
{
    ui_information_scene_scope info_scope;
    char ch;
    int selected = 0;

    /* File type is "TEXT" */
    FILE_TYPE(FILE_TYPE_TEXT);

    if (!knowledge_enter_information_scene_or_report(&info_scope,
            "knowledge menu",
            "Knowledge menu unavailable."))
    {
        return;
    }

    /* Interact until done */
    while (1)
    {
        {
            app_ui_scene scene;

            if (!knowledge_present_ui_scene_or_abort(&info_scope,
                    knowledge_build_root_menu_scene(&scene, selected), &scene,
                    "knowledge root menu", "Knowledge menu unavailable."))
            {
                goto cleanup;
            }
        }

        ch = (char)ui_information_scene_wait_key();
        if (steamdeck_controls_active() && ch == steamdeck_back_key())
            ch = ESCAPE;

        {
            int d = target_dir(ch);

            if (d && !ddx[d] && ddy[d] < 0)
            {
                if (selected > 0)
                    selected--;
                continue;
            }
            if (d && !ddx[d] && ddy[d] > 0)
            {
                if (selected < 5)
                    selected++;
                continue;
            }
            if (ch == '\r' || ch == '\n' || ch == ' '
                || ch == INPUT_BIND_CONFIRM
                || (steamdeck_confirm_key() != GAMEPAD_BIND_NONE
                    && ch == steamdeck_confirm_key()))
            {
                ch = (char)('1' + selected);
            }
        }

        /* Done */
        if (ch == ESCAPE)
            break;

        /* Known lore browser */
        if (ch == '1')
        {
            if (!knowledge_pause_information_scene(&info_scope))
                goto cleanup;
            do_cmd_knowledge_browser_page(g_knowledge_last_page);
            if (!knowledge_resume_information_scene(&info_scope))
                goto cleanup;
        }

        /* Scores */
        else if (ch == '2')
        {
            if (!knowledge_pause_information_scene(&info_scope))
                goto cleanup;
            do_cmd_knowledge_supplies(NULL);
            if (!knowledge_resume_information_scene(&info_scope))
                goto cleanup;
        }

        /* Scores */
        else if (ch == '3')
        {
            if (!knowledge_pause_information_scene(&info_scope))
                goto cleanup;
            show_scores_interactive(true);
            if (!knowledge_resume_information_scene(&info_scope))
                goto cleanup;
        }

        /* Kill counts */
        else if (ch == '4')
        {
            if (!knowledge_pause_information_scene(&info_scope))
                goto cleanup;
            do_cmd_knowledge_kills();
            if (!knowledge_resume_information_scene(&info_scope))
                goto cleanup;
        }

        /* Notes file, if one exists */
        else if (ch == '5')
        {
            /* Spawn */
            if (!knowledge_pause_information_scene(&info_scope))
                goto cleanup;
            do_cmd_knowledge_notes();
            if (!knowledge_resume_information_scene(&info_scope))
                goto cleanup;
        }

        /* Oath status */
        else if (ch == '6')
        {
            if (!knowledge_pause_information_scene(&info_scope))
                goto cleanup;
            do_cmd_knowledge_oaths();
            if (!knowledge_resume_information_scene(&info_scope))
                goto cleanup;
        }

        /* Unknown option */
        else
        {
            bell("Illegal command for knowledge!");
        }

        /* Flush messages */
        message_flush();
    }

cleanup:
    if (info_scope.active)
        ui_information_scene_leave(&info_scope);
}

