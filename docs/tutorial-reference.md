# Gameplay tutorial reference

This continuous document contains the authored lessons from `lib/help/tutorials.json`. The UI owns wrapping and pagination. `{subject}`, `{detail}` and `{context}` remain runtime placeholders; they contain only information already available to the hero.

Info and decision explanations use Continue. Required action steps complete only after the matching real action commits. Reading, skipping and reviewing are free; game actions retain their normal costs and consequences. The archive turns every step into a read-only explanation.

The catalogue contains 510 lessons, including 107 ability previews. Every live ability serial, item kind handled by the aware-effect producer and meaningful public terrain serial has a checked entry. This checks source/data coverage, not physical-device interaction.

## Resource route

`src/init/init-paths.c` resolves `ANGBAND_DIR_HELP` from the installed data root. `src/tutorial/tutorial.c` lazily reads `tutorials.json` from that directory. `build-cmake.bat` stages Help for both Windows deployments; `CMakeLists.txt` includes Help JSON in iOS resources; `android/app/build.gradle` synchronizes the game library into Android assets before building. The validator checks these source routes; this is not confirmation of a device installation.

## Tutorial modes

Default: **Extended**. The catalogue has **146 Normal** lessons and **364 Extended** lessons. The card's single mode button cycles **Disabled → Normal → Extended → Disabled**.

Normal covers core controls, survival, general item handling and its complete action chains, storage, main menus, combat fundamentals and Tale events. Extended includes all Normal lessons and adds individual abilities and item effects, learned monster traits, terrain and region details, individual quest introductions and specialist status or knowledge pages.

Filtering a lesson never marks it learned or skipped. Lowering the mode withdraws an Extended card and pending Extended observations while keeping completed steps in the Tale history. The learned archive remains read-only and can show encountered lessons from either level, including when gameplay tutorials are Disabled.

Characters deferred during an upgrade remain exempt from automatic tutorials for that character. Changing mode or resetting the Tale lesson history does not bypass that gate. The upgrade notice belongs to its own native UI and is independent of catalogue filtering.

Skeletons and chests use Normal informational feature lessons. Their old generic item IDs remain untouched if present in saved history, and they do not enter examine/equip/use tutorial chains.

## Your first steps

`opening.move`

Level: **Normal**.

**1. Info**

Sil-More is a hardcore roguelike. Move slowly, plan ahead, and treat each new challenge as a puzzle: inspect what is known, find a counter, and choose your next action deliberately. Reading is free. Continue advances explanations; Skip ends this lesson. The mode button cycles Disabled, Normal and Extended tutorials.

**2. Info**

Find a suitable weapon and armour. If your oath restricts found equipment, prepare gear that follows its rules. Carrying, readying and equipping are different actions. Inspect the item and the proposed action before committing; actual game actions retain their normal costs.

**3. Action**

Move one square onto safe open floor. Choose any legal direction. This is a real move: creatures can act and the minimum-depth clock advances normally. Skip if you prefer another action.

Required action: `move`.

Trigger: A new Story hero starts at playerturn <= 1, excluding restored saves.

Sources: `src/tutorial/tutorial-game.c`.

## A creature in sight

`combat.first_monster`

Level: **Normal**.

**1. Info**

{subject} is visible. Awareness says whether a creature has noticed you; morale describes its willingness to fight. Neither is a guarantee about what it will do next.

**2. Action**

Enter Stealth mode. Stealth helps avoid notice, but moving stealthily is slower. An already alert foe may still follow you. Skip if remaining fast is more useful.

Required action: `stealth`.

Trigger: A non-peaceful creature is visible in line of sight; the player is not hallucinating or raging.

Sources: `src/melee/melee-process.c`, `src/tutorial/tutorial-game.c`.

## Make one melee attack

`combat.first_adjacent`

Level: **Normal**.

**1. Info**

{subject} is beside you. Moving toward an adjacent hostile creature attacks instead of moving. Check the active weapon and the target before committing.

**2. Action**

Attack an adjacent legal hostile once. A miss still completes this practice. The creature may retaliate; Skip lets you choose a different tactic.

Required action: `attack`; subject: `monster`.

Trigger: A visible adjacent hostile is legally attackable, melee is active, and fear/confusion/truce/oath checks permit attack.

Sources: `src/tutorial/tutorial-game.c`.

## Read the attack result

`combat.first_result`

Level: **Normal**.

**1. Info**

{detail} An attack hits only when Attack + d20 exceeds Evasion + d20. A tie misses. With equal scores, the hit chance is 47.5%. Higher attack improves both hit chance and critical margins.

**2. Info**

On a hit, damage dice are rolled, then the target's applicable Protection is rolled and subtracted. A hit can deal zero damage. Check the combat history for the rolls and any revealed effects.

**3. Info**

Criticals add damage dice. The base interval is about 7 + weapon weight in pounds per die. For a 3 lb melee weapon, margins 10 and 20 add one and two dice. Finesse changes those thresholds to 8 and 16.

Trigger: After a committed player attack, including a miss.

Sources: `src/cmd/combat/cmd-combat.c`, `src/melee/melee-attack.c`.

## You lost Health

`combat.first_damage`

Level: **Normal**.

**1. Info**

{detail} Damage and a status effect are separate outcomes. Examine the combat result and current conditions before deciding whether to fight, retreat or use a known remedy.

Trigger: Explicit public observation at a safe player or menu boundary.

Sources: `src/tutorial/tutorial-game.c`.

## A fleeing enemy

`combat.fleeing`

Level: **Normal**.

**1. Info**

{subject} is fleeing. This is a morale state, separate from whether it has noticed you. Pursuit spends turns and may expose you to other enemies. Oath of Valour forbids harming fleeing foes.

Trigger: Explicit public observation at a safe player or menu boundary.

Sources: `src/melee/melee-process.c`, `src/cmd/combat/cmd-combat.c`, `lib/edit/oath.txt`.

## Several adjacent foes

`combat.surrounded`

Level: **Normal**.

**1. Info**

Each additional adjacent opponent helps the enemies surround you. Crowd Fighting halves their surrounding bonus. Narrow passages can reduce the number attacking together, but moving remains a real action.

Trigger: Explicit public observation at a safe player or menu boundary.

Sources: `src/cmd/combat/cmd-combat.c`.

## Critical hits

`combat.critical`

Level: **Normal**.

**1. Info**

Critical damage comes from extra dice, not a fixed damage multiplier. Weight makes additional critical dice harder to earn. Finesse reduces the base interval by 2; Power increases it by 1. Subtlety subtracts another 2 only under its equipment requirements.

Trigger: Explicit public observation at a safe player or menu boundary.

Sources: `src/cmd/combat/cmd-combat.c`.

## A new level

`world.depth`

Level: **Normal**.

**1. Info**

{detail} Going through stairs or a shaft creates a new level. Plan around your current minimum depth and any oath restrictions; the map you left is not a route you can count on returning to.

Trigger: Explicit public observation at a safe player or menu boundary.

Sources: `src/cmd/movement/cmd-movement.c`.

## Minimum depth advances

`world.minimum_depth`

Level: **Normal**.

**1. Info**

{detail} The pressure advances with game actions. Reading cards, menus and descriptions does not spend turns. Running, resting, repeated commands and accessing the Pack can still advance game time.

Trigger: Explicit public observation at a safe player or menu boundary.

Sources: `src/dungeon/dungeon-loop.c`, `src/cmd/movement/cmd-movement.c`.

## Read an item before using it

`item.first_description`

Level: **Normal**.

**1. Info**

{subject} has a description. It separates known properties from information your hero has not discovered. Do not assume an unfamiliar item is a remedy.

**2. Action**

Examine the item. Select its description through Look, Inventory or Equipment. Opening a description is free; actually picking up, equipping or using it keeps its usual rules.

Required action: `examine`.

Trigger: Explicit public observation at a safe player or menu boundary.

Sources: `src/tutorial/tutorial-game.c`.

## Understanding the description

`item.description`

Level: **Normal**.

**1. Info**

{detail} Damage such as (2d5) means two five-sided dice. Protection such as [1d3] is rolled when it applies. Read attack, evasion, weight, volume, active-hand requirements, known bonuses and drawbacks together.

Trigger: Explicit public observation at a safe player or menu boundary.

Sources: `src/object/object-info.c`, `src/object/object-desc.c`.

## Melee weapons

`item.weapon`

Level: **Normal**.

**1. Info**

Compare damage dice, attack, evasion, weight and hand requirements. Strength changes damage sides within the weapon's weight limit; a lighter weapon also earns critical dice at smaller margins. Ready the weapon and make melee active when appropriate.

Trigger: The item type is publicly encountered; no hidden subtype or property is used.

Sources: `src/object/object-info.c`, `src/object/object-inventory.c`, `src/tutorial/tutorial-game.c`.

## Throwing weapons

`item.throwing`

Level: **Normal**.

**1. Info**

Throwing uses its own range, accuracy and handling rules. A throwable item in the Pack is not automatically available as an active weapon. Read its Harness, quick-throw and hand requirements before aiming.

Trigger: The item type is publicly encountered; no hidden subtype or property is used.

Sources: `src/object/object-info.c`, `src/object/object-inventory.c`, `src/tutorial/tutorial-game.c`.

## Bows

`item.bow`

Level: **Normal**.

**1. Info**

A bow needs compatible arrows and must be active to fire normally. Strength and bow weight affect damage. Firing near alert foes can allow attacks of opportunity; Point Blank Archery protects only against the adjacent target.

Trigger: The item type is publicly encountered; no hidden subtype or property is used.

Sources: `src/object/object-info.c`, `src/object/object-inventory.c`, `src/tutorial/tutorial-game.c`.

## Arrows and the Quiver

`item.arrows`

Level: **Normal**.

**1. Info**

Arrows can occupy the Quiver or Pack. Quiver capacity and Pack volume are separate limits; a partial stack may fit even when the whole stack does not. Choose the arrow stack used by your bow and read its known bonuses.

Trigger: The item type is publicly encountered; no hidden subtype or property is used.

Sources: `src/object/object-info.c`, `src/object/object-inventory.c`, `src/tutorial/tutorial-game.c`.

## Armour

`item.armour`

Level: **Normal**.

**1. Info**

Protection reduces damage after a hit; Evasion helps the hit miss. Read both, plus weight and skill penalties. Some abilities require only light armour. Ordinary physical protection does not automatically apply to elemental damage.

Trigger: The item type is publicly encountered; no hidden subtype or property is used.

Sources: `src/object/object-info.c`, `src/object/object-inventory.c`, `src/tutorial/tutorial-game.c`.

## Shields

`item.shield`

Level: **Normal**.

**1. Info**

A shield contributes only when your active weapon and hand arrangement allow it. A two-handed weapon can prevent the shield from helping. Blocking doubles shield protection if you did not move on the previous turn.

Trigger: The item type is publicly encountered; no hidden subtype or property is used.

Sources: `src/object/object-info.c`, `src/object/object-inventory.c`, `src/tutorial/tutorial-game.c`.

## Rings

`item.ring`

Level: **Normal**.

**1. Info**

Known ring bonuses and drawbacks apply only under their equipment rules. Inspect the full description before changing jewelry. A curse can prevent removal; unidentified properties remain unknown.

Trigger: The item type is publicly encountered; no hidden subtype or property is used.

Sources: `src/object/object-info.c`, `src/object/object-inventory.c`, `src/tutorial/tutorial-game.c`.

## Amulets

`item.amulet`

Level: **Normal**.

**1. Info**

Read the known attribute, skill, resistance and drawback lines before equipping. Jewelry sets offer a way to organise combinations; selecting a set is separate from learning an unknown item's properties.

Trigger: The item type is publicly encountered; no hidden subtype or property is used.

Sources: `src/object/object-info.c`, `src/object/object-inventory.c`, `src/tutorial/tutorial-game.c`.

## Potions

`item.potion`

Level: **Normal**.

**1. Info**

A known potion names an effect; an unfamiliar one may be harmful. Read whether it cures a condition, restores a resource, grants a temporary benefit or merely offsets a penalty. Use a relevant known potion when you choose, not just to identify it.

Trigger: The item type is publicly encountered; no hidden subtype or property is used.

Sources: `src/object/object-info.c`, `src/object/object-inventory.c`, `src/tutorial/tutorial-game.c`.

## Food and herbs

`item.food`

Level: **Normal**.

**1. Info**

Ordinary food restores nourishment. Herbs may also heal, restore drained attributes, grant a temporary effect or cause harm. Read the known effect before eating. Hunger, attribute drain and wounds need different remedies.

Trigger: The item type is publicly encountered; no hidden subtype or property is used.

Sources: `src/object/object-info.c`, `src/object/object-inventory.c`, `src/tutorial/tutorial-game.c`.

## Light sources

`item.light`

Level: **Normal**.

**1. Info**

Radius determines how far light reaches; intensity determines its strength. Fuel is another separate limit. Equip an appropriate light and inspect its remaining fuel; blindness and darkness effects require different responses.

Trigger: The item type is publicly encountered; no hidden subtype or property is used.

Sources: `src/object/object-info.c`, `src/object/object-inventory.c`, `src/tutorial/tutorial-game.c`.

## Oil containers

`item.oil`

Level: **Normal**.

**1. Info**

Oil goes into Supplies using an oil slot. A container holds up to 2,500 turns of light. Refuelling and carrying oil have their own rules; an oil flask is not itself a wearable light source.

Trigger: The item type is publicly encountered; no hidden subtype or property is used.

Sources: `src/object/object-info.c`, `src/object/object-inventory.c`, `src/tutorial/tutorial-game.c`.

## Gems

`item.gem`

Level: **Normal**.

**1. Info**

Gems have known consumable effects such as detection, identification or warding. Read the effect and any target selection before using one. You can inspect a choice without spending the gem; committing its effect is a separate action.

Trigger: The item type is publicly encountered; no hidden subtype or property is used.

Sources: `src/object/object-info.c`, `src/object/object-inventory.c`, `src/tutorial/tutorial-game.c`.

## Digging tools

`item.digging`

Level: **Normal**.

**1. Info**

A digging tool helps with terrain interactions. Read the terrain and required tool before tunnelling. Digging can take repeated turns and make noise; a tool does not make every wall or hazard safe.

Trigger: The item type is publicly encountered; no hidden subtype or property is used.

Sources: `src/object/object-info.c`, `src/object/object-inventory.c`, `src/tutorial/tutorial-game.c`.

## Smithing metal

`item.metal`

Level: **Normal**.

**1. Info**

Mithril and star iron are crafting materials. A forge, applicable smithing abilities, resources and sufficient difficulty allowance govern what you can make. Inspect the finished proposal and its costs before committing.

Trigger: The item type is publicly encountered; no hidden subtype or property is used.

Sources: `src/object/object-info.c`, `src/object/object-inventory.c`, `src/tutorial/tutorial-game.c`.

## Opening chests

`world.chest`

Level: **Normal**.

**1. Info**

Use the chest's contextual action on the dungeon floor. Chest inspection, disarming and opening are real actions with their own checks and risks; discovering a trap does not disarm it. This differs from a free item preview. Opening releases loot, while an empty chest provides no extra storage space. The chest menu can be cancelled before committing an action.

Trigger: A chest is actually encountered on the player square or visibly adjacent, including an empty chest.

Sources: `src/cmd/world/cmd-interact-chest.c`, `src/cmd/item/cmd-item-core.c`, `src/tutorial/tutorial-game.c`.

## Searching skeletons

`world.skeleton`

Level: **Normal**.

**1. Info**

Search a skeleton on the dungeon floor using its contextual action. A skeleton can be searched only once. It may yield food, a light, damaged gear or nothing. Searching can also reveal a note or hint; those hints have their own setting and can be reviewed later. This is a search action, not a request to equip or use the bones.

Trigger: Skeleton remains are actually encountered on the player square or visibly adjacent, including already-searched remains.

Sources: `src/cmd/world/cmd-interact-chest.c`, `src/cmd/item/cmd-item-core.c`, `src/tutorial/tutorial-game.c`.

## Handling items

`item.item`

Level: **Normal**.

**1. Info**

Read the item's public description and available actions. Pack, Harness, Quiver, Supplies and worn equipment have different capacities and access rules. The displayed action menu determines what is currently possible.

Trigger: The item type is publicly encountered; no hidden subtype or property is used.

Sources: `src/object/object-info.c`, `src/object/object-inventory.c`, `src/tutorial/tutorial-game.c`.

## Staves

`item.staff`

Level: **Normal**.

**1. Info**

A staff uses charges and its handling rules may require readying it before use. Read its known effect, remaining charges and requirements. An unfamiliar staff can be unhelpful or dangerous; this lesson does not require using it.

Trigger: The item type is publicly encountered; no hidden subtype or property is used.

Sources: `src/object/object-info.c`, `src/object/object-inventory.c`, `src/tutorial/tutorial-game.c`.

## Horns

`item.horn`

Level: **Normal**.

**1. Info**

A horn uses Voice and produces an effect along the chosen direction or area. Read its known effect, Voice cost and handling requirements. A loud horn can alert other creatures. Do not blow an unknown horn just to finish a lesson.

Trigger: The item type is publicly encountered; no hidden subtype or property is used.

Sources: `src/object/object-info.c`, `src/object/object-inventory.c`, `src/tutorial/tutorial-game.c`.

## Ready a staff

`item.staff.ready`

Level: **Normal**.

**1. Action**

This staff is available to ready. Choose its Ready action. This changes your equipment arrangement and keeps the normal game-time cost.

Required action: `ready`; subject: `staff`.

Trigger: A carried, currently usable staff can legally be readied; its examination lesson is already completed.

Sources: `src/tutorial/tutorial-game.c`.

## Use a known staff

`item.staff.use`

Level: **Normal**.

**1. Info**

The known staff is ready and has a relevant use now. {detail} Check the target and cost. Skip if you want to save it.

**2. Action**

Use the known staff once, selecting a valid target if needed. Only a committed use completes the step; cancellation does not.

Required action: `use-item`; subject: `staff`.

Trigger: A known readied staff has resources and a useful, legal current target or effect; unsafe/unknown uses are not offered.

Sources: `src/tutorial/tutorial-game.c`.

## Ready a horn

`item.horn.ready`

Level: **Normal**.

**1. Action**

This horn is available to ready. Choose its Ready action. This changes your equipment arrangement and keeps the normal game-time cost.

Required action: `ready`; subject: `horn`.

Trigger: A carried, currently usable horn can legally be readied; its examination lesson is already completed.

Sources: `src/tutorial/tutorial-game.c`.

## Use a known horn

`item.horn.use`

Level: **Normal**.

**1. Info**

The known horn is ready and has a relevant use now. {detail} Check the target and cost. Skip if you want to save it.

**2. Action**

Use the known horn once, selecting a valid target if needed. Only a committed use completes the step; cancellation does not.

Required action: `use-item`; subject: `horn`.

Trigger: A known readied horn has resources and a useful, legal current target or effect; unsafe/unknown uses are not offered.

Sources: `src/tutorial/tutorial-game.c`.

## A known artefact

`item.artefact`

Level: **Normal**.

**1. Info**

{subject} is a unique crafted item. Read all its revealed properties, including drawbacks and granted abilities. An artefact name does not make every equipment arrangement useful or safe.

Trigger: Explicit public observation at a safe player or menu boundary.

Sources: `src/object/object-info.c`.

## New item knowledge

`identification.item`

Level: **Normal**.

**1. Info**

{detail} Open the updated description to see the property or identity that was actually revealed. Learning one property does not imply that all other properties are now known.

Trigger: Explicit public observation at a safe player or menu boundary.

Sources: `src/cmd/item/cmd-identify.c`, `src/cmd/combat/cmd-combat.c`.

## Resistance revealed

`identification.elemental`

Level: **Normal**.

**1. Info**

An elemental attack revealed new information about equipment or resistance. Read the resulting description and combat result. Resistance, vulnerability and Protection are separate mechanics; the observation does not expose every hidden property.

Trigger: Explicit public observation at a safe player or menu boundary.

Sources: `src/cmd/item/cmd-identify.c`, `src/cmd/combat/cmd-combat.c`.

## A weapon property revealed

`identification.brand`

Level: **Normal**.

**1. Info**

A committed attack revealed a brand, slay or other weapon property. Read its description for the affected targets and damage rules. A visible combat effect can teach a property without identifying every part of the item.

Trigger: Explicit public observation at a safe player or menu boundary.

Sources: `src/cmd/item/cmd-identify.c`, `src/cmd/combat/cmd-combat.c`.

## Poisoned

`status.poisoned`

Level: **Normal**.

**1. Info**

{detail} Poison severity determines later damage ticks: the next tick is ceil(severity / 5) Health. Poison prevents ordinary Health regeneration. A known Antidote removes poison; healing Health alone is not an antidote.

Trigger: The public condition becomes active, or the displayed resource crosses its warning threshold.

Sources: `src/player/effects.c`, `src/dungeon/dungeon-loop.c`, `src/use-obj.c`.

## Bleeding

`status.cut`

Level: **Normal**.

**1. Info**

{detail} Bleeding severity determines later damage ticks: ceil(severity / 5) Health. Bleeding prevents ordinary Health regeneration. Healing consumables halve current bleeding, so one use may leave it active. Song of Staunching can stop it.

Trigger: The public condition becomes active, or the displayed resource crosses its warning threshold.

Sources: `src/player/effects.c`, `src/dungeon/dungeon-loop.c`, `src/use-obj.c`.

## Stunned

`status.stun`

Level: **Normal**.

**1. Info**

{detail} Stun applies -2 to every skill below 50 severity and -4 at 50 or more. Above 100, you cannot act. A known Clarity or Miruvor can cure stun if you can act to use it.

Trigger: The public condition becomes active, or the displayed resource crosses its warning threshold.

Sources: `src/player/effects.c`, `src/dungeon/dungeon-loop.c`, `src/use-obj.c`.

## Afraid

`status.afraid`

Level: **Normal**.

**1. Info**

{detail} Fear prevents normal melee attacks and proper ranged aiming. A known fear remedy or waiting for the effect to expire can help, but nearby foes still act when you spend turns. Do not mistake a speed benefit for curing fear.

Trigger: The public condition becomes active, or the displayed resource crosses its warning threshold.

Sources: `src/player/effects.c`, `src/dungeon/dungeon-loop.c`, `src/use-obj.c`.

## Confused

`status.confused`

Level: **Normal**.

**1. Info**

{detail} Confusion disrupts direction and aiming choices. A known Clarity or Miruvor can cure it. Examine your options before committing movement or a targeted effect.

Trigger: The public condition becomes active, or the displayed resource crosses its warning threshold.

Sources: `src/player/effects.c`, `src/dungeon/dungeon-loop.c`, `src/use-obj.c`.

## Blind

`status.blind`

Level: **Normal**.

**1. Info**

{detail} Ordinary sight is unavailable. Remembered terrain is not a current sighting of enemies or objects. Known true Sight or Miruvor can restore sight; more lamp fuel does not cure blindness.

Trigger: The public condition becomes active, or the displayed resource crosses its warning threshold.

Sources: `src/player/effects.c`, `src/dungeon/dungeon-loop.c`, `src/use-obj.c`.

## Hallucinating

`status.image`

Level: **Normal**.

**1. Info**

{detail} Displayed creature and item identities can be unreliable. Known Clarity, true Sight or Miruvor can remove hallucination. The tutorial will not identify a disguised apparent subject for you.

Trigger: The public condition becomes active, or the displayed resource crosses its warning threshold.

Sources: `src/player/effects.c`, `src/dungeon/dungeon-loop.c`, `src/use-obj.c`.

## Entranced

`status.entranced`

Level: **Normal**.

**1. Info**

{detail} You cannot act until the trance ends. Continue closes this explanation and lets the normal scheduler resume; it does not cure you or create a free turn. No required item use is offered while you cannot act.

Trigger: The public condition becomes active, or the displayed resource crosses its warning threshold.

Sources: `src/player/effects.c`, `src/dungeon/dungeon-loop.c`, `src/use-obj.c`.

## Slowed

`status.slow`

Level: **Normal**.

**1. Info**

{detail} Slowing changes how often you act relative to other creatures. Quickness may offset the speed penalty while the slow condition remains. An offset is not the same as removing the underlying condition.

Trigger: The public condition becomes active, or the displayed resource crosses its warning threshold.

Sources: `src/player/effects.c`, `src/dungeon/dungeon-loop.c`, `src/use-obj.c`.

## Fast

`status.fast`

Level: **Normal**.

**1. Info**

{detail} Increased speed changes how often you act relative to enemies. It does not guarantee that every chosen command is safe or that a foe cannot act afterward. Check when the temporary effect expires.

Trigger: The public condition becomes active, or the displayed resource crosses its warning threshold.

Sources: `src/player/effects.c`, `src/dungeon/dungeon-loop.c`, `src/use-obj.c`.

## Rage

`status.rage`

Level: **Normal**.

**1. Info**

{detail} Rage gives +1 Strength and Constitution, -1 Dexterity and Grace, fear resistance and a special melee attack. It restricts awareness and prevents Stealth. Clarity ends rage; do not rely on calm targeting while it lasts.

Trigger: The public condition becomes active, or the displayed resource crosses its warning threshold.

Sources: `src/player/effects.c`, `src/dungeon/dungeon-loop.c`, `src/use-obj.c`.

## Your light is dimmed

`status.darkened`

Level: **Normal**.

**1. Info**

{detail} Darkening reduces your light. Light radius, light intensity and remaining fuel are different quantities. Check the actual condition and equipment before choosing a remedy; dim light is not automatically blindness.

Trigger: The public condition becomes active, or the displayed resource crosses its warning threshold.

Sources: `src/player/effects.c`, `src/dungeon/dungeon-loop.c`, `src/use-obj.c`.

## Temporary Strength

`status.tmp_str`

Level: **Extended**.

**1. Info**

{detail} Temporary Strength adds to current Strength and sustains it while active. Inspect the attribute breakdown and weapon damage. A temporary bonus eventually ends; restoring drained Strength is a separate effect.

Trigger: The public condition becomes active, or the displayed resource crosses its warning threshold.

Sources: `src/player/effects.c`, `src/dungeon/dungeon-loop.c`, `src/use-obj.c`.

## Temporary Dexterity

`status.tmp_dex`

Level: **Extended**.

**1. Info**

{detail} Temporary Dexterity improves Dexterity and its associated skills and sustains it while active. Inspect the current skill and attribute breakdowns. The bonus eventually expires.

Trigger: The public condition becomes active, or the displayed resource crosses its warning threshold.

Sources: `src/player/effects.c`, `src/dungeon/dungeon-loop.c`, `src/use-obj.c`.

## Temporary Constitution

`status.tmp_con`

Level: **Extended**.

**1. Info**

{detail} Temporary Constitution changes Constitution and maximum Health and sustains the attribute while active. A larger maximum is different from a healing item; inspect current and maximum Health together.

Trigger: The public condition becomes active, or the displayed resource crosses its warning threshold.

Sources: `src/player/effects.c`, `src/dungeon/dungeon-loop.c`, `src/use-obj.c`.

## Temporary Grace

`status.tmp_gra`

Level: **Extended**.

**1. Info**

{detail} Temporary Grace changes Grace-based skills and maximum Voice and sustains Grace while active. Check the current attribute and skill breakdowns, particularly before spending Voice.

Trigger: The public condition becomes active, or the displayed resource crosses its warning threshold.

Sources: `src/player/effects.c`, `src/dungeon/dungeon-loop.c`, `src/use-obj.c`.

## Temporary Perception

`status.tmp_per`

Level: **Extended**.

**1. Info**

{detail} Temporary Perception improves the current Perception skill. This can help its associated detection and skill checks; it does not reveal every hidden creature or property automatically.

Trigger: The public condition becomes active, or the displayed resource crosses its warning threshold.

Sources: `src/player/effects.c`, `src/dungeon/dungeon-loop.c`, `src/use-obj.c`.

## True sight

`status.tim_invis`

Level: **Extended**.

**1. Info**

{detail} True sight helps detect invisible creatures and protects against blindness and hallucination. Seeing an otherwise hidden creature still depends on the actual detection rules; read what the game has revealed.

Trigger: The public condition becomes active, or the displayed resource crosses its warning threshold.

Sources: `src/player/effects.c`, `src/dungeon/dungeon-loop.c`, `src/use-obj.c`.

## Temporary fire resistance

`status.oppose_fire`

Level: **Extended**.

**1. Info**

{detail} Temporary fire resistance adds a resistance layer. Resistance reduces elemental damage, while eligible Protection is a separate roll. It does not automatically protect every carried item from damage.

Trigger: The public condition becomes active, or the displayed resource crosses its warning threshold.

Sources: `src/player/effects.c`, `src/dungeon/dungeon-loop.c`, `src/use-obj.c`.

## Temporary cold resistance

`status.oppose_cold`

Level: **Extended**.

**1. Info**

{detail} Temporary cold resistance adds a resistance layer. Resistance and applicable Protection are separate; inspect the resulting damage and known equipment properties rather than assuming immunity.

Trigger: The public condition becomes active, or the displayed resource crosses its warning threshold.

Sources: `src/player/effects.c`, `src/dungeon/dungeon-loop.c`, `src/use-obj.c`.

## Temporary poison resistance

`status.oppose_pois`

Level: **Extended**.

**1. Info**

{detail} Temporary poison resistance reduces poison damage through the resistance rules. It is distinct from an Antidote removing existing poison. Read your current condition and the known item effect.

Trigger: The public condition becomes active, or the displayed resource crosses its warning threshold.

Sources: `src/player/effects.c`, `src/dungeon/dungeon-loop.c`, `src/use-obj.c`.

## A lingering Challenge effect

`status.song_challenge_effect`

Level: **Extended**.

**1. Info**

{detail} A song has left a temporary Challenge effect. Read the current skill breakdown to see the active modifier. The effect and its remaining duration are separate from the song you are currently singing.

Trigger: The public condition becomes active, or the displayed resource crosses its warning threshold.

Sources: `src/player/effects.c`, `src/dungeon/dungeon-loop.c`, `src/use-obj.c`.

## A lingering Elbereth effect

`status.song_elbereth_effect`

Level: **Extended**.

**1. Info**

{detail} A song has left a temporary Elbereth effect. Inspect your current skill breakdown and condition. A lingering effect can remain distinct from the currently selected song.

Trigger: The public condition becomes active, or the displayed resource crosses its warning threshold.

Sources: `src/player/effects.c`, `src/dungeon/dungeon-loop.c`, `src/use-obj.c`.

## Low Health

`status.health`

Level: **Normal**.

**1. Info**

{detail} Your Health is at or below the configured warning threshold. Read the latest damage and inspect a known healing option or escape route. Health healing, stopping bleeding and curing poison are different effects.

Trigger: The public condition becomes active, or the displayed resource crosses its warning threshold.

Sources: `src/player/effects.c`, `src/dungeon/dungeon-loop.c`, `src/use-obj.c`.

## Low Voice

`status.voice`

Level: **Normal**.

**1. Info**

{detail} Voice fuels songs and horns. It does not regenerate while you sing. Stop singing to allow recovery, or choose a known Voice-restoring item when needed. This explanation does not spend or restore Voice.

