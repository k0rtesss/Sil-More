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
#include "init2-internal.h"
#include "init-object-bonuses.h"
#include <ctype.h>

#ifdef ALLOW_TEMPLATES
static void ability_req_copy_token(char* out, size_t out_sz, cptr src)
{
    size_t len;
    size_t i;
    size_t j = 0;

    if (!out || out_sz == 0)
        return;

    out[0] = '\0';

    if (!src)
        return;

    while (*src && isspace((unsigned char)*src))
        src++;

    len = strlen(src);
    while (len > 0 && isspace((unsigned char)src[len - 1]))
        len--;

    for (i = 0; i < len && j + 1 < out_sz; i++)
    {
        unsigned char c = (unsigned char)src[i];

        if ((c == '-') || isspace(c))
            out[j++] = '_';
        else
            out[j++] = (char)toupper(c);
    }

    out[j] = '\0';
}

static bool ability_req_parse_int(cptr src, int max_value, int* out)
{
    char token[32];
    int value = 0;
    int i;

    if (!out)
        return false;

    ability_req_copy_token(token, sizeof(token), src);

    if (!token[0])
        return false;

    for (i = 0; token[i]; i++)
    {
        if (!isdigit((unsigned char)token[i]))
            return false;

        value = (value * 10) + (token[i] - '0');
        if (value > max_value)
            return false;
    }

    *out = value;
    return true;
}

static bool ability_req_parse_stat_name(cptr src, int* index)
{
    char token[64];
    cptr name;

    if (!index)
        return false;

    ability_req_copy_token(token, sizeof(token), src);
    name = token;

    if (!strncmp(name, "STAT_", 5))
        name += 5;
    else if (!strncmp(name, "A_", 2))
        name += 2;
    else if (!strncmp(name, "STAT", 4) && isdigit((unsigned char)name[4]))
        name += 4;
    else if ((name[0] == 'A') && isdigit((unsigned char)name[1]))
        name += 1;

    if (ability_req_parse_int(name, A_MAX - 1, index))
        return true;

    if (streq(name, "STR") || streq(name, "STRENGTH"))
    {
        *index = A_STR;
        return true;
    }
    if (streq(name, "DEX") || streq(name, "DEXTERITY"))
    {
        *index = A_DEX;
        return true;
    }
    if (streq(name, "CON") || streq(name, "CONSTITUTION"))
    {
        *index = A_CON;
        return true;
    }
    if (streq(name, "GRA") || streq(name, "GRACE"))
    {
        *index = A_GRA;
        return true;
    }

    return false;
}

static bool ability_req_parse_skill_name(cptr src, int* index)
{
    char token[64];
    cptr name;

    if (!index)
        return false;

    ability_req_copy_token(token, sizeof(token), src);
    name = token;

    if (!strncmp(name, "SKILL_", 6))
        name += 6;
    else if (!strncmp(name, "S_", 2))
        name += 2;
    else if (!strncmp(name, "SKILL", 5) && isdigit((unsigned char)name[5]))
        name += 5;
    else if ((name[0] == 'S') && isdigit((unsigned char)name[1]))
        name += 1;

    if (ability_req_parse_int(name, S_MAX - 1, index))
        return true;

    if (streq(name, "MEL") || streq(name, "MELEE"))
    {
        *index = S_MEL;
        return true;
    }
    if (streq(name, "ARC") || streq(name, "ARCHERY"))
    {
        *index = S_ARC;
        return true;
    }
    if (streq(name, "EVN") || streq(name, "EVASION"))
    {
        *index = S_EVN;
        return true;
    }
    if (streq(name, "STL") || streq(name, "STEALTH"))
    {
        *index = S_STL;
        return true;
    }
    if (streq(name, "PER") || streq(name, "PERCEPTION"))
    {
        *index = S_PER;
        return true;
    }
    if (streq(name, "WIL") || streq(name, "WILL"))
    {
        *index = S_WIL;
        return true;
    }
    if (streq(name, "SMT") || streq(name, "CMT") || streq(name, "SMITHING"))
    {
        *index = S_SMT;
        return true;
    }
    if (streq(name, "SNG") || streq(name, "SONG"))
    {
        *index = S_SNG;
        return true;
    }
    if (streq(name, "SPC") || streq(name, "SPECIAL"))
    {
        *index = S_SPC;
        return true;
    }

    return false;
}

