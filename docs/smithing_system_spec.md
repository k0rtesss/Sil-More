# Sil-Morë Smithing Stat Scaling — Implementation Spec

## Goal
Replace current `effective_smithing = Smithing + Grace` with a larger multi-stat contribution so Difficulty ~30 is reachable without abandoning all non-Smithing development.

Keep existing Smithing ability levels/names unless noted below. Current Smithing abilities are defined in `ability.txt` (`Weaponsmith`..`Reforging`).

---

## 1. Core formula

```text
effective_smithing = base_smithing + flat_smithing_bonuses + floor(category_bonus + mastery_bonus)
```

- `base_smithing` = invested Smithing ranks only.
- `flat_smithing_bonuses` = existing explicit Smithing bonuses (forge, Oath of the Smith, etc.).
- Replace the old direct `+ Grace`; do **not** stack it on top of this system.
- Round once, after summing all stat-derived bonuses.

### Craft stats
Use intrinsic/permanent character stats for crafting.

Do **not** include temporary/conditional combat boosts (e.g. Strength in Adversity, Charge, temporary song/buff effects) in Smithing stat contribution.

---

## 2. Base category bonuses

All normal craft categories start at approximately **3 stats worth** of contribution.

```text
HEAVY_METAL = 0.5*STR + 1.5*DEX + 1.0*GRA
MAIL        = 2.0*DEX + 1.0*GRA
JEWELLERY   = 1.0*DEX + 2.0*GRA
LIGHT_CRAFT = 2.0*DEX + 1.0*GRA
```

### Category mapping

- `HEAVY_METAL`: swords, axes, hammers, spearheads, metal shields, helms, solid/formed metal armour pieces.
- `MAIL`: mail corslets, hauberks, mail coifs, linked-ring armour.
- `JEWELLERY`: rings, amulets, horns, light sources, fine ornamental objects.
- `LIGHT_CRAFT`: bows, wooden shields, leather/cloth equipment if still handled by Smithing UI.

Category is fixed by base item type; affixes/weight changes must not change the formula.

---

## 3. Mastery bonuses from abilities

These bonuses apply to **all items the character is otherwise allowed to craft**, including ordinary items.

```text
Expertise:   +1.0 * DEX
Enchantment: +1.0 * GRA
Artifice:    +1.0 * GRA
```

Thus full advanced contribution becomes:

```text
HEAVY_METAL + full mastery = 0.5*STR + 2.5*DEX + 3.0*GRA
MAIL        + full mastery = 3.0*DEX + 3.0*GRA
JEWELLERY   + full mastery = 2.0*DEX + 4.0*GRA
LIGHT_CRAFT + full mastery = 3.0*DEX + 3.0*GRA
```

Do not give `Alloy mastery` or `Reforging` additional raw Smithing capacity in the first implementation pass.

---

## 4. Smithing ability changes

### Weaponsmith — level 2
No stat requirement.

Unlock weapon crafting and identification effects as now.

### Armoursmith — level 3
No stat requirement.

Unlock armour crafting and identification effects as now.

### Jeweller — level 4
No stat requirement.

Unlock jewellery/light/horn crafting and identification effects as now.

### Enchantment — level 5
Requirement:

```text
GRA >= 2
```

Effect additions:

```text
+ GRA to effective Smithing
```

Retain special-item/enchantment functionality.

### Expertise — level 6
Prerequisite: at least one of `Weaponsmith`, `Armoursmith`, `Jeweller`.

Requirement:

```text
DEX >= 2
```

Effect additions:

```text
+ DEX to effective Smithing
```

Retain halved forging time and current cost-efficiency behavior, except it must **not** negate Masterpiece/Aulë overcap sacrifice costs.

### Artifice — level 7
Prerequisites:

```text
Enchantment
AND at least one of Weaponsmith / Armoursmith / Jeweller
```

High stat requirements:

```text
DEX >= 4
GRA >= 4
```

Effect additions:

```text
+ GRA to effective Smithing
```

Retain highly customised artifact creation and identification effects.