Trigger: The public condition becomes active, or the displayed resource crosses its warning threshold.

Sources: `src/player/effects.c`, `src/dungeon/dungeon-loop.c`, `src/use-obj.c`.

## Hungry

`status.hungry`

Level: **Normal**.

**1. Info**

{detail} Your nourishment is below the alert threshold. Check known food before it becomes urgent. Reading a food description is free; eating and accessing the Pack retain their normal costs.

Trigger: The public condition becomes active, or the displayed resource crosses its warning threshold.

Sources: `src/player/effects.c`, `src/dungeon/dungeon-loop.c`, `src/use-obj.c`.

## Weak from hunger

`status.weak`

Level: **Normal**.

**1. Info**

{detail} Low nourishment now reduces Strength. Ordinary food or a known nourishing herb addresses hunger. A temporary Strength potion can conceal a symptom without filling your stomach.

Trigger: The public condition becomes active, or the displayed resource crosses its warning threshold.

Sources: `src/player/effects.c`, `src/dungeon/dungeon-loop.c`, `src/use-obj.c`.

## Starving

`status.starving`

Level: **Normal**.

**1. Info**

{detail} Starvation damages you and prevents Health regeneration. A known nourishing food addresses the cause. Healing Health without eating leaves starvation active.

Trigger: The public condition becomes active, or the displayed resource crosses its warning threshold.

Sources: `src/player/effects.c`, `src/dungeon/dungeon-loop.c`, `src/use-obj.c`.

## A remedy for poison

`status.poisoned.remedy`

Level: **Normal**.

**1. Info**

A known relevant remedy is available. {detail} Read the effect: it may remove a condition, reduce its severity, restore a resource or restore an attribute. Skip if another tactic is better.

**2. Action**

Choose and use a known remedy for poison. The step completes only after a relevant item is actually used. A cancellation or unrelated item does not complete it.

Required action: `use-item`; subject: `poisoned`.

Trigger: The condition is active, the player can act, and a carried aware item passes item_is_remedy for poisoned.

Sources: `src/tutorial/tutorial-game.c`, `src/use-obj.c`.

## A remedy for bleeding

`status.cut.remedy`

Level: **Normal**.

**1. Info**

A known relevant remedy is available. {detail} Read the effect: it may remove a condition, reduce its severity, restore a resource or restore an attribute. Skip if another tactic is better.

**2. Action**

Choose and use a known remedy for bleeding. The step completes only after a relevant item is actually used. A cancellation or unrelated item does not complete it.

Required action: `use-item`; subject: `cut`.

Trigger: The condition is active, the player can act, and a carried aware item passes item_is_remedy for cut.

Sources: `src/tutorial/tutorial-game.c`, `src/use-obj.c`.

## A remedy for stun

`status.stun.remedy`

Level: **Normal**.

**1. Info**

A known relevant remedy is available. {detail} Read the effect: it may remove a condition, reduce its severity, restore a resource or restore an attribute. Skip if another tactic is better.

**2. Action**

Choose and use a known remedy for stun. The step completes only after a relevant item is actually used. A cancellation or unrelated item does not complete it.

Required action: `use-item`; subject: `stun`.

Trigger: The condition is active, the player can act, and a carried aware item passes item_is_remedy for stun.

Sources: `src/tutorial/tutorial-game.c`, `src/use-obj.c`.

## A remedy for fear

`status.afraid.remedy`

Level: **Normal**.

**1. Info**

A known relevant remedy is available. {detail} Read the effect: it may remove a condition, reduce its severity, restore a resource or restore an attribute. Skip if another tactic is better.

**2. Action**

Choose and use a known remedy for fear. The step completes only after a relevant item is actually used. A cancellation or unrelated item does not complete it.

Required action: `use-item`; subject: `afraid`.

Trigger: The condition is active, the player can act, and a carried aware item passes item_is_remedy for afraid.

Sources: `src/tutorial/tutorial-game.c`, `src/use-obj.c`.

## A remedy for confusion

`status.confused.remedy`

Level: **Normal**.

**1. Info**

A known relevant remedy is available. {detail} Read the effect: it may remove a condition, reduce its severity, restore a resource or restore an attribute. Skip if another tactic is better.

**2. Action**

Choose and use a known remedy for confusion. The step completes only after a relevant item is actually used. A cancellation or unrelated item does not complete it.

Required action: `use-item`; subject: `confused`.

Trigger: The condition is active, the player can act, and a carried aware item passes item_is_remedy for confused.

Sources: `src/tutorial/tutorial-game.c`, `src/use-obj.c`.

## A remedy for blindness

`status.blind.remedy`

Level: **Normal**.

**1. Info**

A known relevant remedy is available. {detail} Read the effect: it may remove a condition, reduce its severity, restore a resource or restore an attribute. Skip if another tactic is better.

**2. Action**

Choose and use a known remedy for blindness. The step completes only after a relevant item is actually used. A cancellation or unrelated item does not complete it.

Required action: `use-item`; subject: `blind`.

Trigger: The condition is active, the player can act, and a carried aware item passes item_is_remedy for blind.

Sources: `src/tutorial/tutorial-game.c`, `src/use-obj.c`.

## A remedy for hallucination

`status.image.remedy`

Level: **Normal**.

**1. Info**

A known relevant remedy is available. {detail} Read the effect: it may remove a condition, reduce its severity, restore a resource or restore an attribute. Skip if another tactic is better.

**2. Action**

Choose and use a known remedy for hallucination. The step completes only after a relevant item is actually used. A cancellation or unrelated item does not complete it.

Required action: `use-item`; subject: `image`.

Trigger: The condition is active, the player can act, and a carried aware item passes item_is_remedy for image.

Sources: `src/tutorial/tutorial-game.c`, `src/use-obj.c`.

## A remedy for rage

`status.rage.remedy`

Level: **Normal**.

**1. Info**

A known relevant remedy is available. {detail} Read the effect: it may remove a condition, reduce its severity, restore a resource or restore an attribute. Skip if another tactic is better.

**2. Action**

Choose and use a known remedy for rage. The step completes only after a relevant item is actually used. A cancellation or unrelated item does not complete it.

Required action: `use-item`; subject: `rage`.

Trigger: The condition is active, the player can act, and a carried aware item passes item_is_remedy for rage.

Sources: `src/tutorial/tutorial-game.c`, `src/use-obj.c`.

## A remedy for low health

`status.health.remedy`

Level: **Normal**.

**1. Info**

A known relevant remedy is available. {detail} Read the effect: it may remove a condition, reduce its severity, restore a resource or restore an attribute. Skip if another tactic is better.

**2. Action**

Choose and use a known remedy for low health. The step completes only after a relevant item is actually used. A cancellation or unrelated item does not complete it.

Required action: `use-item`; subject: `health`.

Trigger: The condition is active, the player can act, and a carried aware item passes item_is_remedy for health.

Sources: `src/tutorial/tutorial-game.c`, `src/use-obj.c`.

## A remedy for low voice

`status.voice.remedy`

Level: **Normal**.

**1. Info**

A known relevant remedy is available. {detail} Read the effect: it may remove a condition, reduce its severity, restore a resource or restore an attribute. Skip if another tactic is better.

**2. Action**

Choose and use a known remedy for low voice. The step completes only after a relevant item is actually used. A cancellation or unrelated item does not complete it.

Required action: `use-item`; subject: `voice`.

Trigger: The condition is active, the player can act, and a carried aware item passes item_is_remedy for voice.

Sources: `src/tutorial/tutorial-game.c`, `src/use-obj.c`.

## A remedy for hunger

`status.hunger.remedy`

Level: **Normal**.

**1. Info**

A known relevant remedy is available. {detail} Read the effect: it may remove a condition, reduce its severity, restore a resource or restore an attribute. Skip if another tactic is better.

**2. Action**

Choose and use a known remedy for hunger. The step completes only after a relevant item is actually used. A cancellation or unrelated item does not complete it.

Required action: `use-item`; subject: `hunger`.

Trigger: The condition is active, the player can act, and a carried aware item passes item_is_remedy for hunger.

Sources: `src/tutorial/tutorial-game.c`, `src/use-obj.c`.

## A remedy for drained attributes

`status.drain.remedy`

Level: **Normal**.

**1. Info**

A known relevant remedy is available. {detail} Read the effect: it may remove a condition, reduce its severity, restore a resource or restore an attribute. Skip if another tactic is better.

**2. Action**

Choose and use a known remedy for drained attributes. The step completes only after a relevant item is actually used. A cancellation or unrelated item does not complete it.

Required action: `use-item`; subject: `drain`.

Trigger: The condition is active, the player can act, and a carried aware item passes item_is_remedy for drain.

Sources: `src/tutorial/tutorial-game.c`, `src/use-obj.c`.

## Strength drain

`status.drain0`

Level: **Normal**.

**1. Info**

{detail} Drain reduces the attribute independently of equipment penalties or expiring temporary bonuses. Restoration restores up to 3 points per attribute. A corresponding attribute potion also restores its attribute; inspect the known effect.

Trigger: Explicit public observation at a safe player or menu boundary.

Sources: `src/use-obj.c`, `src/player/effects.c`.

## Dexterity drain

`status.drain1`

Level: **Normal**.

**1. Info**

{detail} Drain reduces the attribute independently of equipment penalties or expiring temporary bonuses. Restoration restores up to 3 points per attribute. A corresponding attribute potion also restores its attribute; inspect the known effect.

Trigger: Explicit public observation at a safe player or menu boundary.

Sources: `src/use-obj.c`, `src/player/effects.c`.

## Constitution drain

`status.drain2`

Level: **Normal**.

**1. Info**

{detail} Drain reduces the attribute independently of equipment penalties or expiring temporary bonuses. Restoration restores up to 3 points per attribute. A corresponding attribute potion also restores its attribute; inspect the known effect.

Trigger: Explicit public observation at a safe player or menu boundary.

Sources: `src/use-obj.c`, `src/player/effects.c`.

## Grace drain

`status.drain3`

Level: **Normal**.

**1. Info**

{detail} Drain reduces the attribute independently of equipment penalties or expiring temporary bonuses. Restoration restores up to 3 points per attribute. A corresponding attribute potion also restores its attribute; inspect the known effect.

Trigger: Explicit public observation at a safe player or menu boundary.

Sources: `src/use-obj.c`, `src/player/effects.c`.

## Abilities

`menu.abilities`

Level: **Normal**.

**1. Info**

Select an ability to read its effect, skill requirement, prerequisites and current XP cost. Continue on a tutorial returns to the purchase decision; it never buys or enables the ability. Toggleable abilities work only while active and their conditions hold.

Trigger: The named menu is actually opened: abilities.

Sources: `src/tutorial/tutorial-game.c`.

## Skills and experience

`menu.skills`

Level: **Normal**.

**1. Info**

Inspect the base skill, attribute contribution and other modifiers separately. Buying skill points spends XP; browsing a proposal does not. A higher displayed skill does not necessarily satisfy a requirement based on invested points.

Trigger: The named menu is actually opened: skills.

Sources: `src/tutorial/tutorial-game.c`.

## Songs

`menu.songs`

Level: **Normal**.

**1. Info**

Choose among learned songs and inspect the effect and Voice cost. Singing uses Voice over time and stops Voice regeneration. A song is not automatically beneficial in every situation; oaths can forbid singing.

Trigger: The named menu is actually opened: songs.

Sources: `src/tutorial/tutorial-game.c`.

## Settings

`menu.settings`

Level: **Normal**.

**1. Info**

Use settings to adjust presentation, input and gameplay preferences. Gameplay tutorials cycle between Disabled, Normal and Extended. Normal covers core controls and survival; Extended adds detailed lessons. Learned lessons and per-Tale reset are available separately. Mouse, touch, controller tutorials and skeleton hints retain their own settings.

Trigger: The named menu is actually opened: settings.

Sources: `src/tutorial/tutorial-game.c`.

## Inventory

`menu.inventory`

Level: **Normal**.

**1. Info**

Pack, Harness, Quiver and Supplies are distinct storage routes. Read location, weight, volume and available actions. Opening the list is free; reaching into the Pack or changing equipment can spend turns.

Trigger: The named menu is actually opened: inventory.

Sources: `src/tutorial/tutorial-game.c`.

## The Pack

`menu.pack`

Level: **Normal**.

**1. Info**

The Pack holds stored items subject to weight and volume limits. Pack access can cost time before the selected action. For an urgent item, inspect whether it can be readied in the Harness or handled through Supplies beforehand.

Trigger: The named menu is actually opened: pack.

Sources: `src/tutorial/tutorial-game.c`.

## The Harness

`menu.harness`

Level: **Normal**.

**1. Info**

The Harness makes readied items available under their handling rules and has its own volume capacity. Carrying an item in the Pack does not make it ready. Examine Ready and Unready choices before changing the arrangement.

Trigger: The named menu is actually opened: harness.

Sources: `src/tutorial/tutorial-game.c`.

## Equipment

`menu.equipment`

Level: **Normal**.

**1. Info**

Worn, readied and active equipment are related but different. The active weapon determines which hand arrangement and shield bonuses apply. Read the actual action and previewed changes before committing.

Trigger: The named menu is actually opened: equipment.

Sources: `src/tutorial/tutorial-game.c`.

## Supplies

`menu.supplies`

Level: **Normal**.

**1. Info**

Supplies organise consumables and containers. Select an item to inspect its known effect, remaining amount and available action. Choosing an unrelated consumable is not a cure merely because it appears in the same menu.

Trigger: The named menu is actually opened: supplies.

Sources: `src/tutorial/tutorial-game.c`.

## The active weapon

`menu.active-weapon`

Level: **Normal**.

**1. Info**

Choose which readied weapon you are using. Changing active weapons can spend time; particular abilities grant one eligible free change before the next action. Repeated changes are not all free.

Trigger: The named menu is actually opened: active-weapon.

Sources: `src/tutorial/tutorial-game.c`.

## Item description

`menu.item-description`

Level: **Normal**.

**1. Info**

Read only the knowledge shown here. Check attack, damage, evasion, Protection, weight, volume, known effects and handling requirements. Browsing does not pick up, equip, consume or identify the item.

Trigger: The named menu is actually opened: item-description.

Sources: `src/tutorial/tutorial-game.c`.

## Jewelry

`menu.jewelry`

Level: **Normal**.

**1. Info**

Compare known ring and amulet effects and drawbacks. Read which slots or saved set will change before committing. An unknown item's hidden properties remain hidden in a preview.

Trigger: The named menu is actually opened: jewelry.

Sources: `src/tutorial/tutorial-game.c`.

## Jewelry sets

`menu.jewelry-sets`

Level: **Extended**.

**1. Info**

Saved sets help manage ring and amulet combinations. Check what is in the chosen set and what can legally be equipped now. A set label does not bypass curses, unavailable items or equipment-change costs.

Trigger: The named menu is actually opened: jewelry-sets.

Sources: `src/tutorial/tutorial-game.c`.

## Targeting

`menu.targeting`

Level: **Normal**.

**1. Info**

Choose a legal target or direction and inspect the line of fire. Confirmation commits the normal action; cancellation does not. Walls, range, awareness, allies and your oath can change whether a shot is useful or allowed.

Trigger: The named menu is actually opened: targeting.

Sources: `src/tutorial/tutorial-game.c`.

## Resting

`menu.rest`

Level: **Normal**.

**1. Info**

Resting repeatedly spends game turns. Other creatures and the minimum-depth timer continue. Health will not regenerate normally while poisoned, bleeding or starving; Voice will not regenerate while singing.

Trigger: The named menu is actually opened: rest.

Sources: `src/tutorial/tutorial-game.c`.

## The game menu

`menu.main-menu`

Level: **Normal**.

**1. Info**

This menu gathers character, knowledge, equipment, settings and game-management actions. Opening or reading a page is free; a selected action may have its normal game-time cost or ask for confirmation.

Trigger: The named menu is actually opened: main-menu.

Sources: `src/tutorial/tutorial-game.c`.

## Character details

`menu.character`

Level: **Normal**.

**1. Info**

Read attributes, skills, Health, Voice, active abilities and current effects here. Separate base values, equipment contributions, temporary effects and drain before choosing a remedy or purchase.

Trigger: The named menu is actually opened: character.

Sources: `src/tutorial/tutorial-game.c`.

## Object knowledge

`menu.knowledge-objects`

Level: **Extended**.

**1. Info**

Review known object types and their descriptions. A known kind and an individual fully identified item are different: one piece of equipment can still have unrevealed properties.

Trigger: The named menu is actually opened: objects.

Sources: `src/tutorial/tutorial-game.c`.

## Artefact knowledge

`menu.knowledge-artefacts`

Level: **Extended**.

**1. Info**

Read the artefacts and properties known to this Tale. An artefact can combine useful bonuses with significant drawbacks. Knowledge does not imply the item is currently carried or available.

Trigger: The named menu is actually opened: artefacts.

Sources: `src/tutorial/tutorial-game.c`.

## Monster memory

`menu.knowledge-monsters`

Level: **Extended**.

**1. Info**

Monster memory records revealed creature information. Read known attacks, resistances and other lore, but leave unobserved abilities unknown. A creature's awareness and morale are separate current states.

Trigger: The named menu is actually opened: monsters.

Sources: `src/tutorial/tutorial-game.c`.

## Known curses

`menu.knowledge-curses`

Level: **Extended**.

**1. Info**

Review revealed curses and their current effects. Curse stacks, equipment curses and broken-oath consequences use different rules. This page does not reveal an unknown curse merely because a tutorial exists.

Trigger: The named menu is actually opened: curses.

Sources: `src/tutorial/tutorial-game.c`.

## Smithing

`menu.smithing`

Level: **Normal**.

**1. Info**

Inspect the proposed item, difficulty, forge uses, materials, time, attribute and XP costs before committing. Abilities can alter these costs. A preview does not create an item, spend forge uses or make an oath-breaking action harmless.

Trigger: The named menu is actually opened: smithing.

Sources: `src/tutorial/tutorial-game.c`.

## Nearby creatures

`menu.nearby-monsters`

Level: **Normal**.

**1. Info**

This list refers to creatures currently known through the game's visibility rules. Select a creature to inspect its public description and position. Hidden creatures and unknown abilities remain undisclosed.

Trigger: The named menu is actually opened: nearby-monsters.

Sources: `src/tutorial/tutorial-game.c`.

## Nearby objects

`menu.nearby-objects`

Level: **Normal**.

**1. Info**

Inspect marked objects and their current location before selecting an action. The -) floor entry and - shortcut refer to floor items. Selection does not guarantee the full stack fits your storage.

Trigger: The named menu is actually opened: nearby-objects.

Sources: `src/tutorial/tutorial-game.c`.

## Look

`menu.look`

Level: **Normal**.

**1. Info**

Look inspects known squares, items and creatures without spending a turn. Remembered terrain and current visibility differ. An examine action reads a description; it does not automatically equip or use the subject.

Trigger: The named menu is actually opened: look.

Sources: `src/tutorial/tutorial-game.c`.

## The map

`menu.map`

Level: **Normal**.

**1. Info**

The map separates explored terrain from what you can currently observe. A remembered square may contain a creature you cannot now see. Plan legal routes; moving or following a path still advances game time.

Trigger: The named menu is actually opened: map.

Sources: `src/tutorial/tutorial-game.c`.

## Messages

`menu.messages`

Level: **Normal**.

**1. Info**

Review the messages that explain recent actions and newly revealed information. Reading history costs no game time. A past message describes that event, not necessarily the current condition.

Trigger: The named menu is actually opened: messages.

Sources: `src/tutorial/tutorial-game.c`.

## Combat history

`menu.combat-history`

Level: **Normal**.

**1. Info**

Inspect attack and evasion rolls, damage, applicable Protection and resulting effects. A miss, a blocked hit and a damaging hit are different outcomes. Critical dice and elemental or slay dice are separate contributions.

Trigger: The named menu is actually opened: combat-history.

Sources: `src/tutorial/tutorial-game.c`.

## Quests

`menu.quests`

Level: **Normal**.

**1. Info**

Read the currently revealed objective, restrictions and progress. Accepting a quest can change which actions are permitted or desirable. Tutorial explanations do not accept a quest or reveal an unseen objective.

Trigger: The named menu is actually opened: quests.

Sources: `src/tutorial/tutorial-game.c`.

## Hints

`menu.hints`

Level: **Normal**.

**1. Info**

Hints and learned tutorial lessons serve different purposes. Read a hint for its situation, and inspect the actual current state before acting. The skeleton-hint setting is separate from gameplay tutorials.

Trigger: The named menu is actually opened: hints.

Sources: `src/tutorial/tutorial-game.c`.

## Thralls

`menu.thralls`

Level: **Normal**.

**1. Info**

Inspect the thrall's public state and available actions before making a choice. A tutorial explains the current interaction; it does not promise a hidden result or commit a rescue, sacrifice or reward.

Trigger: The named menu is actually opened: thralls.

Sources: `src/tutorial/tutorial-game.c`.

## Tales

`menu.tales`

Level: **Normal**.

**1. Info**

A Tale spans its heroes, deaths and restarts. Gameplay lesson history belongs to the selected Tale. Starting another Tale has separate progress; the global Disabled, Normal or Extended preference applies across Tales. Reviewing encountered lessons remains available regardless of the selected mode.

Trigger: The named menu is actually opened: tales.

Sources: `src/tutorial/tutorial-game.c`.

## Halls of the dead

`menu.halls`

Level: **Extended**.

**1. Info**

Recorded heroes belong to their Tale history. Read the actual outcome and score details. Viewing a record does not load or revive that character.

Trigger: The named menu is actually opened: halls.

Sources: `src/tutorial/tutorial-game.c`.

## Gameplay reference

`menu.help`

Level: **Normal**.

**1. Info**

Help explains the game's mechanics. The learned tutorial archive lets you reread encountered or skipped lessons. Both are free to read, and neither commits the actions described.

Trigger: The named menu is actually opened: help.

Sources: `src/tutorial/tutorial-game.c`.

## Power

`ability.0.preview`

Level: **Extended**.

**1. Decision**

+1 damage side for melee attacks, but +1 to the critical interval base. With a 3 lb weapon, the unmodified first critical margin rises from 10 to 11. Extra damage sides and extra critical dice are different bonuses.

**2. Decision**

Its requirements and active state still apply when granted by equipment. Continue returns to the real choice without buying, enabling or using it.

Trigger: Public ability preview or newly available ability; raw serial 0, skill 0, ability slot 0.

Sources: `lib/edit/ability.txt`, `src/cmd/combat/cmd-combat.c`, `src/birth/birth-traits.c`, `src/player/combat-stats.c`, `src/cmd/ui/cmd-ui-abilities.c`.

## Finesse

`ability.1.preview`

Level: **Extended**.

**1. Decision**

Reduce the melee critical interval base from 7 to 5. For a 3 lb weapon, one extra die needs margin 8 instead of 10; two need 16 instead of 20. This changes critical thresholds, not your Attack score or the basic hit roll.

**2. Decision**

Its requirements and active state still apply when granted by equipment. Continue returns to the real choice without buying, enabling or using it.

Trigger: Public ability preview or newly available ability; raw serial 1, skill 0, ability slot 1.

Sources: `lib/edit/ability.txt`, `src/cmd/combat/cmd-combat.c`, `src/birth/birth-traits.c`, `src/cmd/ui/cmd-ui-abilities.c`.

## Knock Back

`ability.2.preview`

Level: **Extended**.

**1. Decision**

Chance to knock enemies back one square in melee (your Strength vs their Constitution).

**2. Decision**

Its requirements and active state still apply when granted by equipment. Continue returns to the real choice without buying, enabling or using it.

Trigger: Public ability preview or newly available ability; raw serial 2, skill 0, ability slot 2.

Sources: `lib/edit/ability.txt`, `src/cmd/combat/cmd-combat.c`, `src/birth/birth-traits.c`, `src/cmd/ui/cmd-ui-abilities.c`.

## Throwing

`ability.3.preview`

Level: **Extended**.

**1. Decision**

+1 attack with throwing weapons. Distance penalties halved. Thrown criticals easier. Throwing weapons use 20% less Harness volume. Harness daggers can be quick-thrown when the active weapon is a one-handed or one-and-a-half-handed melee weapon, or a bow. Your first active weapon change before your next action is free when changing between throwing weapons.

**2. Decision**

With Warden, it may instead change between melee and throwing or between melee weapons. With Versatility, it may instead change between bow and throwing or between bows. With both Warden and Versatility, it may be any active weapon change.

**3. Decision**

Its requirements and active state still apply when granted by equipment. Continue returns to the real choice without buying, enabling or using it.

Trigger: Public ability preview or newly available ability; raw serial 3, skill 0, ability slot 3.

Sources: `lib/edit/ability.txt`, `src/cmd/combat/cmd-combat.c`, `src/birth/birth-traits.c`, `src/player/player-active-weapon.c`, `src/cmd/combat/cmd-ranged.c`.

## Polearm Mastery

`ability.4.preview`

Level: **Extended**.

**1. Decision**

+2 attack with polearms while melee is active. Set polearms to receive free attacks on advancing enemies when waiting, and when switching from ranged to melee.

**2. Decision**

Its requirements and active state still apply when granted by equipment. Continue returns to the real choice without buying, enabling or using it.

Trigger: Public ability preview or newly available ability; raw serial 4, skill 0, ability slot 4.

Sources: `lib/edit/ability.txt`, `src/cmd/combat/cmd-combat.c`, `src/birth/birth-traits.c`, `src/monster/monster-move.c`, `src/player/combat-stats.c`, `src/player/player-active-weapon.c`, `src/cmd/ui/cmd-ui-abilities.c`, `src/cmd/ui/cmd-ui-smithing.c`.

## Charge

`ability.5.preview`

Level: **Extended**.

**1. Decision**

+3 Strength and Dexterity when attacking immediately after moving towards the target.

**2. Decision**

Its requirements and active state still apply when granted by equipment. Continue returns to the real choice without buying, enabling or using it.

Trigger: Public ability preview or newly available ability; raw serial 5, skill 0, ability slot 5.

Sources: `lib/edit/ability.txt`, `src/cmd/combat/cmd-combat.c`, `src/birth/birth-traits.c`, `src/cmd/ui/cmd-ui-abilities.c`.

## Follow-Through

`ability.6.preview`

Level: **Extended**.

**1. Decision**

Continue attacking the next adjacent enemy after killing one.

**2. Decision**

Its requirements and active state still apply when granted by equipment. Continue returns to the real choice without buying, enabling or using it.

Trigger: Public ability preview or newly available ability; raw serial 6, skill 0, ability slot 6.

Sources: `lib/edit/ability.txt`, `src/cmd/combat/cmd-combat.c`, `src/birth/birth-traits.c`, `src/cmd/ui/cmd-ui-abilities.c`.

## Impale

`ability.7.preview`

Level: **Extended**.

**1. Decision**

Strike through opponents to hit an enemy behind them (polearms and greatswords only).

**2. Decision**

Its requirements and active state still apply when granted by equipment. Continue returns to the real choice without buying, enabling or using it.

Trigger: Public ability preview or newly available ability; raw serial 7, skill 0, ability slot 7.

Sources: `lib/edit/ability.txt`, `src/cmd/combat/cmd-combat.c`, `src/birth/birth-traits.c`, `src/cmd/combat/cmd-ranged.c`, `src/cmd/ui/cmd-ui-abilities.c`.

## Subtlety

`ability.8.preview`

Level: **Extended**.

**1. Decision**

Reduce the melee critical interval base by 2 while using a qualifying one-handed melee weapon with the off hand empty. A shield prevents this benefit. Combined with Finesse, a 3 lb weapon needs margins 6 and 12 for one and two extra dice. Certain special traits or items extend the qualifying weapon rules.

**2. Decision**

Its requirements and active state still apply when granted by equipment. Continue returns to the real choice without buying, enabling or using it.

Trigger: Public ability preview or newly available ability; raw serial 8, skill 0, ability slot 8.

Sources: `lib/edit/ability.txt`, `src/cmd/combat/cmd-combat.c`, `src/birth/birth-traits.c`, `src/player/combat-stats.c`, `src/cmd/ui/cmd-ui-abilities.c`.

## Whirlwind Attack

`ability.9.preview`

Level: **Extended**.

**1. Decision**

Free attack on all adjacent enemies when you attack one. Requires 5 open adjacent squares. Also works with flanking or retreat attacks.

**2. Decision**

Its requirements and active state still apply when granted by equipment. Continue returns to the real choice without buying, enabling or using it.

Trigger: Public ability preview or newly available ability; raw serial 9, skill 0, ability slot 9.

Sources: `lib/edit/ability.txt`, `src/cmd/combat/cmd-combat.c`, `src/birth/birth-traits.c`, `src/cmd/ui/cmd-ui-abilities.c`.

## Zone of Control

`ability.10.preview`

Level: **Extended**.

**1. Decision**

Free attack when an opponent moves between two squares adjacent to you, so long as you did not move on your previous turn.

**2. Decision**

Its requirements and active state still apply when granted by equipment. Continue returns to the real choice without buying, enabling or using it.

Trigger: Public ability preview or newly available ability; raw serial 10, skill 0, ability slot 10.

Sources: `lib/edit/ability.txt`, `src/cmd/combat/cmd-combat.c`, `src/birth/birth-traits.c`, `src/monster/monster-move.c`, `src/cmd/ui/cmd-ui-abilities.c`.

## Smite

`ability.11.preview`

Level: **Extended**.

**1. Decision**

First melee attack with a two-handed weapon deals maximum damage. Costs a turn to recover afterward.

**2. Decision**

Its requirements and active state still apply when granted by equipment. Continue returns to the real choice without buying, enabling or using it.

Trigger: Public ability preview or newly available ability; raw serial 11, skill 0, ability slot 11.

Sources: `lib/edit/ability.txt`, `src/cmd/combat/cmd-combat.c`, `src/birth/birth-traits.c`, `src/ui/status/status-panel.c`, `src/cmd/ui/cmd-ui-abilities.c`.

## Two Weapon Fighting

`ability.12.preview`

Level: **Extended**.

**1. Decision**

Wield a one-handed weapon in off-hand for an extra attack at -3 Strength and Dexterity.

**2. Decision**

Its requirements and active state still apply when granted by equipment. Continue returns to the real choice without buying, enabling or using it.

Trigger: Public ability preview or newly available ability; raw serial 12, skill 0, ability slot 12.

Sources: `lib/edit/ability.txt`, `src/cmd/combat/cmd-combat.c`, `src/birth/birth-traits.c`, `src/player/player-active-weapon.c`, `src/player/player-bonuses.c`, `src/sdl/core/sdl-layout.c`, `src/cmd/item/cmd-item-core.c`, `src/cmd/ui/cmd-ui-abilities.c`.

## Rapid Attack

`ability.13.preview`

Level: **Extended**.

**1. Decision**

Extra melee attack at -3 Strength and Dexterity. Your first active weapon change before your next action is free when changing between melee weapons.

**2. Decision**

Its requirements and active state still apply when granted by equipment. Continue returns to the real choice without buying, enabling or using it.