static bool ability_req_parse_name(cptr src, bool* is_stat, int* index)
{
    if (!is_stat || !index)
        return false;

    if (ability_req_parse_stat_name(src, index))
    {
        *is_stat = true;
        return true;
    }

    if (ability_req_parse_skill_name(src, index))
    {
        *is_stat = false;
        return true;
    }

    return false;
}

static errr parse_ability_requirement_line(ability_type* b_ptr, char* s)
{
    int count = 0;
    char* comment = strchr(s, '#');
    if (comment) *comment = '\0';
    while (*s)
    {
        char* name = s;
        char* value = strchr(name, ':');
        char* next;
        int index, minimum;
        bool is_stat = true;
        if (!value) return PARSE_ERROR_GENERIC;
        *value++ = '\0';
        next = strchr(value, ':');
        if (next) *next++ = '\0';
        if (!ability_req_parse_name(name, &is_stat, &index))
            return PARSE_ERROR_GENERIC;
        if (!is_stat && index == S_SPC)
            return PARSE_ERROR_OUT_OF_BOUNDS;
        if (!ability_req_parse_int(value,
                is_stat ? 255 : BASE_SKILL_MAX, &minimum))
            return PARSE_ERROR_OUT_OF_BOUNDS;
        if (is_stat) b_ptr->stat_req[index] = (byte)minimum;
        else b_ptr->skill_req[index] = MAX(b_ptr->skill_req[index], minimum);
        count++;
        if (!next) break;
        s = next;
    }
    return count ? 0 : PARSE_ERROR_GENERIC;
}

static void ability_score_copy_weight_token(char* out, size_t out_sz, cptr src)
{
    size_t len;
    size_t i;
    size_t j = 0;

    if (!out || out_sz == 0)
        return;

    out[0] = '\0';

    if (!src)
        return;

    while (*src && isspace((unsigned char)*src))
        src++;

    len = strlen(src);
    while (len > 0 && isspace((unsigned char)src[len - 1]))
        len--;

    for (i = 0; i < len && j + 1 < out_sz; i++)
    {
        out[j++] = (char)toupper((unsigned char)src[i]);
    }

    out[j] = '\0';
}

static bool ability_score_parse_signed_long(cptr src, long* out)
{
    char token[32];
    int i = 0;
    int sign = 1;
    long value = 0;

    if (!out)
        return false;

    ability_score_copy_weight_token(token, sizeof(token), src);

    if (!token[0])
        return false;

    if (token[i] == '+')
        i++;
    else if (token[i] == '-')
    {
        sign = -1;
        i++;
    }

    if (!isdigit((unsigned char)token[i]))
        return false;

    for (; token[i]; i++)
    {
        if (!isdigit((unsigned char)token[i]))
            return false;

        value = (value * 10) + (token[i] - '0');
        if (value > 1000000L)
            return false;
    }

    *out = value * sign;
    return true;
}

static long ability_score_div_round(long num, long denom)
{
    if (denom <= 0)
        return 0;

    if (num >= 0)
        return (num + (denom / 2)) / denom;

    return -((-num + (denom / 2)) / denom);
}

