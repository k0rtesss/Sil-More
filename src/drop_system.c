#include "angband.h"
#include "drop_system.h"
#include "drop/drop-system-internal.h"

drop_quality drop_quality_from_flags(bool good, bool great, bool superb)
{
    if (superb)
        return DROP_QUALITY_SUPERB;
    if (great)
        return DROP_QUALITY_GREAT;
    if (good)
        return DROP_QUALITY_GOOD;
    return DROP_QUALITY_NORMAL;
}

void drop_profile_default(drop_profile* profile)
{
    if (!profile)
        return;

    profile->weight_weapon = DROP_DEFAULT_CAT_WEIGHT;
    profile->weight_armor = DROP_DEFAULT_CAT_WEIGHT;
    profile->weight_jewelry = DROP_DEFAULT_CAT_WEIGHT;
    profile->weight_supply = DROP_DEFAULT_CAT_WEIGHT;
    profile->supply_potion = DROP_DEFAULT_SUPPLY_WEIGHT;
    profile->supply_herb = DROP_DEFAULT_SUPPLY_WEIGHT;
    profile->supply_gem = DROP_DEFAULT_SUPPLY_WEIGHT;
    profile->supply_staff = DROP_DEFAULT_SUPPLY_WEIGHT;
    profile->supply_misc = DROP_DEFAULT_SUPPLY_WEIGHT;
    profile->supply_tunneling = 0; /* Disabled by default */
    profile->allow_damaged = false;
}