Trigger: Public ability preview or newly available ability; raw serial 13, skill 0, ability slot 13.

Sources: `lib/edit/ability.txt`, `src/cmd/combat/cmd-combat.c`, `src/birth/birth-traits.c`, `src/player/player-active-weapon.c`, `src/player/player-bonuses.c`, `src/ui/character-screen.c`, `src/ui/status/status-panel.c`, `src/sdl/ui/sdl-screens.c`.

## Strength

`ability.14.preview`

Level: **Extended**.

**1. Decision**

+1 Strength.

**2. Decision**

Its requirements and active state still apply when granted by equipment. Continue returns to the real choice without buying, enabling or using it.

Trigger: Public ability preview or newly available ability; raw serial 14, skill 0, ability slot 14.

Sources: `lib/edit/ability.txt`, `src/cmd/combat/cmd-combat.c`, `src/birth/birth-traits.c`, `src/player/player-bonuses.c`.

## Warden

`ability.15.preview`

Level: **Extended**.

**1. Decision**

Archery bonus of half (Melee - Archery) when Melee exceeds Archery, or one third of your full Melee skill, rounded up, if you also have Versatility. Your first active weapon change before your next action is free from melee to bow. With Throwing, it may instead change between melee and throwing or between melee weapons. With Versatility, it may instead change between melee weapons or between bows.

**2. Decision**

Its requirements and active state still apply when granted by equipment. Continue returns to the real choice without buying, enabling or using it.

Trigger: Public ability preview or newly available ability; raw serial 15, skill 0, ability slot 15.

Sources: `lib/edit/ability.txt`, `src/cmd/combat/cmd-combat.c`, `src/birth/birth-traits.c`, `src/player/player-active-weapon.c`, `src/player/player-bonuses.c`, `src/player/player-skills.c`, `src/cmd/ui/cmd-ui-abilities.c`.

## Power Throw

`ability.16.preview`

Level: **Extended**.

**1. Decision**

After spending a turn readying your melee weapon or waiting, throw a Harness spear or hand axe at an adjacent foe while striking in melee. Roll both attacks separately against Evasion, combine the damage of successful attacks, then roll protection once. Your melee weapon remains active.

**2. Decision**

Its requirements and active state still apply when granted by equipment. Continue returns to the real choice without buying, enabling or using it.

Trigger: Public ability preview or newly available ability; raw serial 16, skill 0, ability slot 16.

Sources: `lib/edit/ability.txt`, `src/cmd/combat/cmd-combat.c`, `src/birth/birth-traits.c`, `src/player/player-active-weapon.c`, `src/cmd/ui/cmd-ui-abilities.c`.

## Rout

`ability.20.preview`

Level: **Extended**.

**1. Decision**

+5 Dexterity when firing at fleeing monsters.

**2. Decision**

Its requirements and active state still apply when granted by equipment. Continue returns to the real choice without buying, enabling or using it.

Trigger: Public ability preview or newly available ability; raw serial 20, skill 1, ability slot 0.

Sources: `lib/edit/ability.txt`, `src/cmd/combat/cmd-ranged.c`, `src/birth/birth-traits.c`, `src/cmd/ui/cmd-ui-abilities.c`.

## Fletchery

`ability.21.preview`

Level: **Extended**.

**1. Decision**

Use Fletchery to craft arrows to +3 without changing their affixes (one turn each). Carve 3 arrows from torches, 6 from staves. Loose arrows use 20% less Pack volume.

**2. Decision**

Its requirements and active state still apply when granted by equipment. Continue returns to the real choice without buying, enabling or using it.

Trigger: Public ability preview or newly available ability; raw serial 21, skill 1, ability slot 1.

Sources: `lib/edit/ability.txt`, `src/cmd/combat/cmd-ranged.c`, `src/birth/birth-traits.c`, `src/sdl/input/sdl-player-actions.c`, `src/sdl/ui/sdl-main-menu.c`, `src/cmd/item/cmd-fletchery.c`.

## Point Blank Archery

`ability.22.preview`

Level: **Extended**.

**1. Decision**

When you shoot an adjacent monster, that target does not make an attack of opportunity. Other adjacent alert enemies still can. Point Blank Archery lets you make a round shield active with a shortbow.

**2. Decision**

Its requirements and active state still apply when granted by equipment. Continue returns to the real choice without buying, enabling or using it.

Trigger: Public ability preview or newly available ability; raw serial 22, skill 1, ability slot 2.

Sources: `lib/edit/ability.txt`, `src/cmd/combat/cmd-ranged.c`, `src/birth/birth-traits.c`, `src/player/player-active-weapon.c`, `src/cmd/ui/cmd-ui-abilities.c`.

## Puncture

`ability.23.preview`

Level: **Extended**.

**1. Decision**

Deal 5 flat damage when enemy armour would fully block your archery damage.

**2. Decision**

Its requirements and active state still apply when granted by equipment. Continue returns to the real choice without buying, enabling or using it.

Trigger: Public ability preview or newly available ability; raw serial 23, skill 1, ability slot 3.

Sources: `lib/edit/ability.txt`, `src/cmd/combat/cmd-ranged.c`, `src/birth/birth-traits.c`, `src/cmd/ui/cmd-ui-abilities.c`.

## Ambush

`ability.24.preview`

Level: **Extended**.

**1. Decision**

Extra critical damage die when hitting unwary or sleeping monsters with arrows.

**2. Decision**

Its requirements and active state still apply when granted by equipment. Continue returns to the real choice without buying, enabling or using it.

Trigger: Public ability preview or newly available ability; raw serial 24, skill 1, ability slot 4.

Sources: `lib/edit/ability.txt`, `src/cmd/combat/cmd-ranged.c`, `src/birth/birth-traits.c`, `src/cmd/ui/cmd-ui-abilities.c`.

## Versatility

`ability.25.preview`

Level: **Extended**.

**1. Decision**

Melee bonus of half (Archery - Melee) when Archery exceeds Melee, or one third of your full Archery skill, rounded up, if you also have Warden. Your first active weapon change before your next action is free from bow to melee. With Throwing, it may instead change between bow and throwing or between bows. With Warden, it may instead change between melee weapons or between bows.

**2. Decision**

Its requirements and active state still apply when granted by equipment. Continue returns to the real choice without buying, enabling or using it.

Trigger: Public ability preview or newly available ability; raw serial 25, skill 1, ability slot 5.

Sources: `lib/edit/ability.txt`, `src/cmd/combat/cmd-ranged.c`, `src/birth/birth-traits.c`, `src/player/player-active-weapon.c`, `src/player/player-bonuses.c`, `src/player/player-skills.c`, `src/cmd/ui/cmd-ui-abilities.c`.

## Crippling Shot

`ability.26.preview`

Level: **Extended**.

**1. Decision**

