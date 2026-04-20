/* File: monster/monster-message.c */
/*
 * Copyright (C) 2025-2026 Sil-More contributors
 *
 * This file is part of Sil-More.
 *
 * Sil-More is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Sil-More is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See LICENSE.md
 * for more details.
 */

/*
 * Monster description and pain-message helpers split from monster2.c
 * for ownership reduction.
 */

#include "angband.h"
#include "monster/monster.h"

void monster_desc(char* desc, size_t max, const monster_type* m_ptr, int mode)
{
    cptr res;
    monster_race* r_ptr;
    cptr name;
    bool seen, pron;

    if (p_ptr->image)
    {
        r_ptr = &r_info[m_ptr->image_r_idx];
    }
    else
    {
        r_ptr = &r_info[m_ptr->r_idx];
    }

    name = (r_name + r_ptr->name);

    /* Can we "see" it (forced, or not hidden + visible) */
    seen = ((mode & (0x80)) || (!(mode & (0x40)) && m_ptr->ml));

    /* Sexed Pronouns (seen and forced, or unseen and allowed) */
    pron = ((seen && (mode & (0x20))) || (!seen && (mode & (0x10))));

    /* First, try using pronouns, or describing hidden monsters */
    if (!seen || pron)
    {
        /* an encoding of the monster "sex" */
        int kind = 0x00;

        /* Extract the gender (if applicable) */
        if (r_ptr->flags1 & (RF1_FEMALE))
            kind = 0x20;
        else if (r_ptr->flags1 & (RF1_MALE))
            kind = 0x10;

        /* Ignore the gender (if desired) */
        if (!m_ptr || !pron)
            kind = 0x00;

        /* Assume simple result */
        res = "it";

        /* Brute force: split on the possibilities */
        switch (kind + (mode & 0x07))
        {
        /* Neuter, or unknown */
        case 0x00:
            res = "it";
            break;
        case 0x01:
            res = "it";
            break;
        case 0x02:
            res = "its";
            break;
        case 0x03:
            res = "itself";
            break;
        case 0x04:
            res = "something";
            break;
        case 0x05:
            res = "something";
            break;
        case 0x06:
            res = "something's";
            break;
        case 0x07:
            res = "itself";
            break;

        /* Male (assume human if vague) */
        case 0x10:
            res = "he";
            break;
        case 0x11:
            res = "him";
            break;
        case 0x12:
            res = "his";
            break;
        case 0x13:
            res = "himself";
            break;
        case 0x14:
            res = "someone";
            break;
        case 0x15:
            res = "someone";
            break;
        case 0x16:
            res = "someone's";
            break;
        case 0x17:
            res = "himself";
            break;

        /* Female (assume human if vague) */
        case 0x20:
            res = "she";
            break;
        case 0x21:
            res = "her";
            break;
        case 0x22:
            res = "her";
            break;
        case 0x23:
            res = "herself";
            break;
        case 0x24:
            res = "someone";
            break;
        case 0x25:
            res = "someone";
            break;
        case 0x26:
            res = "someone's";
            break;
        case 0x27:
            res = "herself";
            break;
        }

        /* Copy the result */
        SDL_strlcpy(desc, res, max);
    }

    /* Handle visible monsters, "reflexive" request */
    else if ((mode & 0x02) && (mode & 0x01))
    {
        /* The monster is visible, so use its gender */
        if (r_ptr->flags1 & (RF1_FEMALE))
            SDL_strlcpy(desc, "herself", max);
        else if (r_ptr->flags1 & (RF1_MALE))
            SDL_strlcpy(desc, "himself", max);
        else
            SDL_strlcpy(desc, "itself", max);
    }

    /* Handle all other visible monster requests */
    else
    {
        /* It could be a Unique */
        if (r_ptr->flags1 & (RF1_UNIQUE))
        {
            /* Start with the name (thus nominative and objective) */
            SDL_strlcpy(desc, name, max);
        }

        /* It could be an indefinite monster */
        else if (mode & 0x08)
        {
            /* XXX Check plurality for "some" */

            /* Indefinite monsters need an indefinite article */
            SDL_strlcpy(desc, is_a_vowel(name[0]) ? "an " : "a ", max);
            SDL_strlcat(desc, name, max);
        }

        /* It could be a normal, definite, monster */
        else
        {
            /* Definite monsters need a definite article */
            SDL_strlcpy(desc, "the ", max);
            SDL_strlcat(desc, name, max);
        }

        /* Handle the Possessive as a special afterthought */
        if (mode & 0x02)
        {
            /* XXX Check for trailing "s" */

            /* Simply append "apostrophe" and "s" */
            SDL_strlcat(desc, "'s", max);
        }

        /* Mention "offscreen" monsters XXX XXX */
        if (!panel_contains(m_ptr->fy, m_ptr->fx))
        {
            /* Append special notation */
            SDL_strlcat(desc, " (offscreen)", max);
        }
    }
}