static bool ability_score_parse_weight(cptr src, int* out)
{
    char token[32];
    char* slash;
    bool is_percent = false;
    size_t len;
    int sign = 1;
    int i = 0;
    bool saw_digit = false;
    long whole = 0;
    long frac = 0;
    long frac_scale = 1;
    long value;

    if (!out)
        return false;

    ability_score_copy_weight_token(token, sizeof(token), src);

    if (!token[0])
        return false;

    len = strlen(token);
    if ((len > 0) && (token[len - 1] == 'X'))
        token[--len] = '\0';
    if ((len > 0) && (token[len - 1] == '%'))
    {
        is_percent = true;
        token[--len] = '\0';
    }

    slash = strchr(token, '/');
    if (slash)
    {
        long numerator;
        long denominator;

        *slash++ = '\0';
        if (!ability_score_parse_signed_long(token, &numerator)
            || !ability_score_parse_signed_long(slash, &denominator)
            || (denominator <= 0))
        {
            return false;
        }

        value = ability_score_div_round(numerator * 100L, denominator);
        if (value < -32768L || value > 32767L)
            return false;

        *out = (int)value;
        return true;
    }

    if (token[i] == '+')
        i++;
    else if (token[i] == '-')
    {
        sign = -1;
        i++;
    }

    while (isdigit((unsigned char)token[i]))
    {
        saw_digit = true;
        whole = (whole * 10) + (token[i] - '0');
        if (whole > 32768L)
            return false;
        i++;
    }

    if (token[i] == '.')
    {
        i++;
        while (isdigit((unsigned char)token[i]))
        {
            saw_digit = true;
            if (frac_scale < 100000L)
            {
                frac = (frac * 10) + (token[i] - '0');
                frac_scale *= 10;
            }
            i++;
        }
    }

    if (!saw_digit || token[i])
        return false;

    if (is_percent)
        value = whole + ability_score_div_round(frac, frac_scale);
    else
        value = (whole * 100L) + ability_score_div_round(frac * 100L, frac_scale);

    value *= sign;

    if (value < -32768L || value > 32767L)
        return false;

    *out = (int)value;
    return true;
}

static errr parse_ability_score_line(ability_type* b_ptr, char* s)
{
    int count = 0;
    char* comment;

    if (!b_ptr || !s)
        return (PARSE_ERROR_GENERIC);

    comment = strchr(s, '#');
    if (comment)
        *comment = '\0';

    while (*s)
    {
        char* score_name;
        char* score_weight;
        char* next;
        int weight;
        int index;
        bool is_stat;

        while (*s && (isspace((unsigned char)*s) || (*s == ':')))
            s++;

        if (!*s)
            break;

        score_name = s;
        score_weight = strchr(score_name, ':');
        if (!score_weight)
            return (PARSE_ERROR_GENERIC);

        *score_weight++ = '\0';
        next = strchr(score_weight, ':');
        if (next)
            *next++ = '\0';

        if (!ability_req_parse_name(score_name, &is_stat, &index))
            return (PARSE_ERROR_GENERIC);

        if (!ability_score_parse_weight(score_weight, &weight))
            return (PARSE_ERROR_OUT_OF_BOUNDS);

        if (is_stat)
        {
            b_ptr->stat_score_weight[index] = (s16b)weight;
            b_ptr->stat_score_weight_set[index] = true;
        }
        else
        {
            b_ptr->skill_score_weight[index] = (s16b)weight;
            b_ptr->skill_score_weight_set[index] = true;
        }

        b_ptr->score_weights_set = true;
        count++;

        if (!next)
            break;

        s = next;
    }

    return (count > 0) ? 0 : (PARSE_ERROR_GENERIC);
}

