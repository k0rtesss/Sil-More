#ifndef INCLUDED_OBJECT_DIFFICULTY_H
#define INCLUDED_OBJECT_DIFFICULTY_H

#include "angband.h"

enum object_difficulty_part {
    OBJECT_DIF_BASE, OBJECT_DIF_WEIGHT, OBJECT_DIF_ATTACK, OBJECT_DIF_EVASION,
    OBJECT_DIF_DAMAGE, OBJECT_DIF_PROTECTION, OBJECT_DIF_WEAPON,
    OBJECT_DIF_TUNNEL, OBJECT_DIF_STATS, OBJECT_DIF_SKILLS, OBJECT_DIF_MULTIPLE,
    OBJECT_DIF_SUSTAINS, OBJECT_DIF_POWERS, OBJECT_DIF_RESISTS,
    OBJECT_DIF_DRAWBACKS, OBJECT_DIF_ABILITIES, OBJECT_DIF_PARTS
};

typedef struct object_difficulty_breakdown {
    int parts[OBJECT_DIF_PARTS];
    int subtotal, character_percent, slot_percent, enchantable_percent;
    int multiplier, scaled, total;
    bool artefact_arrows;
} object_difficulty_breakdown;

/* One intrinsic valuation for drop entries, descriptions, and smithing.
 * Adjustments are percentage points, combined before the single division.
 * Character-specific smithing discounts never change the neutral item value. */
int object_difficulty_base(const object_type* object);
int object_difficulty_adjusted(const object_type* object, int percent_adjustment);

/* Optional accounting records the same arithmetic used for the returned value. */
int object_difficulty_evaluate(const object_type* object, int percent_adjustment,
    object_difficulty_breakdown* breakdown);

#endif
