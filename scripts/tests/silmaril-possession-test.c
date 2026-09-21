#include "angband.h"
#include "object/object-inventory.h"
#include "score/score_entry.c"
#include <assert.h>

static object_type held[INVEN_TOTAL];

static void clear_carried_items(void)
{
    memset(held, 0, sizeof(held));
    player_carried_extra_reset_store();
    inventory = held;
}

int main(void)
{
    object_type extra = {0};

    clear_carried_items();
    held[INVEN_LITE].tval = TV_LIGHT;
    held[INVEN_LITE].sval = SV_LIGHT_SILMARIL;
    held[INVEN_LITE].number = 1;
    assert(silmarils_possessed() == 1);

    clear_carried_items();
    extra.k_idx = 1;
    extra.tval = TV_LIGHT;
    extra.sval = SV_LIGHT_SILMARIL;
    extra.number = 1;
    extra.storage = OBJECT_STORAGE_HARNESS;
    assert(player_carried_extra_load(&extra));
    assert(silmarils_possessed() == 1);

    clear_carried_items();
    memset(&extra, 0, sizeof(extra));
    extra.k_idx = 1;
    extra.name1 = ART_MORGOTH_2;
    extra.number = 1;
    extra.storage = OBJECT_STORAGE_HARNESS;
    assert(player_carried_extra_load(&extra));
    assert(silmarils_possessed() == 2);
    assert(has_iron_crown() == ART_MORGOTH_2);

    puts("Silmaril possession: legacy and expandable Harness entries are counted: PASS");
    return 0;
}