errr parse_b_info(char* buf, header* head)
{
    int i;

    char *s, *t;

    static int cur_t = 0;

    /* Current entry */
    ability_type* b_ptr = PARSED_RECORD(ability_type, head);

    /* Process 'N' for "New/Number/Name" */
    if (buf[0] == 'N')
    {
        /* Find the colon before the name */
        s = strchr(buf + 2, ':');

        /* Verify that colon */
        if (!s)
            return (PARSE_ERROR_GENERIC);

        /* Nuke the colon, advance to the name */
        *s++ = '\0';

        /* Paranoia -- require a name */
        if (!*s)
            return (PARSE_ERROR_GENERIC);

        /* Get the index */
        i = atoi(buf + 2);

        /* Verify information */
        if (i <= error_idx)
            return (PARSE_ERROR_NON_SEQUENTIAL_RECORDS);

        /* Verify information */
        if (i >= head->info_num)
            return (PARSE_ERROR_TOO_MANY_ENTRIES);

        /* Save the index */
        error_idx = i;

        /* Point at the "info" */
        b_ptr = (ability_type*)head->info_ptr + i;

        /* Store the name */
        if (!(b_ptr->name = add_name(head, s)))
            return (PARSE_ERROR_OUT_OF_MEMORY);

        /* Start with the first of the tval indices */
        cur_t = 0;
    }

    /* Process 'I' for "Info" (one line only) */
    else if (buf[0] == 'I')
    {
        int skilltype, abilitynum, level;

        /* There better be a current k_ptr */
        if (!b_ptr)
            return (PARSE_ERROR_MISSING_RECORD_HEADER);

        /* Scan for the values */
        if (3 != sscanf(buf + 2, "%d:%d:%d", &skilltype, &abilitynum, &level))
            return (PARSE_ERROR_GENERIC);

        if (skilltype < 0 || skilltype >= S_MAX || abilitynum < 0
            || abilitynum >= ABILITIES_MAX || level < 0 || level > 255)
            return PARSE_ERROR_OUT_OF_BOUNDS;

        /* Save the values */
        b_ptr->skilltype = skilltype;
        b_ptr->abilitynum = abilitynum;
        b_ptr->level = level;
    }

    /* R: minimum permanent stats/base skills; S: weighted stats/skills. */
    else if (buf[0] == 'R' || buf[0] == 'S')
    {
        if (!b_ptr) return PARSE_ERROR_MISSING_RECORD_HEADER;
        return buf[0] == 'R' ? parse_ability_requirement_line(b_ptr, buf + 2)
                             : parse_ability_score_line(b_ptr, buf + 2);
    }

    else if (buf[0] == 'K' || buf[0] == 'U')
    {
        int cost;
        if (!b_ptr) return PARSE_ERROR_MISSING_RECORD_HEADER;
        if (!ability_req_parse_int(buf + 2, 255, &cost))
            return PARSE_ERROR_OUT_OF_BOUNDS;
        if (buf[0] == 'U') b_ptr->insight_upgrade_cost = (byte)cost;
        else b_ptr->insight_cost = (byte)cost;
    }

    /* C: sustained Voice cost as amount:turn interval, for songs only. */
    else if (buf[0] == 'C')
    {
        int cost;
        int interval;
        char trailing;

        if (!b_ptr) return PARSE_ERROR_MISSING_RECORD_HEADER;
        if (b_ptr->skilltype != S_SNG)
            return PARSE_ERROR_GENERIC;
        if (sscanf(buf + 2, "%d:%d%c", &cost, &interval, &trailing) != 2)
            return PARSE_ERROR_GENERIC;
        if (cost <= 0 || cost > 255 || interval <= 0 || interval > 255)
            return PARSE_ERROR_OUT_OF_BOUNDS;
        if (b_ptr->voice_cost || b_ptr->voice_cost_interval)
            return PARSE_ERROR_GENERIC;

        b_ptr->voice_cost = (byte)cost;
        b_ptr->voice_cost_interval = (byte)interval;
    }

    /* Insight is a presentation/purchase branch, not a new engine skill. Keep
     * I: identities stable for equipment, prerequisites and older saves. */
    else if (buf[0] == 'L')
    {
        int branch;
        if (!b_ptr) return PARSE_ERROR_MISSING_RECORD_HEADER;
        if (!ability_req_parse_int(buf + 2, 1, &branch))
            return PARSE_ERROR_OUT_OF_BOUNDS;
        b_ptr->insight_branch = (byte)branch;
    }

    /* B: parent skill/ability : Insight price : exclusive sibling group.
     * Repeat B: for alternative preceding stages; all alternatives share the
     * same price and choice group. */
    else if (buf[0] == 'B')
    {
        int skill, ability, cost, group;
        char tail;
        if (!b_ptr) return PARSE_ERROR_MISSING_RECORD_HEADER;
        if (sscanf(buf + 2, "%d/%d:%d:%d%c", &skill, &ability, &cost, &group, &tail) != 4)
            return PARSE_ERROR_GENERIC;
        if (skill < 0 || skill >= S_SPC || ability < 0 || ability >= ABILITIES_MAX
            || cost < 1 || cost > 255 || group < 0 || group > 255)
            return PARSE_ERROR_OUT_OF_BOUNDS;
        if (b_ptr->stage_parent_count >= ABILITY_STAGE_PARENTS_MAX)
            return PARSE_ERROR_OUT_OF_BOUNDS;
        if (b_ptr->stage_parent_count > 0
            && (b_ptr->stage_cost != cost
                || b_ptr->stage_choice_group != group))
            return PARSE_ERROR_GENERIC;
        for (int i = 0; i < b_ptr->stage_parent_count; ++i)
            if (b_ptr->stage_parent_skill[i] == skill
                && b_ptr->stage_parent_ability[i] == ability)
                return PARSE_ERROR_GENERIC;
        b_ptr->stage_parent_skill[b_ptr->stage_parent_count] = (byte)skill;
        b_ptr->stage_parent_ability[b_ptr->stage_parent_count] = (byte)ability;
        b_ptr->stage_parent_count++;
        b_ptr->stage_cost = (byte)cost;
        b_ptr->stage_choice_group = (byte)group;
    }

    /* A: every listed ability is mandatory, alongside the P: alternatives. */
    else if (buf[0] == 'A')
    {
        int skill, ability;
        char tail;
        if (!b_ptr) return PARSE_ERROR_MISSING_RECORD_HEADER;
        if (sscanf(buf + 2, "%d/%d%c", &skill, &ability, &tail) != 2)
            return PARSE_ERROR_GENERIC;
        if (skill < 0 || skill >= S_MAX || ability < 0
            || ability >= ABILITIES_MAX || b_ptr->required_count >= 4)
            return PARSE_ERROR_OUT_OF_BOUNDS;
        i = b_ptr->required_count++;
        b_ptr->required_skilltype[i] = (byte)skill;
        b_ptr->required_abilitynum[i] = (byte)ability;
    }

    /* Process 'Y' for a learned carriage-efficiency rule. */
    else if (buf[0] == 'Y')
    {
        char target[24];
        int percent;
        char trailing;

        if (!b_ptr)
            return (PARSE_ERROR_MISSING_RECORD_HEADER);
        if (2 != sscanf(buf + 2, "%23[^:]:%d%c", target, &percent,
                &trailing))
        {
            return (PARSE_ERROR_GENERIC);
        }
        if (percent <= 0 || percent >= 100)
            return (PARSE_ERROR_GENERIC);

        if (streq(target, "THROWING"))
            b_ptr->carriage_target = ABILITY_CARRIAGE_THROWING;
        else if (streq(target, "ARROWS"))
            b_ptr->carriage_target = ABILITY_CARRIAGE_ARROWS;
        else
            return (PARSE_ERROR_GENERIC);

        b_ptr->carriage_reduction_percent = (byte)percent;
    }

    /* Process 'P' for "Prerequisites" (one line only) */
    else if (buf[0] == 'P')
    {
        int i;

        /* There better be a current b_ptr */
        if (!b_ptr)
            return (PARSE_ERROR_MISSING_RECORD_HEADER);

        /* XXX Simply read each number following a colon */
        for (i = 0, s = buf + 1; s && (s[0] == ':') && s[1]; ++i)
        {
            /* Sanity check */
            if (i > 3)
                return (PARSE_ERROR_TOO_MANY_ALLOCATIONS);

            /* Default abilitynum */
            b_ptr->prereq_abilitynum[i] = 0;

            /* Store the skilltype */
            b_ptr->prereq_skilltype[i] = atoi(s + 1);

            /* List this prerequisite */
            b_ptr->prereqs++;

            /* Find the slash */
            t = strchr(s + 1, '/');

            /* Find the next colon */
            s = strchr(s + 1, ':');

            /* If the slash is "nearby", use it */
            if (t && (!s || t < s))
            {
                int prereq_abilitynum = atoi(t + 1);
                if (prereq_abilitynum > 0)
                    b_ptr->prereq_abilitynum[i] = prereq_abilitynum;
            }
        }
    }

    /* Process 'D' for "Description" */
    else if (buf[0] == 'D')
    {
        /* There better be a current k_ptr */
        if (!b_ptr)
            return (PARSE_ERROR_MISSING_RECORD_HEADER);

        /* Get the text */
        s = buf + 2;

        /* Store the text */
        if (!add_text(&(b_ptr->text), head, s))
            return (PARSE_ERROR_OUT_OF_MEMORY);
    }

    /* Process 'E' for "Effect" (mechanical description) */
    else if (buf[0] == 'E')
    {
        /* There better be a current b_ptr */
        if (!b_ptr)
            return (PARSE_ERROR_MISSING_RECORD_HEADER);

        /* Get the text */
        s = buf + 2;

        /* Store the effect text */
        if (!add_text(&(b_ptr->effect), head, s))
            return (PARSE_ERROR_OUT_OF_MEMORY);
    }

    /* Process 'T' for "Types allowed" (up to five lines) */
    else if (buf[0] == 'T')
    {
        int tval, sval1, sval2;

        /* There better be a current b_ptr */
        if (!b_ptr)
            return (PARSE_ERROR_MISSING_RECORD_HEADER);

        /* Scan for the values */
        if (3 != sscanf(buf + 2, "%d:%d:%d", &tval, &sval1, &sval2))
            return (PARSE_ERROR_GENERIC);

        /* Save the values */
        b_ptr->tval[cur_t] = (byte)tval;
        b_ptr->min_sval[cur_t] = (byte)sval1;
        b_ptr->max_sval[cur_t] = (byte)sval2;

        /* Increase counter for 'possible tval' index */
        cur_t++;

        /* Allow only a limited number of T: lines */
        if (cur_t > ABILITY_TVALS_MAX)
            return (PARSE_ERROR_GENERIC);
    }

    else
    {
        /* Oops */
        return (PARSE_ERROR_UNDEFINED_DIRECTIVE);
    }

    /* Success */
    return (0);
}