Artifice does not grant unknown recipes/material secrets automatically.

### Masterpiece — level 8
Prerequisite:

```text
Enchantment
AND at least one of Weaponsmith / Armoursmith / Jeweller
```

High stat requirements:

```text
DEX >= 4
GRA >= 5
```

Keep existing overcap effect:

```text
excess = item_difficulty - normal_effective_smithing
cost   = excess base Smithing ranks
```

Only usable when `excess > 0` and within the existing allowed overcap limit.

Masterpiece does not bypass missing craft permissions, recipes, materials, Enchantment, Artifice, or material knowledge.

### Grace — level 10
Keep `+1 Grace` unchanged. Because Grace is now multiplied by category/mastery coefficients, this ability becomes substantially more valuable automatically.

### Alloy mastery — level 6
Keep as material/recipe capability only in this pass.

### Reforging — level 4
Use the target item's category formula when checking Smithing capacity.

Retain current `1.5x difficulty increase` rule unless separately rebalanced.

---

## 5. Aulë's Forge / Masterpiece interaction

Aulë's Forge supersedes Masterpiece exactly as now.

```text
normal_effective_smithing = base + flat + stats + mastery
excess = item_difficulty - normal_effective_smithing
```

- Masterpiece: existing 1 base Smithing per excess point.
- Aulë's Forge: existing 1 base Smithing per 2 excess points, rounded up, with its existing maximum excess rule.
- Expertise must never remove these sacrifice costs.

---

## 6. Ability requirement checks

Stat requirements must use intrinsic/permanent stats, not temporary/conditional buffs.

`Quick Study` may bypass ability-tree prerequisites if that remains its intended behavior, but should **not** bypass:

- Smithing level requirement
- DEX/GRA requirement
- item-category permission
- material/recipe knowledge

If current engine semantics intentionally allow Quick Study to bypass level requirements too, leave level behavior unchanged; only DEX/GRA gates must remain mandatory.

---

## 7. Expected balance targets

Example character:

```text
STR 3 / DEX 3 / GRA 3 / Smithing 12
```

For `HEAVY_METAL`:

```text
Base category: 0.5*3 + 1.5*3 + 3 = 9
Smithing 12 + category 9 = 21
+ Expertise               = 24
+ Enchantment              = 27
+ Artifice                 = 30
```

Target result: Difficulty 30 should be attainable around Smithing 12-16 with strong crafting development and reasonable stats, rather than requiring ~27+ invested Smithing or total build sacrifice.

Do **not** globally increase recipe difficulties when implementing this system. Rebalance individual outliers only after playtesting.

---

## 8. Acceptance tests

1. `STR=3 DEX=3 GRA=3`, Smithing 12, no advanced mastery, heavy-metal item => effective Smithing 21 before flat bonuses.
2. Same + Expertise => 24.
3. Same + Expertise + Enchantment => 27.
4. Same + Expertise + Enchantment + Artifice => 30.
5. Mail with `DEX=4 GRA=2`, no mastery => stat bonus 10.
6. Jewellery with `DEX=2 GRA=4`, no mastery => stat bonus 10.
7. Temporary `+3 STR/DEX/GRA` from Strength in Adversity does not change Smithing capacity.
8. Artifice cannot be learned with `DEX=3` or `GRA=3`; both must be at least 4.
9. Masterpiece cannot be learned below `DEX=4` and `GRA=5`.
10. Expertise does not erase Smithing loss paid by Masterpiece or Aulë's Forge.
11. Reforging a mail item uses `MAIL`; reforging a ring uses `JEWELLERY`.
12. Old standalone `+Grace` contribution is removed; Grace is counted only through the new category/mastery formulas.

---

## 9. Data / engine work

`ability.txt` currently supports ability skill/level/prerequisite/effect text, but no explicit attribute-requirement field. Implement DEX/GRA requirement enforcement in engine code or extend ability data parsing before changing descriptions.

Update `E:` descriptions after mechanics are implemented so UI reflects the new bonuses and stat gates.