/*
 * Build a string describing a monster race, currently used for quests.
 *
 * Assumes a singular monster.  This may need to be run through the
 * plural_aux function in the quest.c file.  (Changes "wolf" to
 * wolves, etc.....)
 *
 * I am assuming that no monster name is more than 65 characters long,
 * so that "char desc[80];" is sufficiently large for any result, even
 * when the "offscreen" notation is added.
 *
 */
void monster_desc_race(char* desc, size_t max, int r_idx)
{
    monster_race* r_ptr = &r_info[r_idx];

    cptr name = (r_name + r_ptr->name);

    /* Write the name */
    SDL_strlcpy(desc, name, max);
}

void message_pain(int m_idx, int dam)
{
    long oldhp, newhp, tmp;
    int percentage;

    monster_type* m_ptr = &mon_list[m_idx];
    monster_race* r_ptr = &r_info[m_ptr->r_idx];

    char m_name[80];

    // Ignore the monster if it is visible
    if (m_ptr->ml)
        return;

    /* Get the monster name */
    monster_desc(m_name, sizeof(m_name), m_ptr, 0);

    /* Note -- subtle fix -CFT */
    newhp = (long)(m_ptr->hp);
    oldhp = newhp + (long)(dam);
    tmp = (newhp * 100L) / oldhp;
    percentage = (int)(tmp);

    /* Wolves */
    if (strchr("C", r_ptr->d_char))
    {
        if (percentage > 66)
            msg_print("You hear a snarl.");
        else if (percentage > 33)
            msg_print("You hear a yelp.");
        else
            msg_print("You hear a feeble yelp.");
    }

    /* Serpents, Dragons, Centipedes */
    else if (strchr("sScdD", r_ptr->d_char))
    {
        if (percentage > 66)
            msg_print("You hear a hiss.");
        else if (percentage > 33)
            msg_print("You hear a furious hissing.");
        else
            msg_print("You hear thrashing about.");
    }

    /* Felines */
    else if (strchr("f", r_ptr->d_char))
    {
        if (percentage > 66)
            msg_print("You hear a feline snarl.");
        else if (percentage > 33)
            msg_print("You hear a mewling sound.");
        else
            msg_print("You hear a pitiful mewling.");
    }

    /* Insects, Spiders */
    else if (strchr("IM", r_ptr->d_char))
    {
        if (percentage > 66)
            msg_print("You hear an angry droning.");
        else if (percentage > 33)
            msg_print("You hear a scuttling sound.");
        else
            msg_print("You hear a skittering sound.");
    }

    /* Birds, Bats, Vampires */
    else if (strchr("bv", r_ptr->d_char))
    {
        if (percentage > 66)
            msg_print("You hear a squeal.");
        else if (percentage > 33)
            msg_print("You hear a shrieks.");
        else
            msg_print("You hear erratic fluttering.");
    }

    /* Humanoid monsters */
    else if (strchr("@oTGV", r_ptr->d_char))
    {
        if (percentage > 66)
            msg_print("You hear a grunt.");
        else if (percentage > 33)
            msg_print("You hear a cry of pain.");
        else
            msg_print("You hear a feeble cry.");
    }

    /* Some other monsters */
    else if (strchr("HRN", r_ptr->d_char))
    {
        if (percentage > 66)
            msg_print("You hear a strange grunt.");
        else if (percentage > 33)
            msg_print("You hear a terrible cry.");
        else
            msg_print("You hear a unnatural cry.");
    }

    // m, w are silent
}