/* Insight's changing progression is deliberately kept out of ability.txt.
 * This parser applies the optional ability-insight.txt overlay to the stable
 * ability records after the normal ability template has loaded. */
static ability_type* insight_overlay_b_ptr;

static errr parse_b_insight_overlay(char* buf, header* head)
{
    int i;
    char* s;

    if (buf[0] == 'N')
    {
        cptr expected_name;

        s = strchr(buf + 2, ':');
        if (!s)
            return PARSE_ERROR_GENERIC;
        *s++ = '\0';
        if (!*s)
            return PARSE_ERROR_GENERIC;

        i = atoi(buf + 2);
        if (i <= error_idx)
            return PARSE_ERROR_NON_SEQUENTIAL_RECORDS;
        if (i < 0 || i >= head->info_num)
            return PARSE_ERROR_TOO_MANY_ENTRIES;

        insight_overlay_b_ptr = (ability_type*)head->info_ptr + i;
        if (!head->name_ptr)
        {
            insight_overlay_b_ptr = NULL;
            return PARSE_ERROR_GENERIC;
        }
        expected_name = head->name_ptr + insight_overlay_b_ptr->name;
        if (!streq(expected_name, s))
        {
            insight_overlay_b_ptr = NULL;
            return PARSE_ERROR_GENERIC;
        }

        error_idx = i;
    }
    else if (buf[0] == 'K' || buf[0] == 'U')
    {
        int cost;

        if (!insight_overlay_b_ptr)
            return PARSE_ERROR_MISSING_RECORD_HEADER;
        if (!ability_req_parse_int(buf + 2, 255, &cost))
            return PARSE_ERROR_OUT_OF_BOUNDS;

        if (buf[0] == 'K')
        {
            if (insight_overlay_b_ptr->insight_cost)
                return PARSE_ERROR_GENERIC;
            insight_overlay_b_ptr->insight_cost = (byte)cost;
        }
        else
        {
            if (insight_overlay_b_ptr->insight_upgrade_cost)
                return PARSE_ERROR_GENERIC;
            insight_overlay_b_ptr->insight_upgrade_cost = (byte)cost;
        }
    }
    else if (buf[0] == 'L')
    {
        int branch;

        if (!insight_overlay_b_ptr)
            return PARSE_ERROR_MISSING_RECORD_HEADER;
        if (!ability_req_parse_int(buf + 2, 1, &branch))
            return PARSE_ERROR_OUT_OF_BOUNDS;
        if (insight_overlay_b_ptr->insight_branch)
            return PARSE_ERROR_GENERIC;
        insight_overlay_b_ptr->insight_branch = (byte)branch;
    }
    else if (buf[0] == 'B')
    {
        int skill, ability, cost, group;
        char tail;
        int slot;

        if (!insight_overlay_b_ptr)
            return PARSE_ERROR_MISSING_RECORD_HEADER;
        if (sscanf(buf + 2, "%d/%d:%d:%d%c", &skill, &ability, &cost,
                &group, &tail) != 4)
            return PARSE_ERROR_GENERIC;
        if (skill < 0 || skill >= S_SPC || ability < 0
            || ability >= ABILITIES_MAX || cost < 1 || cost > 255
            || group < 0 || group > 255)
            return PARSE_ERROR_OUT_OF_BOUNDS;

        slot = insight_overlay_b_ptr->stage_parent_count;
        if (slot >= ABILITY_STAGE_PARENTS_MAX)
            return PARSE_ERROR_OUT_OF_BOUNDS;
        if (slot > 0
            && (insight_overlay_b_ptr->stage_cost != cost
                || insight_overlay_b_ptr->stage_choice_group != group))
            return PARSE_ERROR_GENERIC;
        for (i = 0; i < slot; ++i)
        {
            if (insight_overlay_b_ptr->stage_parent_skill[i] == skill
                && insight_overlay_b_ptr->stage_parent_ability[i] == ability)
                return PARSE_ERROR_GENERIC;
        }

        insight_overlay_b_ptr->stage_parent_skill[slot] = (byte)skill;
        insight_overlay_b_ptr->stage_parent_ability[slot] = (byte)ability;
        insight_overlay_b_ptr->stage_parent_count++;
        insight_overlay_b_ptr->stage_cost = (byte)cost;
        insight_overlay_b_ptr->stage_choice_group = (byte)group;
    }
    else
    {
        /* The overlay may only change Insight metadata. */
        return PARSE_ERROR_UNDEFINED_DIRECTIVE;
    }

    return 0;
}

