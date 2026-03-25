/* File: player/player-oaths.c */

#include "angband.h"
#include "externs.h"

#include "player/player-oaths.h"

static u32b oath_flag[] = { 0L, OATH_MERCY_FLAG, OATH_SILENCE_FLAG,
    OATH_IRON_FLAG, OATH_SMITH_FLAG, OATH_VALOROUS_FLAG, OATH_LIGHT_FLAG };

char* oath_name[] = {
    "Nothing",
    "Mercy",
    "Silence",
    "Iron",
    "Smith",
    "Valorous Heart",
    "Light",
};

bool oath_invalid(int oath_id)
{
    if (oath_id < 0 || oath_id >= (int)N_ELEMENTS(oath_flag))
        return false;

    return ((p_ptr->oaths_broken & oath_flag[oath_id]) > 0);
}

bool chosen_oath(int oath_id)
{
    return (p_ptr->oath_type == oath_id);
}

char* oath_confirmation_prompt(int oath_id)
{
    if (oath_id < 0 || oath_id >= z_info->oath_max)
        return "";

    if (!oath_info[oath_id].confirmation_prompt)
        return "";

    return oath_name_text + oath_info[oath_id].confirmation_prompt;
}

char* oath_curse_message(int oath_id)
{
    if (oath_id < 0 || oath_id >= z_info->oath_max)
        return "";

    if (!oath_info[oath_id].curse_message)
        return "";

    return oath_name_text + oath_info[oath_id].curse_message;
}

char* oath_permanent_message(int oath_id)
{
    if (oath_id < 0 || oath_id >= z_info->oath_max)
        return "";

    if (!oath_info[oath_id].permanent_message)
        return "";

    return oath_name_text + oath_info[oath_id].permanent_message;
}

char* oath_death_message(int oath_id)
{
    if (oath_id < 0 || oath_id >= z_info->oath_max)
        return "";

    if (!oath_info[oath_id].death_message)
        return "";

    return oath_name_text + oath_info[oath_id].death_message;
}

char* oath_banned_text(int oath_id)
{
    if (oath_id < 0 || oath_id >= z_info->oath_max)
        return "";

    if (!oath_info[oath_id].banned_text)
        return "";

    return oath_desc_text + oath_info[oath_id].banned_text;
}

char* oath_name_str(int oath_id)
{
    if (oath_id == 0)
        return "No oath";

    if (!z_info)
        return "";

    if (oath_id < 0 || oath_id >= z_info->oath_max)
        return "";

    if (!oath_info[oath_id].name)
        return "";

    return oath_name_text + oath_info[oath_id].name;
}

char* oath_description(int oath_id)
{
    if (oath_id < 0 || oath_id >= z_info->oath_max)
        return "";

    if (!oath_info[oath_id].text)
        return "";

    return oath_desc_text + oath_info[oath_id].text;
}

char* oath_pledge(int oath_id)
{
    if (oath_id < 0 || oath_id >= z_info->oath_max)
        return "";

    if (!oath_info[oath_id].pledge_text)
        return "";

    return oath_name_text + oath_info[oath_id].pledge_text;
}

char* oath_forbidden(int oath_id)
{
    if (oath_id < 0 || oath_id >= z_info->oath_max)
        return "";

    if (!oath_info[oath_id].forbidden_text)
        return "";

    return oath_name_text + oath_info[oath_id].forbidden_text;
}

char* oath_reward_text(int oath_id)
{
    if (oath_id < 0 || oath_id >= z_info->oath_max)
        return "";

    if (!oath_info[oath_id].reward_text)
        return "";

    return oath_name_text + oath_info[oath_id].reward_text;
}
