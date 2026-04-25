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

#include "angband.h"

#include "app-scene-birth-ui-internal.h"

int birth_collect_character_trait_lines(int race, int character,
    bool short_labels, birth_compact_flag_line out[], int out_max,
    int* max_line_len)
{
    int total = 0;

    byte attr_affinity = TERM_GREEN;
    byte attr_mastery = TERM_L_GREEN;
    byte attr_penalty = TERM_RED;
    byte attr_gr_penalty = TERM_L_RED;

    birth_compact_flag_line uniq_buf[32], ma_buf[16], af_buf[16], pen_buf[32];
    int uniq_n = 0, ma_n = 0, af_n = 0, pen_n = 0;

#define PUSH(arr, n, text, color)                                             \
    do {                                                                      \
        if ((text) && (n) < (int)N_ELEMENTS(arr))                             \
        {                                                                     \
            (arr)[(n)].txt = (text);                                          \
            (arr)[(n)++].attr = (color);                                      \
        }                                                                     \
    } while (0)

#define HANDLE_SKILL_EX(LABEL_LONG, LABEL_SHORT, AFF_FLAG, PEN_FLAG)          \
    do {                                                                      \
        int score = 0;                                                        \
        if (p_info[race].flags & (AFF_FLAG)) score++;                         \
        if (c_info[character].flags & (AFF_FLAG)) score++;                    \
        if ((PEN_FLAG) && (p_info[race].flags & (PEN_FLAG))) score--;         \
        if ((PEN_FLAG) && (c_info[character].flags & (PEN_FLAG))) score--;    \
        score += curse_flag_count_rhf(AFF_FLAG);                              \
        if ((PEN_FLAG)) score -= curse_flag_count_rhf(PEN_FLAG);              \
        if (score > 2) score = 2;                                             \
        if (score < -2) score = -2;                                           \
        if (score == 2)                                                       \
            PUSH(ma_buf, ma_n,                                                \
                short_labels ? LABEL_SHORT "++" : LABEL_LONG " mastery",      \
                attr_mastery);                                                \
        else if (score == 1)                                                  \
            PUSH(af_buf, af_n,                                                \
                short_labels ? LABEL_SHORT "+ " : LABEL_LONG " affinity",     \
                attr_affinity);                                               \
        else if (score == -1)                                                 \
            PUSH(pen_buf, pen_n,                                              \
                short_labels ? LABEL_SHORT "- " : LABEL_LONG " penalty",      \
                attr_penalty);                                                \
        else if (score == -2)                                                 \
            PUSH(pen_buf, pen_n,                                              \
                short_labels ? LABEL_SHORT "--" : LABEL_LONG " grand penalty",\
                attr_gr_penalty);                                             \
    } while (0)

#define HANDLE_UNIQUE_EX(LABEL_LONG, LABEL_SHORT, FLAG, COLOR)                \
    do {                                                                      \
        if ((p_info[race].flags & (FLAG))                                     \
            || (c_info[character].flags & (FLAG)))                            \
        {                                                                     \
            PUSH(uniq_buf, uniq_n, short_labels ? LABEL_SHORT : LABEL_LONG,   \
                (COLOR));                                                     \
        }                                                                     \
    } while (0)

#define HANDLE_UNIQUE_U_EX(LABEL_LONG, LABEL_SHORT, FLAG, COLOR)              \
    do {                                                                      \
        if (c_info[character].flags_u & (FLAG))                               \
            PUSH(uniq_buf, uniq_n, short_labels ? LABEL_SHORT : LABEL_LONG,   \
                (COLOR));                                                     \
    } while (0)

#define EMIT(arr, n)                                                          \
    do {                                                                      \
        for (int _i = 0; _i < (n); ++_i)                                      \
        {                                                                     \
            cptr _txt = (arr)[_i].txt ? (arr)[_i].txt : "";                  \
            if (max_line_len && (int)strlen(_txt) > *max_line_len)            \
                *max_line_len = (int)strlen(_txt);                            \
            if (out && total < out_max)                                       \
                out[total] = (arr)[_i];                                       \
            total++;                                                          \
        }                                                                     \
    } while (0)

    HANDLE_SKILL_EX("melee", "melee", RHF_MEL_AFFINITY, RHF_MEL_PENALTY);
    HANDLE_SKILL_EX("evasion", "evasion", RHF_EVN_AFFINITY, RHF_EVN_PENALTY);
    HANDLE_SKILL_EX("stealth", "stealth", RHF_STL_AFFINITY, RHF_STL_PENALTY);
    HANDLE_SKILL_EX("archery", "archery", RHF_ARC_AFFINITY, RHF_ARC_PENALTY);
    HANDLE_SKILL_EX("will", "will", RHF_WIL_AFFINITY, RHF_WIL_PENALTY);
    HANDLE_SKILL_EX("perception", "perception", RHF_PER_AFFINITY,
        RHF_PER_PENALTY);
    HANDLE_SKILL_EX("smithing", "smithing", RHF_SMT_AFFINITY,
        RHF_SMT_PENALTY);
    HANDLE_SKILL_EX("song", "song", RHF_SNG_AFFINITY, RHF_SNG_PENALTY);
    HANDLE_SKILL_EX("bow", "bow", RHF_BOW_PROFICIENCY, 0);
    HANDLE_SKILL_EX("axe", "axe", RHF_AXE_PROFICIENCY, 0);

    HANDLE_UNIQUE_U_EX("Master Artisan", "Master Artisan", UNQ_SMT_FEANOR,
        TERM_VIOLET);
    HANDLE_UNIQUE_U_EX("Creator of Galvorn", "Galvorn Maker", UNQ_SMT_EOL,
        TERM_VIOLET);
    HANDLE_UNIQUE_U_EX("One Handed", "One Handed", UNQ_MEL_MAEDHROS,
        TERM_VIOLET);
    HANDLE_UNIQUE_U_EX("Agarwaen", "Agarwaen", UNQ_WIL_TURIN, TERM_VIOLET);
    HANDLE_UNIQUE_U_EX("Hidden city", "Hidden City", UNQ_SNG_TURGON,
        TERM_VIOLET);
    HANDLE_UNIQUE_U_EX("Chosen of Ulmo", "Ulmo's Chosen", UNQ_WIL_TUOR,
        TERM_VIOLET);
    HANDLE_UNIQUE_U_EX("Indominable Will", "Indom. Will", UNQ_EARENDIL,
        TERM_VIOLET);
    HANDLE_UNIQUE_U_EX("Oromë Himself", "Oromë", UNQ_WIL_FIN, TERM_VIOLET);
    HANDLE_UNIQUE_U_EX("Songs of Power", "Songs of Power", UNQ_SNG_FIN,
        TERM_VIOLET);
    HANDLE_UNIQUE_U_EX("Elven Dance", "Elven Dance", UNQ_SNG_LUT,
        TERM_VIOLET);
    HANDLE_UNIQUE_U_EX("Girdle of Melian", "Melian's Girdle", UNQ_SNG_MEL,
        TERM_VIOLET);
    HANDLE_UNIQUE_U_EX("Creator of Angrist", "Angrist Maker", UNQ_SMT_TELCHAR,
        TERM_VIOLET);
    HANDLE_UNIQUE_U_EX("Old Master", "Old Master", UNQ_SMT_GAMIL,
        TERM_VIOLET);
    HANDLE_UNIQUE_U_EX("Ring Master", "Ring Master", UNQ_SMT_CELEBRIMBOR,
        TERM_VIOLET);
    HANDLE_UNIQUE_U_EX("Aurë entuluva", "Aurë Entuluva", UNQ_SNG_HURIN,
        TERM_VIOLET);
    HANDLE_UNIQUE_U_EX("Voice of Girdle", "Girdle Voice", UNQ_SNG_THINGOL,
        TERM_VIOLET);
    HANDLE_UNIQUE_U_EX("Forgotten", "Forgotten", UNQ_MIM, TERM_VIOLET);
    HANDLE_UNIQUE_U_EX("Minstrel", "Minstrel", UNQ_MINSTREL, TERM_VIOLET);
    HANDLE_UNIQUE_U_EX("Woven Master", "Woven Master", UNQ_WOVEN_MASTER,
        TERM_VIOLET);

    HANDLE_UNIQUE_EX("Gift of Eru", "Gift of Eru", RHF_GIFTERU, TERM_VIOLET);
    HANDLE_UNIQUE_EX("Seafarer", "Seafarer", RHF_FREE, TERM_VIOLET);
    HANDLE_UNIQUE_EX("Kinslayer", "Kinslayer", RHF_KINSLAYER, TERM_UMBER);
    HANDLE_UNIQUE_EX("Treacherous", "Treacherous", RHF_TREACHERY,
        TERM_UMBER);
    HANDLE_UNIQUE_EX("Doom of Mandos", "Mandos' Doom", RHF_CURSE,
        TERM_UMBER);
    HANDLE_UNIQUE_EX("Morgoth Curse", "Morgoth Curse", RHF_MOR_CURSE,
        TERM_UMBER);

    EMIT(uniq_buf, uniq_n);
    EMIT(ma_buf, ma_n);
    EMIT(af_buf, af_n);
    EMIT(pen_buf, pen_n);

#undef EMIT
#undef HANDLE_UNIQUE_U_EX
#undef HANDLE_UNIQUE_EX
#undef HANDLE_SKILL_EX
#undef PUSH

    return total;
}