errr init_b_insight_overlay(header* head)
{
    SDL_IOStream* fp;
    char path[1024];
    errr err;
    int i;
    u32b name_size;
    u32b text_size;

    /* The base raw cache may have been generated before the split. Clear all
     * overlay-owned fields first so removed or relocated stages cannot linger
     * when an old cache is otherwise considered usable. */
    for (i = 0; i < head->info_num; ++i)
    {
        ability_type* ability = (ability_type*)head->info_ptr + i;
        ability->insight_branch = 0;
        ability->insight_cost = 0;
        ability->insight_upgrade_cost = 0;
        ability->stage_parent_count = 0;
        memset(ability->stage_parent_skill, 0,
            sizeof(ability->stage_parent_skill));
        memset(ability->stage_parent_ability, 0,
            sizeof(ability->stage_parent_ability));
        ability->stage_cost = 0;
        ability->stage_choice_group = 0;
    }

    path_build(path, sizeof(path), ANGBAND_DIR_EDIT, "ability-insight.txt");
    fp = sdl_fopen(path, "r");
    if (!fp)
    {
        log_debug("Optional Insight ability overlay '%s' is not present", path);
        return 0;
    }

    /* init_info_txt is also used for allocating template strings. Preserve
     * the already-loaded base header sizes because this overlay adds none. */
    name_size = head->name_size;
    text_size = head->text_size;
    insight_overlay_b_ptr = NULL;
    err = init_info_txt(fp, path, head, parse_b_insight_overlay);
    sdl_fclose(fp);
    head->name_size = name_size;
    head->text_size = text_size;

    if (err)
    {
        display_parse_error("ability-insight", err, path);
        return err;
    }

    return 0;
}

/*
 * Grab one flag in an artefact_type from a textual string
 */

#endif /* ALLOW_TEMPLATES */
