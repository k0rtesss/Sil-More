#include "angband.h"
#include "externs.h"
#include "fs/io_sdl.h"
#include "fs/path.h"
#include "h-define.h"
#include "init.h"
#include "log/log.h"
#include "metarun.h"
#include "score/score_guid.h"
#include "init-parse-internal.h"
#include "init-object-bonuses.h"
#include <ctype.h>
#include <errno.h>
#include <math.h>

#ifdef ALLOW_TEMPLATES

/* Parse complete tokens: malformed template values must not silently become zero. */
static bool quest_parse_uint(const char *text, int maximum, int *value)
{
    char *end;
    long parsed;
    if (!text || !*text || !isdigit((unsigned char)*text)) return false;
    errno = 0;
    parsed = strtol(text, &end, 10);
    if (errno || *end || parsed < 0 || parsed > maximum) return false;
    *value = (int)parsed;
    return true;
}

static int quest_tokens(char *text, char **tokens, int maximum)
{
    int count = 0;
    char *next = text;
    while (next) {
        char *colon;
        if (count == maximum || !*next) return -1;
        tokens[count++] = next;
        colon = strchr(next, ':');
        if (colon) *colon++ = '\0';
        next = colon;
    }
    return count;
}

static bool quest_parse_skill(const char *text, int *skill)
{
    static const char *names[] = {"MEL", "ARC", "EVN", "STL", "PER", "WIL", "SMT", "SNG"};
    static const int ids[] = {S_MEL, S_ARC, S_EVN, S_STL, S_PER, S_WIL, S_SMT, S_SNG};
    for (size_t i = 0; i < N_ELEMENTS(names); ++i) {
        if (SDL_strcasecmp(text, names[i]) == 0) { *skill = ids[i]; return true; }
    }
    return quest_parse_uint(text, S_SNG, skill);
}

