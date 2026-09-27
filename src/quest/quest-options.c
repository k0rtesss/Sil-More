#include "angband.h"
#include "blitz.h"
#include "externs.h"
#include "quest/quest-options.h"

static int quest_option_for_id(int id)
{
    static const int options[] = {
        OPT_quest_1, OPT_quest_2, OPT_quest_3, OPT_quest_4,
        OPT_quest_5, OPT_quest_6, OPT_quest_7, OPT_quest_8,
        OPT_quest_9, OPT_quest_10, OPT_quest_11, OPT_quest_12,
        OPT_quest_13, OPT_quest_14, OPT_quest_15, OPT_quest_16,
    };

    if (id < 1 || id > (int)N_ELEMENTS(options))
        return -1;

    return options[id - 1];
}

bool quest_enabled(int id)
{
    int opt;

    if (!op_ptr || (id > 6 && run_mode_is_blitz()))
        return false;

    opt = quest_option_for_id(id);
    return opt >= 0 && op_ptr->opt[opt];
}

static bool quest_system_option_enabled(int opt)
{
    return !run_mode_is_blitz() && op_ptr && op_ptr->opt[opt];
}

bool quest_rules_enabled(void)
{
    return quest_system_option_enabled(OPT_quest_rules_beta);
}

bool quest_rewards_enabled(void)
{
    return quest_system_option_enabled(OPT_quest_rewards_beta);
}

bool quest_challenges_enabled(void)
{
    return quest_system_option_enabled(OPT_quest_challenges_beta);
}

bool quest_lineage_enabled(void)
{
    return quest_system_option_enabled(OPT_quest_lineage_beta);
}

void quest_set_enabled(int id, bool enabled)
{
    int opt;

    if (!op_ptr)
        return;

    opt = quest_option_for_id(id);
    if (opt >= 0)
        op_ptr->opt[opt] = enabled;
}