Critical hits may temporarily slow monsters (critical level vs monster's Will).

**2. Decision**

Its requirements and active state still apply when granted by equipment. Continue returns to the real choice without buying, enabling or using it.

Trigger: Public ability preview or newly available ability; raw serial 26, skill 1, ability slot 6.

Sources: `lib/edit/ability.txt`, `src/cmd/combat/cmd-ranged.c`, `src/birth/birth-traits.c`, `src/cmd/ui/cmd-ui-abilities.c`.

## Deadly Hail

`ability.27.preview`

Level: **Extended**.

**1. Decision**

Arrows deal double damage the turn after killing an enemy with an arrow.

**2. Decision**

Its requirements and active state still apply when granted by equipment. Continue returns to the real choice without buying, enabling or using it.

Trigger: Public ability preview or newly available ability; raw serial 27, skill 1, ability slot 7.

Sources: `lib/edit/ability.txt`, `src/cmd/combat/cmd-ranged.c`, `src/birth/birth-traits.c`, `src/ui/status/status-panel.c`, `src/cmd/ui/cmd-ui-abilities.c`.

## Dexterity

`ability.28.preview`

Level: **Extended**.

**1. Decision**

+1 Dexterity.

**2. Decision**

Its requirements and active state still apply when granted by equipment. Continue returns to the real choice without buying, enabling or using it.

Trigger: Public ability preview or newly available ability; raw serial 28, skill 1, ability slot 8.

Sources: `lib/edit/ability.txt`, `src/cmd/combat/cmd-ranged.c`, `src/birth/birth-traits.c`, `src/player/player-bonuses.c`.

## Skirmishing

`ability.29.preview`

Level: **Extended**.

**1. Decision**

After moving on your previous turn, firing a bow costs half a turn while wearing only light armour. Your first active weapon change before your next action is free when changing between bows.

**2. Decision**

Its requirements and active state still apply when granted by equipment. Continue returns to the real choice without buying, enabling or using it.

Trigger: Public ability preview or newly available ability; raw serial 29, skill 1, ability slot 9.

Sources: `lib/edit/ability.txt`, `src/cmd/combat/cmd-ranged.c`, `src/player/player-active-weapon.c`, `src/cmd/ui/cmd-ui-abilities.c`.

## Dodging

`ability.40.preview`

Level: **Extended**.

**1. Decision**

+3 evasion if you moved on your last turn. Only works while wearing only light armour.

**2. Decision**

Its requirements and active state still apply when granted by equipment. Continue returns to the real choice without buying, enabling or using it.

Trigger: Public ability preview or newly available ability; raw serial 40, skill 2, ability slot 0.

Sources: `lib/edit/ability.txt`, `src/player/player-bonuses.c`, `src/birth/birth-traits.c`, `src/melee/melee-attack.c`, `src/cmd/ui/cmd-ui-abilities.c`.

## Blocking

`ability.41.preview`

Level: **Extended**.

**1. Decision**

Double shield protection if you did not move on your last turn.

**2. Decision**

Its requirements and active state still apply when granted by equipment. Continue returns to the real choice without buying, enabling or using it.

Trigger: Public ability preview or newly available ability; raw serial 41, skill 2, ability slot 1.

Sources: `lib/edit/ability.txt`, `src/player/player-bonuses.c`, `src/birth/birth-traits.c`, `src/melee/melee-attack.c`, `src/ui/status/status-panel.c`.

## Parry

`ability.42.preview`

Level: **Extended**.

**1. Decision**

Double the evasion bonus from your primary melee weapon.

**2. Decision**

Its requirements and active state still apply when granted by equipment. Continue returns to the real choice without buying, enabling or using it.

Trigger: Public ability preview or newly available ability; raw serial 42, skill 2, ability slot 2.

Sources: `lib/edit/ability.txt`, `src/player/player-bonuses.c`, `src/birth/birth-traits.c`, `src/cmd/ui/cmd-ui-abilities.c`.

## Crowd Fighting

`ability.43.preview`

Level: **Extended**.

**1. Decision**

Halves the surrounding bonus opponents get against you.

**2. Decision**

Its requirements and active state still apply when granted by equipment. Continue returns to the real choice without buying, enabling or using it.

Trigger: Public ability preview or newly available ability; raw serial 43, skill 2, ability slot 3.

Sources: `lib/edit/ability.txt`, `src/player/player-bonuses.c`, `src/birth/birth-traits.c`, `src/cmd/combat/cmd-combat.c`.

## Leaping

`ability.44.preview`

Level: **Extended**.

**1. Decision**

Leap over chasms and traps if you moved towards them last turn (not roosts or webs).

**2. Decision**

Its requirements and active state still apply when granted by equipment. Continue returns to the real choice without buying, enabling or using it.

Trigger: Public ability preview or newly available ability; raw serial 44, skill 2, ability slot 4.

Sources: `lib/edit/ability.txt`, `src/player/player-bonuses.c`, `src/birth/birth-traits.c`, `src/sdl/input/sdl-mouse-path.c`, `src/cmd/movement/cmd-run.c`.

## Sprinting

`ability.45.preview`

Level: **Extended**.

**1. Decision**

Increased speed after running in roughly the same direction for 4+ squares while wearing only light armour, or 5+ squares otherwise.

**2. Decision**

Its requirements and active state still apply when granted by equipment. Continue returns to the real choice without buying, enabling or using it.

Trigger: Public ability preview or newly available ability; raw serial 45, skill 2, ability slot 5.

Sources: `lib/edit/ability.txt`, `src/player/player-bonuses.c`, `src/birth/birth-traits.c`, `src/player/player-skills.c`, `src/sdl/input/sdl-mouse-path.c`.

## Flanking

`ability.46.preview`

Level: **Extended**.

**1. Decision**

Free attack on an opponent when stepping between two squares adjacent to it. Only works while wearing only light armour.

**2. Decision**

Its requirements and active state still apply when granted by equipment. Continue returns to the real choice without buying, enabling or using it.

Trigger: Public ability preview or newly available ability; raw serial 46, skill 2, ability slot 6.

Sources: `lib/edit/ability.txt`, `src/player/player-bonuses.c`, `src/birth/birth-traits.c`, `src/cmd/combat/cmd-combat.c`, `src/cmd/ui/cmd-ui-abilities.c`.

## Heavy Armour Use

`ability.47.preview`

Level: **Extended**.

**1. Decision**

Gain fixed physical Protection equal to the whole number of 15 lb units of worn armour weight; the engine rolls Xd1, not 1dX. This bonus does not apply to elemental damage. Mail Corslets and Hauberks also receive +1 Evasion.

**2. Decision**

Its requirements and active state still apply when granted by equipment. Continue returns to the real choice without buying, enabling or using it.

Trigger: Public ability preview or newly available ability; raw serial 47, skill 2, ability slot 7.

Sources: `lib/edit/ability.txt`, `src/player/player-bonuses.c`, `src/melee/melee-attack.c`, `src/birth/birth-traits.c`, `src/cmd/ui/cmd-ui-abilities.c`.

## Riposte

`ability.48.preview`

Level: **Extended**.

**1. Decision**

Free attack when opponent misses by 10+ weapon weight (once per round).

**2. Decision**

Its requirements and active state still apply when granted by equipment. Continue returns to the real choice without buying, enabling or using it.

Trigger: Public ability preview or newly available ability; raw serial 48, skill 2, ability slot 8.

Sources: `lib/edit/ability.txt`, `src/player/player-bonuses.c`, `src/birth/birth-traits.c`, `src/melee/melee-attack.c`.

## Controlled Retreat

`ability.49.preview`

Level: **Extended**.

**1. Decision**

Free attack when stepping away from an opponent, if you did not move last round.

**2. Decision**

Its requirements and active state still apply when granted by equipment. Continue returns to the real choice without buying, enabling or using it.

Trigger: Public ability preview or newly available ability; raw serial 49, skill 2, ability slot 9.

Sources: `lib/edit/ability.txt`, `src/player/player-bonuses.c`, `src/birth/birth-traits.c`, `src/cmd/combat/cmd-combat.c`.

## Dexterity

`ability.50.preview`

Level: **Extended**.

**1. Decision**

+1 Dexterity.

**2. Decision**

Its requirements and active state still apply when granted by equipment. Continue returns to the real choice without buying, enabling or using it.

Trigger: Public ability preview or newly available ability; raw serial 50, skill 2, ability slot 10.

Sources: `lib/edit/ability.txt`, `src/player/player-bonuses.c`, `src/birth/birth-traits.c`.

## Disguise

`ability.60.preview`

Level: **Extended**.

**1. Decision**

Halves line-of-sight detection bonus awake unwary enemies have against you.

**2. Decision**

Its requirements and active state still apply when granted by equipment. Continue returns to the real choice without buying, enabling or using it.

Trigger: Public ability preview or newly available ability; raw serial 60, skill 3, ability slot 0.

Sources: `lib/edit/ability.txt`, `src/melee/melee-process.c`, `src/birth/birth-traits.c`, `src/cave/cave-visuals.c`, `src/melee/melee-process.c`, `src/sdl/render/sdl-term-callbacks.c`.

## Assassination

`ability.61.preview`

Level: **Extended**.

**1. Decision**

Melee bonus equal to your Stealth score vs non-alert creatures and monsters fooled by Song of Disguise.

**2. Decision**

Its requirements and active state still apply when granted by equipment. Continue returns to the real choice without buying, enabling or using it.

Trigger: Public ability preview or newly available ability; raw serial 61, skill 3, ability slot 1.

Sources: `lib/edit/ability.txt`, `src/melee/melee-process.c`, `src/birth/birth-traits.c`, `src/cmd/combat/cmd-combat.c`, `src/cmd/ui/cmd-ui-abilities.c`.

## Cruel Blow

`ability.62.preview`

Level: **Extended**.

**1. Decision**

Critical hits may confuse monsters (critical level vs monster's Will). If granted by an equipped item, it also speeds the minimum depth timer as much as Deep Call, even when disabled.

**2. Decision**

Its requirements and active state still apply when granted by equipment. Continue returns to the real choice without buying, enabling or using it.

Trigger: Public ability preview or newly available ability; raw serial 62, skill 3, ability slot 2.

Sources: `lib/edit/ability.txt`, `src/melee/melee-process.c`, `src/birth/birth-traits.c`, `src/object/object-info.c`, `src/cmd/combat/cmd-combat.c`, `src/cmd/combat/cmd-ranged.c`, `src/cmd/movement/cmd-depth-bonus.c`.

## Exchange Places

`ability.63.preview`

Level: **Extended**.

**1. Decision**

Use Exchange Places to swap positions with an adjacent enemy. They get a free attack as you pass.

**2. Decision**

Its requirements and active state still apply when granted by equipment. Continue returns to the real choice without buying, enabling or using it.

Trigger: Public ability preview or newly available ability; raw serial 63, skill 3, ability slot 3.

Sources: `lib/edit/ability.txt`, `src/melee/melee-process.c`, `src/birth/birth-traits.c`, `src/sdl/input/sdl-player-actions.c`, `src/sdl/ui/sdl-main-menu.c`, `src/cmd/item/cmd-item-utility.c`.

## Opportunist

`ability.64.preview`

Level: **Extended**.

**1. Decision**

Free attack when an adjacent opponent moves away from you.

**2. Decision**

Its requirements and active state still apply when granted by equipment. Continue returns to the real choice without buying, enabling or using it.

Trigger: Public ability preview or newly available ability; raw serial 64, skill 3, ability slot 4.

Sources: `lib/edit/ability.txt`, `src/melee/melee-process.c`, `src/birth/birth-traits.c`, `src/melee/melee-process.c`, `src/monster/monster-move.c`.

## Vanish

`ability.65.preview`

Level: **Extended**.

**1. Decision**

+10 stealth towards making enemies unwary when out of their line of sight.

**2. Decision**

Its requirements and active state still apply when granted by equipment. Continue returns to the real choice without buying, enabling or using it.

Trigger: Public ability preview or newly available ability; raw serial 65, skill 3, ability slot 5.

Sources: `lib/edit/ability.txt`, `src/melee/melee-process.c`, `src/birth/birth-traits.c`, `src/melee/melee-process.c`.

## Dexterity

`ability.66.preview`

Level: **Extended**.

**1. Decision**

+1 Dexterity.

**2. Decision**

Its requirements and active state still apply when granted by equipment. Continue returns to the real choice without buying, enabling or using it.

Trigger: Public ability preview or newly available ability; raw serial 66, skill 3, ability slot 6.

Sources: `lib/edit/ability.txt`, `src/melee/melee-process.c`, `src/birth/birth-traits.c`, `src/player/player-bonuses.c`.

## Quick Study

`ability.80.preview`

Level: **Extended**.

**1. Decision**

Take advanced abilities without prerequisites. Modest bonus to identifying items.

**2. Decision**

Its requirements and active state still apply when granted by equipment. Continue returns to the real choice without buying, enabling or using it.

Trigger: Public ability preview or newly available ability; raw serial 80, skill 4, ability slot 0.

Sources: `lib/edit/ability.txt`, `src/player/player-bonuses.c`, `src/birth/birth-traits.c`, `src/player/player-lore.c`, `src/cmd/ui/cmd-ui-abilities.c`.

## Focused attack

`ability.81.preview`

Level: **Extended**.

**1. Decision**

+Perception/2 attack bonus if you passed the previous turn.

**2. Decision**

Its requirements and active state still apply when granted by equipment. Continue returns to the real choice without buying, enabling or using it.

Trigger: Public ability preview or newly available ability; raw serial 81, skill 4, ability slot 1.

Sources: `lib/edit/ability.txt`, `src/player/player-bonuses.c`, `src/birth/birth-traits.c`, `src/sdl/ui/sdl-panes.c`, `src/cmd/combat/cmd-combat.c`, `src/cmd/ui/cmd-ui-abilities.c`.

## Keen Senses

`ability.82.preview`

Level: **Extended**.

**1. Decision**

See enemies just beyond light's edge. +5 to spotting invisible creatures.

**2. Decision**

Its requirements and active state still apply when granted by equipment. Continue returns to the real choice without buying, enabling or using it.

Trigger: Public ability preview or newly available ability; raw serial 82, skill 4, ability slot 2.

Sources: `lib/edit/ability.txt`, `src/player/player-bonuses.c`, `src/birth/birth-traits.c`, `src/cave/cave-geometry.c`, `src/monster/monster-update.c`.

## Concentration

`ability.83.preview`

Level: **Extended**.

**1. Decision**

+1 attack per consecutive round attacking the same enemy (max Perception/2).

**2. Decision**

Its requirements and active state still apply when granted by equipment. Continue returns to the real choice without buying, enabling or using it.

Trigger: Public ability preview or newly available ability; raw serial 83, skill 4, ability slot 3.

Sources: `lib/edit/ability.txt`, `src/player/player-bonuses.c`, `src/birth/birth-traits.c`, `src/sdl/ui/sdl-panes.c`, `src/cmd/combat/cmd-combat.c`, `src/cmd/ui/cmd-ui-abilities.c`.

## Alchemy

`ability.84.preview`

Level: **Extended**.

**1. Decision**

Auto-identify herbs, potions, staves, and horns. Potions with thrown effects can be quick-thrown, splashing the impact square and every adjacent square. +50% range for Gems of Revelation, Foes, and Treasures.

**2. Decision**

Its requirements and active state still apply when granted by equipment. Continue returns to the real choice without buying, enabling or using it.

Trigger: Public ability preview or newly available ability; raw serial 84, skill 4, ability slot 4.

Sources: `lib/edit/ability.txt`, `src/player/player-bonuses.c`, `src/use-obj.c`, `src/birth/birth-traits.c`, `src/object/object-info.c`, `src/player/player-active-weapon.c`, `src/player/player-lore.c`.

## Bane

`ability.85.preview`

Level: **Extended**.

**1. Decision**

Bonus to all skill rolls vs a chosen enemy type. Bonus increases with kills.

**2. Decision**

Its requirements and active state still apply when granted by equipment. Continue returns to the real choice without buying, enabling or using it.

Trigger: Public ability preview or newly available ability; raw serial 85, skill 4, ability slot 5.

Sources: `lib/edit/ability.txt`, `src/player/player-bonuses.c`, `src/birth/birth-traits.c`, `src/melee/melee-process.c`, `src/object/object-info.c`, `src/ui/character-dump.c`, `src/cmd/ui/cmd-ui-abilities.c`.

## Outwit

`ability.86.preview`

Level: **Extended**.

**1. Decision**

When hit critically, roll Perception vs attacker's Perception to negate all critical damage.

**2. Decision**

Its requirements and active state still apply when granted by equipment. Continue returns to the real choice without buying, enabling or using it.

Trigger: Public ability preview or newly available ability; raw serial 86, skill 4, ability slot 6.

Sources: `lib/edit/ability.txt`, `src/player/player-bonuses.c`, `src/birth/birth-traits.c`, `src/cmd/combat/cmd-combat.c`.

## Resonance

`ability.87.preview`

Level: **Extended**.

**1. Decision**

Detect unseen enemies each turn. Double the Perception portion of identification; Grace still counts only once.

**2. Decision**

Its requirements and active state still apply when granted by equipment. Continue returns to the real choice without buying, enabling or using it.

Trigger: Public ability preview or newly available ability; raw serial 87, skill 4, ability slot 7.

Sources: `lib/edit/ability.txt`, `src/player/player-bonuses.c`, `src/birth/birth-traits.c`, `src/monster/monster-lore.c`, `src/monster/monster-update.c`, `src/player/player-lore.c`.

## Master Hunter

`ability.88.preview`

Level: **Extended**.

**1. Decision**

+1 attack per previous kill of the same monster type (max Perception/2).

**2. Decision**

Its requirements and active state still apply when granted by equipment. Continue returns to the real choice without buying, enabling or using it.

Trigger: Public ability preview or newly available ability; raw serial 88, skill 4, ability slot 8.

Sources: `lib/edit/ability.txt`, `src/player/player-bonuses.c`, `src/birth/birth-traits.c`, `src/cmd/combat/cmd-combat.c`, `src/cmd/ui/cmd-ui-abilities.c`.

## Grace

`ability.89.preview`

Level: **Extended**.

**1. Decision**

+1 Grace. +6.0 qt Harness capacity.

**2. Decision**

Its requirements and active state still apply when granted by equipment. Continue returns to the real choice without buying, enabling or using it.

Trigger: Public ability preview or newly available ability; raw serial 89, skill 4, ability slot 9.

Sources: `lib/edit/ability.txt`, `src/player/player-bonuses.c`, `src/birth/birth-traits.c`, `src/object/object-inventory-limits.c`.

## Rewire Traps

`ability.90.preview`

Level: **Extended**.

**1. Decision**

When you disarm a suitable trap, you instead re-key its mechanism: it no longer harms you, and monsters that cross it may set off its altered workings. The greater your margin on the attempt, the harder foes find it to notice or undo the change.

**2. Decision**

Its requirements and active state still apply when granted by equipment. Continue returns to the real choice without buying, enabling or using it.

Trigger: Public ability preview or newly available ability; raw serial 90, skill 4, ability slot 10.

Sources: `lib/edit/ability.txt`, `src/player/player-bonuses.c`, `src/cmd/world/cmd-interact-chest.c`, `src/cmd/world/cmd-interact.c`.

## Curse Breaking

`ability.100.preview`

Level: **Extended**.

**1. Decision**

Break curses on removal. Significant bonus to identifying items, especially cursed ones. Lets gems of Sanctity break jinxed egos.

**2. Decision**

Its requirements and active state still apply when granted by equipment. Continue returns to the real choice without buying, enabling or using it.

Trigger: Public ability preview or newly available ability; raw serial 100, skill 5, ability slot 0.

Sources: `lib/edit/ability.txt`, `src/player/player-bonuses.c`, `src/use-obj.c`, `src/birth/birth-traits.c`, `src/player/player-lore.c`, `src/cmd/combat/cmd-ranged.c`, `src/cmd/item/cmd-item-activate.c`, `src/cmd/item/cmd-item-core.c`.

## Channeling

`ability.101.preview`

Level: **Extended**.

**1. Decision**

Automatically identify staves and horns. Channel charges from a compatible floor staff into a carried staff through the explicit Channel action. Horn Voice cost falls from 20 to 10, and a Gem of Recharging restores twice its usual amount. Inspect the actual charge transfer before committing.

**2. Decision**

Its requirements and active state still apply when granted by equipment. Continue returns to the real choice without buying, enabling or using it.

Trigger: Public ability preview or newly available ability; raw serial 101, skill 5, ability slot 1.

Sources: `lib/edit/ability.txt`, `src/player/player-bonuses.c`, `src/use-obj.c`, `src/birth/birth-traits.c`, `src/player/player-lore.c`, `src/spell/spell-identify.c`, `src/cmd/item/cmd-pickup.c`.

## Strength in Adversity

`ability.102.preview`

Level: **Extended**.

**1. Decision**

+1 Str/Dex/Gra at 50% HP or below. +3 Str/Dex/Gra at 25% HP or below.

**2. Decision**

Its requirements and active state still apply when granted by equipment. Continue returns to the real choice without buying, enabling or using it.

Trigger: Public ability preview or newly available ability; raw serial 102, skill 5, ability slot 2.

Sources: `lib/edit/ability.txt`, `src/player/player-bonuses.c`, `src/birth/birth-traits.c`, `src/cmd/ui/cmd-ui-abilities.c`.

## Formidable

`ability.103.preview`

Level: **Extended**.

**1. Decision**

Melee kills scare all visible enemies. Enemies ignore your injuries for morale purposes.

**2. Decision**

Its requirements and active state still apply when granted by equipment. Continue returns to the real choice without buying, enabling or using it.

Trigger: Public ability preview or newly available ability; raw serial 103, skill 5, ability slot 3.

Sources: `lib/edit/ability.txt`, `src/player/player-bonuses.c`, `src/birth/birth-traits.c`, `src/melee/melee-movement-path.c`, `src/melee/melee-process.c`, `src/cmd/combat/cmd-combat.c`.

## Inner Light

`ability.104.preview`

Level: **Extended**.

**1. Decision**

+2 light intensity within your light radius (no radius increase).

**2. Decision**

Its requirements and active state still apply when granted by equipment. Continue returns to the real choice without buying, enabling or using it.

Trigger: Public ability preview or newly available ability; raw serial 104, skill 5, ability slot 4.

Sources: `lib/edit/ability.txt`, `src/player/player-bonuses.c`, `src/birth/birth-traits.c`, `src/cave/cave-view.c`.

## Indomitable

`ability.105.preview`

Level: **Extended**.

**1. Decision**

Resist fear, confusion, stunning, and hallucination. Hunger reduced to 1/3 normal rate.

**2. Decision**

Its requirements and active state still apply when granted by equipment. Continue returns to the real choice without buying, enabling or using it.

Trigger: Public ability preview or newly available ability; raw serial 105, skill 5, ability slot 5.

Sources: `lib/edit/ability.txt`, `src/player/player-bonuses.c`, `src/birth/birth-traits.c`.

## Oath

`ability.106.preview`

Level: **Extended**.

**1. Decision**

Swear a great oath for a reward. Breaking the oath has consequences.

**2. Decision**

Its requirements and active state still apply when granted by equipment. Continue returns to the real choice without buying, enabling or using it.

Trigger: Public ability preview or newly available ability; raw serial 106, skill 5, ability slot 6.

Sources: `lib/edit/ability.txt`, `src/player/player-bonuses.c`, `src/birth/birth-traits.c`, `src/ui/character-dump.c`, `src/cmd/ui/cmd-ui-abilities.c`.

## Poison Resistance

`ability.107.preview`

Level: **Extended**.

**1. Decision**

Grants resistance to poison.

**2. Decision**

Its requirements and active state still apply when granted by equipment. Continue returns to the real choice without buying, enabling or using it.

Trigger: Public ability preview or newly available ability; raw serial 107, skill 5, ability slot 7.

Sources: `lib/edit/ability.txt`, `src/player/player-bonuses.c`, `src/birth/birth-traits.c`.

## Vengeance

`ability.108.preview`

Level: **Extended**.

**1. Decision**

+1 damage die on your next melee hit after being damaged in melee. Does not stack.

**2. Decision**

Its requirements and active state still apply when granted by equipment. Continue returns to the real choice without buying, enabling or using it.

Trigger: Public ability preview or newly available ability; raw serial 108, skill 5, ability slot 8.

Sources: `lib/edit/ability.txt`, `src/player/player-bonuses.c`, `src/birth/birth-traits.c`, `src/melee/melee-attack.c`, `src/player/combat-stats.c`, `src/sdl/ui/sdl-panes.c`, `src/cmd/ui/cmd-ui-abilities.c`.

## Majesty

`ability.109.preview`

Level: **Extended**.

**1. Decision**

Reduce enemy morale by half the difference between your Will and theirs.

**2. Decision**

Its requirements and active state still apply when granted by equipment. Continue returns to the real choice without buying, enabling or using it.

Trigger: Public ability preview or newly available ability; raw serial 109, skill 5, ability slot 9.

Sources: `lib/edit/ability.txt`, `src/player/player-bonuses.c`, `src/birth/birth-traits.c`, `src/melee/melee-process.c`.

## Constitution

`ability.110.preview`

Level: **Extended**.

**1. Decision**

+1 Constitution. +6.0 qt Pack capacity.

**2. Decision**

Its requirements and active state still apply when granted by equipment. Continue returns to the real choice without buying, enabling or using it.

Trigger: Public ability preview or newly available ability; raw serial 110, skill 5, ability slot 10.

Sources: `lib/edit/ability.txt`, `src/player/player-bonuses.c`, `src/birth/birth-traits.c`, `src/object/object-inventory-limits.c`.

## Weaponsmith

`ability.120.preview`

Level: **Extended**.

**1. Decision**

Create weapons at the forge. Modest bonus to identifying weapons.

**2. Decision**

Its requirements and active state still apply when granted by equipment. Continue returns to the real choice without buying, enabling or using it.

Trigger: Public ability preview or newly available ability; raw serial 120, skill 6, ability slot 0.

Sources: `lib/edit/ability.txt`, `src/cmd/ui/cmd-ui-smithing.c`, `src/birth/birth-traits.c`, `src/player/player-lore.c`.

## Armoursmith

`ability.121.preview`

Level: **Extended**.

**1. Decision**

Create armour at the forge. Modest bonus to identifying armour.

**2. Decision**

Its requirements and active state still apply when granted by equipment. Continue returns to the real choice without buying, enabling or using it.

Trigger: Public ability preview or newly available ability; raw serial 121, skill 6, ability slot 1.

Sources: `lib/edit/ability.txt`, `src/cmd/ui/cmd-ui-smithing.c`, `src/birth/birth-traits.c`, `src/player/player-lore.c`.

## Jeweller

`ability.122.preview`

Level: **Extended**.

**1. Decision**

Create rings, amulets, horns, and light sources. Auto-identify them. Modest jewellery ID bonus.

**2. Decision**

Its requirements and active state still apply when granted by equipment. Continue returns to the real choice without buying, enabling or using it.

Trigger: Public ability preview or newly available ability; raw serial 122, skill 6, ability slot 2.

Sources: `lib/edit/ability.txt`, `src/cmd/ui/cmd-ui-smithing.c`, `src/birth/birth-traits.c`, `src/player/player-lore.c`.

## Enchantment

`ability.123.preview`

Level: **Extended**.

**1. Decision**

Create {special} items. Determine enchantments on items. Modest ID bonus.

**2. Decision**

Its requirements and active state still apply when granted by equipment. Continue returns to the real choice without buying, enabling or using it.

Trigger: Public ability preview or newly available ability; raw serial 123, skill 6, ability slot 3.

Sources: `lib/edit/ability.txt`, `src/cmd/ui/cmd-ui-smithing.c`, `src/birth/birth-traits.c`, `src/player/player-lore.c`.

## Expertise

`ability.124.preview`

Level: **Extended**.

**1. Decision**

Halve forging time. Negate all experience and stat costs of smithing.

**2. Decision**

Its requirements and active state still apply when granted by equipment. Continue returns to the real choice without buying, enabling or using it.

Trigger: Public ability preview or newly available ability; raw serial 124, skill 6, ability slot 4.

Sources: `lib/edit/ability.txt`, `src/cmd/ui/cmd-ui-smithing.c`, `src/birth/birth-traits.c`.

## Artifice

`ability.125.preview`

Level: **Extended**.

**1. Decision**

Create highly customised artifacts. Significant bonus to identifying items.

**2. Decision**

Its requirements and active state still apply when granted by equipment. Continue returns to the real choice without buying, enabling or using it.

Trigger: Public ability preview or newly available ability; raw serial 125, skill 6, ability slot 5.

Sources: `lib/edit/ability.txt`, `src/cmd/ui/cmd-ui-smithing.c`, `src/birth/birth-traits.c`, `src/player/player-lore.c`.

## Masterpiece

`ability.126.preview`

Level: **Extended**.

**1. Decision**

Create items beyond normal difficulty limit. Drains Smithing skill per excess difficulty point.

**2. Decision**

Its requirements and active state still apply when granted by equipment. Continue returns to the real choice without buying, enabling or using it.

Trigger: Public ability preview or newly available ability; raw serial 126, skill 6, ability slot 6.

Sources: `lib/edit/ability.txt`, `src/cmd/ui/cmd-ui-smithing.c`, `src/birth/birth-traits.c`, `src/cmd/ui/cmd-ui-abilities.c`.

## Grace

`ability.127.preview`

Level: **Extended**.

**1. Decision**

+1 Grace.

**2. Decision**

Its requirements and active state still apply when granted by equipment. Continue returns to the real choice without buying, enabling or using it.

Trigger: Public ability preview or newly available ability; raw serial 127, skill 6, ability slot 7.

Sources: `lib/edit/ability.txt`, `src/cmd/ui/cmd-ui-smithing.c`, `src/birth/birth-traits.c`, `src/player/player-bonuses.c`.

## Alloy mastery

`ability.128.preview`

Level: **Extended**.

**1. Decision**

Alloy weapons and armour with mithril or star iron at no extra cost. Forge mithril/star-iron gear.

**2. Decision**

Its requirements and active state still apply when granted by equipment. Continue returns to the real choice without buying, enabling or using it.

Trigger: Public ability preview or newly available ability; raw serial 128, skill 6, ability slot 8.

Sources: `lib/edit/ability.txt`, `src/cmd/ui/cmd-ui-smithing.c`, `src/birth/birth-traits.c`.

## Reforging

`ability.129.preview`

Level: **Extended**.

**1. Decision**

Repair damaged items at a forge, or add a missing prefix to a found item. Reforging difficulty is 1.5x the difficulty increase.

**2. Decision**

Its requirements and active state still apply when granted by equipment. Continue returns to the real choice without buying, enabling or using it.

Trigger: Public ability preview or newly available ability; raw serial 129, skill 6, ability slot 9.

Sources: `lib/edit/ability.txt`, `src/cmd/ui/cmd-ui-smithing.c`.

## Song of Elbereth

`ability.140.preview`

Level: **Extended**.

**1. Decision**

Causes enemies to flee. Reduces enemy Will by Song/5.

**2. Decision**

Its requirements and active state still apply when granted by equipment. Continue returns to the real choice without buying, enabling or using it.

Trigger: Public ability preview or newly available ability; raw serial 140, skill 7, ability slot 0.

Sources: `lib/edit/ability.txt`, `src/player/player-song-effects.c`, `src/birth/birth-traits.c`, `src/monster/monster-attr.c`, `src/player/player-bonuses.c`, `src/player/player-skills.c`, `src/player/player-song-duels.c`, `src/player/player-songs.c`.

## Song of Challenge

`ability.141.preview`

Level: **Extended**.

**1. Decision**

Reduces enemy Will and Stealth by Song/5 each. Enrages foes into reckless attacks.

**2. Decision**

Its requirements and active state still apply when granted by equipment. Continue returns to the real choice without buying, enabling or using it.

Trigger: Public ability preview or newly available ability; raw serial 141, skill 7, ability slot 1.

Sources: `lib/edit/ability.txt`, `src/player/player-song-effects.c`, `src/birth/birth-traits.c`, `src/melee/melee-process.c`, `src/monster/monster-attr.c`, `src/player/effects.c`, `src/player/player-bonuses.c`, `src/player/player-skills.c`.

## Song of Delvings

`ability.142.preview`

Level: **Extended**.

**1. Decision**

Gradually reveals surrounding passages and chambers, expanding from explored areas.

**2. Decision**

Its requirements and active state still apply when granted by equipment. Continue returns to the real choice without buying, enabling or using it.

Trigger: Public ability preview or newly available ability; raw serial 142, skill 7, ability slot 2.

Sources: `lib/edit/ability.txt`, `src/player/player-song-effects.c`, `src/birth/birth-traits.c`, `src/player/player-bonuses.c`, `src/player/player-skills.c`, `src/player/player-song-duels.c`, `src/player/player-songs.c`, `src/cmd/ui/cmd-ui-abilities.c`.

## Song of Freedom

`ability.143.preview`

Level: **Extended**.

**1. Decision**

Reveals hidden doors, disarms traps, clears rubble. Grants freedom from slowing effects.

**2. Decision**

Its requirements and active state still apply when granted by equipment. Continue returns to the real choice without buying, enabling or using it.

Trigger: Public ability preview or newly available ability; raw serial 143, skill 7, ability slot 3.

Sources: `lib/edit/ability.txt`, `src/player/player-song-effects.c`, `src/birth/birth-traits.c`, `src/dungeon/dungeon-player.c`, `src/player/player-bonuses.c`, `src/player/player-skills.c`, `src/player/player-song-duels.c`, `src/player/player-songs.c`.

## Song of Silence

`ability.144.preview`

Level: **Extended**.

**1. Decision**

Muffles sounds, making it harder for enemies to detect you or call for allies.

**2. Decision**

Its requirements and active state still apply when granted by equipment. Continue returns to the real choice without buying, enabling or using it.

Trigger: Public ability preview or newly available ability; raw serial 144, skill 7, ability slot 4.

Sources: `lib/edit/ability.txt`, `src/player/player-song-effects.c`, `src/birth/birth-traits.c`, `src/melee/melee-attack-ranged.c`, `src/melee/melee-movement-resolution.c`, `src/melee/melee-process.c`, `src/monster/monster-update.c`, `src/player/player-bonuses.c`.

## Song of Staunching

`ability.145.preview`

Level: **Extended**.

**1. Decision**

On each song effect tick, clear bleeding and restore Health at an average rate of effective Song / 12 per turn. At effective Song 12, this is 1 Health per tick. This direct song healing is separate from ordinary regeneration; singing still consumes Voice.

**2. Decision**

Its requirements and active state still apply when granted by equipment. Continue returns to the real choice without buying, enabling or using it.

Trigger: Public ability preview or newly available ability; raw serial 145, skill 7, ability slot 5.

Sources: `lib/edit/ability.txt`, `src/player/player-song-effects.c`, `src/birth/birth-traits.c`, `src/player/player-bonuses.c`, `src/player/player-skills.c`, `src/player/player-song-duels.c`, `src/player/player-songs.c`, `src/cmd/ui/cmd-ui-abilities.c`.

## Song of Thresholds

`ability.146.preview`

Level: **Extended**.

**1. Decision**

Closed doors become sealed barriers, resistant to enemies.

**2. Decision**

Its requirements and active state still apply when granted by equipment. Continue returns to the real choice without buying, enabling or using it.

Trigger: Public ability preview or newly available ability; raw serial 146, skill 7, ability slot 6.

Sources: `lib/edit/ability.txt`, `src/player/player-song-effects.c`, `src/birth/birth-traits.c`, `src/player/player-bonuses.c`, `src/player/player-skills.c`, `src/player/player-songs.c`, `src/cmd/ui/cmd-ui-abilities.c`, `src/cmd/world/cmd-interact.c`.

## Song of the Trees

`ability.147.preview`

Level: **Extended**.

**1. Decision**

+1 light radius at 0-5 Song, +2 at 6-11, +3 at 12-18, and so on. Creatures of darkness may be stunned or wounded (vs Will).

**2. Decision**

Its requirements and active state still apply when granted by equipment. Continue returns to the real choice without buying, enabling or using it.

Trigger: Public ability preview or newly available ability; raw serial 147, skill 7, ability slot 7.

Sources: `lib/edit/ability.txt`, `src/player/player-song-effects.c`, `src/birth/birth-traits.c`, `src/player/player-bonuses.c`, `src/player/player-light.c`, `src/player/player-skills.c`, `src/player/player-song-duels.c`.

## Woven Themes

`ability.148.preview`

Level: **Extended**.

**1. Decision**

Sing a minor theme alongside your major song at half Song score.

**2. Decision**

Its requirements and active state still apply when granted by equipment. Continue returns to the real choice without buying, enabling or using it.

Trigger: Public ability preview or newly available ability; raw serial 148, skill 7, ability slot 9.

Sources: `lib/edit/ability.txt`, `src/player/player-song-effects.c`, `src/birth/birth-traits.c`, `src/player/player-songs.c`, `src/sdl/input/sdl-player-actions.c`, `src/cmd/ui/cmd-ui-abilities.c`.

## Song of Slaying

`ability.149.preview`

Level: **Extended**.

**1. Decision**

Melee critical hits slay the foe if their HP is at most 2x your Song score.

**2. Decision**

Its requirements and active state still apply when granted by equipment. Continue returns to the real choice without buying, enabling or using it.

Trigger: Public ability preview or newly available ability; raw serial 149, skill 7, ability slot 10.

Sources: `lib/edit/ability.txt`, `src/player/player-song-effects.c`, `src/birth/birth-traits.c`, `src/player/player-bonuses.c`, `src/player/player-skills.c`, `src/player/player-song-duels.c`, `src/player/player-songs.c`, `src/cmd/combat/cmd-combat.c`.

## Song of Revealing

`ability.150.preview`

Level: **Extended**.

**1. Decision**

Roll Song each turn to reveal nearby monsters and items. Revealed, carried, and equipped items get +1d5 identification.

**2. Decision**

Its requirements and active state still apply when granted by equipment. Continue returns to the real choice without buying, enabling or using it.

Trigger: Public ability preview or newly available ability; raw serial 150, skill 7, ability slot 8.

Sources: `lib/edit/ability.txt`, `src/player/player-song-effects.c`, `src/birth/birth-traits.c`, `src/player/player-lore.c`, `src/player/player-skills.c`, `src/player/player-song-duels.c`, `src/player/player-songs.c`, `src/cmd/ui/cmd-ui-abilities.c`.

## Song of Elveness

`ability.151.preview`

Level: **Extended**.

**1. Decision**

+1 Grace. +1 Evasion at 0-7 Song, +2 at 8-15, +3 at 16-24, and so on.

**2. Decision**

Its requirements and active state still apply when granted by equipment. Continue returns to the real choice without buying, enabling or using it.

Trigger: Public ability preview or newly available ability; raw serial 151, skill 7, ability slot 11.

Sources: `lib/edit/ability.txt`, `src/player/player-song-effects.c`, `src/birth/birth-traits.c`, `src/player/player-bonuses.c`, `src/player/player-skills.c`, `src/player/player-song-duels.c`, `src/player/player-songs.c`, `src/cmd/ui/cmd-ui-abilities.c`.

## Song of Staying

`ability.152.preview`

Level: **Extended**.

**1. Decision**

+Song/2 to Will. [2d2] protection against all damage.

**2. Decision**

Its requirements and active state still apply when granted by equipment. Continue returns to the real choice without buying, enabling or using it.

Trigger: Public ability preview or newly available ability; raw serial 152, skill 7, ability slot 12.

Sources: `lib/edit/ability.txt`, `src/player/player-song-effects.c`, `src/birth/birth-traits.c`, `src/melee/melee-attack.c`, `src/player/player-bonuses.c`, `src/player/player-skills.c`, `src/player/player-song-duels.c`, `src/player/player-songs.c`.

## Song of Disguise

`ability.153.preview`

Level: **Extended**.

**1. Decision**

You cannot start Song of Disguise while an alert non-peaceful creature observes you. Your attack ends the disguise. Each round, effective Song + 5 + your Will contests enemy Will + Perception, with penalties for observers, creatures that already saw through you and recent attackers. Distance helps; a success fools that creature for the current round.

**2. Decision**

Its requirements and active state still apply when granted by equipment. Continue returns to the real choice without buying, enabling or using it.

Trigger: Public ability preview or newly available ability; raw serial 153, skill 7, ability slot 13.

Sources: `lib/edit/ability.txt`, `src/player/player-song-effects.c`, `src/birth/birth-traits.c`, `src/monster/monster-move.c`, `src/player/player-bonuses.c`, `src/player/player-skills.c`, `src/player/player-song-disguise.c`, `src/player/player-song-duels.c`.

## Song of Lórien

`ability.154.preview`

Level: **Extended**.

**1. Decision**

Gradually puts nearby opponents to sleep.

**2. Decision**

Its requirements and active state still apply when granted by equipment. Continue returns to the real choice without buying, enabling or using it.

Trigger: Public ability preview or newly available ability; raw serial 154, skill 7, ability slot 14.

Sources: `lib/edit/ability.txt`, `src/player/player-song-effects.c`, `src/birth/birth-traits.c`, `src/player/player-bonuses.c`, `src/player/player-skills.c`, `src/player/player-song-duels.c`, `src/player/player-songs.c`, `src/cmd/ui/cmd-ui-abilities.c`.

## Song of Shattering

`ability.155.preview`

Level: **Extended**.

**1. Decision**

Shatter enemy weapons and armour, even the stone bodies of stone creatures, weakening their attacks and defenses (resisted by Will).

**2. Decision**

Its requirements and active state still apply when granted by equipment. Continue returns to the real choice without buying, enabling or using it.

Trigger: Public ability preview or newly available ability; raw serial 155, skill 7, ability slot 15.

Sources: `lib/edit/ability.txt`, `src/player/player-song-effects.c`, `src/birth/birth-traits.c`, `src/player/player-skills.c`, `src/player/player-song-duels.c`, `src/player/player-songs.c`, `src/cmd/ui/cmd-ui-abilities.c`.

## Song of Mastery

`ability.156.preview`

Level: **Extended**.

**1. Decision**

May prevent enemy movement or action by overwhelming their minds.

**2. Decision**

Its requirements and active state still apply when granted by equipment. Continue returns to the real choice without buying, enabling or using it.

Trigger: Public ability preview or newly available ability; raw serial 156, skill 7, ability slot 16.

Sources: `lib/edit/ability.txt`, `src/player/player-song-effects.c`, `src/birth/birth-traits.c`, `src/melee/melee-process.c`, `src/player/player-bonuses.c`, `src/player/player-skills.c`, `src/player/player-song-duels.c`, `src/player/player-songs.c`.

## Grace

`ability.157.preview`

Level: **Extended**.

**1. Decision**

+1 Grace.

**2. Decision**

Its requirements and active state still apply when granted by equipment. Continue returns to the real choice without buying, enabling or using it.

Trigger: Public ability preview or newly available ability; raw serial 157, skill 7, ability slot 17.

Sources: `lib/edit/ability.txt`, `src/player/player-song-effects.c`, `src/birth/birth-traits.c`, `src/player/player-bonuses.c`, `src/sdl/input/sdl-player-actions.c`, `src/cmd/ui/cmd-ui-abilities.c`.

## Song of Contest

`ability.158.preview`

Level: **Extended**.

**1. Decision**

Contest challenges one eligible foe to repeated opposed rolls. Winning permanently reduces its Will, Stealth, Evasion and armour dice. Losing drains one randomly chosen attribute by 1. A completed duel stops the song and locks singing for 10 turns; you cannot repeat the same completed duel against that foe.

**2. Decision**

Its requirements and active state still apply when granted by equipment. Continue returns to the real choice without buying, enabling or using it.

Trigger: Public ability preview or newly available ability; raw serial 158, skill 7, ability slot 18.

Sources: `lib/edit/ability.txt`, `src/player/player-song-effects.c`, `src/birth/birth-traits.c`, `src/monster/monster-lore.c`, `src/player/player-skills.c`, `src/player/player-song-duels.c`, `src/player/player-songs.c`, `src/cmd/ui/cmd-ui-abilities.c`.

## Song of Lament

`ability.159.preview`

Level: **Extended**.

**1. Decision**

Lament works toward permanently reducing an eligible target's Will, maximum Health and damage dice. Completing the effect drains your Grace by 1, even on success. The song then stops and singing is locked for 10 turns; you cannot repeat the completed duel against that foe. Read the live target and Voice cost before committing.

**2. Decision**

Its requirements and active state still apply when granted by equipment. Continue returns to the real choice without buying, enabling or using it.

Trigger: Public ability preview or newly available ability; raw serial 159, skill 7, ability slot 19.

Sources: `lib/edit/ability.txt`, `src/player/player-song-effects.c`, `src/birth/birth-traits.c`, `src/monster/monster-lore.c`, `src/player/player-skills.c`, `src/player/player-song-duels.c`, `src/player/player-songs.c`, `src/cmd/ui/cmd-ui-abilities.c`.

## Mandos' Doom

`ability.160.preview`

Level: **Extended**.

**1. Decision**

This is a special ability, oath or quest reward. Read its current source and requirements. Immune to fear, hallucination, trance, rage, stun, and confusion. Quest reward.

**2. Decision**

Read your current ability and oath state. Continue acknowledges the explanation; it does not grant the reward or make a binding choice.

Trigger: Public ability preview or newly available ability; raw serial 160, skill 8, ability slot 0.

Sources: `lib/edit/ability.txt`, `src/player/player-bonuses.c`, `src/birth/birth-traits.c`, `src/dungeon/dungeon-startup.c`.

## Aulë's Forge

`ability.161.preview`

Level: **Extended**.

**1. Decision**

This is a special ability, oath or quest reward. Read its current source and requirements. Counts as Masterpiece with +2 extra difficulty allowance. Supersedes Masterpiece. Quest reward.

**2. Decision**

Read your current ability and oath state. Continue acknowledges the explanation; it does not grant the reward or make a binding choice.

Trigger: Public ability preview or newly available ability; raw serial 161, skill 8, ability slot 1.

Sources: `lib/edit/ability.txt`, `src/player/player-bonuses.c`, `src/birth/birth-traits.c`, `src/dungeon/dungeon-startup.c`, `src/cmd/ui/cmd-ui-abilities.c`, `src/cmd/ui/cmd-ui-smithing.c`.

## Oath of Mercy

`ability.162.preview`

Level: **Extended**.

**1. Decision**

This is a special ability, oath or quest reward. Read its current source and requirements. Oath of Mercy grants +1 Grace. Its active rule forbids attacking or harming Men or Elves. Breaking it has Tale consequences. This is not a general rule about whether a foe is helpless.

**2. Decision**

Read your current ability and oath state. Continue acknowledges the explanation; it does not grant the reward or make a binding choice.

Trigger: Public ability preview or newly available ability; raw serial 162, skill 8, ability slot 2.

Sources: `lib/edit/ability.txt`, `src/player/player-bonuses.c`, `lib/edit/oath.txt`, `src/cmd/combat/cmd-combat.c`, `src/birth/birth-setup.c`, `src/birth/birth-traits.c`, `src/player/player-light.c`, `src/cmd/ui/cmd-ui-abilities.c`, `src/cmd/ui/cmd-ui-knowledge.c`.

## Oath of Silence

`ability.163.preview`

Level: **Extended**.

**1. Decision**

This is a special ability, oath or quest reward. Read its current source and requirements. Oath of Silence grants +1 Dexterity. Singing breaks the oath. Read the real confirmation before making a forbidden choice; the tutorial never swears or breaks the oath for you.

**2. Decision**

Read your current ability and oath state. Continue acknowledges the explanation; it does not grant the reward or make a binding choice.

Trigger: Public ability preview or newly available ability; raw serial 163, skill 8, ability slot 3.

Sources: `lib/edit/ability.txt`, `src/player/player-bonuses.c`, `lib/edit/oath.txt`, `src/cmd/combat/cmd-combat.c`, `src/birth/birth-setup.c`, `src/birth/birth-traits.c`, `src/player/player-light.c`, `src/cmd/ui/cmd-ui-abilities.c`, `src/cmd/ui/cmd-ui-knowledge.c`.

## Oath of Iron

`ability.164.preview`

Level: **Extended**.

**1. Decision**

This is a special ability, oath or quest reward. Read its current source and requirements. Oath of Iron grants +1 Constitution. It forbids going upstairs or leaving the depths without possessing a Silmaril. Check the staircase decision and the current oath state.

**2. Decision**

Read your current ability and oath state. Continue acknowledges the explanation; it does not grant the reward or make a binding choice.

Trigger: Public ability preview or newly available ability; raw serial 164, skill 8, ability slot 4.

Sources: `lib/edit/ability.txt`, `src/player/player-bonuses.c`, `lib/edit/oath.txt`, `src/cmd/combat/cmd-combat.c`, `src/birth/birth-setup.c`, `src/birth/birth-traits.c`, `src/player/player-light.c`, `src/cmd/ui/cmd-ui-abilities.c`, `src/cmd/ui/cmd-ui-knowledge.c`.

## Nienna's Gift of Mercy

`ability.165.preview`

Level: **Extended**.

**1. Decision**

This is a special ability, oath or quest reward. Read its current source and requirements. Gain Stealth equal to ceil(10 × (non-unique creatures seen − killed) / creatures seen), with no bonus before any are seen. The live character view shows the current count and bonus. Killing more of the observed creatures reduces the benefit.

**2. Decision**

Read your current ability and oath state. Continue acknowledges the explanation; it does not grant the reward or make a binding choice.

Trigger: Public ability preview or newly available ability; raw serial 165, skill 8, ability slot 5.

Sources: `lib/edit/ability.txt`, `src/player/player-bonuses.c`, `src/birth/birth-traits.c`, `src/cmd/ui/cmd-ui-abilities.c`.

## Oath of the Smith

`ability.166.preview`

Level: **Extended**.

**1. Decision**

This is a special ability, oath or quest reward. Read its current source and requirements. Oath of the Smith grants +5 Smithing. It forbids picking up, wielding or wearing weapons or armour made by others. Check the actual item action and oath confirmation before handling found equipment.

**2. Decision**

Read your current ability and oath state. Continue acknowledges the explanation; it does not grant the reward or make a binding choice.

Trigger: Public ability preview or newly available ability; raw serial 166, skill 8, ability slot 6.

Sources: `lib/edit/ability.txt`, `src/player/player-bonuses.c`, `lib/edit/oath.txt`, `src/cmd/combat/cmd-combat.c`, `src/birth/birth-setup.c`, `src/birth/birth-traits.c`, `src/player/player-light.c`, `src/cmd/ui/cmd-ui-abilities.c`, `src/cmd/ui/cmd-ui-knowledge.c`.

## Oath of the Valorous Heart

`ability.167.preview`

Level: **Extended**.

**1. Decision**

This is a special ability, oath or quest reward. Read its current source and requirements. Oath of Valour grants +1 Strength. Attacking or otherwise harming fleeing enemies breaks it. Fleeing is a morale state; it is different from being unaware or asleep.

**2. Decision**

Read your current ability and oath state. Continue acknowledges the explanation; it does not grant the reward or make a binding choice.

Trigger: Public ability preview or newly available ability; raw serial 167, skill 8, ability slot 7.

Sources: `lib/edit/ability.txt`, `src/player/player-bonuses.c`, `lib/edit/oath.txt`, `src/cmd/combat/cmd-combat.c`, `src/birth/birth-setup.c`, `src/birth/birth-traits.c`, `src/player/player-light.c`, `src/cmd/ui/cmd-ui-abilities.c`, `src/cmd/ui/cmd-ui-knowledge.c`.

## Unique Bane

`ability.168.preview`

Level: **Extended**.

**1. Decision**

This is a special ability, oath or quest reward. Read its current source and requirements. +3 attack and evasion vs unique monsters. Quest reward.

**2. Decision**

Read your current ability and oath state. Continue acknowledges the explanation; it does not grant the reward or make a binding choice.

Trigger: Public ability preview or newly available ability; raw serial 168, skill 8, ability slot 8.

Sources: `lib/edit/ability.txt`, `src/player/player-bonuses.c`, `src/birth/birth-traits.c`, `src/quest/valar/other-valar.c`, `src/cmd/ui/cmd-ui-abilities.c`.

## Oath of Light

`ability.169.preview`

Level: **Extended**.

**1. Decision**

This is a special ability, oath or quest reward. Read its current source and requirements. Oath of Light grants +1 light radius while valid. Equipping items with Darkness or Unlight breaks it. Radius, intensity and fuel are different quantities; inspect known equipment properties before changing gear.

**2. Decision**

Read your current ability and oath state. Continue acknowledges the explanation; it does not grant the reward or make a binding choice.

Trigger: Public ability preview or newly available ability; raw serial 169, skill 8, ability slot 9.

Sources: `lib/edit/ability.txt`, `src/player/player-bonuses.c`, `lib/edit/oath.txt`, `src/cmd/combat/cmd-combat.c`, `src/birth/birth-setup.c`, `src/birth/birth-traits.c`, `src/player/player-light.c`, `src/cmd/item/cmd-item-core.c`.

## Chasm

`terrain.2`

Level: **Extended**.

**1. Info**

A chasm can cause a dangerous fall to a deeper level. Inspect the route. Leaping has a run-up requirement and is different from simply walking onto the chasm; no lesson requires you to jump.

Trigger: Feature 2 is on the player square or visibly adjacent and marked; secret/unrevealed terrain is excluded.

Sources: `lib/edit/terrain.txt`, `src/cmd/world/cmd-interact.c`, `src/cmd/movement/cmd-movement.c`.

## Glyph of warding

`terrain.3`

Level: **Extended**.

**1. Info**

A glyph of warding makes crossing difficult for opponents. It is a defensive feature, not a guarantee that enemies cannot reach you. Inspect the surrounding route before relying on it.

Trigger: Feature 3 is on the player square or visibly adjacent and marked; secret/unrevealed terrain is excluded.

Sources: `lib/edit/terrain.txt`, `src/cmd/world/cmd-interact.c`, `src/cmd/movement/cmd-movement.c`.

## Open door

`terrain.4`

Level: **Extended**.

**1. Info**

An open door is traversable. Closing a doorway can change movement, sight and how many enemies can engage at once. Door interactions still spend normal turns.

Trigger: Feature 4 is on the player square or visibly adjacent and marked; secret/unrevealed terrain is excluded.

Sources: `lib/edit/terrain.txt`, `src/cmd/world/cmd-interact.c`, `src/cmd/movement/cmd-movement.c`.

## Broken door

`terrain.5`

Level: **Extended**.

**1. Info**

This door is broken. Do not rely on it as a closed barrier; inspect the passage and nearby threats.

Trigger: Feature 5 is on the player square or visibly adjacent and marked; secret/unrevealed terrain is excluded.

Sources: `lib/edit/terrain.txt`, `src/cmd/world/cmd-interact.c`, `src/cmd/movement/cmd-movement.c`.

## Warded door

`terrain.6`

Level: **Extended**.

**1. Info**

A warded door is a magical barrier. Its strength affects attempts to pass it. Inspect the current door and legal interactions; a ward is not permanent immunity from enemies.

Trigger: Feature 6 is on the player square or visibly adjacent and marked; secret/unrevealed terrain is excluded.

Sources: `lib/edit/terrain.txt`, `src/cmd/world/cmd-interact.c`, `src/cmd/movement/cmd-movement.c`.

## Warded door

`terrain.7`

Level: **Extended**.

**1. Info**

A warded door is a magical barrier. Its strength affects attempts to pass it. Inspect the current door and legal interactions; a ward is not permanent immunity from enemies.

Trigger: Feature 7 is on the player square or visibly adjacent and marked; secret/unrevealed terrain is excluded.

Sources: `lib/edit/terrain.txt`, `src/cmd/world/cmd-interact.c`, `src/cmd/movement/cmd-movement.c`.

## Warded door

`terrain.8`

Level: **Extended**.

**1. Info**

A warded door is a magical barrier. Its strength affects attempts to pass it. Inspect the current door and legal interactions; a ward is not permanent immunity from enemies.

Trigger: Feature 8 is on the player square or visibly adjacent and marked; secret/unrevealed terrain is excluded.

Sources: `lib/edit/terrain.txt`, `src/cmd/world/cmd-interact.c`, `src/cmd/movement/cmd-movement.c`.

## Patch of sunlight

`terrain.9`

Level: **Extended**.

**1. Info**

Sunlight is a visible environmental feature. Light-sensitive creatures may react differently to bright squares; use only the lore and effects actually revealed to you.

Trigger: Feature 9 is on the player square or visibly adjacent and marked; secret/unrevealed terrain is excluded.

Sources: `lib/edit/terrain.txt`, `src/cmd/world/cmd-interact.c`, `src/cmd/movement/cmd-movement.c`.

## False floor

`terrain.16`

Level: **Extended**.

**1. Info**

A known false floor can drop you. Inspect alternative routes or a legal disarm option; discovering the trap does not remove it.

Trigger: Feature 16 is on the player square or visibly adjacent and marked; secret/unrevealed terrain is excluded.

Sources: `lib/edit/terrain.txt`, `src/cmd/world/cmd-interact.c`, `src/cmd/movement/cmd-movement.c`.

## Pit

`terrain.17`

Level: **Extended**.

**1. Info**

A pit can trap or hurt you and changes movement out of the square. Inspect it before stepping in. A valid leap differs from normal movement.

Trigger: Feature 17 is on the player square or visibly adjacent and marked; secret/unrevealed terrain is excluded.

Sources: `lib/edit/terrain.txt`, `src/cmd/world/cmd-interact.c`, `src/cmd/movement/cmd-movement.c`.

## Spiked pit

`terrain.18`

Level: **Extended**.

**1. Info**

A spiked pit can injure you. Avoidance, disarming and leaping have different requirements and costs. The tutorial will not force you onto it.

Trigger: Feature 18 is on the player square or visibly adjacent and marked; secret/unrevealed terrain is excluded.

Sources: `lib/edit/terrain.txt`, `src/cmd/world/cmd-interact.c`, `src/cmd/movement/cmd-movement.c`.

## Dart trap

`terrain.19`

Level: **Extended**.

**1. Info**

A dart mechanism is a trap, even after you see it. Inspect a legal disarm option or choose another route; failure can still have consequences.

Trigger: Feature 19 is on the player square or visibly adjacent and marked; secret/unrevealed terrain is excluded.

Sources: `lib/edit/terrain.txt`, `src/cmd/world/cmd-interact.c`, `src/cmd/movement/cmd-movement.c`.

## Gas trap

`terrain.20`

Level: **Extended**.

**1. Info**

A known gas trap releases its effect when triggered. Its visible identity is not permission to assume every hidden detail. Inspect your resistances and a route around it.

Trigger: Feature 20 is on the player square or visibly adjacent and marked; secret/unrevealed terrain is excluded.

Sources: `lib/edit/terrain.txt`, `src/cmd/world/cmd-interact.c`, `src/cmd/movement/cmd-movement.c`.

## Gas trap

`terrain.21`

Level: **Extended**.

**1. Info**

A known gas trap can apply a condition. Inspect it before crossing; the matching condition lesson appears only after an effect is actually observed.

Trigger: Feature 21 is on the player square or visibly adjacent and marked; secret/unrevealed terrain is excluded.

Sources: `lib/edit/terrain.txt`, `src/cmd/world/cmd-interact.c`, `src/cmd/movement/cmd-movement.c`.

## Alarm trap

`terrain.22`

Level: **Extended**.

**1. Info**

An alarm trap can attract attention. Avoid or disarm it if you want to preserve stealth. A visible trap remains active unless the game reports that it was disabled.

Trigger: Feature 22 is on the player square or visibly adjacent and marked; secret/unrevealed terrain is excluded.

Sources: `lib/edit/terrain.txt`, `src/cmd/world/cmd-interact.c`, `src/cmd/movement/cmd-movement.c`.

## Flash trap

`terrain.23`

Level: **Extended**.

**1. Info**

A flash trap can interfere with sight. Inspect the trap and your known protections before crossing or disarming.

Trigger: Feature 23 is on the player square or visibly adjacent and marked; secret/unrevealed terrain is excluded.

Sources: `lib/edit/terrain.txt`, `src/cmd/world/cmd-interact.c`, `src/cmd/movement/cmd-movement.c`.

## Caltrop field

`terrain.24`

Level: **Extended**.

**1. Info**

Caltrops make this square hazardous. Consider a route around them or an available interaction. A revealed hazard is still a hazard.

Trigger: Feature 24 is on the player square or visibly adjacent and marked; secret/unrevealed terrain is excluded.

Sources: `lib/edit/terrain.txt`, `src/cmd/world/cmd-interact.c`, `src/cmd/movement/cmd-movement.c`.

## Roost

`terrain.25`

Level: **Extended**.

**1. Info**

A roost is a special trap feature. Leaping does not bypass roosts. Inspect its public description and choose a legal route or interaction.

Trigger: Feature 25 is on the player square or visibly adjacent and marked; secret/unrevealed terrain is excluded.

Sources: `lib/edit/terrain.txt`, `src/cmd/world/cmd-interact.c`, `src/cmd/movement/cmd-movement.c`.

## Web

`terrain.26`

Level: **Extended**.

**1. Info**

A web can catch you and affect movement. Leaping does not bypass webs. Read your current state before repeating a movement command.

Trigger: Feature 26 is on the player square or visibly adjacent and marked; secret/unrevealed terrain is excluded.

Sources: `lib/edit/terrain.txt`, `src/cmd/world/cmd-interact.c`, `src/cmd/movement/cmd-movement.c`.

## Deadfall trap

`terrain.27`

Level: **Extended**.

**1. Info**

A deadfall is a physical trap. Inspect it and consider avoiding or disarming it; a failed disarm can still trigger harm.

Trigger: Feature 27 is on the player square or visibly adjacent and marked; secret/unrevealed terrain is excluded.

Sources: `lib/edit/terrain.txt`, `src/cmd/world/cmd-interact.c`, `src/cmd/movement/cmd-movement.c`.

## Discoloured spot

`terrain.28`

Level: **Extended**.

**1. Info**

A discoloured spot is a suspicious known feature. Inspect what your hero knows before choosing an interaction. The tutorial does not reveal a hidden trap subtype.

Trigger: Feature 28 is on the player square or visibly adjacent and marked; secret/unrevealed terrain is excluded.

Sources: `lib/edit/terrain.txt`, `src/cmd/world/cmd-interact.c`, `src/cmd/movement/cmd-movement.c`.

## Imprisonment trap

`terrain.29`

Level: **Extended**.

**1. Info**

An imprisonment trap can close routes around you. Inspect the area and available escape paths before crossing.

Trigger: Feature 29 is on the player square or visibly adjacent and marked; secret/unrevealed terrain is excluded.

Sources: `lib/edit/terrain.txt`, `src/cmd/world/cmd-interact.c`, `src/cmd/movement/cmd-movement.c`.

## Door

`terrain.32`

Level: **Extended**.

**1. Info**

This closed or locked door blocks the passage. Opening and unlocking are actions with their own checks and time costs. Check for nearby enemies before trying repeatedly.

Trigger: Feature 32 is on the player square or visibly adjacent and marked; secret/unrevealed terrain is excluded.

Sources: `lib/edit/terrain.txt`, `src/cmd/world/cmd-interact.c`, `src/cmd/movement/cmd-movement.c`.

## Locked door

`terrain.33`

Level: **Extended**.

**1. Info**

This closed or locked door blocks the passage. Opening and unlocking are actions with their own checks and time costs. Check for nearby enemies before trying repeatedly.

Trigger: Feature 33 is on the player square or visibly adjacent and marked; secret/unrevealed terrain is excluded.

Sources: `lib/edit/terrain.txt`, `src/cmd/world/cmd-interact.c`, `src/cmd/movement/cmd-movement.c`.

## Locked door

`terrain.34`

Level: **Extended**.

**1. Info**

This closed or locked door blocks the passage. Opening and unlocking are actions with their own checks and time costs. Check for nearby enemies before trying repeatedly.

Trigger: Feature 34 is on the player square or visibly adjacent and marked; secret/unrevealed terrain is excluded.

Sources: `lib/edit/terrain.txt`, `src/cmd/world/cmd-interact.c`, `src/cmd/movement/cmd-movement.c`.

## Locked door

`terrain.35`

Level: **Extended**.

**1. Info**

This closed or locked door blocks the passage. Opening and unlocking are actions with their own checks and time costs. Check for nearby enemies before trying repeatedly.

Trigger: Feature 35 is on the player square or visibly adjacent and marked; secret/unrevealed terrain is excluded.

Sources: `lib/edit/terrain.txt`, `src/cmd/world/cmd-interact.c`, `src/cmd/movement/cmd-movement.c`.

## Locked door

`terrain.36`

Level: **Extended**.

**1. Info**

This closed or locked door blocks the passage. Opening and unlocking are actions with their own checks and time costs. Check for nearby enemies before trying repeatedly.

Trigger: Feature 36 is on the player square or visibly adjacent and marked; secret/unrevealed terrain is excluded.

Sources: `lib/edit/terrain.txt`, `src/cmd/world/cmd-interact.c`, `src/cmd/movement/cmd-movement.c`.

## Locked door

`terrain.37`

Level: **Extended**.

**1. Info**

This closed or locked door blocks the passage. Opening and unlocking are actions with their own checks and time costs. Check for nearby enemies before trying repeatedly.

Trigger: Feature 37 is on the player square or visibly adjacent and marked; secret/unrevealed terrain is excluded.

Sources: `lib/edit/terrain.txt`, `src/cmd/world/cmd-interact.c`, `src/cmd/movement/cmd-movement.c`.

## Locked door

`terrain.38`

Level: **Extended**.

**1. Info**

This closed or locked door blocks the passage. Opening and unlocking are actions with their own checks and time costs. Check for nearby enemies before trying repeatedly.

Trigger: Feature 38 is on the player square or visibly adjacent and marked; secret/unrevealed terrain is excluded.

Sources: `lib/edit/terrain.txt`, `src/cmd/world/cmd-interact.c`, `src/cmd/movement/cmd-movement.c`.

## Locked door

`terrain.39`

Level: **Extended**.

**1. Info**

This closed or locked door blocks the passage. Opening and unlocking are actions with their own checks and time costs. Check for nearby enemies before trying repeatedly.

Trigger: Feature 39 is on the player square or visibly adjacent and marked; secret/unrevealed terrain is excluded.

Sources: `lib/edit/terrain.txt`, `src/cmd/world/cmd-interact.c`, `src/cmd/movement/cmd-movement.c`.

## Jammed door

`terrain.40`

Level: **Extended**.

**1. Info**

A jammed door does not open normally. Bashing or another available interaction may clear it, with noise and time costs. Inspect the current options rather than repeatedly issuing Open.

Trigger: Feature 40 is on the player square or visibly adjacent and marked; secret/unrevealed terrain is excluded.

Sources: `lib/edit/terrain.txt`, `src/cmd/world/cmd-interact.c`, `src/cmd/movement/cmd-movement.c`.

## Jammed door

`terrain.41`

Level: **Extended**.

**1. Info**

A jammed door does not open normally. Bashing or another available interaction may clear it, with noise and time costs. Inspect the current options rather than repeatedly issuing Open.

Trigger: Feature 41 is on the player square or visibly adjacent and marked; secret/unrevealed terrain is excluded.

Sources: `lib/edit/terrain.txt`, `src/cmd/world/cmd-interact.c`, `src/cmd/movement/cmd-movement.c`.

## Jammed door

`terrain.42`

Level: **Extended**.

**1. Info**

A jammed door does not open normally. Bashing or another available interaction may clear it, with noise and time costs. Inspect the current options rather than repeatedly issuing Open.

Trigger: Feature 42 is on the player square or visibly adjacent and marked; secret/unrevealed terrain is excluded.

Sources: `lib/edit/terrain.txt`, `src/cmd/world/cmd-interact.c`, `src/cmd/movement/cmd-movement.c`.

## Jammed door

`terrain.43`

Level: **Extended**.

**1. Info**

A jammed door does not open normally. Bashing or another available interaction may clear it, with noise and time costs. Inspect the current options rather than repeatedly issuing Open.

Trigger: Feature 43 is on the player square or visibly adjacent and marked; secret/unrevealed terrain is excluded.

Sources: `lib/edit/terrain.txt`, `src/cmd/world/cmd-interact.c`, `src/cmd/movement/cmd-movement.c`.

## Jammed door

`terrain.44`

Level: **Extended**.

**1. Info**

A jammed door does not open normally. Bashing or another available interaction may clear it, with noise and time costs. Inspect the current options rather than repeatedly issuing Open.

Trigger: Feature 44 is on the player square or visibly adjacent and marked; secret/unrevealed terrain is excluded.

Sources: `lib/edit/terrain.txt`, `src/cmd/world/cmd-interact.c`, `src/cmd/movement/cmd-movement.c`.

## Jammed door

`terrain.45`

Level: **Extended**.

**1. Info**

A jammed door does not open normally. Bashing or another available interaction may clear it, with noise and time costs. Inspect the current options rather than repeatedly issuing Open.

Trigger: Feature 45 is on the player square or visibly adjacent and marked; secret/unrevealed terrain is excluded.

Sources: `lib/edit/terrain.txt`, `src/cmd/world/cmd-interact.c`, `src/cmd/movement/cmd-movement.c`.

## Jammed door

`terrain.46`

Level: **Extended**.

**1. Info**

A jammed door does not open normally. Bashing or another available interaction may clear it, with noise and time costs. Inspect the current options rather than repeatedly issuing Open.

Trigger: Feature 46 is on the player square or visibly adjacent and marked; secret/unrevealed terrain is excluded.

Sources: `lib/edit/terrain.txt`, `src/cmd/world/cmd-interact.c`, `src/cmd/movement/cmd-movement.c`.

## Jammed door

`terrain.47`

Level: **Extended**.

**1. Info**

A jammed door does not open normally. Bashing or another available interaction may clear it, with noise and time costs. Inspect the current options rather than repeatedly issuing Open.

Trigger: Feature 47 is on the player square or visibly adjacent and marked; secret/unrevealed terrain is excluded.

Sources: `lib/edit/terrain.txt`, `src/cmd/world/cmd-interact.c`, `src/cmd/movement/cmd-movement.c`.

## Pile of rubble

`terrain.49`

Level: **Extended**.

**1. Info**

Rubble blocks a route until it is cleared. Tunnelling and other effects have their own requirements and time costs. Repeated digging advances enemies and depth pressure.

Trigger: Feature 49 is on the player square or visibly adjacent and marked; secret/unrevealed terrain is excluded.

Sources: `lib/edit/terrain.txt`, `src/cmd/world/cmd-interact.c`, `src/cmd/movement/cmd-movement.c`.

## Quartz vein

`terrain.51`

Level: **Extended**.

**1. Info**

A quartz vein is solid terrain. Inspect the tunnelling action and appropriate tool. Clearing terrain can take repeated turns.

Trigger: Feature 51 is on the player square or visibly adjacent and marked; secret/unrevealed terrain is excluded.

Sources: `lib/edit/terrain.txt`, `src/cmd/world/cmd-interact.c`, `src/cmd/movement/cmd-movement.c`.

## Granite wall

`terrain.63`

Level: **Extended**.

**1. Info**

A granite wall blocks ordinary movement. Inspect the available interactions before committing; do not assume that every visible wall can be dug through.

Trigger: Feature 63 is on the player square or visibly adjacent and marked; secret/unrevealed terrain is excluded.

Sources: `lib/edit/terrain.txt`, `src/cmd/world/cmd-interact.c`, `src/cmd/movement/cmd-movement.c`.

## Forge (exhausted)

`terrain.64`

Level: **Extended**.

**1. Info**

This forge has the uses shown in its description. A smithing proposal shows difficulty and resource costs; committing consumes the applicable forge uses and time. An exhausted forge cannot provide a fresh use.

Trigger: Feature 64 is on the player square or visibly adjacent and marked; secret/unrevealed terrain is excluded.

Sources: `lib/edit/terrain.txt`, `src/cmd/world/cmd-interact.c`, `src/cmd/movement/cmd-movement.c`.

## Forge (1 use remaining)

`terrain.65`

Level: **Extended**.

**1. Info**

This forge has the uses shown in its description. A smithing proposal shows difficulty and resource costs; committing consumes the applicable forge uses and time. An exhausted forge cannot provide a fresh use.

Trigger: Feature 65 is on the player square or visibly adjacent and marked; secret/unrevealed terrain is excluded.

Sources: `lib/edit/terrain.txt`, `src/cmd/world/cmd-interact.c`, `src/cmd/movement/cmd-movement.c`.

## Forge (2 uses remaining)

`terrain.66`

Level: **Extended**.

**1. Info**

This forge has the uses shown in its description. A smithing proposal shows difficulty and resource costs; committing consumes the applicable forge uses and time. An exhausted forge cannot provide a fresh use.

Trigger: Feature 66 is on the player square or visibly adjacent and marked; secret/unrevealed terrain is excluded.

Sources: `lib/edit/terrain.txt`, `src/cmd/world/cmd-interact.c`, `src/cmd/movement/cmd-movement.c`.

## Forge (3 uses remaining)

`terrain.67`

Level: **Extended**.

**1. Info**

This forge has the uses shown in its description. A smithing proposal shows difficulty and resource costs; committing consumes the applicable forge uses and time. An exhausted forge cannot provide a fresh use.

Trigger: Feature 67 is on the player square or visibly adjacent and marked; secret/unrevealed terrain is excluded.

Sources: `lib/edit/terrain.txt`, `src/cmd/world/cmd-interact.c`, `src/cmd/movement/cmd-movement.c`.

## Forge (4 uses remaining)

`terrain.68`

Level: **Extended**.

**1. Info**

This forge has the uses shown in its description. A smithing proposal shows difficulty and resource costs; committing consumes the applicable forge uses and time. An exhausted forge cannot provide a fresh use.

Trigger: Feature 68 is on the player square or visibly adjacent and marked; secret/unrevealed terrain is excluded.

Sources: `lib/edit/terrain.txt`, `src/cmd/world/cmd-interact.c`, `src/cmd/movement/cmd-movement.c`.

## Forge (5 uses remaining)

`terrain.69`

Level: **Extended**.

**1. Info**

This forge has the uses shown in its description. A smithing proposal shows difficulty and resource costs; committing consumes the applicable forge uses and time. An exhausted forge cannot provide a fresh use.

Trigger: Feature 69 is on the player square or visibly adjacent and marked; secret/unrevealed terrain is excluded.

Sources: `lib/edit/terrain.txt`, `src/cmd/world/cmd-interact.c`, `src/cmd/movement/cmd-movement.c`.

## Enchanted forge (exhausted)

`terrain.70`

Level: **Extended**.

**1. Info**

This forge has the uses shown in its description. A smithing proposal shows difficulty and resource costs; committing consumes the applicable forge uses and time. An exhausted forge cannot provide a fresh use.

Trigger: Feature 70 is on the player square or visibly adjacent and marked; secret/unrevealed terrain is excluded.

Sources: `lib/edit/terrain.txt`, `src/cmd/world/cmd-interact.c`, `src/cmd/movement/cmd-movement.c`.

## Enchanted forge (1 use remaining)

`terrain.71`

Level: **Extended**.

**1. Info**

This forge has the uses shown in its description. A smithing proposal shows difficulty and resource costs; committing consumes the applicable forge uses and time. An exhausted forge cannot provide a fresh use.

Trigger: Feature 71 is on the player square or visibly adjacent and marked; secret/unrevealed terrain is excluded.

Sources: `lib/edit/terrain.txt`, `src/cmd/world/cmd-interact.c`, `src/cmd/movement/cmd-movement.c`.

## Enchanted forge (2 uses remaining)

`terrain.72`

Level: **Extended**.

**1. Info**

This forge has the uses shown in its description. A smithing proposal shows difficulty and resource costs; committing consumes the applicable forge uses and time. An exhausted forge cannot provide a fresh use.

Trigger: Feature 72 is on the player square or visibly adjacent and marked; secret/unrevealed terrain is excluded.

Sources: `lib/edit/terrain.txt`, `src/cmd/world/cmd-interact.c`, `src/cmd/movement/cmd-movement.c`.

## Enchanted forge (3 uses remaining)

`terrain.73`

Level: **Extended**.

**1. Info**

This forge has the uses shown in its description. A smithing proposal shows difficulty and resource costs; committing consumes the applicable forge uses and time. An exhausted forge cannot provide a fresh use.

Trigger: Feature 73 is on the player square or visibly adjacent and marked; secret/unrevealed terrain is excluded.

Sources: `lib/edit/terrain.txt`, `src/cmd/world/cmd-interact.c`, `src/cmd/movement/cmd-movement.c`.

## Enchanted forge (4 uses remaining)

`terrain.74`

Level: **Extended**.

**1. Info**

This forge has the uses shown in its description. A smithing proposal shows difficulty and resource costs; committing consumes the applicable forge uses and time. An exhausted forge cannot provide a fresh use.

Trigger: Feature 74 is on the player square or visibly adjacent and marked; secret/unrevealed terrain is excluded.

Sources: `lib/edit/terrain.txt`, `src/cmd/world/cmd-interact.c`, `src/cmd/movement/cmd-movement.c`.

## Enchanted forge (5 uses remaining)

`terrain.75`

Level: **Extended**.

**1. Info**

This forge has the uses shown in its description. A smithing proposal shows difficulty and resource costs; committing consumes the applicable forge uses and time. An exhausted forge cannot provide a fresh use.

Trigger: Feature 75 is on the player square or visibly adjacent and marked; secret/unrevealed terrain is excluded.

Sources: `lib/edit/terrain.txt`, `src/cmd/world/cmd-interact.c`, `src/cmd/movement/cmd-movement.c`.

## Forge 'Orodruth' (exhausted)

`terrain.76`

Level: **Extended**.

**1. Info**

This forge has the uses shown in its description. A smithing proposal shows difficulty and resource costs; committing consumes the applicable forge uses and time. An exhausted forge cannot provide a fresh use.

Trigger: Feature 76 is on the player square or visibly adjacent and marked; secret/unrevealed terrain is excluded.

Sources: `lib/edit/terrain.txt`, `src/cmd/world/cmd-interact.c`, `src/cmd/movement/cmd-movement.c`.

## Forge 'Orodruth' (1 use remaining)

`terrain.77`

Level: **Extended**.

**1. Info**

This forge has the uses shown in its description. A smithing proposal shows difficulty and resource costs; committing consumes the applicable forge uses and time. An exhausted forge cannot provide a fresh use.

Trigger: Feature 77 is on the player square or visibly adjacent and marked; secret/unrevealed terrain is excluded.

Sources: `lib/edit/terrain.txt`, `src/cmd/world/cmd-interact.c`, `src/cmd/movement/cmd-movement.c`.

## Forge 'Orodruth' (2 uses remaining)

`terrain.78`

Level: **Extended**.

**1. Info**

This forge has the uses shown in its description. A smithing proposal shows difficulty and resource costs; committing consumes the applicable forge uses and time. An exhausted forge cannot provide a fresh use.

Trigger: Feature 78 is on the player square or visibly adjacent and marked; secret/unrevealed terrain is excluded.

Sources: `lib/edit/terrain.txt`, `src/cmd/world/cmd-interact.c`, `src/cmd/movement/cmd-movement.c`.

## Forge 'Orodruth' (3 uses remaining)

`terrain.79`

Level: **Extended**.

**1. Info**

This forge has the uses shown in its description. A smithing proposal shows difficulty and resource costs; committing consumes the applicable forge uses and time. An exhausted forge cannot provide a fresh use.

Trigger: Feature 79 is on the player square or visibly adjacent and marked; secret/unrevealed terrain is excluded.

Sources: `lib/edit/terrain.txt`, `src/cmd/world/cmd-interact.c`, `src/cmd/movement/cmd-movement.c`.

## Up staircase

`terrain.80`

Level: **Extended**.

**1. Info**

An up staircase leaves this level if the current minimum depth and oath rules permit. Read the actual confirmation. Ascending creates a level; it is not a promise to return to the exact map you left.

Trigger: Feature 80 is on the player square or visibly adjacent and marked; secret/unrevealed terrain is excluded.

Sources: `lib/edit/terrain.txt`, `src/cmd/world/cmd-interact.c`, `src/cmd/movement/cmd-movement.c`.

## Down staircase

`terrain.81`

Level: **Extended**.

**1. Info**

A down staircase takes you deeper and generates a new level. Check Health, light, supplies and any unfinished local objective before committing.

Trigger: Feature 81 is on the player square or visibly adjacent and marked; secret/unrevealed terrain is excluded.

Sources: `lib/edit/terrain.txt`, `src/cmd/world/cmd-interact.c`, `src/cmd/movement/cmd-movement.c`.

## Up shaft

`terrain.82`

Level: **Extended**.

**1. Info**

An up shaft can change depth by more than a normal staircase. Minimum-depth and oath restrictions still apply. Inspect the destination stated by the actual action.

Trigger: Feature 82 is on the player square or visibly adjacent and marked; secret/unrevealed terrain is excluded.

Sources: `lib/edit/terrain.txt`, `src/cmd/world/cmd-interact.c`, `src/cmd/movement/cmd-movement.c`.

## Down shaft

`terrain.83`

Level: **Extended**.

**1. Info**

A down shaft descends farther than a normal staircase. Prepare before committing: the game will generate the destination level and retain the normal action costs.

Trigger: Feature 83 is on the player square or visibly adjacent and marked; secret/unrevealed terrain is excluded.

Sources: `lib/edit/terrain.txt`, `src/cmd/world/cmd-interact.c`, `src/cmd/movement/cmd-movement.c`.

## Imprisonment — known effect

`effect.191`

Level: **Extended**.

**1. Info**

Closes and locks doors in line of sight. Inspect the room first: changing doors can affect your own retreat as well as enemies.

Trigger: Item kind 191 is aware and publicly encountered; no forced use.

Sources: `lib/edit/object.txt`, `src/use-obj.c`.

## Freedom — known effect

`effect.192`

Level: **Extended**.

**1. Info**

Reveals doors and traps in line of sight and can open or destroy them. Read what actually changed before moving.

Trigger: Item kind 192 is aware and publicly encountered; no forced use.

Sources: `lib/edit/object.txt`, `src/use-obj.c`.

## Light — known effect

`effect.193`

Level: **Extended**.

**1. Info**

Lights a radius of 7 squares and the current room, and stuns light-sensitive creatures in the affected area.

Trigger: Item kind 193 is aware and publicly encountered; no forced use.

Sources: `lib/edit/object.txt`, `src/use-obj.c`.

## Sanctity — known effect

`effect.195`

Level: **Extended**.

**1. Info**

Cleanses a chosen item from equipment, Pack or floor, removing curses where the rules allow. Curse Breaking also permits breaking qualifying jinxed affixes. Select a real eligible target before committing.

Trigger: Item kind 195 is aware and publicly encountered; no forced use.

Sources: `lib/edit/object.txt`, `src/use-obj.c`.

## Understanding — known effect

`effect.196`

Level: **Extended**.

**1. Info**

Identifies a chosen item. Cancelling the selection does not consume the use. The result applies to that selected item.

Trigger: Item kind 196 is aware and publicly encountered; no forced use.

Sources: `lib/edit/object.txt`, `src/use-obj.c`.

## Revelations — known effect

`effect.197`

Level: **Extended**.

**1. Info**

Reveals surrounding terrain. Knowledge of the map is not the same as currently seeing every creature on it.

Trigger: Item kind 197 is aware and publicly encountered; no forced use.

Sources: `lib/edit/object.txt`, `src/use-obj.c`.

## Treasures — known effect

`effect.198`

Level: **Extended**.

**1. Info**

Detects items on the level. The result reveals their location under the detection rules; it does not put them in your inventory.

Trigger: Item kind 198 is aware and publicly encountered; no forced use.

Sources: `lib/edit/object.txt`, `src/use-obj.c`.

## Foes — known effect

`effect.199`

Level: **Extended**.

**1. Info**

Detects monsters on the level. Detection is different from normal current line of sight and from knowing every ability of a creature.

Trigger: Item kind 199 is aware and publicly encountered; no forced use.

Sources: `lib/edit/object.txt`, `src/use-obj.c`.

## Slumber — known effect

`effect.200`

Level: **Extended**.

**1. Info**

Attempts to make monsters in line of sight sleepy. The effect is resisted; it does not promise that every foe will stop acting.

Trigger: Item kind 200 is aware and publicly encountered; no forced use.

Sources: `lib/edit/object.txt`, `src/use-obj.c`.

## Majesty — known effect

`effect.201`

Level: **Extended**.

**1. Info**

Attempts to frighten monsters in line of sight. Fear changes morale; whether it takes effect depends on the target and the effect check.

Trigger: Item kind 201 is aware and publicly encountered; no forced use.

Sources: `lib/edit/object.txt`, `src/use-obj.c`.

## Self Knowledge — known effect

`effect.202`

Level: **Extended**.

**1. Info**

Shows your current status effects and character traits. Each use can also reveal one active curse. It does not remove that curse.

Trigger: Item kind 202 is aware and publicly encountered; no forced use.

Sources: `lib/edit/object.txt`, `src/use-obj.c`.

## Warding — known effect

`effect.203`

Level: **Extended**.

**1. Info**

Creates a glyph of warding where the placement rules permit. The glyph makes crossing difficult for opponents; it is not an impenetrable wall.

Trigger: Item kind 203 is aware and publicly encountered; no forced use.

Sources: `lib/edit/object.txt`, `src/use-obj.c`.

## Dismay — known effect

`effect.204`

Level: **Extended**.

**1. Info**

Attempts to confuse monsters in line of sight. Check what actually happened before relying on a target being confused.

Trigger: Item kind 204 is aware and publicly encountered; no forced use.

Sources: `lib/edit/object.txt`, `src/use-obj.c`.

## Recharging — known effect

`effect.206`

Level: **Extended**.

**1. Info**

Partially recharges a staff. Inspect the selected staff and the channeling/recharging rules before committing; restoring charges is separate from using the staff's effect.

Trigger: Item kind 206 is aware and publicly encountered; no forced use.

Sources: `lib/edit/object.txt`, `src/use-obj.c`.

## Summoning — known effect

`effect.210`

Level: **Extended**.

**1. Info**

Summons monsters to the current level's stairs. This can create danger. No tutorial requires using it just to demonstrate the effect.

Trigger: Item kind 210 is aware and publicly encountered; no forced use.

Sources: `lib/edit/object.txt`, `src/use-obj.c`.

## Shadows — known effect

`effect.211`

Level: **Extended**.

**1. Info**

Darkens a radius of 7 squares and the current room. For that round's stealth checks it adds your Will to Stealth. Darkness also changes what you can see.

Trigger: Item kind 211 is aware and publicly encountered; no forced use.

Sources: `lib/edit/object.txt`, `src/use-obj.c`.

## Terror — known effect

`effect.240`

Level: **Extended**.

**1. Info**

Attempts to frighten creatures affected by the horn. Choose the direction and check your Voice cost. Its sound may alert other creatures.

Trigger: Item kind 240 is aware and publicly encountered; no forced use.

Sources: `lib/edit/object.txt`, `src/use-obj.c`.

## Thunder — known effect

`effect.241`

Level: **Extended**.

**1. Info**

Attempts to stun affected creatures. A stun attempt is not guaranteed to stop every enemy; inspect the actual result before advancing.

Trigger: Item kind 241 is aware and publicly encountered; no forced use.

Sources: `lib/edit/object.txt`, `src/use-obj.c`.

## Force — known effect

`effect.242`

Level: **Extended**.

**1. Info**

Pushes affected enemies back when the effect succeeds. Check the direction and terrain behind the target; the blast spends Voice.

Trigger: Item kind 242 is aware and publicly encountered; no forced use.

Sources: `lib/edit/object.txt`, `src/use-obj.c`.

## Blasting — known effect

`effect.243`

Level: **Extended**.

**1. Info**

Can shatter stone. It can be aimed along a compass direction, up or down through the normal targeting choices. Inspect the area and cost before committing a destructive effect.

Trigger: Item kind 243 is aware and publicly encountered; no forced use.

Sources: `lib/edit/object.txt`, `src/use-obj.c`.

## Warning — known effect

`effect.250`

Level: **Extended**.

**1. Info**

Raises a loud warning and challenges nearby foes. It draws attention; do not use it merely because another horn would have been helpful.

Trigger: Item kind 250 is aware and publicly encountered; no forced use.

Sources: `lib/edit/object.txt`, `src/use-obj.c`.

## Miruvor — known effect

`effect.313`

Level: **Extended**.

**1. Info**

Cures stun, confusion, hallucination, poison, blindness and fear; halves current bleeding; restores 20 + floor(20% of maximum Health) and all Voice. Medicine equipment can increase the healing.

Trigger: Item kind 313 is aware and publicly encountered; no forced use.

Sources: `lib/edit/object.txt`, `src/use-obj.c`.

## Orcish Liquor — known effect

`effect.315`

Level: **Extended**.

**1. Info**

Removes fear and heals 6 + floor(8% of maximum Health), but can add 2d4 stun severity, subject to stun protection. Medicine equipment can increase healing. The drawback matters even when healing is useful.

Trigger: Item kind 315 is aware and publicly encountered; no forced use.

Sources: `lib/edit/object.txt`, `src/use-obj.c`.

## Esgalduin — known effect

`effect.316`

Level: **Extended**.

**1. Info**

Grants +10 Perception for 20d4 turns and restores one quarter of maximum Voice. The bonus is temporary.

Trigger: Item kind 316 is aware and publicly encountered; no forced use.

Sources: `lib/edit/object.txt`, `src/use-obj.c`.

## Clarity — known effect

`effect.317`

Level: **Extended**.

**1. Info**

Cures stun, confusion and hallucination, and ends rage. It is a condition remedy, not a general Health potion.

Trigger: Item kind 317 is aware and publicly encountered; no forced use.

Sources: `lib/edit/object.txt`, `src/use-obj.c`.

## Healing — known effect

`effect.318`

Level: **Extended**.

**1. Info**

Halves current bleeding and restores 15 + floor(16% of maximum Health). Medicine equipment can increase healing. Remaining bleeding can still cause later damage.

Trigger: Item kind 318 is aware and publicly encountered; no forced use.

Sources: `lib/edit/object.txt`, `src/use-obj.c`.

## Voice — known effect

`effect.319`

Level: **Extended**.

**1. Info**

Restores Voice. It does not cure poison, stop bleeding or restore Health.

Trigger: Item kind 319 is aware and publicly encountered; no forced use.

Sources: `lib/edit/object.txt`, `src/use-obj.c`.

## true Sight — known effect

`effect.320`

Level: **Extended**.

**1. Info**

Cures blindness and hallucination and grants resistance to both plus improved invisible-creature detection for 10d4 turns.

Trigger: Item kind 320 is aware and publicly encountered; no forced use.

Sources: `lib/edit/object.txt`, `src/use-obj.c`.

## Antidote — known effect

`effect.321`

Level: **Extended**.

**1. Info**

Removes current poison. It does not restore the Health poison has already taken.

Trigger: Item kind 321 is aware and publicly encountered; no forced use.

Sources: `lib/edit/object.txt`, `src/use-obj.c`.

## Quickness — known effect

`effect.322`

Level: **Extended**.

**1. Info**

Grants +1 speed for 10d4 turns. This can offset slowing without removing the slow condition.

Trigger: Item kind 322 is aware and publicly encountered; no forced use.

Sources: `lib/edit/object.txt`, `src/use-obj.c`.

## Elemental Resistance — known effect

`effect.323`

Level: **Extended**.

**1. Info**

Grants temporary fire and cold resistance for 20d4 turns. It does not grant poison resistance.

Trigger: Item kind 323 is aware and publicly encountered; no forced use.

Sources: `lib/edit/object.txt`, `src/use-obj.c`.

## Strength — known effect

`effect.327`

Level: **Extended**.

**1. Info**

If Strength is drained by at least 3 points, this use restores 3 points instead of granting the temporary buff. Otherwise it grants +3 temporary Strength for 20d4 turns; when that buff expires it restores up to 3 drained points. Check the current drain before choosing it.

Trigger: Item kind 327 is aware and publicly encountered; no forced use.

Sources: `lib/edit/object.txt`, `src/use-obj.c`, `src/player/effects.c`.

## Dexterity — known effect

`effect.328`

Level: **Extended**.

**1. Info**

If Dexterity is drained by at least 3 points, this use restores 3 points instead of granting the temporary buff. Otherwise it grants +3 temporary Dexterity for 20d4 turns; when that buff expires it restores up to 3 drained points. Check the current drain before choosing it.

Trigger: Item kind 328 is aware and publicly encountered; no forced use.

Sources: `lib/edit/object.txt`, `src/use-obj.c`, `src/player/effects.c`.

## Constitution — known effect

`effect.329`

Level: **Extended**.

**1. Info**

If Constitution is drained by at least 3 points, this use restores 3 points instead of granting the temporary buff. Otherwise it grants +3 temporary Constitution for 20d4 turns; when that buff expires it restores up to 3 drained points. Check the current drain before choosing it.

Trigger: Item kind 329 is aware and publicly encountered; no forced use.

Sources: `lib/edit/object.txt`, `src/use-obj.c`, `src/player/effects.c`.

## Grace — known effect

`effect.330`

Level: **Extended**.

**1. Info**

If Grace is drained by at least 3 points, this use restores 3 points instead of granting the temporary buff. Otherwise it grants +3 temporary Grace for 20d4 turns; when that buff expires it restores up to 3 drained points. Check the current drain before choosing it.

Trigger: Item kind 330 is aware and publicly encountered; no forced use.

Sources: `lib/edit/object.txt`, `src/use-obj.c`, `src/player/effects.c`.

## Slowness — known effect

`effect.343`

Level: **Extended**.

**1. Info**

Applies -1 speed for 10d4 turns. A known harmful potion is not a required tutorial action.

Trigger: Item kind 343 is aware and publicly encountered; no forced use.

Sources: `lib/edit/object.txt`, `src/use-obj.c`.

## Poison — known effect

`effect.344`

Level: **Extended**.

**1. Info**

Inflicts 5d4 poison severity. Poison damage then happens through the condition's damage ticks.

Trigger: Item kind 344 is aware and publicly encountered; no forced use.

Sources: `lib/edit/object.txt`, `src/use-obj.c`.

## Blindness — known effect

`effect.345`

Level: **Extended**.

**1. Info**

Can blind you for 10d4 turns, subject to the protection checks. More light does not cure blindness.

Trigger: Item kind 345 is aware and publicly encountered; no forced use.

Sources: `lib/edit/object.txt`, `src/use-obj.c`.

## Confusion — known effect

`effect.346`

Level: **Extended**.

**1. Info**

Can confuse you for 5d4 turns, subject to the protection checks. Confusion interferes with directions and aiming.

Trigger: Item kind 346 is aware and publicly encountered; no forced use.

Sources: `lib/edit/object.txt`, `src/use-obj.c`.

## Awkwardness — known effect

`effect.348`

Level: **Extended**.

**1. Info**

Drains Dexterity by 1. This is attribute drain rather than a short-lived bonus wearing off.

Trigger: Item kind 348 is aware and publicly encountered; no forced use.

Sources: `lib/edit/object.txt`, `src/use-obj.c`.

## Disconnection — known effect

`effect.350`

Level: **Extended**.

**1. Info**

Drains Grace by 1. Restoration or a matching restoration effect addresses the drain.

Trigger: Item kind 350 is aware and publicly encountered; no forced use.

Sources: `lib/edit/object.txt`, `src/use-obj.c`.

## Rage — known effect

`effect.380`

Level: **Extended**.

**1. Info**

Grants rage for 10d4 turns: +1 Strength/Constitution, -1 Dexterity/Grace, a special melee attack and fear resistance. Rage restricts awareness and Stealth. It also supplies ordinary herb nourishment.

Trigger: Item kind 380 is aware and publicly encountered; no forced use.

Sources: `lib/edit/object.txt`, `src/use-obj.c`.

## Sustenance — known effect

`effect.381`

Level: **Extended**.

**1. Info**

Provides about 2,000 ordinary turns of nourishment. Actual consumption rate can change with your current modifiers.

Trigger: Item kind 381 is aware and publicly encountered; no forced use.

Sources: `lib/edit/object.txt`, `src/use-obj.c`.

## Terror — known effect

`effect.382`

Level: **Extended**.

**1. Info**

If fear protection does not prevent the effect, causes fear for 10d4 turns and speed for 5d4 turns. Preventing the fear also prevents this speed benefit. It provides ordinary herb nourishment.

Trigger: Item kind 382 is aware and publicly encountered; no forced use.

Sources: `lib/edit/object.txt`, `src/use-obj.c`.

## Healing — known effect

`effect.383`

Level: **Extended**.

**1. Info**

Halves bleeding and heals 11 + floor(12% of maximum Health), increased by Medicine equipment. It also provides ordinary herb nourishment.

Trigger: Item kind 383 is aware and publicly encountered; no forced use.

Sources: `lib/edit/object.txt`, `src/use-obj.c`.

## Restoration — known effect

`effect.384`

Level: **Extended**.

**1. Info**

Restores every attribute by up to 3 drained points. It also provides ordinary herb nourishment. It does not remove unrelated equipment penalties.

Trigger: Item kind 384 is aware and publicly encountered; no forced use.

Sources: `lib/edit/object.txt`, `src/use-obj.c`.

## Emptiness — known effect

`effect.385`

Level: **Extended**.

**1. Info**

Reduces nourishment by about 1,000 ordinary turns. This can worsen hunger; it is not a remedy.

Trigger: Item kind 385 is aware and publicly encountered; no forced use.

Sources: `lib/edit/object.txt`, `src/use-obj.c`.

## Visions — known effect

`effect.386`

Level: **Extended**.

**1. Info**

Causes hallucination for 80d4 turns but removes blindness. This tradeoff is different from a clean sight remedy.

Trigger: Item kind 386 is aware and publicly encountered; no forced use.

Sources: `lib/edit/object.txt`, `src/use-obj.c`.

## Entrancement — known effect

`effect.387`

Level: **Extended**.

**1. Info**

Can entrance you for 10d4 turns. While entranced you cannot choose ordinary actions; this tutorial never requires eating it.

Trigger: Item kind 387 is aware and publicly encountered; no forced use.

Sources: `lib/edit/object.txt`, `src/use-obj.c`.

## Weakness — known effect

`effect.388`

Level: **Extended**.

**1. Info**

Drains Strength by 1 and provides ordinary herb nourishment. Nourishment does not cancel its attribute drawback.

Trigger: Item kind 388 is aware and publicly encountered; no forced use.

Sources: `lib/edit/object.txt`, `src/use-obj.c`.

## Sickness — known effect

`effect.389`

Level: **Extended**.

**1. Info**

Drains Constitution by 1 and provides ordinary herb nourishment. Check maximum Health after attribute changes.

Trigger: Item kind 389 is aware and publicly encountered; no forced use.

Sources: `lib/edit/object.txt`, `src/use-obj.c`.

## Piece of Dark Bread — known effect

`effect.399`

Level: **Extended**.

**1. Info**

Provides about 1,500 ordinary turns of nourishment.

Trigger: Item kind 399 is aware and publicly encountered; no forced use.

Sources: `lib/edit/object.txt`, `src/use-obj.c`.

## Strip of Dried Meat — known effect

`effect.400`

Level: **Extended**.

**1. Info**

Provides about 2,000 ordinary turns of nourishment.

Trigger: Item kind 400 is aware and publicly encountered; no forced use.

Sources: `lib/edit/object.txt`, `src/use-obj.c`.

## Fragment of Lembas — known effect

`effect.401`

Level: **Extended**.

**1. Info**

Provides about 3,000 ordinary turns of nourishment and restores 1 drained Grace.

Trigger: Item kind 401 is aware and publicly encountered; no forced use.

Sources: `lib/edit/object.txt`, `src/use-obj.c`.

## Throwing weapon: ready

`item.throwing.ready`

Level: **Normal**.

**1. Action**

Ready the throwing weapon using the available equipment action. Readying and choosing it as the active weapon are separate steps.

Required action: `ready`; subject: `throwing`.

Trigger: The relevant item is available and the ready action has a legal current target and all required handling resources.

Sources: `src/tutorial/tutorial-game.c`, `src/player/player-active-weapon.c`, `src/cmd/combat/cmd-ranged.c`.

## Throwing weapon: active

`item.throwing.active`

Level: **Normal**.

**1. Action**

Choose the readied throwing weapon as your active weapon. Inspect the resulting hands, shield and ammunition arrangement. Only an actual active-weapon change completes this step.

Required action: `change-active`; subject: `throwing`.

Trigger: The relevant item is available and the active action has a legal current target and all required handling resources.

Sources: `src/tutorial/tutorial-game.c`, `src/player/player-active-weapon.c`, `src/cmd/combat/cmd-ranged.c`.

## Throwing weapon: use

`item.throwing.use`

Level: **Normal**.

**1. Action**

Aim at a currently legal visible hostile and throw the weapon. Check the path, range and oath first. A committed miss still counts; cancellation does not. Skip to save ammunition or choose a different tactic.

Required action: `throw`; subject: `throwing`.

Trigger: The relevant item is available and the use action has a legal current target and all required handling resources.

Sources: `src/tutorial/tutorial-game.c`, `src/player/player-active-weapon.c`, `src/cmd/combat/cmd-ranged.c`.

## Bow: ready

`item.bow.ready`

Level: **Normal**.

**1. Action**

Ready the bow using the available equipment action. Readying and choosing it as the active weapon are separate steps.

Required action: `ready`; subject: `bow`.

Trigger: The relevant item is available and the ready action has a legal current target and all required handling resources.

Sources: `src/tutorial/tutorial-game.c`, `src/player/player-active-weapon.c`, `src/cmd/combat/cmd-ranged.c`.

## Bow: active

`item.bow.active`

Level: **Normal**.

**1. Action**

Choose the readied bow as your active weapon. Inspect the resulting hands, shield and ammunition arrangement. Only an actual active-weapon change completes this step.

Required action: `change-active`; subject: `bow`.

Trigger: The relevant item is available and the active action has a legal current target and all required handling resources.

Sources: `src/tutorial/tutorial-game.c`, `src/player/player-active-weapon.c`, `src/cmd/combat/cmd-ranged.c`.

## Bow: use

`item.bow.use`

Level: **Normal**.

**1. Action**

Aim at a currently legal visible hostile and fire one arrow. Check the path, range and oath first. A committed miss still counts; cancellation does not. Skip to save ammunition or choose a different tactic.

Required action: `fire`; subject: `bow`.

Trigger: The relevant item is available and the use action has a legal current target and all required handling resources.

Sources: `src/tutorial/tutorial-game.c`, `src/player/player-active-weapon.c`, `src/cmd/combat/cmd-ranged.c`.

## Select active arrows

`item.arrows.active`

Level: **Normal**.

**1. Action**

Select a different available arrow stack for your active bow. Changing only arrows is free; changing other active equipment follows its own cost rules.

Required action: `change-active`; subject: `arrows`.

Trigger: A compatible alternate arrow stack is available and a bow is active.

Sources: `src/player/player-active-weapon.c`.

## Equip armour

`item.armour.equip`

Level: **Normal**.

**1. Action**

Use the selected armour's Equip action. Check the resulting Protection, Evasion, penalties and oath restrictions before confirming. This is a real equipment change.

Required action: `equip`; subject: `armour`.

Trigger: Known uncursed armour can fill an empty equipment slot and the active oath permits equipping it; the player may perform the real equip action or skip.

Sources: `src/cmd/item/cmd-item-core.c`, `src/tutorial/tutorial-game.c`.

## Pack storage

`storage.pack`

Level: **Normal**.

**1. Info**

The Pack has its own volume limit. Weight also contributes to your overall load. Read both limits when selecting what to carry; a small but heavy object and a bulky light object create different constraints.

Trigger: The corresponding public state transition or explicit player action has occurred; storage.pack.

Sources: `src/tutorial/tutorial-world.c`, `src/tutorial/tutorial-game.c`.

## Harness capacity

`storage.harness`

Level: **Normal**.

**1. Info**

The Harness has a separate volume limit from the Pack. Readied gear can be reached under its own action rules. Moving an item between storage locations does not create unlimited total carrying capacity.

Trigger: The corresponding public state transition or explicit player action has occurred; storage.harness.

Sources: `src/tutorial/tutorial-world.c`, `src/tutorial/tutorial-game.c`.

## Quiver capacity

`storage.quiver`

Level: **Normal**.

**1. Info**

The Quiver holds arrows under its own capacity rules. Excess arrows may need Pack space. A stack can be picked up partially when only part fits; inspect the quantity actually collected.

Trigger: The corresponding public state transition or explicit player action has occurred; storage.quiver.

Sources: `src/tutorial/tutorial-world.c`, `src/tutorial/tutorial-game.c`.

## Carried weight

`storage.weight`

Level: **Normal**.

**1. Info**

Carried weight is separate from Pack and Harness volume. Your current load can affect action costs and movement. Check the displayed burden and the actual item weight before taking more.

Trigger: The corresponding public state transition or explicit player action has occurred; storage.weight.

Sources: `src/tutorial/tutorial-world.c`, `src/tutorial/tutorial-game.c`.

## Reaching into the Pack

`storage.pack_access`

Level: **Normal**.

**1. Info**

Accessing the Pack can spend time before the item action. Enemies and the world can act during that cost. Inspect and ready urgent gear before danger when you have the opportunity.

Trigger: The corresponding public state transition or explicit player action has occurred; storage.pack_access.

Sources: `src/tutorial/tutorial-game.c`.

## Pack access interrupted

`storage.pack_interrupted`

Level: **Normal**.

**1. Info**

The Pack transaction was interrupted. Check which action actually completed before issuing another command. The tutorial does not finish a cancelled equipment change or replay the old selection.

Trigger: The corresponding public state transition or explicit player action has occurred; storage.pack_interrupted.

Sources: `src/tutorial/tutorial-game.c`.

## Only part of the stack fits

`storage.partial_pickup`

Level: **Normal**.

**1. Info**

A partial stack was collected because of the applicable storage limit. Check the amount in your inventory and what remains on the floor. A successful partial pickup is different from taking the whole stack.

Trigger: The corresponding public state transition or explicit player action has occurred; storage.partial_pickup.

Sources: `src/tutorial/tutorial-game.c`.

## Resting spends turns

`world.rest`

Level: **Normal**.

**1. Info**

Rest advances game time repeatedly. Other creatures can act and minimum depth can advance. Poison, bleeding and starvation prevent ordinary Health regeneration; singing prevents Voice regeneration.

Trigger: The corresponding public state transition or explicit player action has occurred; world.rest.

Sources: `src/tutorial/tutorial-game.c`.

## Running and repeated movement

`world.run`

Level: **Normal**.

**1. Info**

Running issues repeated real moves until interrupted. New threats, terrain and other interruptions can stop it. Reading this lesson pauses input automation; it does not make the route safe or reveal unseen squares.

Trigger: The corresponding public state transition or explicit player action has occurred; world.run.

Sources: `src/tutorial/tutorial-game.c`.

## A known trap

`world.trap`

Level: **Normal**.

**1. Info**

A trap has been revealed or triggered. Read the actual effect and current condition before acting again. Detecting a trap does not remove it. Disarming, avoiding and leaping have different requirements and costs.

Trigger: The corresponding public state transition or explicit player action has occurred; world.trap.

Sources: `src/tutorial/tutorial-game.c`.

## At a forge

`world.forge`

Level: **Normal**.

**1. Info**

A forge offers a limited number of uses. Inspect the proposed item's difficulty, materials, time and other costs before creating it. Smithing abilities and quest rules can change what is possible.

Trigger: The corresponding public state transition or explicit player action has occurred; world.forge.

Sources: `src/tutorial/tutorial-game.c`.

## Fuel is running low

`world.light_low`

Level: **Normal**.

**1. Info**

Your current light is low on fuel. Check a compatible refuelling or replacement option before it goes out. Low fuel is different from a darkness effect or blindness.

Trigger: The corresponding public state transition or explicit player action has occurred; world.light_low.

Sources: `src/tutorial/tutorial-world.c`, `src/tutorial/tutorial-game.c`.

## Your light has gone out

`world.light_out`

Level: **Normal**.

**1. Info**

The current light has no fuel. Inspect a replacement or refuelling option. Remembered terrain is not current visibility; a light source in storage does not automatically become equipped.

Trigger: The corresponding public state transition or explicit player action has occurred; world.light_out.

Sources: `src/tutorial/tutorial-world.c`, `src/tutorial/tutorial-game.c`.

## Train a skill

`advancement.skills`

Level: **Normal**.

**1. Decision**

The next base skill rank costs 100 times the new rank in XP. Attributes, equipment and temporary effects contribute separately. Inspect the purchase proposal; Continue returns to the real choice without spending XP.

Trigger: The corresponding public state transition or explicit player action has occurred; advancement.skills.

Sources: `src/tutorial/tutorial-game.c`.

## A song is available

`advancement.song`

Level: **Normal**.

**1. Decision**

Read the song's current effect and Voice cost, then choose when it is useful. Singing is a separate action and may violate an oath. No tutorial buys or starts a song automatically.

Trigger: The corresponding public state transition or explicit player action has occurred; advancement.song.

Sources: `src/tutorial/tutorial-game.c`.

## Quest progress

`quest.progress`

Level: **Normal**.

**1. Info**

{detail} Read the current objective and progress in Quests. Progress toward a target is not the same as completing the quest or collecting its reward.

Trigger: The corresponding public state transition or explicit player action has occurred; quest.progress.

Sources: `src/tutorial/tutorial-world.c`, `src/tutorial/tutorial-game.c`.

## Quest completed

`quest.completed`

Level: **Normal**.

**1. Info**

{detail} Read the actual reward and any newly granted ability or unlocked oath. Completing this quest does not silently accept another oath or quest.

Trigger: The corresponding public state transition or explicit player action has occurred; quest.completed.

Sources: `src/tutorial/tutorial-world.c`, `src/tutorial/tutorial-game.c`.

## Quest failed

`quest.failed`

Level: **Normal**.

**1. Info**

{detail} Read the recorded reason and resulting restrictions. This explanation does not undo the action or restore the quest; Tale progress and another hero may have different rules.

Trigger: The corresponding public state transition or explicit player action has occurred; quest.failed.

Sources: `src/tutorial/tutorial-world.c`, `src/tutorial/tutorial-game.c`.

## A curse is revealed

`tale.curse`

Level: **Normal**.

**1. Info**

{detail} Inspect Known Curses for the effect actually revealed and its current stack. A Tale curse is distinct from an equipment curse. Reading the tutorial neither removes it nor reveals other hidden curses.

Trigger: The corresponding public state transition or explicit player action has occurred; tale.curse.

Sources: `src/tutorial/tutorial-world.c`, `src/tutorial/tutorial-game.c`.

## A blessing is available

`tale.blessing`

Level: **Normal**.

**1. Decision**

{detail} Read the available benefit, cost and any limits before making the real choice. A blessing explanation does not spend blessing points or select a reward.

Trigger: The corresponding public state transition or explicit player action has occurred; tale.blessing.

Sources: `src/tutorial/tutorial-world.c`, `src/tutorial/tutorial-game.c`.

## An oath was broken

`tale.oath_break`

Level: **Normal**.

**1. Info**

{detail} The action has already broken the oath and its normal consequences apply. Read your current oath, lost benefit and revealed curse. Skipping this explanation does not reverse them.

Trigger: The corresponding public state transition or explicit player action has occurred; tale.oath_break.

Sources: `src/tutorial/tutorial-world.c`, `src/tutorial/tutorial-game.c`.

## A Silmaril

`tale.silmaril`

Level: **Normal**.

**1. Info**

{detail} Read the current objective, burden and escape conditions. Obtaining a Silmaril changes the story, but it does not teleport you to safety or automatically complete every Tale requirement.

Trigger: The corresponding public state transition or explicit player action has occurred; tale.silmaril.

Sources: `src/tutorial/tutorial-world.c`, `src/tutorial/tutorial-game.c`.

## Leaving Angband

`tale.escape`

Level: **Normal**.

**1. Info**

{detail} The exit result belongs to this hero and the current Tale. Read the actual recovered Silmarils, score and remaining Tale objective; escaping once is not necessarily the end of the Tale.

Trigger: The corresponding public state transition or explicit player action has occurred; tale.escape.

Sources: `src/game/game-lifecycle.c`, `src/tutorial/tutorial-game.c`, `src/tutorial/tutorial-world.c`.

## A hero has fallen

`tale.death`

Level: **Normal**.

**1. Info**

{detail} This hero's life has ended. The Tale can retain its progress, consequences and learned tutorials across another hero, subject to its loss condition. This explanation cannot revive the hero.

Trigger: The corresponding public state transition or explicit player action has occurred; tale.death.

Sources: `src/game/game-lifecycle.c`, `src/tutorial/tutorial-game.c`, `src/tutorial/tutorial-world.c`.

## A truce

`tale.truce`

Level: **Normal**.

**1. Info**

{detail} A truce changes which actions are legal or tolerated. Read the current agreement before attacking, stealing or making another irreversible choice. The tutorial does not break the truce for you.

Trigger: The corresponding public state transition or explicit player action has occurred; tale.truce.

Sources: `src/tutorial/tutorial-world.c`, `src/tutorial/tutorial-game.c`.

## Roomy halls

`world.partition.1`

Level: **Extended**.

**1. Info**

{detail} Rooms and connecting passages create distinct lines of sight and doorways. Observe the actual exits before planning a retreat; the regional description does not reveal unexplored contents.

Trigger: The player enters and publicly discovers partition kind 1.

Sources: `src/tutorial/tutorial-world.c`, `src/externs.h`, `src/level-generation/level-generation-internal.h`, `src/tutorial/tutorial-game.c`.

## Caverns

`world.partition.2`

Level: **Extended**.

**1. Info**

{detail} Natural caverns can offer irregular sight lines and routes. Inspect the visible terrain before moving. A cave theme does not identify hidden creatures or items.

Trigger: The player enters and publicly discovers partition kind 2.

Sources: `src/tutorial/tutorial-world.c`, `src/externs.h`, `src/level-generation/level-generation-internal.h`, `src/tutorial/tutorial-game.c`.

## Ruined halls

`world.partition.3`

Level: **Extended**.

**1. Info**

{detail} Ruined halls contain broken structures and irregular routes. Read the observed terrain and available interactions rather than assuming every doorway works normally.

Trigger: The player enters and publicly discovers partition kind 3.

Sources: `src/tutorial/tutorial-world.c`, `src/externs.h`, `src/level-generation/level-generation-internal.h`, `src/tutorial/tutorial-game.c`.

## Labyrinth

`world.partition.4`

Level: **Extended**.

**1. Info**

{detail} A labyrinth creates narrow, winding routes. Keep track of known intersections and a retreat route. Opening the map does not reveal unexplored passages.

Trigger: The player enters and publicly discovers partition kind 4.

Sources: `src/tutorial/tutorial-world.c`, `src/externs.h`, `src/level-generation/level-generation-internal.h`, `src/tutorial/tutorial-game.c`.

## Chasm region

`world.partition.5`

Level: **Extended**.

**1. Info**

{detail} Chasms divide the routes in this area. A fall can send you deeper. Inspect each crossing and any leap requirements; the regional lesson never requires stepping into a chasm.

Trigger: The player enters and publicly discovers partition kind 5.

Sources: `src/tutorial/tutorial-world.c`, `src/externs.h`, `src/level-generation/level-generation-internal.h`, `src/tutorial/tutorial-game.c`.

## Great cavern

`world.partition.6`

Level: **Extended**.

**1. Info**

{detail} A great cavern opens broad lines of sight and movement. Distant visible threats and available cover matter; inspect what is actually revealed before crossing open ground.

Trigger: The player enters and publicly discovers partition kind 6.

Sources: `src/tutorial/tutorial-world.c`, `src/externs.h`, `src/level-generation/level-generation-internal.h`, `src/tutorial/tutorial-game.c`.

## Fire cavern

`world.partition.fire`

Level: **Extended**.

**1. Info**

{detail} This cavern's revealed theme suggests fire dangers. Inspect actual terrain, known resistance and observed creatures. The theme is not proof of a specific unseen enemy or a guarantee that your equipment protects against every hazard.

Trigger: A discovered great cavern has the corresponding public elemental cave type.

Sources: `src/tutorial/tutorial-world.c`, `src/externs.h`, `src/tutorial/tutorial-game.c`.

## Cold cavern

`world.partition.cold`

Level: **Extended**.

**1. Info**

{detail} This cavern's revealed theme suggests cold dangers. Inspect actual terrain, known resistance and observed creatures. The theme is not proof of a specific unseen enemy or a guarantee that your equipment protects against every hazard.

Trigger: A discovered great cavern has the corresponding public elemental cave type.

Sources: `src/tutorial/tutorial-world.c`, `src/externs.h`, `src/tutorial/tutorial-game.c`.

## Poison cavern

`world.partition.poison`

Level: **Extended**.

**1. Info**

{detail} This cavern's revealed theme suggests poison dangers. Inspect actual terrain, known resistance and observed creatures. The theme is not proof of a specific unseen enemy or a guarantee that your equipment protects against every hazard.

Trigger: A discovered great cavern has the corresponding public elemental cave type.

Sources: `src/tutorial/tutorial-world.c`, `src/externs.h`, `src/tutorial/tutorial-game.c`.

## Tulkas the Strong

`quest.1`

Level: **Extended**.

**1. Decision**

{detail} Hunt the named creature and return for the actual reward. Read the revealed target in Quests; this lesson does not name a creature before the quest does.

Trigger: Quest 1 is publicly offered or accepted; only its revealed text is supplied in context.

Sources: `src/tutorial/tutorial-world.c`, `lib/edit/quest.txt`, `src/quest/quest-status.c`.

## Aulë the Smith

`quest.2`

Level: **Extended**.

**1. Decision**

{detail} At Aulë's forge, create a work meeting the current quest requirements. Inspect the proposal and costs before committing. A normal forged item does not automatically satisfy the trial.

Trigger: Quest 2 is publicly offered or accepted; only its revealed text is supplied in context.

Sources: `src/tutorial/tutorial-world.c`, `lib/edit/quest.txt`, `src/quest/quest-status.c`.

## Mandos the Doomsman

`quest.3`

Level: **Extended**.

**1. Decision**

{detail} Follow the revealed objective concerning the bound spirit. Read the target and completion condition in Quests before committing an attack or leaving the area.

Trigger: Quest 3 is publicly offered or accepted; only its revealed text is supplied in context.

Sources: `src/tutorial/tutorial-world.c`, `lib/edit/quest.txt`, `src/quest/quest-status.c`.

## Nienna, Lady of Pity

`quest.4`

Level: **Extended**.

**1. Decision**

{detail} Find the downward stair without taking a life under the current quest rules. Review the restriction before attacking. An automatic follow-up attack can matter as much as the initial target.

Trigger: Quest 4 is publicly offered or accepted; only its revealed text is supplied in context.

Sources: `src/tutorial/tutorial-world.c`, `lib/edit/quest.txt`, `src/quest/quest-status.c`.

## Oromë, the Great Hunter

`quest.5`

Level: **Extended**.

**1. Decision**

{detail} The hunt tracks the revealed creature categories and their required totals. Read the live counters: wolves, spiders, serpents and vampires have different thresholds. Progress in one category is not interchangeable with another.

Trigger: Quest 5 is publicly offered or accepted; only its revealed text is supplied in context.

Sources: `src/tutorial/tutorial-world.c`, `lib/edit/quest.txt`, `src/quest/quest-status.c`.

## Varda, Lady of the Stars

`quest.6`

Level: **Extended**.

**1. Decision**

{detail} Follow the revealed objective and destination in Quests. Read the target and timing requirement before leaving a level; this tutorial does not reveal the hidden location in advance.

Trigger: Quest 6 is publicly offered or accepted; only its revealed text is supplied in context.

Sources: `src/tutorial/tutorial-world.c`, `lib/edit/quest.txt`, `src/quest/quest-status.c`.

## Unpredictable movement

`monster.rf1_rand_25`

Level: **Extended**.

**1. Info**

{subject}: This creature sometimes chooses a random move. Its next direction is not guaranteed by its last one; inspect the actual position after each turn.

Trigger: A visible creature has this flag in learned lore (l_list), not merely its hidden race definition: RF1_RAND_25.

Sources: `src/tutorial/tutorial-world.c`, `src/defines.h`, `src/monster/monster-recall.c`, `src/melee/melee-process.c`.

## Erratic movement

`monster.rf1_rand_50`

Level: **Extended**.

**1. Info**

{subject}: This creature frequently chooses random movement. It can still threaten adjacent squares and use its known abilities; random movement is not harmlessness.

Trigger: A visible creature has this flag in learned lore (l_list), not merely its hidden race definition: RF1_RAND_50.

Sources: `src/tutorial/tutorial-world.c`, `src/defines.h`, `src/monster/monster-recall.c`, `src/melee/melee-process.c`.

## A unique creature

`monster.rf1_unique`

Level: **Extended**.

**1. Info**

{subject}: This known creature is unique. Read its own learned attacks and defenses; uniqueness by itself is not a complete combat description.

Trigger: A visible creature has this flag in learned lore (l_list), not merely its hidden race definition: RF1_UNIQUE.

Sources: `src/tutorial/tutorial-world.c`, `src/defines.h`, `src/monster/monster-recall.c`, `src/melee/melee-process.c`.

## A peaceful creature

`monster.rf1_peaceful`

Level: **Extended**.

**1. Info**

{subject}: This creature is peaceful under the current game rules. Do not treat movement toward it as an ordinary hostile attack.

Trigger: A visible creature has this flag in learned lore (l_list), not merely its hidden race definition: RF1_PEACEFUL.

Sources: `src/tutorial/tutorial-world.c`, `src/defines.h`, `src/monster/monster-recall.c`, `src/melee/melee-process.c`.

## No ordinary melee blows

`monster.rf1_never_blow`

Level: **Extended**.

**1. Info**

{subject}: Known lore says this creature does not make ordinary physical blows. That does not rule out its other known abilities or hazards.

Trigger: A visible creature has this flag in learned lore (l_list), not merely its hidden race definition: RF1_NEVER_BLOW.

Sources: `src/tutorial/tutorial-world.c`, `src/defines.h`, `src/monster/monster-recall.c`, `src/melee/melee-process.c`.

## An immobile creature

`monster.rf1_never_move`

Level: **Extended**.

**1. Info**

{subject}: This creature does not move normally. It may still threaten nearby squares or use other learned attacks. Check range and terrain.

Trigger: A visible creature has this flag in learned lore (l_list), not merely its hidden race definition: RF1_NEVER_MOVE.

Sources: `src/tutorial/tutorial-world.c`, `src/defines.h`, `src/monster/monster-recall.c`, `src/melee/melee-process.c`.

## Movement out of sight

`monster.rf1_hidden_move`

Level: **Extended**.

**1. Info**

{subject}: This creature moves while outside your view. A momentary sighting does not guarantee its position after you lose sight of it.

Trigger: A visible creature has this flag in learned lore (l_list), not merely its hidden race definition: RF1_HIDDEN_MOVE.

Sources: `src/tutorial/tutorial-world.c`, `src/defines.h`, `src/monster/monster-recall.c`, `src/melee/melee-process.c`.

## Critical immunity

`monster.rf1_no_crit`

Level: **Extended**.

**1. Info**

{subject}: This creature has no vulnerable areas for critical hits. Extra accuracy still helps hit, but critical bonus dice do not apply to it.

Trigger: A visible creature has this flag in learned lore (l_list), not merely its hidden race definition: RF1_NO_CRIT.

Sources: `src/tutorial/tutorial-world.c`, `src/defines.h`, `src/monster/monster-recall.c`, `src/melee/melee-process.c`.

## Critical resistance

`monster.rf1_res_crit`

Level: **Extended**.

**1. Info**

{subject}: This creature resists critical hits. The game halves the computed critical bonus dice, rounding down. Ordinary damage and other bonus dice follow their own rules.

Trigger: A visible creature has this flag in learned lore (l_list), not merely its hidden race definition: RF1_RES_CRIT.

Sources: `src/tutorial/tutorial-world.c`, `src/defines.h`, `src/monster/monster-recall.c`, `src/melee/melee-process.c`.

## A mindless creature

`monster.rf2_mindless`

Level: **Extended**.

**1. Info**

{subject}: Known lore identifies this creature as mindless. Check the actual applicable effects before relying on fear, confusion or another mind-affecting tactic.

Trigger: A visible creature has this flag in learned lore (l_list), not merely its hidden race definition: RF2_MINDLESS.

Sources: `src/tutorial/tutorial-world.c`, `src/defines.h`, `src/monster/monster-recall.c`, `src/melee/melee-process.c`.

## An intelligent foe

`monster.rf2_smart`

Level: **Extended**.

**1. Info**

{subject}: This foe can choose tactics intelligently. Do not assume it will repeat the same approach every turn. Inspect its observed attacks and available routes.

Trigger: A visible creature has this flag in learned lore (l_list), not merely its hidden race definition: RF2_SMART.

Sources: `src/tutorial/tutorial-world.c`, `src/defines.h`, `src/monster/monster-recall.c`, `src/melee/melee-process.c`.

## A territorial foe

`monster.rf2_territorial`

Level: **Extended**.

**1. Info**

{subject}: This creature is territorial and does not pursue around corners in the usual way. Breaking its line of approach may help, but other threats and its actual current state still matter.

Trigger: A visible creature has this flag in learned lore (l_list), not merely its hidden race definition: RF2_TERRITORIAL.

Sources: `src/tutorial/tutorial-world.c`, `src/defines.h`, `src/monster/monster-recall.c`, `src/melee/melee-process.c`.

## A short-sighted creature

`monster.rf2_short_sighted`

Level: **Extended**.

**1. Info**

{subject}: This creature has limited sight. Stealth, distance and noise are separate factors; short sight is not a guarantee that it cannot detect you.

Trigger: A visible creature has this flag in learned lore (l_list), not merely its hidden race definition: RF2_SHORT_SIGHTED.

Sources: `src/tutorial/tutorial-world.c`, `src/defines.h`, `src/monster/monster-recall.c`, `src/melee/melee-process.c`.

## An invisible creature

`monster.rf2_invisible`

Level: **Extended**.

**1. Info**

{subject}: This creature is difficult to see normally. Detection or true sight can help, but a revealed presence and a fully visible target are different states.

Trigger: A visible creature has this flag in learned lore (l_list), not merely its hidden race definition: RF2_INVISIBLE.

Sources: `src/tutorial/tutorial-world.c`, `src/defines.h`, `src/monster/monster-recall.c`, `src/melee/melee-process.c`.

## A luminous creature

`monster.rf2_glow`

Level: **Extended**.

**1. Info**

{subject}: This creature lights its own square. Its light can affect visibility, but it does not illuminate every surrounding passage.

Trigger: A visible creature has this flag in learned lore (l_list), not merely its hidden race definition: RF2_GLOW.

Sources: `src/tutorial/tutorial-world.c`, `src/defines.h`, `src/monster/monster-recall.c`, `src/melee/melee-process.c`.

## Enemy Cruel Blow

`monster.rf2_cruel_blow`

Level: **Extended**.

**1. Info**

{subject}: A sufficiently strong critical blow from this creature can confuse you. Read the actual attack margin, damage and resulting condition before choosing your next action.

Trigger: A visible creature has this flag in learned lore (l_list), not merely its hidden race definition: RF2_CRUEL_BLOW.

Sources: `src/tutorial/tutorial-world.c`, `src/defines.h`, `src/monster/monster-recall.c`, `src/melee/melee-process.c`.

## Enemy Exchange Places

`monster.rf2_exchange_places`

Level: **Extended**.

**1. Info**

{subject}: This creature can exchange places in combat. Do not assume an adjacent blocker will always preserve the same formation or retreat route.

Trigger: A visible creature has this flag in learned lore (l_list), not merely its hidden race definition: RF2_EXCHANGE_PLACES.

Sources: `src/tutorial/tutorial-world.c`, `src/defines.h`, `src/monster/monster-recall.c`, `src/melee/melee-process.c`.

## A multiplying creature

`monster.rf2_multiply`

Level: **Extended**.

**1. Info**

{subject}: This creature can reproduce. Time spent nearby can increase the number of threats. Read the actual visible count rather than assuming the first creature is alone.

Trigger: A visible creature has this flag in learned lore (l_list), not merely its hidden race definition: RF2_MULTIPLY.

Sources: `src/tutorial/tutorial-world.c`, `src/defines.h`, `src/monster/monster-recall.c`, `src/melee/melee-process.c`.

## Enemy regeneration

`monster.rf2_regenerate`

Level: **Extended**.

**1. Info**

{subject}: This creature recovers Health especially quickly. Pausing damage may let it recover; check its observed state before resuming an attack.

Trigger: A visible creature has this flag in learned lore (l_list), not merely its hidden race definition: RF2_REGENERATE.

Sources: `src/tutorial/tutorial-world.c`, `src/defines.h`, `src/monster/monster-recall.c`, `src/melee/melee-process.c`.

## Enemy Riposte

`monster.rf2_riposte`

Level: **Extended**.

**1. Info**

{subject}: A sufficiently poor melee attack can give this foe a counterattack. High Evasion can make careless repeated attacks dangerous.

Trigger: A visible creature has this flag in learned lore (l_list), not merely its hidden race definition: RF2_RIPOSTE.

Sources: `src/tutorial/tutorial-world.c`, `src/defines.h`, `src/monster/monster-recall.c`, `src/melee/melee-process.c`.

## Enemy Flanking

`monster.rf2_flanking`

Level: **Extended**.

**1. Info**

{subject}: This foe can attack while moving between squares adjacent to you. An enemy move near you is not necessarily a turn without an attack.

Trigger: A visible creature has this flag in learned lore (l_list), not merely its hidden race definition: RF2_FLANKING.

Sources: `src/tutorial/tutorial-world.c`, `src/defines.h`, `src/monster/monster-recall.c`, `src/melee/melee-process.c`.

## A surrounding cloud

`monster.rf2_cloud_surround`

Level: **Extended**.

**1. Info**

{subject}: This creature creates a hazardous surrounding cloud. Inspect the observed effect, distance and relevant protection before standing next to it.

Trigger: A visible creature has this flag in learned lore (l_list), not merely its hidden race definition: RF2_CLOUD_SURROUND.

Sources: `src/tutorial/tutorial-world.c`, `src/defines.h`, `src/monster/monster-recall.c`, `src/melee/melee-process.c`.

## A flying creature

`monster.rf2_flying`

Level: **Extended**.

**1. Info**

{subject}: This foe can cross chasms. A gap that blocks your own walking route may not separate it from you.

Trigger: A visible creature has this flag in learned lore (l_list), not merely its hidden race definition: RF2_FLYING.

Sources: `src/tutorial/tutorial-world.c`, `src/defines.h`, `src/monster/monster-recall.c`, `src/melee/melee-process.c`.

## Passing through doors

`monster.rf2_pass_door`

Level: **Extended**.

**1. Info**

{subject}: This creature can pass under doors. Closing a normal door is not a reliable barrier against it.

Trigger: A visible creature has this flag in learned lore (l_list), not merely its hidden race definition: RF2_PASS_DOOR.

Sources: `src/tutorial/tutorial-world.c`, `src/defines.h`, `src/monster/monster-recall.c`, `src/melee/melee-process.c`.

## Unlocking doors

`monster.rf2_unlock_door`

Level: **Extended**.

**1. Info**

{subject}: This creature can unlock doors. A locked doorway can delay rather than permanently exclude it.

Trigger: A visible creature has this flag in learned lore (l_list), not merely its hidden race definition: RF2_UNLOCK_DOOR.

Sources: `src/tutorial/tutorial-world.c`, `src/defines.h`, `src/monster/monster-recall.c`, `src/melee/melee-process.c`.

## Opening doors

`monster.rf2_open_door`

Level: **Extended**.

**1. Info**

{subject}: This creature can open doors. Closing one changes sight and timing, but does not guarantee that it stays closed.

Trigger: A visible creature has this flag in learned lore (l_list), not merely its hidden race definition: RF2_OPEN_DOOR.

Sources: `src/tutorial/tutorial-world.c`, `src/defines.h`, `src/monster/monster-recall.c`, `src/melee/melee-process.c`.

## Bashing doors

`monster.rf2_bash_door`

Level: **Extended**.

**1. Info**

{subject}: This creature can bash through doors. Inspect another retreat route instead of relying on the door lasting indefinitely.

Trigger: A visible creature has this flag in learned lore (l_list), not merely its hidden race definition: RF2_BASH_DOOR.

Sources: `src/tutorial/tutorial-world.c`, `src/defines.h`, `src/monster/monster-recall.c`, `src/melee/melee-process.c`.

## Passing through walls

`monster.rf2_pass_wall`

Level: **Extended**.

**1. Info**

{subject}: This creature can pass through walls. Ordinary solid terrain does not contain it the way it contains walking foes.

Trigger: A visible creature has this flag in learned lore (l_list), not merely its hidden race definition: RF2_PASS_WALL.

Sources: `src/tutorial/tutorial-world.c`, `src/defines.h`, `src/monster/monster-recall.c`, `src/melee/melee-process.c`.

## Destroying walls

`monster.rf2_kill_wall`

Level: **Extended**.

**1. Info**

{subject}: This creature can destroy walls. Terrain that currently blocks a route may change as it approaches.

Trigger: A visible creature has this flag in learned lore (l_list), not merely its hidden race definition: RF2_KILL_WALL.

Sources: `src/tutorial/tutorial-world.c`, `src/defines.h`, `src/monster/monster-recall.c`, `src/melee/melee-process.c`.

## Tunnelling through walls

`monster.rf2_tunnel_wall`

Level: **Extended**.

**1. Info**

{subject}: This foe can tunnel through walls. A blocked route can become passable; watch the actual terrain and sound.

Trigger: A visible creature has this flag in learned lore (l_list), not merely its hidden race definition: RF2_TUNNEL_WALL.

Sources: `src/tutorial/tutorial-world.c`, `src/defines.h`, `src/monster/monster-recall.c`, `src/melee/melee-process.c`.

## Crushing other creatures

`monster.rf2_kill_body`

Level: **Extended**.

**1. Info**

{subject}: This creature can kill other monsters in its way. A weaker enemy may not remain a dependable blocker.

Trigger: A visible creature has this flag in learned lore (l_list), not merely its hidden race definition: RF2_KILL_BODY.

Sources: `src/tutorial/tutorial-world.c`, `src/defines.h`, `src/monster/monster-recall.c`, `src/melee/melee-process.c`.

## Taking floor items

`monster.rf2_take_item`

Level: **Extended**.

**1. Info**

{subject}: This creature can pick up items. A visible object may not remain on the floor while you spend turns elsewhere.

Trigger: A visible creature has this flag in learned lore (l_list), not merely its hidden race definition: RF2_TAKE_ITEM.

Sources: `src/tutorial/tutorial-world.c`, `src/defines.h`, `src/monster/monster-recall.c`, `src/melee/melee-process.c`.

## Destroying floor items

`monster.rf2_kill_item`

Level: **Extended**.

**1. Info**

{subject}: This creature can destroy items in its way. Consider the item's current location and the danger before assuming you can collect it later.

Trigger: A visible creature has this flag in learned lore (l_list), not merely its hidden race definition: RF2_KILL_ITEM.

Sources: `src/tutorial/tutorial-world.c`, `src/defines.h`, `src/monster/monster-recall.c`, `src/melee/melee-process.c`.

## Retreating when power is low

`monster.rf2_low_mana_run`

Level: **Extended**.

**1. Info**

{subject}: This foe may retreat or teleport when its power runs low. Read its actual behavior rather than assuming a withdrawal means it has been defeated.

Trigger: A visible creature has this flag in learned lore (l_list), not merely its hidden race definition: RF2_LOW_MANA_RUN.

Sources: `src/tutorial/tutorial-world.c`, `src/defines.h`, `src/monster/monster-recall.c`, `src/melee/melee-process.c`.

## Enemy Charge

`monster.rf2_charge`

Level: **Extended**.

**1. Info**

{subject}: This creature can gain a stronger attack after moving toward you. Waiting in a straight approach lane can give it that opportunity.

Trigger: A visible creature has this flag in learned lore (l_list), not merely its hidden race definition: RF2_CHARGE.

Sources: `src/tutorial/tutorial-world.c`, `src/defines.h`, `src/monster/monster-recall.c`, `src/melee/melee-process.c`.

## Bane against Elves

`monster.rf2_elfbane`

Level: **Extended**.

**1. Info**

{subject}: This creature has a racial Bane against Elves. Its relevant skill contests can be stronger against a matching hero.

Trigger: A visible creature has this flag in learned lore (l_list), not merely its hidden race definition: RF2_ELFBANE.

Sources: `src/tutorial/tutorial-world.c`, `src/defines.h`, `src/monster/monster-recall.c`, `src/melee/melee-process.c`.

## Enemy Knock Back

`monster.rf2_knock_back`

Level: **Extended**.

**1. Info**

{subject}: This foe can knock you back. Read the terrain behind your square and any nearby chasm or trap before choosing to hold position.

Trigger: A visible creature has this flag in learned lore (l_list), not merely its hidden race definition: RF2_KNOCK_BACK.

Sources: `src/tutorial/tutorial-world.c`, `src/defines.h`, `src/monster/monster-recall.c`, `src/melee/melee-process.c`.

## Enemy Crippling Shot

`monster.rf2_crippling`

Level: **Extended**.

**1. Info**

{subject}: A critical ranged hit can slow you. The damage and slow condition are separate outcomes; read both after a hit.

Trigger: A visible creature has this flag in learned lore (l_list), not merely its hidden race definition: RF2_CRIPPLING.

Sources: `src/tutorial/tutorial-world.c`, `src/defines.h`, `src/monster/monster-recall.c`, `src/melee/melee-process.c`.

## Enemy Opportunist

`monster.rf2_opportunist`

Level: **Extended**.

**1. Info**

{subject}: Moving away from this adjacent foe can give it a free attack. A retreat still may be right, but it is not automatically free of retaliation.

Trigger: A visible creature has this flag in learned lore (l_list), not merely its hidden race definition: RF2_OPPORTUNIST.

Sources: `src/tutorial/tutorial-world.c`, `src/defines.h`, `src/monster/monster-recall.c`, `src/melee/melee-process.c`.

## Enemy Zone of Control

`monster.rf2_zone_of_control`

Level: **Extended**.

**1. Info**

{subject}: Moving between squares adjacent to this foe can provoke its free attack under the ability's conditions. Plan the whole step rather than only the destination.

Trigger: A visible creature has this flag in learned lore (l_list), not merely its hidden race definition: RF2_ZONE_OF_CONTROL.

Sources: `src/tutorial/tutorial-world.c`, `src/defines.h`, `src/monster/monster-recall.c`, `src/melee/melee-process.c`.

## A light-sensitive creature

`monster.rf3_hurt_lite`

Level: **Extended**.

**1. Info**

{subject}: Bright light can penalize this creature, and some light effects can stun or harm it. Light intensity and radius differ; inspect the actual illuminated squares.

Trigger: A visible creature has this flag in learned lore (l_list), not merely its hidden race definition: RF3_HURT_LITE.

Sources: `src/tutorial/tutorial-world.c`, `src/defines.h`, `src/monster/monster-recall.c`, `src/melee/melee-process.c`.

## A creature of stone

`monster.rf3_stone`

Level: **Extended**.

**1. Info**

{subject}: This creature has a stone body. Effects that interact with stone may treat it differently; read the relevant known effect before using it.

Trigger: A visible creature has this flag in learned lore (l_list), not merely its hidden race definition: RF3_STONE.

Sources: `src/tutorial/tutorial-world.c`, `src/defines.h`, `src/monster/monster-recall.c`, `src/melee/melee-process.c`.

## Fire vulnerability

`monster.rf3_hurt_fire`

Level: **Extended**.

**1. Info**

{subject}: This creature is especially vulnerable to fire. A fire effect's range, target and other consequences still matter.

Trigger: A visible creature has this flag in learned lore (l_list), not merely its hidden race definition: RF3_HURT_FIRE.

Sources: `src/tutorial/tutorial-world.c`, `src/defines.h`, `src/monster/monster-recall.c`, `src/melee/melee-process.c`.

## Cold vulnerability

`monster.rf3_hurt_cold`

Level: **Extended**.

**1. Info**

{subject}: This creature is especially vulnerable to cold. Choose a legal current target and read the effect before spending a resource.

Trigger: A visible creature has this flag in learned lore (l_list), not merely its hidden race definition: RF3_HURT_COLD.

Sources: `src/tutorial/tutorial-world.c`, `src/defines.h`, `src/monster/monster-recall.c`, `src/melee/melee-process.c`.

## An armed creature

`monster.rf3_has_weapon`

Level: **Extended**.

**1. Info**

{subject}: This creature fights with forged weapons. Effects that damage or shatter weapons may interact with it; read its learned attack rather than assuming it is unarmed.

Trigger: A visible creature has this flag in learned lore (l_list), not merely its hidden race definition: RF3_HAS_WEAPON.

Sources: `src/tutorial/tutorial-world.c`, `src/defines.h`, `src/monster/monster-recall.c`, `src/melee/melee-process.c`.

## An armoured creature

`monster.rf3_has_armour`

Level: **Extended**.

**1. Info**

{subject}: This creature wears substantial armour. A successful hit can still be blocked. Compare your damage, critical opportunities and any known armour-affecting effect.

Trigger: A visible creature has this flag in learned lore (l_list), not merely its hidden race definition: RF3_HAS_ARMOUR.

Sources: `src/tutorial/tutorial-world.c`, `src/defines.h`, `src/monster/monster-recall.c`, `src/melee/melee-process.c`.

## Lightning resistance

`monster.rf3_res_elec`

Level: **Extended**.

**1. Info**

{subject}: This creature resists lightning. Do not infer resistance to other elements from this one learned property.

Trigger: A visible creature has this flag in learned lore (l_list), not merely its hidden race definition: RF3_RES_ELEC.

Sources: `src/tutorial/tutorial-world.c`, `src/defines.h`, `src/monster/monster-recall.c`, `src/melee/melee-process.c`.

## Fire resistance

`monster.rf3_res_fire`

Level: **Extended**.

**1. Info**

{subject}: This creature resists fire. A fire effect may be less useful; this says nothing by itself about cold or other damage types.

Trigger: A visible creature has this flag in learned lore (l_list), not merely its hidden race definition: RF3_RES_FIRE.

Sources: `src/tutorial/tutorial-world.c`, `src/defines.h`, `src/monster/monster-recall.c`, `src/melee/melee-process.c`.

## Cold resistance

`monster.rf3_res_cold`

Level: **Extended**.

**1. Info**

{subject}: This creature resists cold. Read a different known effect or ordinary damage option before spending a cold resource.

Trigger: A visible creature has this flag in learned lore (l_list), not merely its hidden race definition: RF3_RES_COLD.

Sources: `src/tutorial/tutorial-world.c`, `src/defines.h`, `src/monster/monster-recall.c`, `src/melee/melee-process.c`.

## Poison resistance

`monster.rf3_res_pois`

Level: **Extended**.

**1. Info**

{subject}: This creature resists poison. Poison effects and ordinary weapon damage follow different rules.

Trigger: A visible creature has this flag in learned lore (l_list), not merely its hidden race definition: RF3_RES_POIS.

Sources: `src/tutorial/tutorial-world.c`, `src/defines.h`, `src/monster/monster-recall.c`, `src/melee/melee-process.c`.

## Cannot be slowed

`monster.rf3_no_slow`

Level: **Extended**.

**1. Info**

{subject}: This creature cannot be slowed. A known slowing effect is not a suitable required practice action against it.

Trigger: A visible creature has this flag in learned lore (l_list), not merely its hidden race definition: RF3_NO_SLOW.

Sources: `src/tutorial/tutorial-world.c`, `src/defines.h`, `src/monster/monster-recall.c`, `src/melee/melee-process.c`.

## Cannot be frightened

`monster.rf3_no_fear`

Level: **Extended**.

**1. Info**

{subject}: This creature cannot be frightened. Do not rely on a fear effect to make it retreat.

Trigger: A visible creature has this flag in learned lore (l_list), not merely its hidden race definition: RF3_NO_FEAR.

Sources: `src/tutorial/tutorial-world.c`, `src/defines.h`, `src/monster/monster-recall.c`, `src/melee/melee-process.c`.

## Cannot be stunned

`monster.rf3_no_stun`

Level: **Extended**.

**1. Info**

{subject}: This creature cannot be stunned. A stunning device may still have other effects, but stunning itself is not a viable plan.

Trigger: A visible creature has this flag in learned lore (l_list), not merely its hidden race definition: RF3_NO_STUN.

Sources: `src/tutorial/tutorial-world.c`, `src/defines.h`, `src/monster/monster-recall.c`, `src/melee/melee-process.c`.

## Cannot be confused

`monster.rf3_no_conf`

Level: **Extended**.

**1. Info**

{subject}: This creature cannot be confused. Choose another known tactic instead of spending a confusion effect for that purpose.

Trigger: A visible creature has this flag in learned lore (l_list), not merely its hidden race definition: RF3_NO_CONF.

Sources: `src/tutorial/tutorial-world.c`, `src/defines.h`, `src/monster/monster-recall.c`, `src/melee/melee-process.c`.

## Cannot be put to sleep

`monster.rf3_no_sleep`

Level: **Extended**.

**1. Info**

{subject}: This creature cannot be put to sleep. A slumber effect is not a useful required action against it.

Trigger: A visible creature has this flag in learned lore (l_list), not merely its hidden race definition: RF3_NO_SLEEP.

Sources: `src/tutorial/tutorial-world.c`, `src/defines.h`, `src/monster/monster-recall.c`, `src/melee/melee-process.c`.

## Enemy archery

`monster.rf4_arrow1`

Level: **Extended**.

**1. Info**

{subject}: This creature has a shortbow arrow attack. Lines of fire and nearby cover matter even when it is not adjacent.

Trigger: A visible creature has this flag in learned lore (l_list), not merely its hidden race definition: RF4_ARROW1.

Sources: `src/tutorial/tutorial-world.c`, `src/defines.h`, `src/monster/monster-recall.c`, `src/melee/melee-process.c`.

## Enemy longbow

`monster.rf4_arrow2`

Level: **Extended**.

**1. Info**

{subject}: This creature has a longbow arrow attack. Distance alone does not prevent ranged damage; inspect the line of fire.

Trigger: A visible creature has this flag in learned lore (l_list), not merely its hidden race definition: RF4_ARROW2.

Sources: `src/tutorial/tutorial-world.c`, `src/defines.h`, `src/monster/monster-recall.c`, `src/melee/melee-process.c`.

## Thrown boulders

`monster.rf4_boulder`

Level: **Extended**.

**1. Info**

{subject}: This creature can throw boulders. Check cover and the actual attack result; being outside melee range does not make you safe.

Trigger: A visible creature has this flag in learned lore (l_list), not merely its hidden race definition: RF4_BOULDER.

Sources: `src/tutorial/tutorial-world.c`, `src/defines.h`, `src/monster/monster-recall.c`, `src/melee/melee-process.c`.

## Fire breath

`monster.rf4_brth_fire`

Level: **Extended**.

**1. Info**

{subject}: This creature can breathe fire. Inspect fire resistance, relevant Protection, cover and the observed area before choosing a route.

Trigger: A visible creature has this flag in learned lore (l_list), not merely its hidden race definition: RF4_BRTH_FIRE.

Sources: `src/tutorial/tutorial-world.c`, `src/defines.h`, `src/monster/monster-recall.c`, `src/melee/melee-process.c`.

## Cold breath

`monster.rf4_brth_cold`

Level: **Extended**.

**1. Info**

{subject}: This creature can breathe cold. Inspect cold resistance, applicable Protection and the line of attack.

Trigger: A visible creature has this flag in learned lore (l_list), not merely its hidden race definition: RF4_BRTH_COLD.

Sources: `src/tutorial/tutorial-world.c`, `src/defines.h`, `src/monster/monster-recall.c`, `src/melee/melee-process.c`.

## Poison breath

`monster.rf4_brth_pois`

Level: **Extended**.

**1. Info**

{subject}: This creature can breathe poison. Resistance reduces damage differently from an Antidote curing existing poison.

Trigger: A visible creature has this flag in learned lore (l_list), not merely its hidden race definition: RF4_BRTH_POIS.

Sources: `src/tutorial/tutorial-world.c`, `src/defines.h`, `src/monster/monster-recall.c`, `src/melee/melee-process.c`.

## Dark breath

`monster.rf4_brth_dark`

Level: **Extended**.

**1. Info**

{subject}: This creature can breathe darkness. Light and dark resistance have their own rules; ordinary physical Protection is not automatically applicable.

Trigger: A visible creature has this flag in learned lore (l_list), not merely its hidden race definition: RF4_BRTH_DARK.

Sources: `src/tutorial/tutorial-world.c`, `src/defines.h`, `src/monster/monster-recall.c`, `src/melee/melee-process.c`.

## Earthquakes

`monster.rf4_earthquake`

Level: **Extended**.

**1. Info**

{subject}: This creature can cause an earthquake. The terrain and available routes can change; recheck the map after the actual event.

Trigger: A visible creature has this flag in learned lore (l_list), not merely its hidden race definition: RF4_EARTHQUAKE.

Sources: `src/tutorial/tutorial-world.c`, `src/defines.h`, `src/monster/monster-recall.c`, `src/melee/melee-process.c`.

## Calling for help

`monster.rf4_shriek`

Level: **Extended**.

**1. Info**

{subject}: This creature can shriek for help. Noise can alert other foes. A creature currently alone in view may soon gain support.

Trigger: A visible creature has this flag in learned lore (l_list), not merely its hidden race definition: RF4_SHRIEK.

Sources: `src/tutorial/tutorial-world.c`, `src/defines.h`, `src/monster/monster-recall.c`, `src/melee/melee-process.c`.

## A stunning screech

`monster.rf4_screech`

Level: **Extended**.

**1. Info**

{subject}: This creature can make a loud screech and stun you. Noise and the resulting condition are separate consequences.

Trigger: A visible creature has this flag in learned lore (l_list), not merely its hidden race definition: RF4_SCREECH.

Sources: `src/tutorial/tutorial-world.c`, `src/defines.h`, `src/monster/monster-recall.c`, `src/melee/melee-process.c`.

## Creating darkness

`monster.rf4_darkness`

Level: **Extended**.

**1. Info**

{subject}: This creature can darken the area around you. Recheck what is currently visible; remembered terrain is not proof that enemies have stayed in place.

Trigger: A visible creature has this flag in learned lore (l_list), not merely its hidden race definition: RF4_DARKNESS.

Sources: `src/tutorial/tutorial-world.c`, `src/defines.h`, `src/monster/monster-recall.c`, `src/melee/melee-process.c`.

## Forgetting the map

`monster.rf4_forget`

Level: **Extended**.

**1. Info**

{subject}: This creature can make you forget mapped terrain. This affects remembered knowledge, separate from current sight.

Trigger: A visible creature has this flag in learned lore (l_list), not merely its hidden race definition: RF4_FORGET.

Sources: `src/tutorial/tutorial-world.c`, `src/defines.h`, `src/monster/monster-recall.c`, `src/melee/melee-process.c`.

## Causing fear

`monster.rf4_scare`

Level: **Extended**.

**1. Info**

{subject}: This creature can frighten you. Fear interferes with melee and ranged aiming. Inspect a known remedy after the condition actually appears.

Trigger: A visible creature has this flag in learned lore (l_list), not merely its hidden race definition: RF4_SCARE.

Sources: `src/tutorial/tutorial-world.c`, `src/defines.h`, `src/monster/monster-recall.c`, `src/melee/melee-process.c`.

## Causing confusion

`monster.rf4_conf`

Level: **Extended**.

**1. Info**

{subject}: This creature can confuse you. Direction and aiming choices become unreliable while confused; inspect current conditions before committing.

Trigger: A visible creature has this flag in learned lore (l_list), not merely its hidden race definition: RF4_CONF.

Sources: `src/tutorial/tutorial-world.c`, `src/defines.h`, `src/monster/monster-recall.c`, `src/melee/melee-process.c`.

## Causing entrancement

`monster.rf4_hold`

Level: **Extended**.

**1. Info**

{subject}: This creature can entrance you. A successful trance prevents ordinary actions until it ends. Avoid assuming you can drink a remedy after losing the ability to act.

Trigger: A visible creature has this flag in learned lore (l_list), not merely its hidden race definition: RF4_HOLD.

Sources: `src/tutorial/tutorial-world.c`, `src/defines.h`, `src/monster/monster-recall.c`, `src/melee/melee-process.c`.

## Causing slowing

`monster.rf4_slow`

Level: **Extended**.

**1. Info**

{subject}: This creature can slow you. Compare your current speed and escape route; a temporary speed bonus may offset the penalty without removing it.

Trigger: A visible creature has this flag in learned lore (l_list), not merely its hidden race definition: RF4_SLOW.

Sources: `src/tutorial/tutorial-world.c`, `src/defines.h`, `src/monster/monster-recall.c`, `src/melee/melee-process.c`.

## Hatching spiders

`monster.rf4_hatch_spider`

Level: **Extended**.

**1. Info**

{subject}: This creature can hatch spiders. Additional enemies can alter surrounding pressure and block routes.

Trigger: A visible creature has this flag in learned lore (l_list), not merely its hidden race definition: RF4_HATCH_SPIDER.

Sources: `src/tutorial/tutorial-world.c`, `src/defines.h`, `src/monster/monster-recall.c`, `src/melee/melee-process.c`.

## Dimming light

`monster.rf4_dim`

Level: **Extended**.

**1. Info**

{subject}: This creature can dim your light. Check the darkening effect separately from fuel and blindness.

Trigger: A visible creature has this flag in learned lore (l_list), not merely its hidden race definition: RF4_DIM.

Sources: `src/tutorial/tutorial-world.c`, `src/defines.h`, `src/monster/monster-recall.c`, `src/melee/melee-process.c`.

## An enemy binding song

`monster.rf4_sng_binding`

Level: **Extended**.

**1. Info**

{subject}: This creature sings a binding song. Read its learned description and the conditions actually applied; an enemy song is not your own selectable Song ability.

Trigger: A visible creature has this flag in learned lore (l_list), not merely its hidden race definition: RF4_SNG_BINDING.

Sources: `src/tutorial/tutorial-world.c`, `src/defines.h`, `src/monster/monster-recall.c`, `src/melee/melee-process.c`.

## An enemy piercing song

`monster.rf4_sng_piercing`

Level: **Extended**.

**1. Info**

{subject}: This creature sings a song of piercing. Read the learned effect and actual messages before choosing how to break contact.

Trigger: A visible creature has this flag in learned lore (l_list), not merely its hidden race definition: RF4_SNG_PIERCING.

Sources: `src/tutorial/tutorial-world.c`, `src/defines.h`, `src/monster/monster-recall.c`, `src/melee/melee-process.c`.

## An enemy oath song

`monster.rf4_sng_oaths`

Level: **Extended**.

**1. Info**

{subject}: This creature sings a song of oaths. Read the learned effect and current situation; the tutorial does not select or break an oath for you.

Trigger: A visible creature has this flag in learned lore (l_list), not merely its hidden race definition: RF4_SNG_OATHS.

Sources: `src/tutorial/tutorial-world.c`, `src/defines.h`, `src/monster/monster-recall.c`, `src/melee/melee-process.c`.

## Bane against Dwarves

`monster.rf4_dwarfbane`

Level: **Extended**.

**1. Info**

{subject}: This creature has a racial Bane against Dwarves. Relevant contests can be harder for a matching hero.

Trigger: A visible creature has this flag in learned lore (l_list), not merely its hidden race definition: RF4_DWARFBANE.

Sources: `src/tutorial/tutorial-world.c`, `src/defines.h`, `src/monster/monster-recall.c`, `src/melee/melee-process.c`.

## Bane against Men

`monster.rf4_edainbane`

Level: **Extended**.

**1. Info**

{subject}: This creature has a racial Bane against Men. Relevant contests can be harder for a matching hero.

Trigger: A visible creature has this flag in learned lore (l_list), not merely its hidden race definition: RF4_EDAINBANE.

Sources: `src/tutorial/tutorial-world.c`, `src/defines.h`, `src/monster/monster-recall.c`, `src/melee/melee-process.c`.

## Thrown webs

`monster.rf4_throw_web`

Level: **Extended**.

**1. Info**

{subject}: This creature can throw a web over you. Inspect the resulting terrain and movement restriction before repeating a movement command.

Trigger: A visible creature has this flag in learned lore (l_list), not merely its hidden race definition: RF4_THROW_WEB.

Sources: `src/tutorial/tutorial-world.c`, `src/defines.h`, `src/monster/monster-recall.c`, `src/melee/melee-process.c`.

## Rallying allies

`monster.rf4_rally`

Level: **Extended**.

**1. Info**

{subject}: This creature can rally fleeing allies. A foe that retreated can regain the will to fight; fleeing is not a permanent removal from combat.

Trigger: A visible creature has this flag in learned lore (l_list), not merely its hidden race definition: RF4_RALLY.

Sources: `src/tutorial/tutorial-world.c`, `src/defines.h`, `src/monster/monster-recall.c`, `src/melee/melee-process.c`.

## Bane against Noldor

`monster.rf4_noldorbane`

Level: **Extended**.

**1. Info**

{subject}: This creature has a Bane against Noldor. Its relevant skill contests can be stronger against a matching hero.

Trigger: A visible creature has this flag in learned lore (l_list), not merely its hidden race definition: RF4_NOLDORBANE.

Sources: `src/tutorial/tutorial-world.c`, `src/defines.h`, `src/monster/monster-recall.c`, `src/melee/melee-process.c`.

## Bane against Sindar

`monster.rf4_sindarbane`

Level: **Extended**.

**1. Info**

{subject}: This creature has a Bane against Sindar. Its relevant skill contests can be stronger against a matching hero.

Trigger: A visible creature has this flag in learned lore (l_list), not merely its hidden race definition: RF4_SINDARBANE.

Sources: `src/tutorial/tutorial-world.c`, `src/defines.h`, `src/monster/monster-recall.c`, `src/melee/melee-process.c`.

## Orc lore

`monster.rf3_orc`

Level: **Extended**.

**1. Info**

{subject}: This creature's orc classification is known. Matching slays, Bane choices, quest counters and oath restrictions can depend on that classification. Do not infer other unknown abilities from its group.

Trigger: A visible creature has this flag in learned lore (l_list), not merely its hidden race definition: RF3_ORC.

Sources: `src/tutorial/tutorial-world.c`, `src/defines.h`, `src/monster/monster-recall.c`, `src/melee/melee-process.c`.

## Troll lore

`monster.rf3_troll`

Level: **Extended**.

**1. Info**

{subject}: This creature's troll classification is known. Matching slays, Bane choices, quest counters and oath restrictions can depend on that classification. Do not infer other unknown abilities from its group.

Trigger: A visible creature has this flag in learned lore (l_list), not merely its hidden race definition: RF3_TROLL.

Sources: `src/tutorial/tutorial-world.c`, `src/defines.h`, `src/monster/monster-recall.c`, `src/melee/melee-process.c`.

## Serpent lore

`monster.rf3_serpent`

Level: **Extended**.

**1. Info**

{subject}: This creature's serpent classification is known. Matching slays, Bane choices, quest counters and oath restrictions can depend on that classification. Do not infer other unknown abilities from its group.

Trigger: A visible creature has this flag in learned lore (l_list), not merely its hidden race definition: RF3_SERPENT.

Sources: `src/tutorial/tutorial-world.c`, `src/defines.h`, `src/monster/monster-recall.c`, `src/melee/melee-process.c`.

## Dragon lore

`monster.rf3_dragon`

Level: **Extended**.

**1. Info**

{subject}: This creature's dragon classification is known. Matching slays, Bane choices, quest counters and oath restrictions can depend on that classification. Do not infer other unknown abilities from its group.

Trigger: A visible creature has this flag in learned lore (l_list), not merely its hidden race definition: RF3_DRAGON.

Sources: `src/tutorial/tutorial-world.c`, `src/defines.h`, `src/monster/monster-recall.c`, `src/melee/melee-process.c`.

## Rauko lore

`monster.rf3_rauko`

Level: **Extended**.

**1. Info**

{subject}: This creature's rauko classification is known. Matching slays, Bane choices, quest counters and oath restrictions can depend on that classification. Do not infer other unknown abilities from its group.

Trigger: A visible creature has this flag in learned lore (l_list), not merely its hidden race definition: RF3_RAUKO.

Sources: `src/tutorial/tutorial-world.c`, `src/defines.h`, `src/monster/monster-recall.c`, `src/melee/melee-process.c`.

## Undead lore

`monster.rf3_undead`

Level: **Extended**.

**1. Info**

{subject}: This creature's undead classification is known. Matching slays, Bane choices, quest counters and oath restrictions can depend on that classification. Do not infer other unknown abilities from its group.

Trigger: A visible creature has this flag in learned lore (l_list), not merely its hidden race definition: RF3_UNDEAD.

Sources: `src/tutorial/tutorial-world.c`, `src/defines.h`, `src/monster/monster-recall.c`, `src/melee/melee-process.c`.

## Spider lore

`monster.rf3_spider`

Level: **Extended**.

**1. Info**

{subject}: This creature's spider classification is known. Matching slays, Bane choices, quest counters and oath restrictions can depend on that classification. Do not infer other unknown abilities from its group.

Trigger: A visible creature has this flag in learned lore (l_list), not merely its hidden race definition: RF3_SPIDER.

Sources: `src/tutorial/tutorial-world.c`, `src/defines.h`, `src/monster/monster-recall.c`, `src/melee/melee-process.c`.

## Wolf lore

`monster.rf3_wolf`

Level: **Extended**.

**1. Info**

{subject}: This creature's wolf classification is known. Matching slays, Bane choices, quest counters and oath restrictions can depend on that classification. Do not infer other unknown abilities from its group.

Trigger: A visible creature has this flag in learned lore (l_list), not merely its hidden race definition: RF3_WOLF.

Sources: `src/tutorial/tutorial-world.c`, `src/defines.h`, `src/monster/monster-recall.c`, `src/melee/melee-process.c`.

## Man lore

`monster.rf3_man`

Level: **Extended**.

**1. Info**

{subject}: This creature's man classification is known. Matching slays, Bane choices, quest counters and oath restrictions can depend on that classification. Do not infer other unknown abilities from its group.

Trigger: A visible creature has this flag in learned lore (l_list), not merely its hidden race definition: RF3_MAN.

Sources: `src/tutorial/tutorial-world.c`, `src/defines.h`, `src/monster/monster-recall.c`, `src/melee/melee-process.c`.

## Elf lore

`monster.rf3_elf`

Level: **Extended**.

**1. Info**

{subject}: This creature's elf classification is known. Matching slays, Bane choices, quest counters and oath restrictions can depend on that classification. Do not infer other unknown abilities from its group.

Trigger: A visible creature has this flag in learned lore (l_list), not merely its hidden race definition: RF3_ELF.

Sources: `src/tutorial/tutorial-world.c`, `src/defines.h`, `src/monster/monster-recall.c`, `src/melee/melee-process.c`.

## Giant lore

`monster.rf3_giant`

Level: **Extended**.

**1. Info**

{subject}: This creature's giant classification is known. Matching slays, Bane choices, quest counters and oath restrictions can depend on that classification. Do not infer other unknown abilities from its group.

Trigger: A visible creature has this flag in learned lore (l_list), not merely its hidden race definition: RF3_GIANT.

Sources: `src/tutorial/tutorial-world.c`, `src/defines.h`, `src/monster/monster-recall.c`, `src/melee/melee-process.c`.

## Cat lore

`monster.rf3_cat`

Level: **Extended**.

**1. Info**

{subject}: This creature's cat classification is known. Matching slays, Bane choices, quest counters and oath restrictions can depend on that classification. Do not infer other unknown abilities from its group.

Trigger: A visible creature has this flag in learned lore (l_list), not merely its hidden race definition: RF3_CAT.

Sources: `src/tutorial/tutorial-world.c`, `src/defines.h`, `src/monster/monster-recall.c`, `src/melee/melee-process.c`.

## Horror lore

`monster.rf3_horror`

Level: **Extended**.

**1. Info**

{subject}: This creature's horror classification is known. Matching slays, Bane choices, quest counters and oath restrictions can depend on that classification. Do not infer other unknown abilities from its group.

Trigger: A visible creature has this flag in learned lore (l_list), not merely its hidden race definition: RF3_HORROR.

Sources: `src/tutorial/tutorial-world.c`, `src/defines.h`, `src/monster/monster-recall.c`, `src/melee/melee-process.c`.

## Vampire lore

`monster.rf3_vampire`

Level: **Extended**.

**1. Info**

{subject}: This creature's vampire classification is known. Matching slays, Bane choices, quest counters and oath restrictions can depend on that classification. Do not infer other unknown abilities from its group.

Trigger: A visible creature has this flag in learned lore (l_list), not merely its hidden race definition: RF3_VAMPIRE.

Sources: `src/tutorial/tutorial-world.c`, `src/defines.h`, `src/monster/monster-recall.c`, `src/melee/melee-process.c`.

## Heavy stun

`status.heavy_stun`

Level: **Normal**.

**1. Info**

{detail} At stun severity 50 or more, every skill receives -4 rather than -2. The heavy-stun label itself appears above 50. You can still act until stun exceeds 100; inspect a known remedy while action is possible.

Trigger: The corresponding public status-pane state first becomes active with its owning ability or action requirements satisfied: heavy_stun.

Sources: `src/sdl/ui/sdl-panes.c`, `src/tutorial/tutorial-game.c`, `src/player/player-song-duels.c`, `src/cmd/combat/cmd-combat.c`.

## Knocked out

`status.knocked_out`

Level: **Normal**.

**1. Info**

{detail} Stun above 100 prevents ordinary actions. Continue resumes the normal scheduler, not a free remedy action. You must regain the ability to act before choosing and using an item.

Trigger: The corresponding public status-pane state first becomes active with its owning ability or action requirements satisfied: knocked_out.

Sources: `src/sdl/ui/sdl-panes.c`, `src/tutorial/tutorial-game.c`, `src/player/player-song-duels.c`, `src/cmd/combat/cmd-combat.c`.

## Full

`status.full`

Level: **Normal**.

**1. Info**

{detail} Your nourishment is at or above the Full threshold. Full describes food level, not Health or Voice. Eating again is a separate action and may waste a resource you need later.

Trigger: The corresponding public status-pane state first becomes active with its owning ability or action requirements satisfied: full.

Sources: `src/sdl/ui/sdl-panes.c`, `src/tutorial/tutorial-game.c`, `src/player/player-song-duels.c`, `src/cmd/combat/cmd-combat.c`.

## Song lockout

`status.song_lockout_timer`

Level: **Extended**.

**1. Info**

{detail} A song duel has temporarily prevented starting another song. Wait for the displayed lockout to expire through normal game time. A Voice potion restores Voice but does not remove this lockout.

Trigger: The corresponding public status-pane state first becomes active with its owning ability or action requirements satisfied: song_lockout_timer.

Sources: `src/sdl/ui/sdl-panes.c`, `src/tutorial/tutorial-game.c`, `src/player/player-song-duels.c`, `src/cmd/combat/cmd-combat.c`.

## Losing ground in a song contest

`status.song_contest_player_stacks`

Level: **Extended**.

**1. Info**

{detail} Your opponent has won recent Contest exchanges. These stacks track pressure toward losing the duel. Defeat can drain a random attribute and lock singing; read the actual target and duel state before continuing.

Trigger: The corresponding public status-pane state first becomes active with its owning ability or action requirements satisfied: song_contest_player_stacks.

Sources: `src/sdl/ui/sdl-panes.c`, `src/tutorial/tutorial-game.c`, `src/player/player-song-duels.c`, `src/cmd/combat/cmd-combat.c`.

## Climbing

`status.climbing`

Level: **Extended**.

**1. Info**

{detail} A climbing action is in progress. Its recovery and movement rules still apply. This card only explains the public state; it does not finish the climb or create an extra action.

Trigger: The corresponding public status-pane state first becomes active with its owning ability or action requirements satisfied: climbing.

Sources: `src/sdl/ui/sdl-panes.c`, `src/tutorial/tutorial-game.c`, `src/player/player-song-duels.c`, `src/cmd/combat/cmd-combat.c`.

## Leaping

`status.leaping`

Level: **Extended**.

**1. Info**

{detail} A leap is in progress. Leaping has approach and landing requirements and is distinct from walking onto a hazard. Let the committed action resolve; this explanation does not change its destination.

Trigger: The corresponding public status-pane state first becomes active with its owning ability or action requirements satisfied: leaping.

Sources: `src/sdl/ui/sdl-panes.c`, `src/tutorial/tutorial-game.c`, `src/player/player-song-duels.c`, `src/cmd/combat/cmd-combat.c`.

## Knocked back

`status.knocked_back`

Level: **Extended**.

**1. Info**

{detail} An effect displaced you. Recheck the current square, nearby enemies and the terrain behind or beside you. Knockback can change a route even when you did not choose a move.

Trigger: The corresponding public status-pane state first becomes active with its owning ability or action requirements satisfied: knocked_back.

Sources: `src/sdl/ui/sdl-panes.c`, `src/tutorial/tutorial-game.c`, `src/player/player-song-duels.c`, `src/cmd/combat/cmd-combat.c`.

## A recovery turn is owed

`status.skip_next_turn`

Level: **Extended**.

**1. Info**

{detail} The current action or effect requires a skipped turn, such as recovery from Smite. This is part of the real action cost. Reading a card does not erase the recovery or let you act during it.

Trigger: The corresponding public status-pane state first becomes active with its owning ability or action requirements satisfied: skip_next_turn.

Sources: `src/sdl/ui/sdl-panes.c`, `src/tutorial/tutorial-game.c`, `src/player/player-song-duels.c`, `src/cmd/combat/cmd-combat.c`.

## Waking from entrancement

`status.was_entranced`

Level: **Extended**.

**1. Info**

{detail} You are in the recovery state after a trance. Follow the normal action schedule; Continue acknowledges the explanation and does not grant a free turn.

Trigger: The corresponding public status-pane state first becomes active with its owning ability or action requirements satisfied: was_entranced.

Sources: `src/sdl/ui/sdl-panes.c`, `src/tutorial/tutorial-game.c`, `src/player/player-song-duels.c`, `src/cmd/combat/cmd-combat.c`.

## Vengeance is primed

`status.vengeance`

Level: **Extended**.

**1. Info**

{detail} With Vengeance active, taking melee damage primes one extra damage die for your next melee hit. It does not stack indefinitely, and a ranged attack is not the required melee hit. Check the actual active ability and target.

Trigger: The corresponding public status-pane state first becomes active with its owning ability or action requirements satisfied: vengeance.

Sources: `src/sdl/ui/sdl-panes.c`, `src/tutorial/tutorial-game.c`, `src/player/player-song-duels.c`, `src/cmd/combat/cmd-combat.c`.

## Focused Attack is ready

`status.focused`

Level: **Extended**.

**1. Info**

{detail} Focused Attack is active and a prior qualifying pause has prepared its Perception-based attack bonus. The bonus is not a permanent increase to every attack; the next applicable attack consumes the prepared state.

Trigger: The corresponding public status-pane state first becomes active with its owning ability or action requirements satisfied: focused.

Sources: `src/sdl/ui/sdl-panes.c`, `src/tutorial/tutorial-game.c`, `src/player/player-song-duels.c`, `src/cmd/combat/cmd-combat.c`.

## Concentration is building

`status.concentration`

Level: **Extended**.

**1. Info**

{detail} Concentration rewards consecutive attacks on the same target, up to half your Perception. Attacking another creature changes the target and resets the accumulated sequence. Inspect the displayed current bonus.

Trigger: The corresponding public status-pane state first becomes active with its owning ability or action requirements satisfied: concentration.

Sources: `src/sdl/ui/sdl-panes.c`, `src/tutorial/tutorial-game.c`, `src/player/player-song-duels.c`, `src/cmd/combat/cmd-combat.c`.

## Power Throw is ready

`status.power_throw`

Level: **Extended**.

**1. Info**

{detail} The current setup has an eligible Harness spear or hand axe and a prepared Power Throw opportunity. Its separate thrown and melee rolls combine successful damage before one Protection roll. Your melee weapon remains active.

Trigger: The corresponding public status-pane state first becomes active with its owning ability or action requirements satisfied: power_throw.

Sources: `src/sdl/ui/sdl-panes.c`, `src/tutorial/tutorial-game.c`, `src/player/player-song-duels.c`, `src/cmd/combat/cmd-combat.c`.

## You are singing

`status.singing`

Level: **Normal**.

**1. Info**

{detail} The displayed major and any minor theme are active. Each theme has its own Voice cost. Voice does not regenerate while singing; stopping or changing a song is a separate real action.

Trigger: The corresponding public status-pane state first becomes active with its owning ability or action requirements satisfied: singing.

Sources: `src/sdl/ui/sdl-panes.c`, `src/tutorial/tutorial-game.c`, `src/player/player-song-duels.c`, `src/cmd/combat/cmd-combat.c`.

## In a pit

`status.in_pit`

Level: **Normal**.

**1. Info**

{detail} Your hero is in pit terrain. Leaving has its own movement or climbing rules. Inspect the actual square and nearby enemies before repeating a command.

Trigger: The corresponding public status-pane state first becomes active with its owning ability or action requirements satisfied: in_pit.

Sources: `src/sdl/ui/sdl-panes.c`, `src/tutorial/tutorial-game.c`, `src/player/player-song-duels.c`, `src/cmd/combat/cmd-combat.c`.

## Caught in a web

`status.in_web`

Level: **Normal**.

**1. Info**

{detail} A web occupies your square and can restrict movement. Read the actual escape action and its result; Leaping does not simply bypass an existing web.

Trigger: The corresponding public status-pane state first becomes active with its owning ability or action requirements satisfied: in_web.

Sources: `src/sdl/ui/sdl-panes.c`, `src/tutorial/tutorial-game.c`, `src/player/player-song-duels.c`, `src/cmd/combat/cmd-combat.c`.

## Standing in sunlight

`status.sunlight`

Level: **Extended**.

**1. Info**

{detail} Your square is in sunlight. This is an environmental state, separate from the fuel in your lamp or the radius of your own light. Check the effect on known light-sensitive creatures.

Trigger: The corresponding public status-pane state first becomes active with its owning ability or action requirements satisfied: sunlight.

Sources: `src/sdl/ui/sdl-panes.c`, `src/tutorial/tutorial-game.c`, `src/player/player-song-duels.c`, `src/cmd/combat/cmd-combat.c`.

## Cursed rolls

`status.cursed`

Level: **Normal**.

**1. Info**

{detail} This cursed state makes the hero use the worse of two rolls in affected attack, evasion and skill contests. It is distinct from a cursed piece of equipment preventing removal. Read the actual source and any available remedy.

Trigger: The corresponding public status-pane state first becomes active with its owning ability or action requirements satisfied: cursed.

Sources: `src/sdl/ui/sdl-panes.c`, `src/tutorial/tutorial-game.c`, `src/player/player-song-duels.c`, `src/cmd/combat/cmd-combat.c`.

## Running

`status.running`

Level: **Normal**.

**1. Info**

{detail} Running repeats movement actions until it stops or is interrupted. Every move retains its cost, and enemies can act. The tutorial suspends input automation while the explanation is visible.

Trigger: The corresponding public status-pane state first becomes active with its owning ability or action requirements satisfied: running.

Sources: `src/sdl/ui/sdl-panes.c`, `src/tutorial/tutorial-game.c`, `src/player/player-song-duels.c`, `src/cmd/combat/cmd-combat.c`.

## Smithing in progress

`status.smithing`

Level: **Extended**.

**1. Info**

{detail} Smithing is an ongoing action with remaining work. Time, forge uses and resource costs follow the accepted proposal. Interruption does not mean a finished item has already been produced.

Trigger: The corresponding public status-pane state first becomes active with its owning ability or action requirements satisfied: smithing.

Sources: `src/sdl/ui/sdl-panes.c`, `src/tutorial/tutorial-game.c`, `src/player/player-song-duels.c`, `src/cmd/combat/cmd-combat.c`.

## Fletching in progress

`status.fletching`

Level: **Extended**.

**1. Info**

{detail} Arrow crafting or improvement is in progress. Each completed unit keeps its normal time and material rules. Check the actual quantity and quality after an interruption.

Trigger: The corresponding public status-pane state first becomes active with its owning ability or action requirements satisfied: fletching.

Sources: `src/sdl/ui/sdl-panes.c`, `src/tutorial/tutorial-game.c`, `src/player/player-song-duels.c`, `src/cmd/combat/cmd-combat.c`.

## Resting

`status.resting`

Level: **Normal**.

**1. Info**

{detail} Rest repeats game turns until its chosen goal is met or it is interrupted. Poison, bleeding and starvation prevent ordinary Health regeneration; singing prevents Voice regeneration.

Trigger: The corresponding public status-pane state first becomes active with its owning ability or action requirements satisfied: resting.

Sources: `src/sdl/ui/sdl-panes.c`, `src/tutorial/tutorial-game.c`, `src/player/player-song-duels.c`, `src/cmd/combat/cmd-combat.c`.

## Repeating a command

`status.repeat`

Level: **Normal**.

**1. Info**

{detail} A command is set to repeat for the displayed remaining count. Each successful repetition is a real action. The repetition can stop when the context changes; the tutorial does not replay a cancelled action.

Trigger: The corresponding public status-pane state first becomes active with its owning ability or action requirements satisfied: repeat.

Sources: `src/sdl/ui/sdl-panes.c`, `src/tutorial/tutorial-game.c`, `src/player/player-song-duels.c`, `src/cmd/combat/cmd-combat.c`.

## A mortal wound

`status.mortal_wound`

Level: **Normal**.

**1. Info**

{detail} Bleeding severity is above 100. The next damage tick is ceil(severity / 5) Health, and ordinary Health regeneration is blocked. Healing consumables halve bleeding rather than always stopping it. Read the actual current severity and known remedies before acting.

Trigger: The public status predicate becomes active: cut > 100.

Sources: `src/sdl/ui/sdl-panes.c`, `src/tutorial/tutorial-game.c`, `src/dungeon/dungeon-player.c`.

## On the run

`status.on_the_run`

Level: **Normal**.

**1. Info**

{detail} Your current expedition is in the escape phase. Read the live main objective, route and any changed pursuit or staircase restrictions. This state is different from automatic running movement; it does not mean your hero is currently following a path.

Trigger: The public status predicate becomes active: on_the_run.

Sources: `src/sdl/ui/sdl-panes.c`, `src/tutorial/tutorial-game.c`, `src/dungeon/dungeon-player.c`.