errr parse_quest_info(char* buf, header* head)
{
    int i;
    char *s;

    /* Current entry */
    static quest_type* quest_ptr = NULL;

    /* Process 'N' for "New/Number/Name" or 'Q' for "Quest" */
    if (buf[0] == 'N' || buf[0] == 'Q')
    {
        /* Find the colon before the name */
        s = strchr(buf + 2, ':');

        /* Verify that colon */
        if (!s) return (PARSE_ERROR_GENERIC);

        /* Nuke the colon, advance to the name */
        *s++ = '\0';

        /* Paranoia -- require a name */
        if (!*s) return (PARSE_ERROR_GENERIC);

        /* Get the index */
        if (!quest_parse_uint(buf + 2, 255, &i) || i == 0)
            return PARSE_ERROR_GENERIC;

        /* Verify information */
        if (i <= error_idx) return (PARSE_ERROR_NON_SEQUENTIAL_RECORDS);
        if (i >= head->info_num) return (PARSE_ERROR_TOO_MANY_ENTRIES);

        /* Save the index */
        error_idx = i;

        /* Point at the "info" */
        quest_ptr = (quest_type*)head->info_ptr + i;

        /* Formula bounds default once per record.  E: may precede P:. */
        quest_ptr->quest_num = (byte)i;
        quest_ptr->sequence = 1;
        quest_ptr->completion_cap = METARUN_QUEST_COMPLETION_CAP;
        quest_ptr->depth_min = 0;
        quest_ptr->depth_max = 25;

        /* Store the name */
        if (!(quest_ptr->name = add_name(head, s)))
            return (PARSE_ERROR_OUT_OF_MEMORY);
    }

    /* Process 'T' for "Title text" */
    else if (buf[0] == 'T')
    {
        /* There better be a current quest_ptr */
        if (!quest_ptr) return (PARSE_ERROR_MISSING_RECORD_HEADER);

        /* Store title text in dedicated field */
        if (!add_text(&(quest_ptr->title_text), head, buf + 2))
            return (PARSE_ERROR_OUT_OF_MEMORY);
    }

    /* Process 'C' for "Challenge text" */
    else if (buf[0] == 'C')
    {
        /* There better be a current quest_ptr */
        if (!quest_ptr) return (PARSE_ERROR_MISSING_RECORD_HEADER);

        /* Store challenge text in dedicated field */
        if (!add_text(&(quest_ptr->challenge_text), head, buf + 2))
            return (PARSE_ERROR_OUT_OF_MEMORY);
    }

    /* Process 'Y' for "quest tYpe" */
    else if (buf[0] == 'Y')
    {
        /* There better be a current quest_ptr */
        if (!quest_ptr) return (PARSE_ERROR_MISSING_RECORD_HEADER);

        /* Parse quest type */
        if (!quest_parse_uint(buf + 2, 1, &i)) return PARSE_ERROR_GENERIC;
        quest_ptr->quest_type = i;
    }

    /* Chain metadata is parsed independently of runtime feature switches. */
    else if (strchr("ZJFHL", buf[0]))
    {
        int value;
        if (!quest_ptr) return PARSE_ERROR_MISSING_RECORD_HEADER;
        if (buf[0] == 'Z') {
            static const char *names[] = {"Tulkas", "Aule", "Mandos", "Nienna", "Orome", "Varda"};
            value = 0;
            for (int n = 0; n < VALA_MAX; ++n)
                if (SDL_strcasecmp(buf + 2, names[n]) == 0) value = n + 1;
            if (!value && (!quest_parse_uint(buf + 2, VALA_MAX, &value) || !value))
                return PARSE_ERROR_GENERIC;
            quest_ptr->vala_id = value;
        } else if (buf[0] == 'J' || buf[0] == 'L') {
            if (!quest_parse_uint(buf + 2, buf[0] == 'J' ? VALA_STAGES : METARUN_QUEST_COMPLETION_CAP, &value) || !value)
                return PARSE_ERROR_GENERIC;
            if (buf[0] == 'J') quest_ptr->sequence = value;
            else quest_ptr->completion_cap = value;
        } else if (buf[0] == 'H') {
            static const char *names[] = {"NONE", "DISCONNECTED", "SINGLE_STAIR", "FIXED_50K", "TULKAS_BLUNT", "TORCHLIGHT"};
            value = -1;
            for (size_t n = 0; n < N_ELEMENTS(names); ++n)
                if (SDL_strcasecmp(buf + 2, names[n]) == 0) value = (int)n;
            if (value < 0) return PARSE_ERROR_GENERIC;
            quest_ptr->challenge_unlock = value;
        } else {
            char *flag = strtok(buf + 2, " |\t");
            if (!flag) return PARSE_ERROR_GENERIC;
            while (flag) {
                if (streq(flag, "GLOBAL")) quest_ptr->quest_flags |= QUEST_FLAG_GLOBAL;
                else if (streq(flag, "OPTIONAL_CHAIN")) quest_ptr->quest_flags |= QUEST_FLAG_OPTIONAL_CHAIN;
                else return PARSE_ERROR_GENERIC;
                flag = strtok(NULL, " |\t");
            }
        }
    }
    else if (buf[0] == 'P')
    {
        char *parts[5];
        float params[4];
        int formula;
        if (!quest_ptr) return PARSE_ERROR_MISSING_RECORD_HEADER;
        if (quest_tokens(buf + 2, parts, 5) != 5) return PARSE_ERROR_GENERIC;
        if (streq(parts[0], "LINEAR_DECAY")) formula = FORMULA_LINEAR_DECAY;
        else if (streq(parts[0], "SCALED_RANGE")) formula = FORMULA_SCALED_RANGE;
        else if (streq(parts[0], "LINEAR_INTERPOLATE")) formula = FORMULA_LINEAR_INTERPOLATE;
        else if (streq(parts[0], "FIXED_PERCENT")) formula = FORMULA_FIXED_PERCENT;
        else return PARSE_ERROR_GENERIC;
        for (int n = 0; n < 4; ++n) {
            char *end;
            errno = 0;
            params[n] = strtof(parts[n + 1], &end);
            if (errno || end == parts[n + 1] || *end || !isfinite(params[n]))
                return PARSE_ERROR_GENERIC;
        }
        if (formula == FORMULA_LINEAR_DECAY && params[0] <= 0) return PARSE_ERROR_GENERIC;
        if (formula != FORMULA_LINEAR_DECAY && (params[0] < 0 || params[0] > 1)) return PARSE_ERROR_GENERIC;
        if (formula == FORMULA_LINEAR_INTERPOLATE && (params[1] < 0 || params[1] > 1)) return PARSE_ERROR_GENERIC;
        if (formula == FORMULA_SCALED_RANGE && (params[1] < 0 || params[2] <= 0)) return PARSE_ERROR_GENERIC;
        quest_ptr->formula_type = formula;
        memcpy(quest_ptr->formula_params, params, sizeof(params));
        /* P: never resets the E: bounds, regardless of directive order. */
    }

    else if (buf[0] == 'O')
    {
        if (!quest_ptr) return PARSE_ERROR_MISSING_RECORD_HEADER;
        if (!z_info || !quest_parse_uint(buf + 2, z_info->oath_max - 1, &i)) return PARSE_ERROR_GENERIC;
        quest_ptr->oath_id = i;
    }

    else if (buf[0] == 'E')
    {
        char *parts[4];
        int count, skill = 0, lower, upper = 0, type;
        if (!quest_ptr) return PARSE_ERROR_MISSING_RECORD_HEADER;
        count = quest_tokens(buf + 2, parts, 4);
        if (count == 3 && streq(parts[0], "SKILL_MIN")) {
            if (!quest_parse_skill(parts[1], &skill) || !quest_parse_uint(parts[2], 255, &lower))
                return PARSE_ERROR_GENERIC;
            type = 1;
        } else if (count == 3 && streq(parts[0], "DEPTH_RANGE")) {
            if (!quest_parse_uint(parts[1], 255, &lower) || !quest_parse_uint(parts[2], 255, &upper) || lower > upper)
                return PARSE_ERROR_GENERIC;
            type = 3;
        } else if (count == 4 && streq(parts[0], "SKILL_RANGE")) {
            if (!quest_parse_skill(parts[1], &skill) || !quest_parse_uint(parts[2], 255, &lower) ||
                !quest_parse_uint(parts[3], 255, &upper) || lower > upper) return PARSE_ERROR_GENERIC;
            type = 2;
        } else return PARSE_ERROR_GENERIC;
        quest_ptr->eligibility_type = type;
        quest_ptr->eligibility_skill = skill;
        quest_ptr->eligibility_value = type == 1 ? lower : 0;
        quest_ptr->eligibility_depth_min = type == 1 ? 0 : lower;
        quest_ptr->eligibility_depth_max = type == 1 ? 0 : upper;
        if (type != 1) {
            quest_ptr->depth_min = lower;
            quest_ptr->depth_max = upper;
        }
    }

    else if (buf[0] == 'A')
    {
        char *parts[2];
        int skill, ability;
        if (!quest_ptr) return PARSE_ERROR_MISSING_RECORD_HEADER;
        if (quest_tokens(buf + 2, parts, 2) != 2 ||
            !quest_parse_uint(parts[0], S_SPC, &skill) ||
            !quest_parse_uint(parts[1], ABILITIES_MAX - 1, &ability)) return PARSE_ERROR_GENERIC;
        quest_ptr->ability_type = skill;
        quest_ptr->ability_id = ability;
    }

    /* Process 'D' for "Description" */
    else if (buf[0] == 'D')
    {
        /* There better be a current quest_ptr */
        if (!quest_ptr) return (PARSE_ERROR_MISSING_RECORD_HEADER);

        /* Store the text */
        if (!add_text(&(quest_ptr->text), head, buf + 2))
            return (PARSE_ERROR_OUT_OF_MEMORY);
    }

    else if (buf[0] == 'S')
    {
        char *parts[4];
        int values[4];
        if (!quest_ptr) return PARSE_ERROR_MISSING_RECORD_HEADER;
        if (quest_tokens(buf + 2, parts, 4) != 4) return PARSE_ERROR_GENERIC;
        for (int n = 0; n < 4; ++n)
            if (!quest_parse_uint(parts[n], 255, &values[n])) return PARSE_ERROR_GENERIC;
        for (int n = 0; n < 4; ++n) quest_ptr->stat_bonuses[n] = values[n];
    }
    else if (buf[0] == 'K')
    {
        char *parts[2];
        int skill, bonus;
        if (!quest_ptr) return PARSE_ERROR_MISSING_RECORD_HEADER;
        if (quest_tokens(buf + 2, parts, 2) != 2 || !quest_parse_skill(parts[0], &skill) ||
            !quest_parse_uint(parts[1], 255, &bonus)) return PARSE_ERROR_GENERIC;
        quest_ptr->skill_type = skill;
        quest_ptr->skill_bonus = bonus;
    }

    /* Process 'I' for "Initialization text" */
    else if (buf[0] == 'I')
    {
        /* There better be a current quest_ptr */
        if (!quest_ptr) return (PARSE_ERROR_MISSING_RECORD_HEADER);

        /* Store the initialization text in dedicated field with newline separator */
        if (quest_ptr->init_text != 0) {
            /* Add newline separator if not first line */
            if (!add_text(&(quest_ptr->init_text), head, "\n"))
                return (PARSE_ERROR_OUT_OF_MEMORY);
        }
        
        /* Store the text (buf + 2 points after "I:") */
        if (!add_text(&(quest_ptr->init_text), head, buf + 2))
            return (PARSE_ERROR_OUT_OF_MEMORY);
    }

    /* Process 'W' for "Win/completion text" */
    else if (buf[0] == 'W')
    {
        /* There better be a current quest_ptr */
        if (!quest_ptr) return (PARSE_ERROR_MISSING_RECORD_HEADER);

        /* Store the completion text in dedicated field with newline separator */
        if (quest_ptr->completion_text != 0) {
            /* Add newline separator if not first line */
            if (!add_text(&(quest_ptr->completion_text), head, "\n"))
                return (PARSE_ERROR_OUT_OF_MEMORY);
        }
        
        /* Store the completion text (buf + 2 points after "W:") */
        if (!add_text(&(quest_ptr->completion_text), head, buf + 2))
            return (PARSE_ERROR_OUT_OF_MEMORY);
    }

    /* Process 'R' for "vaRiable name" (quest state mapping) */
    else if (buf[0] == 'R')
    {
        /* There better be a current quest_ptr */
        if (!quest_ptr) return (PARSE_ERROR_MISSING_RECORD_HEADER);

        /* Store the quest state variable name */
        if (!(quest_ptr->quest_state_var = add_name(head, buf + 2)))
            return (PARSE_ERROR_OUT_OF_MEMORY);
    }

    /* Process 'M' for "Metarun quest ID" */
    else if (buf[0] == 'M')
    {
        /* There better be a current quest_ptr */
        if (!quest_ptr) return (PARSE_ERROR_MISSING_RECORD_HEADER);

        /* Store the metarun quest ID string */
        if (!(quest_ptr->metarun_quest_id = add_name(head, buf + 2)))
            return (PARSE_ERROR_OUT_OF_MEMORY);
    }

    else
    {
        /* Oops */
        return (PARSE_ERROR_UNDEFINED_DIRECTIVE);
    }

    /* Success */
    return (0);
}

#endif /* ALLOW_TEMPLATES */
