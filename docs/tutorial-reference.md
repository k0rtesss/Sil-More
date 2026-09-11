# Gameplay tutorial reference

This continuous document contains the authored lessons from `lib/help/tutorials.json`. The UI owns wrapping and pagination. `{subject}`, `{detail}` and `{context}` remain runtime placeholders; they contain only information already available to the hero.

Info and decision explanations use Continue. Required action steps complete only after the matching real action commits. Reading, skipping and reviewing are free; game actions retain their normal costs and consequences. The archive turns every step into a read-only explanation.

The catalogue contains 521 lessons, including 107 ability previews. Every live ability serial, item kind handled by the aware-effect producer and meaningful public terrain serial has a checked entry. Equivalent terrain variants share an automatic lesson; their old entries remain for saved archive history. This checks source/data coverage, not physical-device interaction.

## Resource route

`src/init/init-paths.c` resolves `ANGBAND_DIR_HELP` from the installed data root. `src/tutorial/tutorial.c` lazily reads `tutorials.json` from that directory. `build-cmake.bat` stages Help for both Windows deployments; `CMakeLists.txt` includes Help JSON in iOS resources; `android/app/build.gradle` synchronizes the game library into Android assets before building. The validator checks these source routes; this is not confirmation of a device installation.

## Tutorial modes

Default: **Extended**. The catalogue has **153 Normal** lessons and **368 Extended** lessons. The card's single mode button cycles **Disabled → Normal → Extended → Disabled**.

Normal covers core controls, survival, general item handling and its complete action chains, storage, main menus, combat fundamentals and Tale events. Extended includes all Normal lessons and adds individual abilities and item effects, learned monster traits, terrain and region details, individual quest introductions and specialist status or knowledge pages.

Filtering a lesson never marks it learned or skipped. Lowering the mode withdraws an Extended card and pending Extended observations while keeping completed steps in the Tale history. The learned archive remains read-only and can show encountered lessons from either level, including when gameplay tutorials are Disabled.

Characters deferred during an upgrade remain exempt from automatic tutorials for that character. Changing mode or resetting the Tale lesson history does not bypass that gate. The upgrade notice belongs to its own native UI and is independent of catalogue filtering.

Skeletons and chests use Normal informational feature lessons. Their old generic item IDs remain untouched if present in saved history, and they do not enter examine/equip/use tutorial chains.

## Presentation order

Cards are selected by descending priority at a safe input boundary, not by their position in this document. Equal priorities keep observation order. A higher-priority card can interrupt between steps; the earlier lesson resumes at its saved step when its context is still relevant. Menu and purchase explanations take focus at their owning input boundary so they precede the choice they describe. Reading does not advance game time.

Gameplay observations are checked again against current conditions, reachable items and visible subjects. Expired observations are withdrawn without marking them completed or skipped; a fresh encounter can offer them again. Required actions complete only after the matching real action succeeds or commits.

## Your first steps

`opening.move`

Level: **Normal**.

Priority: **70** (higher appears first).

**1. Info**

Welcome to Sil-More. Your goal is to recover a Silmaril and escape Angband. Explore carefully and prepare before a fight. The world waits while you read. Continue advances a card; Skip ends its lesson. The mode button switches between Disabled, Normal and Extended tutorials.

**2. Info**

Before fighting, prepare a weapon and armour that suit your hero and oath. Items in your Pack are stored; items in your Harness are ready to reach. Your active weapon is the one you attack with. Item lessons will guide you through these choices as you find equipment.

**3. Action**

Move one square onto safe open floor. Choose any legal direction. Moving spends time, so nearby creatures can act. Skip lets you leave this practice and choose another action.

Required action: `move`.

Trigger: A new Story hero starts at playerturn <= 1, excluding restored saves.

Sources: `src/tutorial/tutorial-game.c`, `src/quest/quest-status.c`.

## A creature in sight

`combat.first_monster`

Level: **Normal**.

Priority: **65** (higher appears first).

**1. Info**

{subject} is in sight. A sleeping or unwary creature has not yet become alert to you; an alert one has noticed a threat. Morale is separate: it determines whether a creature fights or flees. Use Look to read its current state and learned attacks before approaching.

Trigger: A non-peaceful creature is visible in line of sight and the player is not hallucinating. Attack legality is not required.

Sources: `src/melee/melee-process.c`, `src/tutorial/tutorial-game.c`.

## Make one melee attack

`combat.first_adjacent`

Level: **Normal**.

Priority: **45** (higher appears first).

**1. Info**

{subject} is beside you. Moving toward an adjacent enemy makes a melee attack. Check your active weapon and remaining Health first.

**2. Action**

Attack an adjacent enemy once. A miss also completes this practice. The enemy can retaliate. Skip if you would rather retreat, use an item or choose another tactic.

Required action: `attack`; subject: `monster`.

Trigger: A visible adjacent hostile is legally attackable, melee is active, and fear/confusion/truce/oath checks permit attack.

Sources: `src/tutorial/tutorial-game.c`.

## Read the attack result

`combat.first_result`

Level: **Normal**.

Priority: **55** (higher appears first).

**1. Info**

{detail} Each side rolls a twenty-sided die. Your Attack plus its roll must exceed the target's Evasion plus its roll; a tie misses. With equal scores, you hit slightly less than half the time.

**2. Info**

On a hit, roll your damage, then subtract the target's Protection roll. Armour can block all the damage. For example, 8 damage against 3 Protection removes 5 Health. Combat history shows the rolls.

**3. Info**

Beating Evasion by a large margin can add critical damage dice. Lighter weapons need smaller margins. For example, a 3 lb melee weapon normally gains one extra die at a margin of 10 and two at 20; Finesse lowers those margins to 8 and 16.

Trigger: After a committed player attack, including a miss.

Sources: `src/cmd/combat/cmd-combat.c`, `src/melee/melee-attack.c`.

## You lost Health

`combat.first_damage`

Level: **Normal**.

Priority: **75** (higher appears first).

**1. Info**

{detail} Check your remaining Health and any new conditions. Retreating, using a known remedy, or limiting how many enemies can reach you may help. Healing restores Health; poison and bleeding may need treatment as well.

Trigger: Explicit public observation at a safe player or menu boundary.

Sources: `src/tutorial/tutorial-game.c`.

## A fleeing enemy

`combat.fleeing`

Level: **Normal**.

Priority: **50** (higher appears first).

**1. Info**

{subject} is fleeing. This can give you time to recover or leave. Chasing may lead you into other enemies, and a fleeing creature can regain courage. Oath of Valour forbids harming fleeing foes.

Trigger: Explicit public observation at a safe player or menu boundary.

Sources: `src/melee/melee-process.c`, `src/cmd/combat/cmd-combat.c`, `lib/edit/oath.txt`.

## Several adjacent foes

`combat.surrounded`

Level: **Normal**.

Priority: **85** (higher appears first).

**1. Info**

Several enemies are beside you. They gain an attack bonus for surrounding you. A doorway or narrow passage can limit how many reach you at once. Crowd Fighting halves their surrounding bonus.

Trigger: Explicit public observation at a safe player or menu boundary.

Sources: `src/cmd/combat/cmd-combat.c`.

## Critical hits

`combat.critical`

Level: **Normal**.

Priority: **55** (higher appears first).

**1. Info**

A critical hit adds damage dice. Lighter weapons need smaller winning margins to earn them. Finesse lowers the melee critical interval by 2; Power raises it by 1. Subtlety lowers it by another 2 when its weapon and free-hand requirements are met.

Trigger: Explicit public observation at a safe player or menu boundary.

Sources: `src/cmd/combat/cmd-combat.c`.

## A new level

`world.depth`

Level: **Normal**.

Priority: **30** (higher appears first).

**1. Info**

{detail} Stairs and shafts generate a new map when you change depth. Leaving a level abandons its current layout and remaining items. Check your objective and supplies first; returning to that depth generates another map.

Trigger: Explicit public observation at a safe player or menu boundary.

Sources: `src/cmd/movement/cmd-movement.c`.

## Minimum depth advances

`world.minimum_depth`

Level: **Normal**.

Priority: **60** (higher appears first).

**1. Info**

{detail} Minimum depth is the shallowest depth you can normally reach. It rises as you spend game time. An up staircase may return you at the same depth or deeper if its destination is above that limit. Reading menus and descriptions is free; resting and running count toward the clock.

Trigger: Explicit public observation at a safe player or menu boundary.

Sources: `src/dungeon/dungeon-loop.c`, `src/cmd/movement/cmd-movement.c`.

## Read an item before using it

`item.first_description`

Level: **Normal**.

Priority: **25** (higher appears first).

**1. Info**

{subject} is nearby or in your belongings. Its description shows what your hero knows, including useful effects and drawbacks. Inspect unfamiliar equipment or consumables before deciding what to do with them.

**2. Action**

Examine the item through Look, Inventory or Equipment. Reading its description is free. You can leave without picking up, equipping or using it.

Required action: `examine`.

Trigger: Explicit public observation at a safe player or menu boundary.

Sources: `src/tutorial/tutorial-game.c`.

## Understanding the description

`item.description`

Level: **Normal**.

Priority: **27** (higher appears first).

**1. Info**

{detail} Damage written as 2d5 means roll two five-sided dice, for 2 to 10 damage before Protection. Protection such as 1d3 blocks 1 to 3 damage when it applies. Also compare Attack, Evasion, weight, volume and known bonuses or drawbacks.

Trigger: Explicit public observation at a safe player or menu boundary.

Sources: `src/object/object-info.c`, `src/object/object-desc.c`.

## Melee weapons

`item.weapon`

Level: **Normal**.

Priority: **24** (higher appears first).

**1. Info**

Compare damage, Attack, Evasion and weight. Strength can increase damage, within the weapon's weight limit. Lighter weapons earn critical dice more easily. Ready the weapon, then select it in Change Active to use it for melee.

Trigger: The item type is publicly encountered; no hidden subtype or property is used.

Sources: `src/object/object-info.c`, `src/object/object-inventory.c`, `src/tutorial/tutorial-game.c`.

## Throwing weapons

`item.throwing`

Level: **Normal**.

Priority: **24** (higher appears first).

**1. Info**

Throwing weapons use Melee for accuracy, with penalties for distance. Ready one in your Harness, then select it in Change Active before throwing normally. Some abilities also allow quick throws without changing your active weapon.

Trigger: The item type is publicly encountered; no hidden subtype or property is used.

Sources: `src/object/object-info.c`, `src/object/object-inventory.c`, `src/tutorial/tutorial-game.c`.

## Bows

`item.bow`

Level: **Normal**.

Priority: **24** (higher appears first).

**1. Info**

Ready a bow and arrows, then choose the bow in Change Active. Archery determines accuracy; Strength and bow weight affect damage. Firing beside alert enemies can give them free attacks. Point Blank Archery protects you from the adjacent target only.

Trigger: The item type is publicly encountered; no hidden subtype or property is used.

Sources: `src/object/object-info.c`, `src/object/object-inventory.c`, `src/tutorial/tutorial-game.c`.

## Arrows and the Quiver

`item.arrows`

Level: **Normal**.

Priority: **24** (higher appears first).

**1. Info**

Ready arrows in the Quiver to fire them with a bow. Spare arrows can be stored in the Pack. When you have more than one ready stack, Change Active lets you select which to fire. Changing only the arrow stack is free.

Trigger: The item type is publicly encountered; no hidden subtype or property is used.

Sources: `src/object/object-info.c`, `src/object/object-inventory.c`, `src/tutorial/tutorial-game.c`.

## Armour

`item.armour`

Level: **Normal**.

Priority: **24** (higher appears first).

**1. Info**

Evasion helps you avoid hits; Protection reduces damage after a hit. Compare both values, along with weight and skill penalties. Heavy armour can prevent abilities that require light armour. Elemental attacks use different Protection rules.

Trigger: The item type is publicly encountered; no hidden subtype or property is used.

Sources: `src/object/object-info.c`, `src/object/object-inventory.c`, `src/tutorial/tutorial-game.c`.

## Shields

`item.shield`

Level: **Normal**.

Priority: **24** (higher appears first).

**1. Info**

A shield helps only when your active weapon leaves a hand for it. Two-handed weapons normally prevent shield use. With Blocking, your shield provides double Protection if you did not move on your previous turn.

Trigger: The item type is publicly encountered; no hidden subtype or property is used.

Sources: `src/object/object-info.c`, `src/object/object-inventory.c`, `src/tutorial/tutorial-game.c`.

## Rings

`item.ring`

Level: **Normal**.

Priority: **24** (higher appears first).

**1. Info**

Rings can improve attributes, skills or resistances, but may have drawbacks. Read the known properties before equipping one. A cursed ring can resist removal, and an unidentified ring may have properties you have not discovered yet.

Trigger: The item type is publicly encountered; no hidden subtype or property is used.

Sources: `src/object/object-info.c`, `src/object/object-inventory.c`, `src/tutorial/tutorial-game.c`.

## Amulets

`item.amulet`

Level: **Normal**.

Priority: **24** (higher appears first).

**1. Info**

Amulets can grant bonuses, resistances or special abilities. Read their drawbacks too. The Jewelry menu lets you equip an amulet and organise saved sets of rings and amulets.

Trigger: The item type is publicly encountered; no hidden subtype or property is used.

Sources: `src/object/object-info.c`, `src/object/object-inventory.c`, `src/tutorial/tutorial-game.c`.

## Potions

`item.potion`

Level: **Normal**.

Priority: **24** (higher appears first).

**1. Info**

A known potion's description explains its effect. Some heal, cure a condition or grant a temporary bonus; others harm you. An unfamiliar potion is a gamble. Keep useful known potions available for the situation they treat.

Trigger: The item type is publicly encountered; no hidden subtype or property is used.

Sources: `src/object/object-info.c`, `src/object/object-inventory.c`, `src/tutorial/tutorial-game.c`.

## Food and herbs

`item.food`

Level: **Normal**.

Priority: **24** (higher appears first).

**1. Info**

Food restores nourishment. Herbs also have special effects, such as healing, restoring drained attributes or causing harm. Read a known herb's effect before eating it. Hunger, wounds and attribute drain are treated differently.

Trigger: The item type is publicly encountered; no hidden subtype or property is used.

Sources: `src/object/object-info.c`, `src/object/object-inventory.c`, `src/tutorial/tutorial-game.c`.

## Light sources

`item.light`

Level: **Normal**.

Priority: **24** (higher appears first).

**1. Info**

A light's radius is how far it reaches; intensity is how bright it is. Torches and lanterns also need fuel. Check remaining fuel before exploring, and keep a compatible refill or replacement available.

Trigger: The item type is publicly encountered; no hidden subtype or property is used.

Sources: `src/object/object-info.c`, `src/object/object-inventory.c`, `src/tutorial/tutorial-game.c`.

## Oil containers

`item.oil`

Level: **Normal**.

Priority: **24** (higher appears first).

**1. Info**

Oil is stored in a Supplies oil slot. One container holds up to 2,500 turns of lantern fuel. Use Refuel with a suitable lantern; torches use a different refuelling method.

Trigger: The item type is publicly encountered; no hidden subtype or property is used.

Sources: `src/object/object-info.c`, `src/object/object-inventory.c`, `src/tutorial/tutorial-game.c`.

## Gems

`item.gem`

Level: **Normal**.

Priority: **24** (higher appears first).

**1. Info**

Gems are single-use items with effects such as mapping, detection, identification or warding. Read the known effect before using one. If a gem asks you to choose an item, cancelling that choice preserves the gem.

Trigger: The item type is publicly encountered; no hidden subtype or property is used.

Sources: `src/object/object-info.c`, `src/object/object-inventory.c`, `src/tutorial/tutorial-game.c`.

## Digging tools

`item.digging`

Level: **Normal**.

Priority: **24** (higher appears first).

**1. Info**

A digging tool helps clear rubble and dig through suitable walls. Select the terrain's tunnelling action. Digging can take several turns and make noise, so check for nearby enemies before starting.

Trigger: The item type is publicly encountered; no hidden subtype or property is used.

Sources: `src/object/object-info.c`, `src/object/object-inventory.c`, `src/tutorial/tutorial-game.c`.

## Smithing metal

`item.metal`

Level: **Normal**.

Priority: **24** (higher appears first).

**1. Info**

Mithril and star iron are smithing materials. Keep them if you plan to forge or improve equipment. A forge, the relevant abilities and enough Smithing are still needed; the smithing preview lists the materials and other costs.

Trigger: The item type is publicly encountered; no hidden subtype or property is used.

Sources: `src/object/object-info.c`, `src/object/object-inventory.c`, `src/tutorial/tutorial-game.c`.

## Opening chests

`world.chest`

Level: **Normal**.

Priority: **35** (higher appears first).

**1. Info**

A chest is opened on the dungeon floor. Use its contextual menu to inspect it, disarm a discovered trap, or open it. Those actions spend turns, and failed checks can be dangerous. Opening releases its contents; an empty chest cannot store your items.

Trigger: A chest is actually encountered on the player square or visibly adjacent, including an empty chest.

Sources: `src/cmd/world/cmd-interact-chest.c`, `src/cmd/item/cmd-item-core.c`, `src/tutorial/tutorial-game.c`.

## Searching skeletons

`world.skeleton`

Level: **Normal**.

Priority: **35** (higher appears first).

**1. Info**

Use the skeleton's contextual Search action on the dungeon floor. Each skeleton can be searched once and may contain supplies, damaged gear, a note, or nothing. Searching orc remains has a 5% chance of disease. Skeleton hints have their own setting and can be reread later.

Trigger: Skeleton remains are actually encountered on the player square or visibly adjacent, including already-searched remains.

Sources: `src/cmd/world/cmd-interact-chest.c`, `src/cmd/item/cmd-item-core.c`, `src/tutorial/tutorial-game.c`.

## Handling items

`item.item`

Level: **Normal**.

Priority: **24** (higher appears first).

**1. Info**

Read the item's description and available actions. Pack, Harness, Quiver, Supplies and worn equipment have different purposes and capacities. The item menu shows where this item can go and what you can do with it.

Trigger: The item type is publicly encountered; no hidden subtype or property is used.

Sources: `src/object/object-info.c`, `src/object/object-inventory.c`, `src/tutorial/tutorial-game.c`.

## Staves

`item.staff`

Level: **Normal**.

Priority: **24** (higher appears first).

**1. Info**

Staves spend charges to produce their known effect. Ready one in the Harness for prompt access; using one from the Pack takes extra handling time. Read the effect and remaining charges before using it. You can skip practice to save charges.

Trigger: The item type is publicly encountered; no hidden subtype or property is used.

Sources: `src/object/object-info.c`, `src/object/object-inventory.c`, `src/tutorial/tutorial-game.c`.

## Horns

`item.horn`

Level: **Normal**.

Priority: **24** (higher appears first).

**1. Info**

Horns spend Voice and make a loud noise. Read the known effect, then aim in the desired direction. They normally cost 20 Voice, or 10 with Channeling. Sounding a horn can alert other creatures.

Trigger: The item type is publicly encountered; no hidden subtype or property is used.

Sources: `src/object/object-info.c`, `src/object/object-inventory.c`, `src/tutorial/tutorial-game.c`.

## Ready a staff

`item.staff.ready`

Level: **Normal**.

Priority: **28** (higher appears first).

**1. Action**

Open Inventory, select the staff, and choose Ready to move it to your Harness. Reaching into the Pack takes three turns and attacks can interrupt it. Skip if you want to keep your current arrangement.

Required action: `ready`; subject: `staff`.

Trigger: A carried, currently usable staff can legally be readied; its examination lesson is already completed.

Sources: `src/tutorial/tutorial-game.c`.

## Use a known staff

`item.staff.use`

Level: **Normal**.

Priority: **29** (higher appears first).

**1. Info**

This identified staff is ready and has enough charge for a useful effect. {detail} Skip if you would rather save its charges.

**2. Action**

Use the known staff once, choosing a target if requested. A completed use finishes the practice. You can cancel the choice or Skip to keep its charge.

Required action: `use-item`; subject: `staff`.

Trigger: A fully identified readied staff has a known available charge and a useful, legal current target or effect; unsafe or unknown uses are not offered.

Sources: `src/tutorial/tutorial-game.c`.

## Ready a horn

`item.horn.ready`

Level: **Normal**.

Priority: **28** (higher appears first).

**1. Action**

Open Inventory, select the horn, and choose Ready to move it to your Harness. Reaching into the Pack takes three turns and attacks can interrupt it. Skip if you want to keep your current arrangement.

Required action: `ready`; subject: `horn`.

Trigger: A carried, currently usable horn can legally be readied; its examination lesson is already completed.

Sources: `src/tutorial/tutorial-game.c`.

## Use a known horn

`item.horn.use`

Level: **Normal**.

Priority: **29** (higher appears first).

**1. Info**

This known horn is ready and you have enough Voice for it. {detail} Skip if you prefer to save Voice or avoid the noise.

**2. Action**

Sound the known horn once in a useful direction. A completed use finishes the practice. You can cancel the choice or Skip to save your Voice.

Required action: `use-item`; subject: `horn`.

Trigger: A known readied horn has resources and a useful, legal current target or effect; unsafe/unknown uses are not offered.

Sources: `src/tutorial/tutorial-game.c`.

## A known artefact

`item.artefact`

Level: **Normal**.

Priority: **32** (higher appears first).

**1. Info**

{subject} is an artefact with its own combination of properties. Check its abilities, bonuses, drawbacks and equipment requirements. Even a powerful artefact may not suit your hero.

Trigger: Explicit public observation at a safe player or menu boundary.

Sources: `src/object/object-info.c`.

## New item knowledge

`identification.item`

Level: **Normal**.

Priority: **45** (higher appears first).

**1. Info**

{detail} Open the updated item description to see what you learned. Discovering one property can still leave others unknown.

Trigger: Explicit public observation at a safe player or menu boundary.

Sources: `src/cmd/item/cmd-identify.c`, `src/cmd/combat/cmd-combat.c`.

## Resistance revealed

`identification.elemental`

Level: **Normal**.

Priority: **45** (higher appears first).

**1. Info**

An elemental attack taught you something about an item or resistance. Check the updated description. Resistance reduces elemental damage; Protection is a separate defence with rules for each damage type.

Trigger: Explicit public observation at a safe player or menu boundary.

Sources: `src/cmd/item/cmd-identify.c`, `src/cmd/combat/cmd-combat.c`.

## A weapon property revealed

`identification.brand`

Level: **Normal**.

Priority: **45** (higher appears first).

**1. Info**

Your attack revealed a weapon property. A brand adds an elemental effect; a slay is effective against particular creature types. Read the updated description for the targets it affects.

Trigger: Explicit public observation at a safe player or menu boundary.

Sources: `src/cmd/item/cmd-identify.c`, `src/cmd/combat/cmd-combat.c`.

## Diseased

`status.diseased`

Level: **Normal**.

Priority: **95** (higher appears first).

**1. Info**

Disease immediately lowers Constitution by 1, then lowers a random attribute by 1 every 50 player turns. It continues while resting. A potion of Healing or Miruvor cures it and restores the points disease took. Herbs of Healing and Restoration do not cure disease.

Trigger: The disease condition is active and its lesson is unseen or in progress, or disease has just appeared.

Sources: `src/player/effects.c`, `src/dungeon/dungeon-loop.c`, `src/use-obj.c`.

## Poisoned

`status.poisoned`

Level: **Normal**.

Priority: **95** (higher appears first).

**1. Info**

Poison deals damage over time and prevents ordinary Health regeneration. The next tick removes one fifth of current poison severity, rounded up, as Health. Antidote or Miruvor removes the poison; you may still need healing afterward.

Trigger: The public condition becomes active, or the displayed resource crosses its warning threshold.

Sources: `src/player/effects.c`, `src/dungeon/dungeon-loop.c`, `src/use-obj.c`.

## Bleeding

`status.cut`

Level: **Normal**.

Priority: **95** (higher appears first).

**1. Info**

Bleeding deals damage over time and prevents ordinary Health regeneration. The next tick removes one fifth of current bleeding severity, rounded up, as Health. Healing herbs or potions and Miruvor halve bleeding. Song of Staunching stops it when its effect occurs.

Trigger: The public condition becomes active, or the displayed resource crosses its warning threshold.

Sources: `src/player/effects.c`, `src/dungeon/dungeon-loop.c`, `src/use-obj.c`.

## Stunned

`status.stun`

Level: **Normal**.

Priority: **95** (higher appears first).

**1. Info**

Stun lowers every skill by 2, or by 4 at severity 50 or more. Above 100, you are knocked out and cannot act. Clarity or Miruvor removes stun; use it while you can still act.

Trigger: The public condition becomes active, or the displayed resource crosses its warning threshold.

Sources: `src/player/effects.c`, `src/dungeon/dungeon-loop.c`, `src/use-obj.c`.

## Afraid

`status.afraid`

Level: **Normal**.

Priority: **80** (higher appears first).

**1. Info**

Fear prevents normal melee attacks and proper ranged aiming. Retreat or use a known fear remedy if possible. Miruvor and Orcish Liquor remove fear, though Orcish Liquor can also stun you.

Trigger: The public condition becomes active, or the displayed resource crosses its warning threshold.

Sources: `src/player/effects.c`, `src/dungeon/dungeon-loop.c`, `src/use-obj.c`.

## Confused

`status.confused`

Level: **Normal**.

Priority: **80** (higher appears first).

**1. Info**

Confusion makes movement and aiming unreliable. Clarity or Miruvor cures it. Choose carefully: a direction you enter may lead to a different action while confused.

Trigger: The public condition becomes active, or the displayed resource crosses its warning threshold.

Sources: `src/player/effects.c`, `src/dungeon/dungeon-loop.c`, `src/use-obj.c`.

## Blind

`status.blind`

Level: **Normal**.

Priority: **80** (higher appears first).

**1. Info**

You are blind. Remembered map squares do not show where creatures are now. A potion of True Sight or Miruvor cures blindness. Refuelling a lamp will not restore your sight.

Trigger: The public condition becomes active, or the displayed resource crosses its warning threshold.

Sources: `src/player/effects.c`, `src/dungeon/dungeon-loop.c`, `src/use-obj.c`.

## Hallucinating

`status.image`

Level: **Normal**.

Priority: **80** (higher appears first).

**1. Info**

Hallucination makes the displayed identities of creatures and items unreliable. Clarity, True Sight or Miruvor removes it. Treat apparent names and shapes with caution until it ends.

Trigger: The public condition becomes active, or the displayed resource crosses its warning threshold.

Sources: `src/player/effects.c`, `src/dungeon/dungeon-loop.c`, `src/use-obj.c`.

## Entranced

`status.entranced`

Level: **Normal**.

Priority: **95** (higher appears first).

**1. Info**

You are entranced and cannot act until the trance ends. You cannot choose or drink a remedy during it. Continue closes the explanation so the game can resume.

Trigger: The public condition becomes active, or the displayed resource crosses its warning threshold.

Sources: `src/player/effects.c`, `src/dungeon/dungeon-loop.c`, `src/use-obj.c`.

## Slowed

`status.slow`

Level: **Normal**.

Priority: **80** (higher appears first).

**1. Info**

You are slowed and act less often relative to enemies. Quickness can offset the speed loss while it lasts, though the Slow condition remains. Recheck nearby threats before spending a turn.

Trigger: The public condition becomes active, or the displayed resource crosses its warning threshold.

Sources: `src/player/effects.c`, `src/dungeon/dungeon-loop.c`, `src/use-obj.c`.

## Fast

`status.fast`

Level: **Normal**.

Priority: **80** (higher appears first).

**1. Info**

You are moving faster and can act more often relative to enemies. The benefit lasts only while Fast is active. Keep enough room to retreat when it expires.

Trigger: The public condition becomes active, or the displayed resource crosses its warning threshold.

Sources: `src/player/effects.c`, `src/dungeon/dungeon-loop.c`, `src/use-obj.c`.

## Rage

`status.rage`

Level: **Normal**.

Priority: **80** (higher appears first).

**1. Info**

Rage gives +1 Strength and Constitution, -1 Dexterity and Grace, fear resistance and a special melee attack. It limits your awareness and prevents Stealth mode. Clarity ends rage.

Trigger: The public condition becomes active, or the displayed resource crosses its warning threshold.

Sources: `src/player/effects.c`, `src/dungeon/dungeon-loop.c`, `src/use-obj.c`.

## Your light is dimmed

`status.darkened`

Level: **Normal**.

Priority: **80** (higher appears first).

**1. Info**

Your light has been dimmed by an effect. This reduces illumination even if your lamp has fuel. Check the Darkened condition and nearby visibility; refuelling alone does not remove it.

Trigger: The public condition becomes active, or the displayed resource crosses its warning threshold.

Sources: `src/player/effects.c`, `src/dungeon/dungeon-loop.c`, `src/use-obj.c`.

## Temporary Strength

`status.tmp_str`

Level: **Extended**.

Priority: **80** (higher appears first).

**1. Info**

Temporary Strength improves melee and ranged damage within weapon weight limits, and protects Strength from ordinary drain while active. Check the attribute breakdown to see the bonus and any existing drain.

Trigger: The public condition becomes active, or the displayed resource crosses its warning threshold.

Sources: `src/player/effects.c`, `src/dungeon/dungeon-loop.c`, `src/use-obj.c`.

## Temporary Dexterity

`status.tmp_dex`

Level: **Extended**.

Priority: **80** (higher appears first).

**1. Info**

Temporary Dexterity improves Dexterity-based skills and protects Dexterity from ordinary drain while active. The bonus ends when the effect expires.

Trigger: The public condition becomes active, or the displayed resource crosses its warning threshold.

Sources: `src/player/effects.c`, `src/dungeon/dungeon-loop.c`, `src/use-obj.c`.

## Temporary Constitution

`status.tmp_con`

Level: **Extended**.

Priority: **80** (higher appears first).

**1. Info**

Temporary Constitution raises maximum Health and protects Constitution from ordinary drain while active. Check both current and maximum Health, especially before the bonus expires.

Trigger: The public condition becomes active, or the displayed resource crosses its warning threshold.

Sources: `src/player/effects.c`, `src/dungeon/dungeon-loop.c`, `src/use-obj.c`.

## Temporary Grace

`status.tmp_gra`

Level: **Extended**.

Priority: **80** (higher appears first).

**1. Info**

Temporary Grace improves Grace-based skills and maximum Voice. It also protects Grace from ordinary drain while active. The extra Voice capacity ends with the bonus.

Trigger: The public condition becomes active, or the displayed resource crosses its warning threshold.

Sources: `src/player/effects.c`, `src/dungeon/dungeon-loop.c`, `src/use-obj.c`.

## Temporary Perception

`status.tmp_per`

Level: **Extended**.

Priority: **80** (higher appears first).

**1. Info**

Temporary Perception improves your Perception skill by 10. This helps spotting creatures, noticing danger and other Perception checks until the effect expires.

Trigger: The public condition becomes active, or the displayed resource crosses its warning threshold.

Sources: `src/player/effects.c`, `src/dungeon/dungeon-loop.c`, `src/use-obj.c`.

## True sight

`status.tim_invis`

Level: **Extended**.

Priority: **80** (higher appears first).

**1. Info**

True Sight improves detection of invisible creatures and provides resistance to blindness and hallucination. Invisible enemies are still subject to detection checks, range and line of sight.

Trigger: The public condition becomes active, or the displayed resource crosses its warning threshold.

Sources: `src/player/effects.c`, `src/dungeon/dungeon-loop.c`, `src/use-obj.c`.

## Temporary fire resistance

`status.oppose_fire`

Level: **Extended**.

Priority: **80** (higher appears first).

**1. Info**

Temporary fire resistance adds one layer of fire resistance. It reduces incoming fire damage while active. Check your total resistance: vulnerability and a fire cave can offset a layer.

Trigger: The public condition becomes active, or the displayed resource crosses its warning threshold.

Sources: `src/player/effects.c`, `src/dungeon/dungeon-loop.c`, `src/use-obj.c`.

## Temporary cold resistance

`status.oppose_cold`

Level: **Extended**.

Priority: **80** (higher appears first).

**1. Info**

Temporary cold resistance adds one layer of cold resistance. It reduces incoming cold damage while active. Check your total resistance: vulnerability and an ice cave can offset a layer.

Trigger: The public condition becomes active, or the displayed resource crosses its warning threshold.

Sources: `src/player/effects.c`, `src/dungeon/dungeon-loop.c`, `src/use-obj.c`.

## Temporary poison resistance

`status.oppose_pois`

Level: **Extended**.

Priority: **80** (higher appears first).

**1. Info**

Temporary poison resistance reduces new poison exposure while active. Antidote removes poison already affecting you. A poison cave reduces your resistance, so check the current total.

Trigger: The public condition becomes active, or the displayed resource crosses its warning threshold.

Sources: `src/player/effects.c`, `src/dungeon/dungeon-loop.c`, `src/use-obj.c`.

## A lingering Challenge effect

`status.song_challenge_effect`

Level: **Extended**.

Priority: **80** (higher appears first).

**1. Info**

{detail} A song has left a temporary Challenge effect. Read the current skill breakdown to see the active modifier. The effect and its remaining duration are separate from the song you are currently singing.

Trigger: The public condition becomes active, or the displayed resource crosses its warning threshold.

Sources: `src/player/effects.c`, `src/dungeon/dungeon-loop.c`, `src/use-obj.c`.

## A lingering Elbereth effect

`status.song_elbereth_effect`

Level: **Extended**.

Priority: **80** (higher appears first).

**1. Info**

{detail} A song has left a temporary Elbereth effect. Inspect your current skill breakdown and condition. A lingering effect can remain distinct from the currently selected song.

Trigger: The public condition becomes active, or the displayed resource crosses its warning threshold.

Sources: `src/player/effects.c`, `src/dungeon/dungeon-loop.c`, `src/use-obj.c`.

## Low Health

`status.health`

Level: **Normal**.

Priority: **95** (higher appears first).

**1. Info**

{detail} Your Health has reached the warning level set in Settings. Consider a known healing item or a retreat before another hit. If you are also poisoned or bleeding, check whether the chosen item treats that condition.

Trigger: The public condition becomes active, or the displayed resource crosses its warning threshold.

Sources: `src/player/effects.c`, `src/dungeon/dungeon-loop.c`, `src/use-obj.c`.

## Low Voice

`status.voice`

Level: **Normal**.

Priority: **80** (higher appears first).

**1. Info**

{detail} Voice powers songs and horns. It recovers while you are not singing. A potion of Voice or Miruvor restores it fully; Esgalduin restores one quarter of maximum Voice.

Trigger: The public condition becomes active, or the displayed resource crosses its warning threshold.

Sources: `src/player/effects.c`, `src/dungeon/dungeon-loop.c`, `src/use-obj.c`.

## Hungry

`status.hungry`

Level: **Normal**.

Priority: **80** (higher appears first).

**1. Info**

{detail} You are getting hungry. Check your food supplies before hunger weakens you. Known food restores nourishment; some herbs also have harmful effects.

Trigger: The public condition becomes active, or the displayed resource crosses its warning threshold.

Sources: `src/player/effects.c`, `src/dungeon/dungeon-loop.c`, `src/use-obj.c`.

## Weak from hunger

`status.weak`

Level: **Normal**.

Priority: **80** (higher appears first).

**1. Info**

{detail} Hunger now reduces Strength. Eat known nourishing food when you can. A Strength potion may improve the attribute temporarily, but you still need food.

Trigger: The public condition becomes active, or the displayed resource crosses its warning threshold.

Sources: `src/player/effects.c`, `src/dungeon/dungeon-loop.c`, `src/use-obj.c`.

## Starving

`status.starving`

Level: **Normal**.

Priority: **95** (higher appears first).

**1. Info**

{detail} Starvation deals damage and prevents Health regeneration. Eat known nourishing food urgently. A healing item alone leaves the cause of the damage in place.

Trigger: The public condition becomes active, or the displayed resource crosses its warning threshold.

Sources: `src/player/effects.c`, `src/dungeon/dungeon-loop.c`, `src/use-obj.c`.

## A remedy for disease

`status.diseased.remedy`

Level: **Normal**.

Priority: **90** (higher appears first).

**1. Info**

Healing or Miruvor cures disease and restores the attribute points it took. Herbs of Healing and Restoration do not. A known remedy is available now. Skip if you prefer another action.

**2. Action**

Choose and use a known remedy for disease. Using a suitable item completes this practice. You can cancel the item choice or Skip the lesson to keep the resource.

Required action: `use-item`; subject: `diseased`.

Trigger: Disease is active, the player can act, and a carried aware item passes item_is_remedy for diseased.

Sources: `src/tutorial/tutorial-game.c`, `src/use-obj.c`.

## A remedy for poison

`status.poisoned.remedy`

Level: **Normal**.

Priority: **90** (higher appears first).

**1. Info**

Antidote or Miruvor removes poison. It stops future poison damage, though lost Health may still need healing. A known remedy is available now. Skip if you prefer another action.

**2. Action**

Choose and use a known remedy for poison. Using a suitable item completes this practice. You can cancel the item choice or Skip the lesson to keep the resource.

Required action: `use-item`; subject: `poisoned`.

Trigger: The condition is active, the player can act, and a carried aware item passes item_is_remedy for poisoned.

Sources: `src/tutorial/tutorial-game.c`, `src/use-obj.c`.

## A remedy for bleeding

`status.cut.remedy`

Level: **Normal**.

Priority: **90** (higher appears first).

**1. Info**

A herb or potion of Healing, or Miruvor, restores Health and halves current bleeding. Check the remaining bleeding afterward. A known remedy is available now. Skip if you prefer another action.

**2. Action**

Choose and use a known remedy for bleeding. Using a suitable item completes this practice. You can cancel the item choice or Skip the lesson to keep the resource.

Required action: `use-item`; subject: `cut`.

Trigger: The condition is active, the player can act, and a carried aware item passes item_is_remedy for cut.

Sources: `src/tutorial/tutorial-game.c`, `src/use-obj.c`.

## A remedy for stun

`status.stun.remedy`

Level: **Normal**.

Priority: **90** (higher appears first).

**1. Info**

Clarity or Miruvor removes stun. Acting before severity rises above 100 can prevent losing the chance to use a remedy. A known remedy is available now. Skip if you prefer another action.

**2. Action**

Choose and use a known remedy for stun. Using a suitable item completes this practice. You can cancel the item choice or Skip the lesson to keep the resource.

Required action: `use-item`; subject: `stun`.

Trigger: The condition is active, the player can act, and a carried aware item passes item_is_remedy for stun.

Sources: `src/tutorial/tutorial-game.c`, `src/use-obj.c`.

## A remedy for fear

`status.afraid.remedy`

Level: **Normal**.

Priority: **90** (higher appears first).

**1. Info**

Miruvor or Orcish Liquor removes fear. Orcish Liquor can also cause stun, so compare the available choices. A known remedy is available now. Skip if you prefer another action.

**2. Action**

Choose and use a known remedy for fear. Using a suitable item completes this practice. You can cancel the item choice or Skip the lesson to keep the resource.

Required action: `use-item`; subject: `afraid`.

Trigger: The condition is active, the player can act, and a carried aware item passes item_is_remedy for afraid.

Sources: `src/tutorial/tutorial-game.c`, `src/use-obj.c`.

## A remedy for confusion

`status.confused.remedy`

Level: **Normal**.

Priority: **90** (higher appears first).

**1. Info**

Clarity or Miruvor removes confusion so that movement and aiming become reliable again. A known remedy is available now. Skip if you prefer another action.

**2. Action**

Choose and use a known remedy for confusion. Using a suitable item completes this practice. You can cancel the item choice or Skip the lesson to keep the resource.

Required action: `use-item`; subject: `confused`.

Trigger: The condition is active, the player can act, and a carried aware item passes item_is_remedy for confused.

Sources: `src/tutorial/tutorial-game.c`, `src/use-obj.c`.

## A remedy for blindness

`status.blind.remedy`

Level: **Normal**.

Priority: **90** (higher appears first).

**1. Info**

True Sight or Miruvor cures blindness. True Sight also grants temporary sight protections. A known remedy is available now. Skip if you prefer another action.

**2. Action**

Choose and use a known remedy for blindness. Using a suitable item completes this practice. You can cancel the item choice or Skip the lesson to keep the resource.

Required action: `use-item`; subject: `blind`.

Trigger: The condition is active, the player can act, and a carried aware item passes item_is_remedy for blind.

Sources: `src/tutorial/tutorial-game.c`, `src/use-obj.c`.

## A remedy for hallucination

`status.image.remedy`

Level: **Normal**.

Priority: **90** (higher appears first).

**1. Info**

Clarity, True Sight or Miruvor removes hallucination and makes displayed identities reliable again. A known remedy is available now. Skip if you prefer another action.

**2. Action**

Choose and use a known remedy for hallucination. Using a suitable item completes this practice. You can cancel the item choice or Skip the lesson to keep the resource.

Required action: `use-item`; subject: `image`.

Trigger: The condition is active, the player can act, and a carried aware item passes item_is_remedy for image.

Sources: `src/tutorial/tutorial-game.c`, `src/use-obj.c`.

## A remedy for rage

`status.rage.remedy`

Level: **Normal**.

Priority: **90** (higher appears first).

**1. Info**

Clarity ends rage. You will lose its attribute bonuses as well as its restrictions. A known remedy is available now. Skip if you prefer another action.

**2. Action**

Choose and use a known remedy for rage. Using a suitable item completes this practice. You can cancel the item choice or Skip the lesson to keep the resource.

Required action: `use-item`; subject: `rage`.

Trigger: The condition is active, the player can act, and a carried aware item passes item_is_remedy for rage.

Sources: `src/tutorial/tutorial-game.c`, `src/use-obj.c`.

## A remedy for low health

`status.health.remedy`

Level: **Normal**.

Priority: **90** (higher appears first).

**1. Info**

A known healing item can restore Health. Compare the amount healed and its other effects; bleeding, poison or disease may need treatment too. A known remedy is available now. Skip if you prefer another action.

**2. Action**

Choose and use a known remedy for low Health. Using a suitable item completes this practice. You can cancel the item choice or Skip the lesson to keep the resource.

Required action: `use-item`; subject: `health`.

Trigger: The condition is active, the player can act, and a carried aware item passes item_is_remedy for health.

Sources: `src/tutorial/tutorial-game.c`, `src/use-obj.c`.

## A remedy for low voice

`status.voice.remedy`

Level: **Normal**.

Priority: **90** (higher appears first).

**1. Info**

Voice or Miruvor restores all Voice. Esgalduin restores one quarter of maximum Voice. Stop singing to allow ordinary Voice recovery. A known remedy is available now. Skip if you prefer another action.

**2. Action**

Choose and use a known remedy for low Voice. Using a suitable item completes this practice. You can cancel the item choice or Skip the lesson to keep the resource.

Required action: `use-item`; subject: `voice`.

Trigger: The condition is active, the player can act, and a carried aware item passes item_is_remedy for voice.

Sources: `src/tutorial/tutorial-game.c`, `src/use-obj.c`.

## A remedy for hunger

`status.hunger.remedy`

Level: **Normal**.

Priority: **90** (higher appears first).

**1. Info**

Known nourishing food can relieve hunger. Check any extra effects: Dried Meat carries a 20% disease risk, while harmful herbs can make your situation worse. A known remedy is available now. Skip if you prefer another action.

**2. Action**

Choose and use a known remedy for hunger. Using a suitable item completes this practice. You can cancel the item choice or Skip the lesson to keep the resource.

Required action: `use-item`; subject: `hunger`.

Trigger: The condition is active, the player can act, and a carried aware item passes item_is_remedy for hunger.

Sources: `src/tutorial/tutorial-game.c`, `src/use-obj.c`.

## A remedy for drained attributes

`status.drain.remedy`

Level: **Normal**.

Priority: **90** (higher appears first).

**1. Info**

Restoration restores up to 3 drained points in each attribute. Attribute potions have conditional restoration rules; read their description before choosing. A known remedy is available now. Skip if you prefer another action.

**2. Action**

Choose and use a known remedy for attribute drain. Using a suitable item completes this practice. You can cancel the item choice or Skip the lesson to keep the resource.

Required action: `use-item`; subject: `drain`.

Trigger: The condition is active, the player can act, and a carried aware item passes item_is_remedy for drain.

Sources: `src/tutorial/tutorial-game.c`, `src/use-obj.c`.

## Strength drain

`status.drain0`

Level: **Normal**.

Priority: **85** (higher appears first).

**1. Info**

{detail} Strength has been drained. A herb of Restoration restores up to 3 drained points in each attribute. A Strength potion restores 3 points immediately if that attribute is drained by at least 3; otherwise it grants a temporary bonus and restores up to 3 points when the bonus ends. Disease penalties need a disease cure.

Trigger: Explicit public observation at a safe player or menu boundary.

Sources: `src/use-obj.c`, `src/player/effects.c`.

## Dexterity drain

`status.drain1`

Level: **Normal**.

Priority: **85** (higher appears first).

**1. Info**

{detail} Dexterity has been drained. A herb of Restoration restores up to 3 drained points in each attribute. A Dexterity potion restores 3 points immediately if that attribute is drained by at least 3; otherwise it grants a temporary bonus and restores up to 3 points when the bonus ends. Disease penalties need a disease cure.

Trigger: Explicit public observation at a safe player or menu boundary.

Sources: `src/use-obj.c`, `src/player/effects.c`.

## Constitution drain

`status.drain2`

Level: **Normal**.

Priority: **85** (higher appears first).

**1. Info**

{detail} Constitution has been drained. A herb of Restoration restores up to 3 drained points in each attribute. A Constitution potion restores 3 points immediately if that attribute is drained by at least 3; otherwise it grants a temporary bonus and restores up to 3 points when the bonus ends. Disease penalties need a disease cure.

Trigger: Explicit public observation at a safe player or menu boundary.

Sources: `src/use-obj.c`, `src/player/effects.c`.

## Grace drain

`status.drain3`

Level: **Normal**.

Priority: **85** (higher appears first).

**1. Info**

{detail} Grace has been drained. A herb of Restoration restores up to 3 drained points in each attribute. A Grace potion restores 3 points immediately if that attribute is drained by at least 3; otherwise it grants a temporary bonus and restores up to 3 points when the bonus ends. Disease penalties need a disease cure.

Trigger: Explicit public observation at a safe player or menu boundary.

Sources: `src/use-obj.c`, `src/player/effects.c`.

## Abilities

`menu.abilities`

Level: **Normal**.

Priority: **40** (higher appears first).

**1. Info**

Select an ability to read its effect, required skill points, prerequisites and XP cost. Buying spends XP. Some learned abilities can be toggled; they work only while active and their equipment or situation requirements are met.

Trigger: The named menu is actually opened: abilities.

Sources: `src/tutorial/tutorial-game.c`.

## Skills and experience

`menu.skills`

Level: **Normal**.

Priority: **40** (higher appears first).

**1. Info**

Base skill points are the ranks you buy with XP. Attributes, equipment and effects add to the displayed total. Ability requirements can use invested ranks, so a temporary bonus may not qualify you for a purchase.

Trigger: The named menu is actually opened: skills.

Sources: `src/tutorial/tutorial-game.c`.

## Songs

`menu.songs`

Level: **Normal**.

Priority: **40** (higher appears first).

**1. Info**

Select a learned song to read its effect and Voice cost. Starting a song spends time, and singing uses Voice over time while preventing Voice recovery. You can stop through this menu. Check any oath against singing.

Trigger: The named menu is actually opened: songs.

Sources: `src/tutorial/tutorial-game.c`.

## Settings

`menu.settings`

Level: **Normal**.

Priority: **40** (higher appears first).

**1. Info**

Gameplay tutorials have three modes: Disabled, Normal and Extended. Normal guides basic actions and survival; Extended adds detailed lessons. You can review encountered lessons or reset this Tale's tutorial progress separately. Device-control tutorials and skeleton hints have their own settings.

Trigger: The named menu is actually opened: settings.

Sources: `src/tutorial/tutorial-game.c`.

## Inventory

`menu.inventory`

Level: **Normal**.

Priority: **40** (higher appears first).

**1. Info**

Your belongings are split into Pack, Harness, Quiver, Supplies and worn equipment. Select an item to read its location, weight, volume and available actions. Browsing is free. A Pack action takes three turns and attacks can interrupt it.

Trigger: The named menu is actually opened: inventory.

Sources: `src/tutorial/tutorial-game.c`.

## The Pack

`menu.pack`

Level: **Normal**.

Priority: **40** (higher appears first).

**1. Info**

The Pack stores spare gear and supplies within its volume limit. Acting on Pack items takes three turns; an attack can interrupt the process. Move urgent gear to the Harness or Supplies while you have time to prepare.

Trigger: The named menu is actually opened: pack.

Sources: `src/tutorial/tutorial-game.c`.

## The Harness

`menu.harness`

Level: **Normal**.

Priority: **40** (higher appears first).

**1. Info**

The Harness holds ready gear within its own volume limit. Select Ready on a suitable Pack item to move it here, or Unready to store it again. A ready weapon must also be selected in Change Active before you attack with it normally.

Trigger: The named menu is actually opened: harness.

Sources: `src/tutorial/tutorial-game.c`.

## Equipment

`menu.equipment`

Level: **Normal**.

Priority: **40** (higher appears first).

**1. Info**

Worn armour and jewellery provide their equipped effects. Readied weapons are available to choose, and your active weapon is the one you currently attack with. Check the active shield too: a two-handed weapon normally needs both hands.

Trigger: The named menu is actually opened: equipment.

Sources: `src/tutorial/tutorial-game.c`.

## Supplies

`menu.supplies`

Level: **Normal**.

Priority: **40** (higher appears first).

**1. Info**

Supplies hold consumables and their containers. Select an item to read its known effect, remaining amount and available actions. Keep the remedy for an urgent condition available before a fight.

Trigger: The named menu is actually opened: supplies.

Sources: `src/tutorial/tutorial-game.c`.

## The active weapon

`menu.active-weapon`

Level: **Normal**.

Priority: **40** (higher appears first).

**1. Info**

Choose a weapon from your ready gear to attack with. This can change whether a shield is active. Changing active weapons usually spends time; some abilities grant one eligible free change before your next action. Changing only arrows is free.

Trigger: The named menu is actually opened: active-weapon.

Sources: `src/tutorial/tutorial-game.c`.

## Item description

`menu.item-description`

Level: **Normal**.

Priority: **40** (higher appears first).

**1. Info**

Compare Attack, damage, Evasion, Protection, weight, volume and known effects. Unknown properties stay hidden until discovered. Reading this page is free; use the item action menu when you decide to equip or use it.

Trigger: The named menu is actually opened: item-description.

Sources: `src/tutorial/tutorial-game.c`.

## Jewelry

`menu.jewelry`

Level: **Normal**.

Priority: **40** (higher appears first).

**1. Info**

Select rings and an amulet by their known bonuses and drawbacks. Check which equipped items the change will replace. Curses can prevent removal.

Trigger: The named menu is actually opened: jewelry.

Sources: `src/tutorial/tutorial-game.c`.

## Jewelry sets

`menu.jewelry-sets`

Level: **Extended**.

Priority: **40** (higher appears first).

**1. Info**

Save combinations of rings and amulets for different situations. Before equipping a set, check its items and the proposed changes. Missing items, curses and equipment-change costs still apply.

Trigger: The named menu is actually opened: jewelry-sets.

Sources: `src/tutorial/tutorial-game.c`.

## Targeting

`menu.targeting`

Level: **Normal**.

Priority: **40** (higher appears first).

**1. Info**

Choose a target or direction and check the line of fire. Walls, range and your current condition can affect the shot. Confirm to commit the attack, or cancel to choose another action.

Trigger: The named menu is actually opened: targeting.

Sources: `src/tutorial/tutorial-game.c`.

## Resting

`menu.rest`

Level: **Normal**.

Priority: **40** (higher appears first).

**1. Info**

Rest repeats turns until its goal is met or something interrupts it. Enemies and the minimum-depth timer continue. Poison, bleeding and starvation stop ordinary Health recovery; singing stops Voice recovery. Treat disease before a long rest.

Trigger: The named menu is actually opened: rest.

Sources: `src/tutorial/tutorial-game.c`.

## The game menu

`menu.main-menu`

Level: **Normal**.

Priority: **40** (higher appears first).

**1. Info**

This menu opens character, equipment, knowledge, settings and game-management pages. Reading pages is free. Actions such as equipping an item or starting a rest spend their normal game time.

Trigger: The named menu is actually opened: main-menu.

Sources: `src/tutorial/tutorial-game.c`.

## Character details

`menu.character`

Level: **Normal**.

Priority: **40** (higher appears first).

**1. Info**

This page shows your attributes, skills, Health, Voice, active abilities and conditions. Use the breakdowns to see how equipment, temporary effects and drain change your totals.

Trigger: The named menu is actually opened: character.

Sources: `src/tutorial/tutorial-game.c`.

## Object knowledge

`menu.knowledge-objects`

Level: **Extended**.

Priority: **40** (higher appears first).

**1. Info**

Review item kinds your Tale has discovered. Recognising a kind tells you its basic use; an individual piece of equipment can still have unknown special properties.

Trigger: The named menu is actually opened: objects.

Sources: `src/tutorial/tutorial-game.c`.

## Artefact knowledge

`menu.knowledge-artefacts`

Level: **Extended**.

Priority: **40** (higher appears first).

**1. Info**

Review artefacts discovered by this Tale and their known properties. Read drawbacks and granted abilities as well as bonuses. A recorded artefact need not be in your current hero's belongings.

Trigger: The named menu is actually opened: artefacts.

Sources: `src/tutorial/tutorial-game.c`.

## Monster memory

`menu.knowledge-monsters`

Level: **Extended**.

Priority: **40** (higher appears first).

**1. Info**

Monster memory records attacks, resistances and other traits you have learned. Missing information is still unknown. Look at a current creature to check its awareness, morale and position.

Trigger: The named menu is actually opened: monsters.

Sources: `src/tutorial/tutorial-game.c`.

## Known curses

`menu.knowledge-curses`

Level: **Extended**.

Priority: **40** (higher appears first).

**1. Info**

Review curses that have been revealed, including their effects and current stacks. Tale curses, cursed equipment and broken oaths have different consequences; read the entry that applies to your situation.

Trigger: The named menu is actually opened: curses.

Sources: `src/tutorial/tutorial-game.c`.

## Smithing

`menu.smithing`

Level: **Normal**.

Priority: **40** (higher appears first).

**1. Info**

Choose an item or improvement, then review its difficulty, forge uses, materials, time and any attribute or XP costs. The preview lets you compare options before committing. Your abilities can change both what you may forge and its costs.

Trigger: The named menu is actually opened: smithing.

Sources: `src/tutorial/tutorial-game.c`.

## Nearby creatures

`menu.nearby-monsters`

Level: **Normal**.

Priority: **40** (higher appears first).

**1. Info**

Select a listed creature to inspect its description and position. Use the list to check threats you currently know about; it does not disclose hidden creatures.

Trigger: The named menu is actually opened: nearby-monsters.

Sources: `src/tutorial/tutorial-game.c`.

## Nearby objects

`menu.nearby-objects`

Level: **Normal**.

Priority: **40** (higher appears first).

**1. Info**

Select a known nearby item to inspect it and see its location. Floor items use the -) entry and the - shortcut where shown. Check storage space before picking up a stack.

Trigger: The named menu is actually opened: nearby-objects.

Sources: `src/tutorial/tutorial-game.c`.

## Look

`menu.look`

Level: **Normal**.

Priority: **40** (higher appears first).

**1. Info**

Look lets you inspect squares, items and creatures without spending a turn. Use it before approaching an unfamiliar feature or enemy. Remembered terrain can remain on the map after you lose sight of it.

Trigger: The named menu is actually opened: look.

Sources: `src/tutorial/tutorial-game.c`.

## The map

`menu.map`

Level: **Normal**.

Priority: **40** (higher appears first).

**1. Info**

Use the map to plan through explored terrain. It remembers places you have seen, but creatures can move outside your sight. Following a route spends turns and may be interrupted by danger.

Trigger: The named menu is actually opened: map.

Sources: `src/tutorial/tutorial-game.c`.

## Messages

`menu.messages`

Level: **Normal**.

Priority: **40** (higher appears first).

**1. Info**

Review messages to find the outcome of a recent action or something your hero just learned. History is free to read. Check current conditions too, since an old message may describe an effect that has ended.

Trigger: The named menu is actually opened: messages.

Sources: `src/tutorial/tutorial-game.c`.

## Combat history

`menu.combat-history`

Level: **Normal**.

Priority: **40** (higher appears first).

**1. Info**

Review Attack and Evasion rolls, damage dice, Protection and resulting effects. A miss deals no hit; a successful hit can still be fully blocked by Protection. Critical dice and elemental or slay bonuses are listed separately.

Trigger: The named menu is actually opened: combat-history.

Sources: `src/tutorial/tutorial-game.c`.

## Quests

`menu.quests`

Level: **Normal**.

Priority: **40** (higher appears first).

**1. Info**

Read each revealed objective, its restrictions and current progress. Check whether completion requires returning for a reward. Accepting a quest can place restrictions on your actions.

Trigger: The named menu is actually opened: quests.

Sources: `src/tutorial/tutorial-game.c`.

## Hints

`menu.hints`

Level: **Normal**.

Priority: **40** (higher appears first).

**1. Info**

Reread discovered hints here. They offer advice for particular situations; check your current equipment and conditions before acting. Skeleton hints have a separate setting from gameplay tutorials.

Trigger: The named menu is actually opened: hints.

Sources: `src/tutorial/tutorial-game.c`.

## Thralls

`menu.thralls`

Level: **Normal**.

Priority: **40** (higher appears first).

**1. Info**

Select a thrall to read their situation and available choices. Check the terms and consequences before rescuing, sacrificing or accepting a reward.

Trigger: The named menu is actually opened: thralls.

Sources: `src/tutorial/tutorial-game.c`.

## Tales

`menu.tales`

Level: **Normal**.

Priority: **40** (higher appears first).

**1. Info**

A Tale spans its heroes, deaths and restarts. Gameplay lesson history belongs to the selected Tale. Starting another Tale has separate progress; the global Disabled, Normal or Extended preference applies across Tales. Reviewing encountered lessons remains available regardless of the selected mode.

Trigger: The named menu is actually opened: tales.

Sources: `src/tutorial/tutorial-game.c`.

## Halls of the dead

`menu.halls`

Level: **Extended**.

Priority: **40** (higher appears first).

**1. Info**

Review the outcomes and scores of recorded heroes. These are past records within their Tales; viewing one does not resume that hero.

Trigger: The named menu is actually opened: halls.

Sources: `src/tutorial/tutorial-game.c`.

## Gameplay reference

`menu.help`

Level: **Normal**.

Priority: **40** (higher appears first).

**1. Info**

Help explains mechanics and controls. The tutorial archive lets you reread encountered or skipped lessons. Both are free to browse.

Trigger: The named menu is actually opened: help.

Sources: `src/tutorial/tutorial-game.c`.

## Power

`ability.0.preview`

Level: **Extended**.

Priority: **65** (higher appears first).

**1. Decision**

+1 damage side for melee attacks, but +1 to the critical interval base. With a 3 lb weapon, the unmodified first critical margin rises from 10 to 11. Extra damage sides and extra critical dice are different bonuses.

Trigger: Public ability preview or newly available ability; raw serial 0, skill 0, ability slot 0.

Sources: `lib/edit/ability.txt`, `src/cmd/combat/cmd-combat.c`, `src/birth/birth-traits.c`, `src/player/combat-stats.c`, `src/cmd/ui/cmd-ui-abilities.c`.

## Finesse

`ability.1.preview`

Level: **Extended**.

Priority: **65** (higher appears first).

**1. Decision**

Reduce the melee critical interval base from 7 to 5. For a 3 lb weapon, one extra die needs margin 8 instead of 10; two need 16 instead of 20. This changes critical thresholds, not your Attack score or the basic hit roll.

Trigger: Public ability preview or newly available ability; raw serial 1, skill 0, ability slot 1.

Sources: `lib/edit/ability.txt`, `src/cmd/combat/cmd-combat.c`, `src/birth/birth-traits.c`, `src/cmd/ui/cmd-ui-abilities.c`.

## Knock Back

`ability.2.preview`

Level: **Extended**.

Priority: **65** (higher appears first).

**1. Decision**

Your melee hits can push an enemy back one square if your Strength wins against its Constitution. Check the space behind the target: displacement can change who can reach you and can send a creature into a hazard.

Trigger: Public ability preview or newly available ability; raw serial 2, skill 0, ability slot 2.

Sources: `lib/edit/ability.txt`, `src/cmd/combat/cmd-combat.c`, `src/birth/birth-traits.c`, `src/cmd/ui/cmd-ui-abilities.c`.

## Throwing

`ability.3.preview`

Level: **Extended**.

Priority: **65** (higher appears first).

**1. Decision**

Gain +1 Attack with throwing weapons, halve distance penalties, and make thrown criticals easier. Personally learning Throwing reduces throwing weapons' Harness volume by 20%. It also allows quick throws of Harness daggers while a one-handed or hand-and-a-half melee weapon, or a bow, is active.

**2. Decision**

Your first active-weapon change before your next action is free between throwing weapons. Warden extends this to melee/throwing and melee/melee; Versatility extends it to bow/throwing and bow/bow. With both Warden and Versatility, the free change can be between any active weapons.

Trigger: Public ability preview or newly available ability; raw serial 3, skill 0, ability slot 3.

Sources: `lib/edit/ability.txt`, `src/cmd/combat/cmd-combat.c`, `src/birth/birth-traits.c`, `src/player/player-active-weapon.c`, `src/cmd/combat/cmd-ranged.c`.

## Polearm Mastery

`ability.4.preview`

Level: **Extended**.

Priority: **65** (higher appears first).

**1. Decision**

While melee is active, polearms gain +2 Attack. Wait or switch from ranged to melee to set your polearm: it can then make a free attack against an enemy that advances into reach.

Trigger: Public ability preview or newly available ability; raw serial 4, skill 0, ability slot 4.

Sources: `lib/edit/ability.txt`, `src/cmd/combat/cmd-combat.c`, `src/birth/birth-traits.c`, `src/monster/monster-move.c`, `src/player/combat-stats.c`, `src/player/player-active-weapon.c`, `src/cmd/ui/cmd-ui-abilities.c`, `src/cmd/ui/cmd-ui-smithing.c`.

## Charge

`ability.5.preview`

Level: **Extended**.

Priority: **65** (higher appears first).

**1. Decision**

Attacking immediately after moving toward your target grants +3 Strength and Dexterity for the charge. Plan the approach square as well as the attack; standing still to attack again does not keep the charge bonus.

Trigger: Public ability preview or newly available ability; raw serial 5, skill 0, ability slot 5.

Sources: `lib/edit/ability.txt`, `src/cmd/combat/cmd-combat.c`, `src/birth/birth-traits.c`, `src/cmd/ui/cmd-ui-abilities.c`.

## Follow-Through

`ability.6.preview`

Level: **Extended**.

Priority: **65** (higher appears first).

**1. Decision**

After killing an enemy in melee, you can continue the attack into another adjacent enemy. Consider nearby targets and your oath before triggering a chain of attacks.

Trigger: Public ability preview or newly available ability; raw serial 6, skill 0, ability slot 6.

Sources: `lib/edit/ability.txt`, `src/cmd/combat/cmd-combat.c`, `src/birth/birth-traits.c`, `src/cmd/ui/cmd-ui-abilities.c`.

## Impale

`ability.7.preview`

Level: **Extended**.

Priority: **65** (higher appears first).

**1. Decision**

With a polearm or greatsword, a melee attack can also strike a second enemy directly behind the first. Position enemies in a line to make use of the reach.

Trigger: Public ability preview or newly available ability; raw serial 7, skill 0, ability slot 7.

Sources: `lib/edit/ability.txt`, `src/cmd/combat/cmd-combat.c`, `src/birth/birth-traits.c`, `src/cmd/combat/cmd-ranged.c`, `src/cmd/ui/cmd-ui-abilities.c`.

## Subtlety

`ability.8.preview`

Level: **Extended**.

Priority: **65** (higher appears first).

**1. Decision**

Reduce the melee critical interval base by 2 while using a qualifying one-handed melee weapon with the off hand empty. A shield prevents this benefit. Combined with Finesse, a 3 lb weapon needs margins 6 and 12 for one and two extra dice. Certain special traits or items extend the qualifying weapon rules.

Trigger: Public ability preview or newly available ability; raw serial 8, skill 0, ability slot 8.

Sources: `lib/edit/ability.txt`, `src/cmd/combat/cmd-combat.c`, `src/birth/birth-traits.c`, `src/player/combat-stats.c`, `src/cmd/ui/cmd-ui-abilities.c`.

## Whirlwind Attack

`ability.9.preview`

Level: **Extended**.

Priority: **65** (higher appears first).

**1. Decision**

Attacking one enemy can also attack all other adjacent enemies when at least five adjacent squares are open. It also works with Flanking and Controlled Retreat attacks. More surrounding enemies can remove the open space it needs.

Trigger: Public ability preview or newly available ability; raw serial 9, skill 0, ability slot 9.

Sources: `lib/edit/ability.txt`, `src/cmd/combat/cmd-combat.c`, `src/birth/birth-traits.c`, `src/cmd/ui/cmd-ui-abilities.c`.

## Zone of Control

`ability.10.preview`

Level: **Extended**.

Priority: **65** (higher appears first).

**1. Decision**

If you did not move on your previous turn, you can make a free attack against an enemy moving between two squares beside you. Holding position lets you threaten movement around you.

Trigger: Public ability preview or newly available ability; raw serial 10, skill 0, ability slot 10.

Sources: `lib/edit/ability.txt`, `src/cmd/combat/cmd-combat.c`, `src/birth/birth-traits.c`, `src/monster/monster-move.c`, `src/cmd/ui/cmd-ui-abilities.c`.

## Smite

`ability.11.preview`

Level: **Extended**.

Priority: **65** (higher appears first).

**1. Decision**

With a two-handed melee weapon, the first blow of a qualifying melee attack rolls maximum damage, including its extra damage dice. You then lose a turn recovering, even if the attack missed. Protection still reduces damage.

Trigger: Public ability preview or newly available ability; raw serial 11, skill 0, ability slot 11.

Sources: `lib/edit/ability.txt`, `src/cmd/combat/cmd-combat.c`, `src/birth/birth-traits.c`, `src/ui/status/status-panel.c`, `src/cmd/ui/cmd-ui-abilities.c`.

## Two Weapon Fighting

`ability.12.preview`

Level: **Extended**.

Priority: **65** (higher appears first).

**1. Decision**

Equip a one-handed weapon in your off hand to gain an extra attack. The off-hand attack uses -3 Strength and Dexterity. Check the resulting weapon and shield arrangement before equipping it.

Trigger: Public ability preview or newly available ability; raw serial 12, skill 0, ability slot 12.

Sources: `lib/edit/ability.txt`, `src/cmd/combat/cmd-combat.c`, `src/birth/birth-traits.c`, `src/player/player-active-weapon.c`, `src/player/player-bonuses.c`, `src/sdl/core/sdl-layout.c`, `src/cmd/item/cmd-item-core.c`, `src/cmd/ui/cmd-ui-abilities.c`.

## Rapid Attack

`ability.13.preview`

Level: **Extended**.

Priority: **65** (higher appears first).

**1. Decision**

Gain an extra melee attack, with -3 Strength and Dexterity applied to the rapid attacks. Your first active-weapon change before your next action is free when switching between melee weapons.

Trigger: Public ability preview or newly available ability; raw serial 13, skill 0, ability slot 13.

Sources: `lib/edit/ability.txt`, `src/cmd/combat/cmd-combat.c`, `src/birth/birth-traits.c`, `src/player/player-active-weapon.c`, `src/player/player-bonuses.c`, `src/ui/character-screen.c`, `src/ui/status/status-panel.c`, `src/sdl/ui/sdl-screens.c`.

## Strength

`ability.14.preview`

Level: **Extended**.

Priority: **65** (higher appears first).

**1. Decision**

+1 Strength.

Trigger: Public ability preview or newly available ability; raw serial 14, skill 0, ability slot 14.

Sources: `lib/edit/ability.txt`, `src/cmd/combat/cmd-combat.c`, `src/birth/birth-traits.c`, `src/player/player-bonuses.c`.

## Warden

`ability.15.preview`

Level: **Extended**.

Priority: **65** (higher appears first).

**1. Decision**

Warden adds half the difference between your invested Melee and Archery to Archery when Melee is higher, rounded down. With Versatility also active, it instead adds one third of your invested Melee, rounded up. Equipment and attribute bonuses are excluded from this calculation.

**2. Decision**

Your first active-weapon change before your next action is free from melee to bow. With Throwing, it may also switch between melee and throwing or between melee weapons. With Versatility, it may also switch between melee weapons or between bows.

Trigger: Public ability preview or newly available ability; raw serial 15, skill 0, ability slot 15.

Sources: `lib/edit/ability.txt`, `src/cmd/combat/cmd-combat.c`, `src/birth/birth-traits.c`, `src/player/player-active-weapon.c`, `src/player/player-bonuses.c`, `src/player/player-skills.c`, `src/cmd/ui/cmd-ui-abilities.c`.

## Power Throw

`ability.16.preview`

Level: **Extended**.

Priority: **65** (higher appears first).

**1. Decision**

After spending a turn readying your melee weapon or waiting, throw a Harness spear or hand axe at an adjacent foe while striking in melee. Roll both attacks separately against Evasion, combine the damage of successful attacks, then roll protection once. Your melee weapon remains active.

Trigger: Public ability preview or newly available ability; raw serial 16, skill 0, ability slot 16.

Sources: `lib/edit/ability.txt`, `src/cmd/combat/cmd-combat.c`, `src/birth/birth-traits.c`, `src/player/player-active-weapon.c`, `src/cmd/ui/cmd-ui-abilities.c`.

## Rout

`ability.20.preview`

Level: **Extended**.

Priority: **65** (higher appears first).

**1. Decision**

Gain +5 Dexterity when firing at fleeing enemies. Check your oath first: Oath of Valour forbids harming them.

Trigger: Public ability preview or newly available ability; raw serial 20, skill 1, ability slot 0.

Sources: `lib/edit/ability.txt`, `src/cmd/combat/cmd-ranged.c`, `src/birth/birth-traits.c`, `src/cmd/ui/cmd-ui-abilities.c`.

## Fletchery

`ability.21.preview`

Level: **Extended**.

Priority: **65** (higher appears first).

**1. Decision**

Fletchery can improve arrows to +3, preserving their special properties, at one turn per arrow. You can also carve 3 arrows from a torch or 6 from a staff, consuming the source. Personally learning Fletchery reduces loose arrows' Pack volume by 20%.

Trigger: Public ability preview or newly available ability; raw serial 21, skill 1, ability slot 1.

Sources: `lib/edit/ability.txt`, `src/cmd/combat/cmd-ranged.c`, `src/birth/birth-traits.c`, `src/sdl/input/sdl-player-actions.c`, `src/sdl/ui/sdl-main-menu.c`, `src/cmd/item/cmd-fletchery.c`.

## Point Blank Archery

`ability.22.preview`

Level: **Extended**.

Priority: **65** (higher appears first).

**1. Decision**

When you shoot an adjacent monster, that target does not make an attack of opportunity. Other adjacent alert enemies still can. Point Blank Archery lets you make a round shield active with a shortbow.

Trigger: Public ability preview or newly available ability; raw serial 22, skill 1, ability slot 2.

Sources: `lib/edit/ability.txt`, `src/cmd/combat/cmd-ranged.c`, `src/birth/birth-traits.c`, `src/player/player-active-weapon.c`, `src/cmd/ui/cmd-ui-abilities.c`.

## Puncture

`ability.23.preview`

Level: **Extended**.

Priority: **65** (higher appears first).

**1. Decision**

If armour completely blocks an arrow's damage, this ability makes the hit deal 5 damage instead. The arrow still needs to hit.

Trigger: Public ability preview or newly available ability; raw serial 23, skill 1, ability slot 3.

Sources: `lib/edit/ability.txt`, `src/cmd/combat/cmd-ranged.c`, `src/birth/birth-traits.c`, `src/cmd/ui/cmd-ui-abilities.c`.

## Ambush

`ability.24.preview`

Level: **Extended**.

Priority: **65** (higher appears first).

**1. Decision**

Arrows gain one extra critical damage die against sleeping or unwary enemies. Alert targets do not provide this benefit.

Trigger: Public ability preview or newly available ability; raw serial 24, skill 1, ability slot 4.

Sources: `lib/edit/ability.txt`, `src/cmd/combat/cmd-ranged.c`, `src/birth/birth-traits.c`, `src/cmd/ui/cmd-ui-abilities.c`.

## Versatility

`ability.25.preview`

Level: **Extended**.

Priority: **65** (higher appears first).

**1. Decision**

Versatility adds half the difference between your invested Archery and Melee to Melee when Archery is higher, rounded down. With Warden also active, it instead adds one third of your invested Archery, rounded up. Equipment and attribute bonuses are excluded from this calculation.

**2. Decision**

Your first active-weapon change before your next action is free from bow to melee. With Throwing, it may also switch between bow and throwing or between bows. With Warden, it may also switch between melee weapons or between bows.

Trigger: Public ability preview or newly available ability; raw serial 25, skill 1, ability slot 5.

Sources: `lib/edit/ability.txt`, `src/cmd/combat/cmd-ranged.c`, `src/birth/birth-traits.c`, `src/player/player-active-weapon.c`, `src/player/player-bonuses.c`, `src/player/player-skills.c`, `src/cmd/ui/cmd-ui-abilities.c`.

## Crippling Shot

`ability.26.preview`

Level: **Extended**.

Priority: **65** (higher appears first).

**1. Decision**

Critical arrow hits can slow the target, with the critical level checked against its Will. A critical hit does not guarantee the slowing effect.

Trigger: Public ability preview or newly available ability; raw serial 26, skill 1, ability slot 6.

Sources: `lib/edit/ability.txt`, `src/cmd/combat/cmd-ranged.c`, `src/birth/birth-traits.c`, `src/cmd/ui/cmd-ui-abilities.c`.

## Deadly Hail

`ability.27.preview`

Level: **Extended**.

Priority: **65** (higher appears first).

**1. Decision**

After killing an enemy with an arrow, arrows deal double damage on your next turn. The opportunity is brief, so check your next target before spending that turn.

Trigger: Public ability preview or newly available ability; raw serial 27, skill 1, ability slot 7.

Sources: `lib/edit/ability.txt`, `src/cmd/combat/cmd-ranged.c`, `src/birth/birth-traits.c`, `src/ui/status/status-panel.c`, `src/cmd/ui/cmd-ui-abilities.c`.

## Dexterity

`ability.28.preview`

Level: **Extended**.

Priority: **65** (higher appears first).

**1. Decision**

+1 Dexterity, improving the skills that depend on Dexterity.

Trigger: Public ability preview or newly available ability; raw serial 28, skill 1, ability slot 8.

Sources: `lib/edit/ability.txt`, `src/cmd/combat/cmd-ranged.c`, `src/birth/birth-traits.c`, `src/player/player-bonuses.c`.

## Skirmishing

`ability.29.preview`

Level: **Extended**.

Priority: **65** (higher appears first).

**1. Decision**

After moving on your previous turn, firing a bow costs half a turn while wearing only light armour. Your first active weapon change before your next action is free when changing between bows.

Trigger: Public ability preview or newly available ability; raw serial 29, skill 1, ability slot 9.

Sources: `lib/edit/ability.txt`, `src/cmd/combat/cmd-ranged.c`, `src/player/player-active-weapon.c`, `src/cmd/ui/cmd-ui-abilities.c`.

## Dodging

`ability.40.preview`

Level: **Extended**.

Priority: **65** (higher appears first).

**1. Decision**

Gain +3 Evasion if you moved on your previous turn and are wearing only light armour. Standing still or wearing heavier armour removes the benefit.

Trigger: Public ability preview or newly available ability; raw serial 40, skill 2, ability slot 0.

Sources: `lib/edit/ability.txt`, `src/player/player-bonuses.c`, `src/birth/birth-traits.c`, `src/melee/melee-attack.c`, `src/cmd/ui/cmd-ui-abilities.c`.

## Blocking

`ability.41.preview`

Level: **Extended**.

Priority: **65** (higher appears first).

**1. Decision**

Your active shield provides double Protection if you did not move on your previous turn. Waiting or attacking from the same square can prepare this defence.

Trigger: Public ability preview or newly available ability; raw serial 41, skill 2, ability slot 1.

Sources: `lib/edit/ability.txt`, `src/player/player-bonuses.c`, `src/birth/birth-traits.c`, `src/melee/melee-attack.c`, `src/ui/status/status-panel.c`.

## Parry

`ability.42.preview`

Level: **Extended**.

Priority: **65** (higher appears first).

**1. Decision**

Double the Evasion bonus of your primary melee weapon. A weapon with a larger Evasion bonus gains more from Parry.

Trigger: Public ability preview or newly available ability; raw serial 42, skill 2, ability slot 2.

Sources: `lib/edit/ability.txt`, `src/player/player-bonuses.c`, `src/birth/birth-traits.c`, `src/cmd/ui/cmd-ui-abilities.c`.

## Crowd Fighting

`ability.43.preview`

Level: **Extended**.

Priority: **65** (higher appears first).

**1. Decision**

Halve the bonus enemies receive for surrounding you. A narrow passage can still help by limiting the number that can attack.

Trigger: Public ability preview or newly available ability; raw serial 43, skill 2, ability slot 3.

Sources: `lib/edit/ability.txt`, `src/player/player-bonuses.c`, `src/birth/birth-traits.c`, `src/cmd/combat/cmd-combat.c`.

## Leaping

`ability.44.preview`

Level: **Extended**.

Priority: **65** (higher appears first).

**1. Decision**

After moving toward a gap or trap on your previous turn, Leaping can carry you over it. Check the landing square first. It can cross water, lava, ice and poisonous seep, but lava still burns you in the air. It cannot bypass roosts or webs.

Trigger: Public ability preview or newly available ability; raw serial 44, skill 2, ability slot 4.

Sources: `lib/edit/ability.txt`, `src/player/player-bonuses.c`, `src/birth/birth-traits.c`, `src/sdl/input/sdl-mouse-path.c`, `src/cmd/movement/cmd-run.c`.

## Sprinting

`ability.45.preview`

Level: **Extended**.

Priority: **65** (higher appears first).

**1. Decision**

Move in roughly the same direction for at least four squares in light armour, or five otherwise, to gain speed. Turning sharply or breaking the run loses the benefit.

Trigger: Public ability preview or newly available ability; raw serial 45, skill 2, ability slot 5.

Sources: `lib/edit/ability.txt`, `src/player/player-bonuses.c`, `src/birth/birth-traits.c`, `src/player/player-skills.c`, `src/sdl/input/sdl-mouse-path.c`.

## Flanking

`ability.46.preview`

Level: **Extended**.

Priority: **65** (higher appears first).

**1. Decision**

While wearing only light armour, moving between two squares beside an enemy grants a free melee attack against it. The move must keep you adjacent to that enemy.

Trigger: Public ability preview or newly available ability; raw serial 46, skill 2, ability slot 6.

Sources: `lib/edit/ability.txt`, `src/player/player-bonuses.c`, `src/birth/birth-traits.c`, `src/cmd/combat/cmd-combat.c`, `src/cmd/ui/cmd-ui-abilities.c`.

## Heavy Armour Use

`ability.47.preview`

Level: **Extended**.

Priority: **65** (higher appears first).

**1. Decision**

Gain 1 fixed physical Protection for each complete 15 lb of worn armour. For example, 30 lb gives 2 Protection, represented as Xd1 rather than a random 1dX roll. This bonus does not protect against elemental damage. Mail Corslets and Hauberks also gain +1 Evasion.

Trigger: Public ability preview or newly available ability; raw serial 47, skill 2, ability slot 7.

Sources: `lib/edit/ability.txt`, `src/player/player-bonuses.c`, `src/melee/melee-attack.c`, `src/birth/birth-traits.c`, `src/cmd/ui/cmd-ui-abilities.c`.

## Riposte

`ability.48.preview`

Level: **Extended**.

Priority: **65** (higher appears first).

**1. Decision**

Once per round, you can counterattack when an enemy misses you by at least 10 plus your weapon's weight in pounds. A lighter weapon makes the required margin smaller.

Trigger: Public ability preview or newly available ability; raw serial 48, skill 2, ability slot 8.

Sources: `lib/edit/ability.txt`, `src/player/player-bonuses.c`, `src/birth/birth-traits.c`, `src/melee/melee-attack.c`.

## Controlled Retreat

`ability.49.preview`

Level: **Extended**.

Priority: **65** (higher appears first).

**1. Decision**

If you did not move on your previous turn, stepping away from an adjacent enemy grants a free melee attack. Check the retreat square before using it.

Trigger: Public ability preview or newly available ability; raw serial 49, skill 2, ability slot 9.

Sources: `lib/edit/ability.txt`, `src/player/player-bonuses.c`, `src/birth/birth-traits.c`, `src/cmd/combat/cmd-combat.c`.

## Dexterity

`ability.50.preview`

Level: **Extended**.

Priority: **65** (higher appears first).

**1. Decision**

+1 Dexterity.

Trigger: Public ability preview or newly available ability; raw serial 50, skill 2, ability slot 10.

Sources: `lib/edit/ability.txt`, `src/player/player-bonuses.c`, `src/birth/birth-traits.c`.

## Disguise

`ability.60.preview`

Level: **Extended**.

Priority: **65** (higher appears first).

**1. Decision**

Halve the line-of-sight detection bonus that awake, unwary enemies gain against you. This helps you approach before they become alert.

Trigger: Public ability preview or newly available ability; raw serial 60, skill 3, ability slot 0.

Sources: `lib/edit/ability.txt`, `src/melee/melee-process.c`, `src/birth/birth-traits.c`, `src/cave/cave-visuals.c`, `src/melee/melee-process.c`, `src/sdl/render/sdl-term-callbacks.c`.

## Assassination

`ability.61.preview`

Level: **Extended**.

Priority: **65** (higher appears first).

**1. Decision**

Gain a melee Attack bonus equal to your Stealth against enemies that are not alert, or enemies currently fooled by Song of Disguise.

Trigger: Public ability preview or newly available ability; raw serial 61, skill 3, ability slot 1.

Sources: `lib/edit/ability.txt`, `src/melee/melee-process.c`, `src/birth/birth-traits.c`, `src/cmd/combat/cmd-combat.c`, `src/cmd/ui/cmd-ui-abilities.c`.

## Cruel Blow

`ability.62.preview`

Level: **Extended**.

Priority: **65** (higher appears first).

**1. Decision**

Critical melee hits can confuse enemies, checked against their Will. If equipped gear grants Cruel Blow, wearing it also speeds the minimum-depth timer as much as Deep Call, even while the ability is disabled.

Trigger: Public ability preview or newly available ability; raw serial 62, skill 3, ability slot 2.

Sources: `lib/edit/ability.txt`, `src/melee/melee-process.c`, `src/birth/birth-traits.c`, `src/object/object-info.c`, `src/cmd/combat/cmd-combat.c`, `src/cmd/combat/cmd-ranged.c`, `src/cmd/movement/cmd-depth-bonus.c`.

## Exchange Places

`ability.63.preview`

Level: **Extended**.

Priority: **65** (higher appears first).

**1. Decision**

Use Exchange Places to swap squares with an adjacent enemy. The enemy gets a free attack as you pass. Check the destination and nearby enemies before confirming.

Trigger: Public ability preview or newly available ability; raw serial 63, skill 3, ability slot 3.

Sources: `lib/edit/ability.txt`, `src/melee/melee-process.c`, `src/birth/birth-traits.c`, `src/sdl/input/sdl-player-actions.c`, `src/sdl/ui/sdl-main-menu.c`, `src/cmd/item/cmd-item-utility.c`.

## Opportunist

`ability.64.preview`

Level: **Extended**.

Priority: **65** (higher appears first).

**1. Decision**

Make a free melee attack when an adjacent enemy moves away from you. An enemy retreat can become an opportunity to strike, subject to normal attack restrictions.

Trigger: Public ability preview or newly available ability; raw serial 64, skill 3, ability slot 4.

Sources: `lib/edit/ability.txt`, `src/melee/melee-process.c`, `src/birth/birth-traits.c`, `src/melee/melee-process.c`, `src/monster/monster-move.c`.

## Vanish

`ability.65.preview`

Level: **Extended**.

Priority: **65** (higher appears first).

**1. Decision**

Gain +10 Stealth toward making enemies unwary again while you are out of their line of sight. Breaking sight is the first step; it does not immediately make them forget you.

Trigger: Public ability preview or newly available ability; raw serial 65, skill 3, ability slot 5.

Sources: `lib/edit/ability.txt`, `src/melee/melee-process.c`, `src/birth/birth-traits.c`, `src/melee/melee-process.c`.

## Dexterity

`ability.66.preview`

Level: **Extended**.

Priority: **65** (higher appears first).

**1. Decision**

+1 Dexterity.

Trigger: Public ability preview or newly available ability; raw serial 66, skill 3, ability slot 6.

Sources: `lib/edit/ability.txt`, `src/melee/melee-process.c`, `src/birth/birth-traits.c`, `src/player/player-bonuses.c`.

## Quick Study

`ability.80.preview`

Level: **Extended**.

Priority: **65** (higher appears first).

**1. Decision**

Learn advanced abilities without their usual ability prerequisites. Their skill and XP requirements still apply. You also gain a modest bonus to identifying items.

Trigger: Public ability preview or newly available ability; raw serial 80, skill 4, ability slot 0.

Sources: `lib/edit/ability.txt`, `src/player/player-bonuses.c`, `src/birth/birth-traits.c`, `src/player/player-lore.c`, `src/cmd/ui/cmd-ui-abilities.c`.

## Focused attack

`ability.81.preview`

Level: **Extended**.

Priority: **65** (higher appears first).

**1. Decision**

After waiting for one turn, gain an Attack bonus equal to half your Perception on the next applicable attack. Use the pause to prepare while the enemy approaches.

Trigger: Public ability preview or newly available ability; raw serial 81, skill 4, ability slot 1.

Sources: `lib/edit/ability.txt`, `src/player/player-bonuses.c`, `src/birth/birth-traits.c`, `src/sdl/ui/sdl-panes.c`, `src/cmd/combat/cmd-combat.c`, `src/cmd/ui/cmd-ui-abilities.c`.

## Keen Senses

`ability.82.preview`

Level: **Extended**.

Priority: **65** (higher appears first).

**1. Decision**

See creatures just beyond the edge of your light and gain +5 to spotting invisible creatures. Walls and other visibility rules still apply.

Trigger: Public ability preview or newly available ability; raw serial 82, skill 4, ability slot 2.

Sources: `lib/edit/ability.txt`, `src/player/player-bonuses.c`, `src/birth/birth-traits.c`, `src/cave/cave-geometry.c`, `src/monster/monster-update.c`.

## Concentration

`ability.83.preview`

Level: **Extended**.

Priority: **65** (higher appears first).

**1. Decision**

Gain +1 Attack for each consecutive round attacking the same enemy, up to half your Perception. Changing targets resets the sequence.

Trigger: Public ability preview or newly available ability; raw serial 83, skill 4, ability slot 3.

Sources: `lib/edit/ability.txt`, `src/player/player-bonuses.c`, `src/birth/birth-traits.c`, `src/sdl/ui/sdl-panes.c`, `src/cmd/combat/cmd-combat.c`, `src/cmd/ui/cmd-ui-abilities.c`.

## Alchemy

`ability.84.preview`

Level: **Extended**.

Priority: **65** (higher appears first).

**1. Decision**

Auto-identify herbs, potions, staves, and horns. Potions with thrown effects can be quick-thrown, splashing the impact square and every adjacent square. +50% range for Gems of Revelation, Foes, and Treasures.

Trigger: Public ability preview or newly available ability; raw serial 84, skill 4, ability slot 4.

Sources: `lib/edit/ability.txt`, `src/player/player-bonuses.c`, `src/use-obj.c`, `src/birth/birth-traits.c`, `src/object/object-info.c`, `src/player/player-active-weapon.c`, `src/player/player-lore.c`.

## Bane

`ability.85.preview`

Level: **Extended**.

Priority: **65** (higher appears first).

**1. Decision**

Choose a creature type to gain bonuses in skill contests against that type. The bonus grows as you kill more of them. Read the available types before making the choice.

Trigger: Public ability preview or newly available ability; raw serial 85, skill 4, ability slot 5.

Sources: `lib/edit/ability.txt`, `src/player/player-bonuses.c`, `src/birth/birth-traits.c`, `src/melee/melee-process.c`, `src/object/object-info.c`, `src/ui/character-dump.c`, `src/cmd/ui/cmd-ui-abilities.c`.

## Outwit

`ability.86.preview`

Level: **Extended**.

Priority: **65** (higher appears first).

**1. Decision**

When an enemy scores a critical hit, your Perception contests its Perception. A success removes the extra critical damage dice; the ordinary hit can still hurt.

Trigger: Public ability preview or newly available ability; raw serial 86, skill 4, ability slot 6.

Sources: `lib/edit/ability.txt`, `src/player/player-bonuses.c`, `src/birth/birth-traits.c`, `src/cmd/combat/cmd-combat.c`.

## Resonance

`ability.87.preview`

Level: **Extended**.

Priority: **65** (higher appears first).

**1. Decision**

Attempt to detect unseen enemies each turn. For item identification, double the Perception contribution while counting Grace once.

Trigger: Public ability preview or newly available ability; raw serial 87, skill 4, ability slot 7.

Sources: `lib/edit/ability.txt`, `src/player/player-bonuses.c`, `src/birth/birth-traits.c`, `src/monster/monster-lore.c`, `src/monster/monster-update.c`, `src/player/player-lore.c`.

## Master Hunter

`ability.88.preview`

Level: **Extended**.

Priority: **65** (higher appears first).

**1. Decision**

Gain +1 Attack for each previous kill of the same monster race, up to half your Perception. Different races have separate kill counts.

Trigger: Public ability preview or newly available ability; raw serial 88, skill 4, ability slot 8.

Sources: `lib/edit/ability.txt`, `src/player/player-bonuses.c`, `src/birth/birth-traits.c`, `src/cmd/combat/cmd-combat.c`, `src/cmd/ui/cmd-ui-abilities.c`.

## Grace

`ability.89.preview`

Level: **Extended**.

Priority: **65** (higher appears first).

**1. Decision**

+1 Grace and +6.0 qt of Harness capacity. More Harness space lets you keep additional gear ready.

Trigger: Public ability preview or newly available ability; raw serial 89, skill 4, ability slot 9.

Sources: `lib/edit/ability.txt`, `src/player/player-bonuses.c`, `src/birth/birth-traits.c`, `src/object/object-inventory-limits.c`.

## Rewire Traps

`ability.90.preview`

Level: **Extended**.

Priority: **65** (higher appears first).

**1. Decision**

When you disarm a suitable trap, you instead re-key its mechanism: it no longer harms you, and monsters that cross it may set off its altered workings. The greater your margin on the attempt, the harder foes find it to notice or undo the change.

Trigger: Public ability preview or newly available ability; raw serial 90, skill 4, ability slot 10.

Sources: `lib/edit/ability.txt`, `src/player/player-bonuses.c`, `src/cmd/world/cmd-interact-chest.c`, `src/cmd/world/cmd-interact.c`.

## Curse Breaking

`ability.100.preview`

Level: **Extended**.

Priority: **65** (higher appears first).

**1. Decision**

Break ordinary item curses when removing the item. Gain a significant identification bonus, especially for cursed items. Gems of Sanctity can also remove qualifying jinxed special properties.

Trigger: Public ability preview or newly available ability; raw serial 100, skill 5, ability slot 0.

Sources: `lib/edit/ability.txt`, `src/player/player-bonuses.c`, `src/use-obj.c`, `src/birth/birth-traits.c`, `src/player/player-lore.c`, `src/cmd/combat/cmd-ranged.c`, `src/cmd/item/cmd-item-activate.c`, `src/cmd/item/cmd-item-core.c`.

## Channeling

`ability.101.preview`

Level: **Extended**.

Priority: **65** (higher appears first).

**1. Decision**

Automatically identify staves and horns. Use Channel to transfer charges from a compatible staff on the floor into a carried staff. Horns cost 10 Voice instead of 20, and Gems of Recharging restore twice the usual charges.

Trigger: Public ability preview or newly available ability; raw serial 101, skill 5, ability slot 1.

Sources: `lib/edit/ability.txt`, `src/player/player-bonuses.c`, `src/use-obj.c`, `src/birth/birth-traits.c`, `src/player/player-lore.c`, `src/spell/spell-identify.c`, `src/cmd/item/cmd-pickup.c`.

## Strength in Adversity

`ability.102.preview`

Level: **Extended**.

Priority: **65** (higher appears first).

**1. Decision**

At half Health or less, gain +1 Strength, Dexterity and Grace. At one quarter Health or less, the bonus becomes +3. You remain vulnerable to death at these low Health levels.

Trigger: Public ability preview or newly available ability; raw serial 102, skill 5, ability slot 2.

Sources: `lib/edit/ability.txt`, `src/player/player-bonuses.c`, `src/birth/birth-traits.c`, `src/cmd/ui/cmd-ui-abilities.c`.

## Formidable

`ability.103.preview`

Level: **Extended**.

Priority: **65** (higher appears first).

**1. Decision**

Melee kills frighten visible enemies. Enemies also stop gaining confidence from your injuries. This can help break a group's morale during a fight.

Trigger: Public ability preview or newly available ability; raw serial 103, skill 5, ability slot 3.

Sources: `lib/edit/ability.txt`, `src/player/player-bonuses.c`, `src/birth/birth-traits.c`, `src/melee/melee-movement-path.c`, `src/melee/melee-process.c`, `src/cmd/combat/cmd-combat.c`.

## Inner Light

`ability.104.preview`

Level: **Extended**.

Priority: **65** (higher appears first).

**1. Decision**

+2 light intensity within your existing light radius. Your light becomes brighter without reaching farther.

Trigger: Public ability preview or newly available ability; raw serial 104, skill 5, ability slot 4.

Sources: `lib/edit/ability.txt`, `src/player/player-bonuses.c`, `src/birth/birth-traits.c`, `src/cave/cave-view.c`.

## Indomitable

`ability.105.preview`

Level: **Extended**.

Priority: **65** (higher appears first).

**1. Decision**

Gain resistance to fear, confusion, stun and hallucination. Hunger advances at one third of its normal rate.

Trigger: Public ability preview or newly available ability; raw serial 105, skill 5, ability slot 5.

Sources: `lib/edit/ability.txt`, `src/player/player-bonuses.c`, `src/birth/birth-traits.c`.

## Oath

`ability.106.preview`

Level: **Extended**.

Priority: **65** (higher appears first).

**1. Decision**

Swear an oath for its benefit and accept its restriction. Read the exact terms before choosing: breaking an oath removes its benefit and has consequences for the Tale.

Trigger: Public ability preview or newly available ability; raw serial 106, skill 5, ability slot 6.

Sources: `lib/edit/ability.txt`, `src/player/player-bonuses.c`, `src/birth/birth-traits.c`, `src/ui/character-dump.c`, `src/cmd/ui/cmd-ui-abilities.c`.

## Poison Resistance

`ability.107.preview`

Level: **Extended**.

Priority: **65** (higher appears first).

**1. Decision**

Gain one layer of poison resistance. It reduces new poison exposure; existing poison still needs time or a remedy to clear.

Trigger: Public ability preview or newly available ability; raw serial 107, skill 5, ability slot 7.

Sources: `lib/edit/ability.txt`, `src/player/player-bonuses.c`, `src/birth/birth-traits.c`.

## Vengeance

`ability.108.preview`

Level: **Extended**.

Priority: **65** (higher appears first).

**1. Decision**

After taking melee damage, your next melee hit gains one extra damage die. Further hits against you do not add more Vengeance dice.

Trigger: Public ability preview or newly available ability; raw serial 108, skill 5, ability slot 8.

Sources: `lib/edit/ability.txt`, `src/player/player-bonuses.c`, `src/birth/birth-traits.c`, `src/melee/melee-attack.c`, `src/player/combat-stats.c`, `src/sdl/ui/sdl-panes.c`, `src/cmd/ui/cmd-ui-abilities.c`.

## Majesty

`ability.109.preview`

Level: **Extended**.

Priority: **65** (higher appears first).

**1. Decision**

Lower the morale of enemies whose Will is lower than yours. A larger Will advantage creates more pressure to flee.

Trigger: Public ability preview or newly available ability; raw serial 109, skill 5, ability slot 9.

Sources: `lib/edit/ability.txt`, `src/player/player-bonuses.c`, `src/birth/birth-traits.c`, `src/melee/melee-process.c`.

## Constitution

`ability.110.preview`

Level: **Extended**.

Priority: **65** (higher appears first).

**1. Decision**

+1 Constitution. +6.0 qt Pack capacity.

Trigger: Public ability preview or newly available ability; raw serial 110, skill 5, ability slot 10.

Sources: `lib/edit/ability.txt`, `src/player/player-bonuses.c`, `src/birth/birth-traits.c`, `src/object/object-inventory-limits.c`.

## Weaponsmith

`ability.120.preview`

Level: **Extended**.

Priority: **65** (higher appears first).

**1. Decision**

Create weapons at a forge. You also gain a modest bonus to identifying weapons.

Trigger: Public ability preview or newly available ability; raw serial 120, skill 6, ability slot 0.

Sources: `lib/edit/ability.txt`, `src/cmd/ui/cmd-ui-smithing.c`, `src/birth/birth-traits.c`, `src/player/player-lore.c`.

## Armoursmith

`ability.121.preview`

Level: **Extended**.

Priority: **65** (higher appears first).

**1. Decision**

Create armour at a forge. You also gain a modest bonus to identifying armour.

Trigger: Public ability preview or newly available ability; raw serial 121, skill 6, ability slot 1.

Sources: `lib/edit/ability.txt`, `src/cmd/ui/cmd-ui-smithing.c`, `src/birth/birth-traits.c`, `src/player/player-lore.c`.

## Jeweller

`ability.122.preview`

Level: **Extended**.

Priority: **65** (higher appears first).

**1. Decision**

Create rings, amulets, horns and lights at a forge, and automatically recognise their kinds. Individual equipment may still need identification of its special properties.

Trigger: Public ability preview or newly available ability; raw serial 122, skill 6, ability slot 2.

Sources: `lib/edit/ability.txt`, `src/cmd/ui/cmd-ui-smithing.c`, `src/birth/birth-traits.c`, `src/player/player-lore.c`.

## Enchantment

`ability.123.preview`

Level: **Extended**.

Priority: **65** (higher appears first).

**1. Decision**

Create enchanted items with named special properties at a forge. You can determine enchantments on items and gain a modest identification bonus.

Trigger: Public ability preview or newly available ability; raw serial 123, skill 6, ability slot 3.

Sources: `lib/edit/ability.txt`, `src/cmd/ui/cmd-ui-smithing.c`, `src/birth/birth-traits.c`, `src/player/player-lore.c`.

## Expertise

`ability.124.preview`

Level: **Extended**.

Priority: **65** (higher appears first).

**1. Decision**

Halve smithing time and remove the normal XP and attribute costs of smithing. Materials and forge uses still matter; inspect the complete proposal.

Trigger: Public ability preview or newly available ability; raw serial 124, skill 6, ability slot 4.

Sources: `lib/edit/ability.txt`, `src/cmd/ui/cmd-ui-smithing.c`, `src/birth/birth-traits.c`.

## Artifice

`ability.125.preview`

Level: **Extended**.

Priority: **65** (higher appears first).

**1. Decision**

Create custom artefacts at a forge and gain a significant identification bonus. The smithing preview shows the cost of your chosen properties.

Trigger: Public ability preview or newly available ability; raw serial 125, skill 6, ability slot 5.

Sources: `lib/edit/ability.txt`, `src/cmd/ui/cmd-ui-smithing.c`, `src/birth/birth-traits.c`, `src/player/player-lore.c`.

## Masterpiece

`ability.126.preview`

Level: **Extended**.

Priority: **65** (higher appears first).

**1. Decision**

Forge an item above your normal difficulty limit by permanently spending base Smithing: one skill point per excess difficulty point. The amount available is limited by your invested Smithing.

Trigger: Public ability preview or newly available ability; raw serial 126, skill 6, ability slot 6.

Sources: `lib/edit/ability.txt`, `src/cmd/ui/cmd-ui-smithing.c`, `src/birth/birth-traits.c`, `src/cmd/ui/cmd-ui-abilities.c`.

## Grace

`ability.127.preview`

Level: **Extended**.

Priority: **65** (higher appears first).

**1. Decision**

+1 Grace.

Trigger: Public ability preview or newly available ability; raw serial 127, skill 6, ability slot 7.

Sources: `lib/edit/ability.txt`, `src/cmd/ui/cmd-ui-smithing.c`, `src/birth/birth-traits.c`, `src/player/player-bonuses.c`.

## Alloy mastery

`ability.128.preview`

Level: **Extended**.

Priority: **65** (higher appears first).

**1. Decision**

Forge mithril or star-iron gear, or alloy suitable weapons and armour with those metals. Alloying adds no extra difficulty cost, but still requires the material.

Trigger: Public ability preview or newly available ability; raw serial 128, skill 6, ability slot 8.

Sources: `lib/edit/ability.txt`, `src/cmd/ui/cmd-ui-smithing.c`, `src/birth/birth-traits.c`.

## Reforging

`ability.129.preview`

Level: **Extended**.

Priority: **65** (higher appears first).

**1. Decision**

At a forge, repair damaged items or add a missing prefix to a found item. Reforging difficulty is 1.5 times the increase in difficulty. Compare the result and costs before committing.

Trigger: Public ability preview or newly available ability; raw serial 129, skill 6, ability slot 9.

Sources: `lib/edit/ability.txt`, `src/cmd/ui/cmd-ui-smithing.c`.

## Song of Elbereth

`ability.140.preview`

Level: **Extended**.

Priority: **65** (higher appears first).

**1. Decision**

Pressure nearby enemies to flee and reduce their Will by one fifth of your effective Song. Targets can resist the fear effect.

Trigger: Public ability preview or newly available ability; raw serial 140, skill 7, ability slot 0.

Sources: `lib/edit/ability.txt`, `src/player/player-song-effects.c`, `src/birth/birth-traits.c`, `src/monster/monster-attr.c`, `src/player/player-bonuses.c`, `src/player/player-skills.c`, `src/player/player-song-duels.c`, `src/player/player-songs.c`.

## Song of Challenge

`ability.141.preview`

Level: **Extended**.

Priority: **65** (higher appears first).

**1. Decision**

Challenge nearby enemies, reducing their Will and Stealth by one fifth of your effective Song and encouraging reckless attacks. Expect to attract attention.

Trigger: Public ability preview or newly available ability; raw serial 141, skill 7, ability slot 1.

Sources: `lib/edit/ability.txt`, `src/player/player-song-effects.c`, `src/birth/birth-traits.c`, `src/melee/melee-process.c`, `src/monster/monster-attr.c`, `src/player/effects.c`, `src/player/player-bonuses.c`, `src/player/player-skills.c`.

## Song of Delvings

`ability.142.preview`

Level: **Extended**.

Priority: **65** (higher appears first).

**1. Decision**

Gradually reveal nearby passages and rooms, expanding outward from explored terrain. Keep singing while you explore to extend the mapped area.

Trigger: Public ability preview or newly available ability; raw serial 142, skill 7, ability slot 2.

Sources: `lib/edit/ability.txt`, `src/player/player-song-effects.c`, `src/birth/birth-traits.c`, `src/player/player-bonuses.c`, `src/player/player-skills.c`, `src/player/player-song-duels.c`, `src/player/player-songs.c`, `src/cmd/ui/cmd-ui-abilities.c`.

## Song of Freedom

`ability.143.preview`

Level: **Extended**.

Priority: **65** (higher appears first).

**1. Decision**

Reveal hidden doors, disarm traps and clear rubble through song checks. It also helps free you from slowing effects. Check the actual terrain changes before crossing.

Trigger: Public ability preview or newly available ability; raw serial 143, skill 7, ability slot 3.

Sources: `lib/edit/ability.txt`, `src/player/player-song-effects.c`, `src/birth/birth-traits.c`, `src/dungeon/dungeon-player.c`, `src/player/player-bonuses.c`, `src/player/player-skills.c`, `src/player/player-song-duels.c`, `src/player/player-songs.c`.

## Song of Silence

`ability.144.preview`

Level: **Extended**.

Priority: **65** (higher appears first).

**1. Decision**

Muffle sounds, making it harder for enemies to hear you or call for allies. Enemies may still see you, so cover and distance remain useful.

Trigger: Public ability preview or newly available ability; raw serial 144, skill 7, ability slot 4.

Sources: `lib/edit/ability.txt`, `src/player/player-song-effects.c`, `src/birth/birth-traits.c`, `src/melee/melee-attack-ranged.c`, `src/melee/melee-movement-resolution.c`, `src/melee/melee-process.c`, `src/monster/monster-update.c`, `src/player/player-bonuses.c`.

## Song of Staunching

`ability.145.preview`

Level: **Extended**.

Priority: **65** (higher appears first).

**1. Decision**

Each song effect stops bleeding and heals Health at an average rate of effective Song divided by 12 per turn. At Song 12, it heals 1 Health per tick. This direct healing works separately from ordinary Health regeneration and spends Voice.

Trigger: Public ability preview or newly available ability; raw serial 145, skill 7, ability slot 5.

Sources: `lib/edit/ability.txt`, `src/player/player-song-effects.c`, `src/birth/birth-traits.c`, `src/player/player-bonuses.c`, `src/player/player-skills.c`, `src/player/player-song-duels.c`, `src/player/player-songs.c`, `src/cmd/ui/cmd-ui-abilities.c`.

## Song of Thresholds

`ability.146.preview`

Level: **Extended**.

Priority: **65** (higher appears first).

**1. Decision**

Ward closed doors so that enemies have more difficulty passing them. Stronger Song strengthens the barriers; they are not permanent walls.

Trigger: Public ability preview or newly available ability; raw serial 146, skill 7, ability slot 6.

Sources: `lib/edit/ability.txt`, `src/player/player-song-effects.c`, `src/birth/birth-traits.c`, `src/player/player-bonuses.c`, `src/player/player-skills.c`, `src/player/player-songs.c`, `src/cmd/ui/cmd-ui-abilities.c`, `src/cmd/world/cmd-interact.c`.

## Song of the Trees

`ability.147.preview`

Level: **Extended**.

Priority: **65** (higher appears first).

**1. Decision**

Increase light radius by 1 at effective Song 0-5, by 2 at 6-11, by 3 at 12-18, and in wider steps thereafter. The light can stun or wound light-sensitive creatures after a contest against their Will.

Trigger: Public ability preview or newly available ability; raw serial 147, skill 7, ability slot 7.

Sources: `lib/edit/ability.txt`, `src/player/player-song-effects.c`, `src/birth/birth-traits.c`, `src/player/player-bonuses.c`, `src/player/player-light.c`, `src/player/player-skills.c`, `src/player/player-song-duels.c`.

## Woven Themes

`ability.148.preview`

Level: **Extended**.

Priority: **65** (higher appears first).

**1. Decision**

Sing a second song as a minor theme at half your Song score. Both themes spend Voice. Choose effects that work well together and watch the remaining Voice.

Trigger: Public ability preview or newly available ability; raw serial 148, skill 7, ability slot 9.

Sources: `lib/edit/ability.txt`, `src/player/player-song-effects.c`, `src/birth/birth-traits.c`, `src/player/player-songs.c`, `src/sdl/input/sdl-player-actions.c`, `src/cmd/ui/cmd-ui-abilities.c`.

## Song of Slaying

`ability.149.preview`

Level: **Extended**.

Priority: **65** (higher appears first).

**1. Decision**

A critical melee hit slays its target if the target's Health is at most twice your effective Song. It must be a critical hit; ordinary hits do not trigger this effect.

Trigger: Public ability preview or newly available ability; raw serial 149, skill 7, ability slot 10.

Sources: `lib/edit/ability.txt`, `src/player/player-song-effects.c`, `src/birth/birth-traits.c`, `src/player/player-bonuses.c`, `src/player/player-skills.c`, `src/player/player-song-duels.c`, `src/player/player-songs.c`, `src/cmd/combat/cmd-combat.c`.

## Song of Revealing

`ability.150.preview`

Level: **Extended**.

Priority: **65** (higher appears first).

**1. Decision**

Make Song checks each turn to reveal nearby creatures and items. Revealed items, including carried and equipped items, gain an extra 1d5 identification roll.

Trigger: Public ability preview or newly available ability; raw serial 150, skill 7, ability slot 8.

Sources: `lib/edit/ability.txt`, `src/player/player-song-effects.c`, `src/birth/birth-traits.c`, `src/player/player-lore.c`, `src/player/player-skills.c`, `src/player/player-song-duels.c`, `src/player/player-songs.c`, `src/cmd/ui/cmd-ui-abilities.c`.

## Song of Elveness

`ability.151.preview`

Level: **Extended**.

Priority: **65** (higher appears first).

**1. Decision**

Gain +1 Grace and an Evasion bonus: +1 at effective Song 0-7, +2 at 8-15, +3 at 16-24, and in wider steps thereafter.

Trigger: Public ability preview or newly available ability; raw serial 151, skill 7, ability slot 11.

Sources: `lib/edit/ability.txt`, `src/player/player-song-effects.c`, `src/birth/birth-traits.c`, `src/player/player-bonuses.c`, `src/player/player-skills.c`, `src/player/player-song-duels.c`, `src/player/player-songs.c`, `src/cmd/ui/cmd-ui-abilities.c`.

## Song of Staying

`ability.152.preview`

Level: **Extended**.

Priority: **65** (higher appears first).

**1. Decision**

Gain Will equal to half your effective Song and 2d2 Protection against all damage. Special character traits can improve the song further.

Trigger: Public ability preview or newly available ability; raw serial 152, skill 7, ability slot 12.

Sources: `lib/edit/ability.txt`, `src/player/player-song-effects.c`, `src/birth/birth-traits.c`, `src/melee/melee-attack.c`, `src/player/player-bonuses.c`, `src/player/player-skills.c`, `src/player/player-song-duels.c`, `src/player/player-songs.c`.

## Song of Disguise

`ability.153.preview`

Level: **Extended**.

Priority: **65** (higher appears first).

**1. Decision**

You cannot start Song of Disguise while an alert non-peaceful creature observes you. Your attack ends the disguise. Each round, effective Song + 5 + your Will contests enemy Will + Perception, with penalties for observers, creatures that already saw through you and recent attackers. Distance helps; a success fools that creature for the current round.

Trigger: Public ability preview or newly available ability; raw serial 153, skill 7, ability slot 13.

Sources: `lib/edit/ability.txt`, `src/player/player-song-effects.c`, `src/birth/birth-traits.c`, `src/monster/monster-move.c`, `src/player/player-bonuses.c`, `src/player/player-skills.c`, `src/player/player-song-disguise.c`, `src/player/player-song-duels.c`.

## Song of Lórien

`ability.154.preview`

Level: **Extended**.

Priority: **65** (higher appears first).

**1. Decision**

Gradually make nearby enemies sleepy through opposed song checks. They can resist, and an enemy may still act before falling asleep.

Trigger: Public ability preview or newly available ability; raw serial 154, skill 7, ability slot 14.

Sources: `lib/edit/ability.txt`, `src/player/player-song-effects.c`, `src/birth/birth-traits.c`, `src/player/player-bonuses.c`, `src/player/player-skills.c`, `src/player/player-song-duels.c`, `src/player/player-songs.c`, `src/cmd/ui/cmd-ui-abilities.c`.

## Song of Shattering

`ability.155.preview`

Level: **Extended**.

Priority: **65** (higher appears first).

**1. Decision**

Attempt to shatter enemy weapons and armour, weakening their attacks and defence. The effect also works against stone bodies and is resisted by Will.

Trigger: Public ability preview or newly available ability; raw serial 155, skill 7, ability slot 15.

Sources: `lib/edit/ability.txt`, `src/player/player-song-effects.c`, `src/birth/birth-traits.c`, `src/player/player-skills.c`, `src/player/player-song-duels.c`, `src/player/player-songs.c`, `src/cmd/ui/cmd-ui-abilities.c`.

## Song of Mastery

`ability.156.preview`

Level: **Extended**.

Priority: **65** (higher appears first).

**1. Decision**

Overwhelm nearby enemies in contests of will to prevent movement or actions. A resisted check leaves the enemy able to act.

Trigger: Public ability preview or newly available ability; raw serial 156, skill 7, ability slot 16.

Sources: `lib/edit/ability.txt`, `src/player/player-song-effects.c`, `src/birth/birth-traits.c`, `src/melee/melee-process.c`, `src/player/player-bonuses.c`, `src/player/player-skills.c`, `src/player/player-song-duels.c`, `src/player/player-songs.c`.

## Grace

`ability.157.preview`

Level: **Extended**.

Priority: **65** (higher appears first).

**1. Decision**

+1 Grace.

Trigger: Public ability preview or newly available ability; raw serial 157, skill 7, ability slot 17.

Sources: `lib/edit/ability.txt`, `src/player/player-song-effects.c`, `src/birth/birth-traits.c`, `src/player/player-bonuses.c`, `src/sdl/input/sdl-player-actions.c`, `src/cmd/ui/cmd-ui-abilities.c`.

## Song of Contest

`ability.158.preview`

Level: **Extended**.

Priority: **65** (higher appears first).

**1. Decision**

Contest challenges one eligible foe to repeated opposed rolls. Winning permanently reduces its Will, Stealth, Evasion and armour dice. Losing drains one randomly chosen attribute by 1. A completed duel stops the song and locks singing for 10 turns; you cannot repeat the same completed duel against that foe.

Trigger: Public ability preview or newly available ability; raw serial 158, skill 7, ability slot 18.

Sources: `lib/edit/ability.txt`, `src/player/player-song-effects.c`, `src/birth/birth-traits.c`, `src/monster/monster-lore.c`, `src/player/player-skills.c`, `src/player/player-song-duels.c`, `src/player/player-songs.c`, `src/cmd/ui/cmd-ui-abilities.c`.

## Song of Lament

`ability.159.preview`

Level: **Extended**.

Priority: **65** (higher appears first).

**1. Decision**

Lament works toward permanently reducing an eligible target's Will, maximum Health and damage dice. Completing the effect drains your Grace by 1, even on success. The song then stops and singing is locked for 10 turns; you cannot repeat the completed duel against that foe. Read the live target and Voice cost before committing.

Trigger: Public ability preview or newly available ability; raw serial 159, skill 7, ability slot 19.

Sources: `lib/edit/ability.txt`, `src/player/player-song-effects.c`, `src/birth/birth-traits.c`, `src/monster/monster-lore.c`, `src/player/player-skills.c`, `src/player/player-song-duels.c`, `src/player/player-songs.c`, `src/cmd/ui/cmd-ui-abilities.c`.

## Mandos' Doom

`ability.160.preview`

Level: **Extended**.

Priority: **65** (higher appears first).

**1. Decision**

Mandos' Doom is a quest reward granting immunity to fear, hallucination, trance, rage, stun and confusion.

Trigger: Public ability preview or newly available ability; raw serial 160, skill 8, ability slot 0.

Sources: `lib/edit/ability.txt`, `src/player/player-bonuses.c`, `src/birth/birth-traits.c`, `src/dungeon/dungeon-startup.c`.

## Aulë's Forge

`ability.161.preview`

Level: **Extended**.

Priority: **65** (higher appears first).

**1. Decision**

Aulë's Forge improves on Masterpiece. Above your normal smithing limit, spend 1 base Smithing for each 2 excess difficulty points, rounding the cost up. You can reach up to twice your base Smithing beyond the normal limit. This quest reward replaces the less efficient Masterpiece rule.

Trigger: Public ability preview or newly available ability; raw serial 161, skill 8, ability slot 1.

Sources: `lib/edit/ability.txt`, `src/player/player-bonuses.c`, `src/birth/birth-traits.c`, `src/dungeon/dungeon-startup.c`, `src/cmd/ui/cmd-ui-abilities.c`, `src/cmd/ui/cmd-ui-smithing.c`.

## Oath of Mercy

`ability.162.preview`

Level: **Extended**.

Priority: **65** (higher appears first).

**1. Decision**

Oath of Mercy grants +1 Grace and forbids attacking or harming Men or Elves. This includes indirect harm. Read the exact oath terms before swearing it.

Trigger: Public ability preview or newly available ability; raw serial 162, skill 8, ability slot 2.

Sources: `lib/edit/ability.txt`, `src/player/player-bonuses.c`, `lib/edit/oath.txt`, `src/cmd/combat/cmd-combat.c`, `src/birth/birth-setup.c`, `src/birth/birth-traits.c`, `src/player/player-light.c`, `src/cmd/ui/cmd-ui-abilities.c`, `src/cmd/ui/cmd-ui-knowledge.c`.

## Oath of Silence

`ability.163.preview`

Level: **Extended**.

Priority: **65** (higher appears first).

**1. Decision**

Oath of Silence grants +1 Dexterity. Singing breaks it. Plan around other ways to use your skills and resources before accepting the oath.

Trigger: Public ability preview or newly available ability; raw serial 163, skill 8, ability slot 3.

Sources: `lib/edit/ability.txt`, `src/player/player-bonuses.c`, `lib/edit/oath.txt`, `src/cmd/combat/cmd-combat.c`, `src/birth/birth-setup.c`, `src/birth/birth-traits.c`, `src/player/player-light.c`, `src/cmd/ui/cmd-ui-abilities.c`, `src/cmd/ui/cmd-ui-knowledge.c`.

## Oath of Iron

`ability.164.preview`

Level: **Extended**.

Priority: **65** (higher appears first).

**1. Decision**

Oath of Iron grants +1 Constitution. Until you possess a Silmaril, it forbids going upstairs or leaving the depths. Prepare for a journey without retreat to shallower levels.

Trigger: Public ability preview or newly available ability; raw serial 164, skill 8, ability slot 4.

Sources: `lib/edit/ability.txt`, `src/player/player-bonuses.c`, `lib/edit/oath.txt`, `src/cmd/combat/cmd-combat.c`, `src/birth/birth-setup.c`, `src/birth/birth-traits.c`, `src/player/player-light.c`, `src/cmd/ui/cmd-ui-abilities.c`, `src/cmd/ui/cmd-ui-knowledge.c`.

## Nienna's Gift of Mercy

`ability.165.preview`

Level: **Extended**.

Priority: **65** (higher appears first).

**1. Decision**

Nienna's Gift grants up to +10 Stealth according to the share of seen non-unique creatures you have spared. For example, sparing half gives +5. The result rounds up; before seeing any, the bonus is 0. Check the current count and bonus in the character view.

Trigger: Public ability preview or newly available ability; raw serial 165, skill 8, ability slot 5.

Sources: `lib/edit/ability.txt`, `src/player/player-bonuses.c`, `src/birth/birth-traits.c`, `src/cmd/ui/cmd-ui-abilities.c`.

## Oath of the Smith

`ability.166.preview`

Level: **Extended**.

Priority: **65** (higher appears first).

**1. Decision**

Oath of the Smith grants +5 Smithing. It forbids picking up, wielding or wearing weapons or armour made by others. Prepare your own equipment and read item actions carefully before accepting it.

Trigger: Public ability preview or newly available ability; raw serial 166, skill 8, ability slot 6.

Sources: `lib/edit/ability.txt`, `src/player/player-bonuses.c`, `lib/edit/oath.txt`, `src/cmd/combat/cmd-combat.c`, `src/birth/birth-setup.c`, `src/birth/birth-traits.c`, `src/player/player-light.c`, `src/cmd/ui/cmd-ui-abilities.c`, `src/cmd/ui/cmd-ui-knowledge.c`.

## Oath of the Valorous Heart

`ability.167.preview`

Level: **Extended**.

Priority: **65** (higher appears first).

**1. Decision**

Oath of Valour grants +1 Strength. Harming fleeing enemies breaks it, including through indirect effects. A fleeing creature has lost courage; it is different from a sleeping or unwary creature.

Trigger: Public ability preview or newly available ability; raw serial 167, skill 8, ability slot 7.

Sources: `lib/edit/ability.txt`, `src/player/player-bonuses.c`, `lib/edit/oath.txt`, `src/cmd/combat/cmd-combat.c`, `src/birth/birth-setup.c`, `src/birth/birth-traits.c`, `src/player/player-light.c`, `src/cmd/ui/cmd-ui-abilities.c`, `src/cmd/ui/cmd-ui-knowledge.c`.

## Unique Bane

`ability.168.preview`

Level: **Extended**.

Priority: **65** (higher appears first).

**1. Decision**

Unique Bane is a quest reward granting +3 Attack and Evasion against unique monsters.

Trigger: Public ability preview or newly available ability; raw serial 168, skill 8, ability slot 8.

Sources: `lib/edit/ability.txt`, `src/player/player-bonuses.c`, `src/birth/birth-traits.c`, `src/quest/valar/other-valar.c`, `src/cmd/ui/cmd-ui-abilities.c`.

## Oath of Light

`ability.169.preview`

Level: **Extended**.

Priority: **65** (higher appears first).

**1. Decision**

Oath of Light grants +1 light radius while valid. Equipping items with Darkness or Unlight breaks it. Check known item properties before changing equipment.

Trigger: Public ability preview or newly available ability; raw serial 169, skill 8, ability slot 9.

Sources: `lib/edit/ability.txt`, `src/player/player-bonuses.c`, `lib/edit/oath.txt`, `src/cmd/combat/cmd-combat.c`, `src/birth/birth-setup.c`, `src/birth/birth-traits.c`, `src/player/player-light.c`, `src/cmd/item/cmd-item-core.c`.

## Chasm

`terrain.2`

Level: **Extended**.

Priority: **38** (higher appears first).

**1. Info**

A chasm can drop you to a deeper level and cause severe damage. Use a route around it when possible. Leaping requires a run-up and a safe landing; walking into a chasm is a fall, not a leap.

Trigger: Feature 2 is on the player square or visibly adjacent and marked; secret/unrevealed terrain is excluded.

Sources: `lib/edit/terrain.txt`, `src/cmd/world/cmd-interact.c`, `src/cmd/movement/cmd-movement.c`.

## Glyph of warding

`terrain.3`

Level: **Extended**.

Priority: **38** (higher appears first).

**1. Info**

A glyph of warding makes it harder for enemies to cross its square. It can help defend a doorway, but opponents can break through. Keep a retreat route available.

Trigger: Feature 3 is on the player square or visibly adjacent and marked; secret/unrevealed terrain is excluded.

Sources: `lib/edit/terrain.txt`, `src/cmd/world/cmd-interact.c`, `src/cmd/movement/cmd-movement.c`.

## Open door

`terrain.4`

Level: **Extended**.

Priority: **38** (higher appears first).

**1. Info**

An open door lets you pass. Closing it can block sight and delay an enemy. Some creatures can open, break or pass through doors, so check their known abilities.

Trigger: Feature 4 is on the player square or visibly adjacent and marked; secret/unrevealed terrain is excluded.

Sources: `lib/edit/terrain.txt`, `src/cmd/world/cmd-interact.c`, `src/cmd/movement/cmd-movement.c`.

## Broken door

`terrain.5`

Level: **Extended**.

Priority: **38** (higher appears first).

**1. Info**

A broken door cannot be closed normally. Look for another doorway or a narrow passage if you need to limit approaching enemies.

Trigger: Feature 5 is on the player square or visibly adjacent and marked; secret/unrevealed terrain is excluded.

Sources: `lib/edit/terrain.txt`, `src/cmd/world/cmd-interact.c`, `src/cmd/movement/cmd-movement.c`.

## Warded doors

`terrain.6`

Level: **Extended**.

Priority: **38** (higher appears first).

**1. Info**

A warded door is harder for enemies to open or break through. The strength of its ward matters. Check the available interaction before relying on it to hold a passage.

Trigger: Known nearby terrain in this feature family, after grouping equivalent strengths or positive remaining-use counts; exhausted forges have separate lessons.

Sources: `lib/edit/terrain.txt`, `src/cmd/world/cmd-interact.c`, `src/cmd/movement/cmd-movement.c`.

## Warded door

`terrain.7`

Level: **Extended**.

Priority: **38** (higher appears first).

Archive compatibility entry. New encounters use `terrain.6` for this terrain family.

**1. Info**

A warded door is harder for enemies to open or break through. The strength of its ward matters. Check the available interaction before relying on it to hold a passage.

Trigger: Legacy lesson retained for saved progress and archive review. New encounters use terrain.6.

Sources: `lib/edit/terrain.txt`, `src/cmd/world/cmd-interact.c`, `src/cmd/movement/cmd-movement.c`.

## Warded door

`terrain.8`

Level: **Extended**.

Priority: **38** (higher appears first).

Archive compatibility entry. New encounters use `terrain.6` for this terrain family.

**1. Info**

A warded door is harder for enemies to open or break through. The strength of its ward matters. Check the available interaction before relying on it to hold a passage.

Trigger: Legacy lesson retained for saved progress and archive review. New encounters use terrain.6.

Sources: `lib/edit/terrain.txt`, `src/cmd/world/cmd-interact.c`, `src/cmd/movement/cmd-movement.c`.

## Patch of sunlight

`terrain.9`

Level: **Extended**.

Priority: **38** (higher appears first).

**1. Info**

Sunlight illuminates this square independently of your equipment. Light-sensitive creatures can suffer in bright light. Check their learned traits before using sunlight in a fight.

Trigger: Feature 9 is on the player square or visibly adjacent and marked; secret/unrevealed terrain is excluded.

Sources: `lib/edit/terrain.txt`, `src/cmd/world/cmd-interact.c`, `src/cmd/movement/cmd-movement.c`.

## False floor

`terrain.16`

Level: **Extended**.

Priority: **38** (higher appears first).

**1. Info**

A false floor can collapse and drop you to a deeper level. Avoid it or inspect a legal disarm or leap option before crossing.

Trigger: Feature 16 is on the player square or visibly adjacent and marked; secret/unrevealed terrain is excluded.

Sources: `lib/edit/terrain.txt`, `src/cmd/world/cmd-interact.c`, `src/cmd/movement/cmd-movement.c`.

## Pit

`terrain.17`

Level: **Extended**.

Priority: **38** (higher appears first).

**1. Info**

A pit can hurt you when you fall in. Climbing out takes time and can leave you exposed to enemies. A legal leap crosses it without entering the pit.

Trigger: Feature 17 is on the player square or visibly adjacent and marked; secret/unrevealed terrain is excluded.

Sources: `lib/edit/terrain.txt`, `src/cmd/world/cmd-interact.c`, `src/cmd/movement/cmd-movement.c`.

## Spiked pit

`terrain.18`

Level: **Extended**.

Priority: **38** (higher appears first).

**1. Info**

A spiked pit can cause damage and bleeding, then delay you while you climb out. Consider going around it or using a legal disarm or leap option.

Trigger: Feature 18 is on the player square or visibly adjacent and marked; secret/unrevealed terrain is excluded.

Sources: `lib/edit/terrain.txt`, `src/cmd/world/cmd-interact.c`, `src/cmd/movement/cmd-movement.c`.

## Dart trap

`terrain.19`

Level: **Extended**.

Priority: **38** (higher appears first).

**1. Info**

A dart trap makes an attack that can damage you and drain Strength if it gets through your armour. Avoid the square or inspect Disarm and its failure risk.

Trigger: Feature 19 is on the player square or visibly adjacent and marked; secret/unrevealed terrain is excluded.

Sources: `lib/edit/terrain.txt`, `src/cmd/world/cmd-interact.c`, `src/cmd/movement/cmd-movement.c`.

## Gas trap

`terrain.20`

Level: **Extended**.

Priority: **38** (higher appears first).

**1. Info**

Gas traps can confuse you or erase remembered map information. Seeing the trap does not disable it. Choose a route around it or inspect the disarm option before crossing.

Trigger: Feature 20 is on the player square or visibly adjacent and marked; secret/unrevealed terrain is excluded.

Sources: `lib/edit/terrain.txt`, `src/cmd/world/cmd-interact.c`, `src/cmd/movement/cmd-movement.c`.

## Gas trap

`terrain.21`

Level: **Extended**.

Priority: **38** (higher appears first).

**1. Info**

Gas traps can confuse you or erase remembered map information. Seeing the trap does not disable it. Choose a route around it or inspect the disarm option before crossing.

Trigger: Feature 21 is on the player square or visibly adjacent and marked; secret/unrevealed terrain is excluded.

Sources: `lib/edit/terrain.txt`, `src/cmd/world/cmd-interact.c`, `src/cmd/movement/cmd-movement.c`.

## Alarm trap

`terrain.22`

Level: **Extended**.

Priority: **38** (higher appears first).

**1. Info**

An alarm trap makes noise that can alert nearby enemies. Avoid or disarm it when you want to stay unnoticed.

Trigger: Feature 22 is on the player square or visibly adjacent and marked; secret/unrevealed terrain is excluded.

Sources: `lib/edit/terrain.txt`, `src/cmd/world/cmd-interact.c`, `src/cmd/movement/cmd-movement.c`.

## Flash trap

`terrain.23`

Level: **Extended**.

Priority: **38** (higher appears first).

**1. Info**

A flash trap can blind you. A revealed trap is still active, so avoid it or inspect the disarm option. True Sight or Miruvor can cure blindness if it occurs.

Trigger: Feature 23 is on the player square or visibly adjacent and marked; secret/unrevealed terrain is excluded.

Sources: `lib/edit/terrain.txt`, `src/cmd/world/cmd-interact.c`, `src/cmd/movement/cmd-movement.c`.

## Caltrop field

`terrain.24`

Level: **Extended**.

Priority: **38** (higher appears first).

**1. Info**

A caltrop field can injure your feet and slow you if you fail to step carefully. Crossing is noisy even when you avoid injury. Consider another route or the available clearing action.

Trigger: Feature 24 is on the player square or visibly adjacent and marked; secret/unrevealed terrain is excluded.

Sources: `lib/edit/terrain.txt`, `src/cmd/world/cmd-interact.c`, `src/cmd/movement/cmd-movement.c`.

## Roost

`terrain.25`

Level: **Extended**.

Priority: **38** (higher appears first).

**1. Info**

Disturbing a roost can summon birds or bats. Leaping does not bypass it. Leave room to deal with new enemies if you choose to cross.

Trigger: Feature 25 is on the player square or visibly adjacent and marked; secret/unrevealed terrain is excluded.

Sources: `lib/edit/terrain.txt`, `src/cmd/world/cmd-interact.c`, `src/cmd/movement/cmd-movement.c`.

## Web

`terrain.26`

Level: **Extended**.

Priority: **38** (higher appears first).

**1. Info**

A web can trap you and attract a spider. Escaping can take several turns. Leaping does not bypass webs, so consider going around or clearing one before entering.

Trigger: Feature 26 is on the player square or visibly adjacent and marked; secret/unrevealed terrain is excluded.

Sources: `lib/edit/terrain.txt`, `src/cmd/world/cmd-interact.c`, `src/cmd/movement/cmd-movement.c`.

## Deadfall trap

`terrain.27`

Level: **Extended**.

Priority: **38** (higher appears first).

**1. Info**

A deadfall drops heavy debris and can change the surrounding terrain. Avoid it or inspect Disarm before crossing; failure can trigger the trap.

Trigger: Feature 27 is on the player square or visibly adjacent and marked; secret/unrevealed terrain is excluded.

Sources: `lib/edit/terrain.txt`, `src/cmd/world/cmd-interact.c`, `src/cmd/movement/cmd-movement.c`.

## Discoloured spot

`terrain.28`

Level: **Extended**.

Priority: **38** (higher appears first).

**1. Info**

This discoloured spot marks a suspicious trap square. Inspect the available actions and avoid testing it with your feet. Discovering it has not made it harmless.

Trigger: Feature 28 is on the player square or visibly adjacent and marked; secret/unrevealed terrain is excluded.

Sources: `lib/edit/terrain.txt`, `src/cmd/world/cmd-interact.c`, `src/cmd/movement/cmd-movement.c`.

## Imprisonment trap

`terrain.29`

Level: **Extended**.

Priority: **38** (higher appears first).

**1. Info**

An imprisonment trap can raise barriers around you and draw attention. Check nearby exits before approaching, or use a route around it.

Trigger: Feature 29 is on the player square or visibly adjacent and marked; secret/unrevealed terrain is excluded.

Sources: `lib/edit/terrain.txt`, `src/cmd/world/cmd-interact.c`, `src/cmd/movement/cmd-movement.c`.

## Door

`terrain.32`

Level: **Extended**.

Priority: **38** (higher appears first).

**1. Info**

A closed door blocks passage and sight. Opening or closing it spends time. Doors can help separate a group of enemies, though some creatures can open or break them.

Trigger: Feature 32 is on the player square or visibly adjacent and marked; secret/unrevealed terrain is excluded.

Sources: `lib/edit/terrain.txt`, `src/cmd/world/cmd-interact.c`, `src/cmd/movement/cmd-movement.c`.

## Locked doors

`terrain.33`

Level: **Extended**.

Priority: **38** (higher appears first).

**1. Info**

A locked door needs a successful unlocking attempt before you can open it. Attempts spend turns. Bashing is another possibility, but it makes noise and can alert enemies.

Trigger: Known nearby terrain in this feature family, after grouping equivalent strengths or positive remaining-use counts; exhausted forges have separate lessons.

Sources: `lib/edit/terrain.txt`, `src/cmd/world/cmd-interact.c`, `src/cmd/movement/cmd-movement.c`.

## Locked door

`terrain.34`

Level: **Extended**.

Priority: **38** (higher appears first).

Archive compatibility entry. New encounters use `terrain.33` for this terrain family.

**1. Info**

A locked door needs a successful unlocking attempt before you can open it. Attempts spend turns. Bashing is another possibility, but it makes noise and can alert enemies.

Trigger: Legacy lesson retained for saved progress and archive review. New encounters use terrain.33.

Sources: `lib/edit/terrain.txt`, `src/cmd/world/cmd-interact.c`, `src/cmd/movement/cmd-movement.c`.

## Locked door

`terrain.35`

Level: **Extended**.

Priority: **38** (higher appears first).

Archive compatibility entry. New encounters use `terrain.33` for this terrain family.

**1. Info**

A locked door needs a successful unlocking attempt before you can open it. Attempts spend turns. Bashing is another possibility, but it makes noise and can alert enemies.

Trigger: Legacy lesson retained for saved progress and archive review. New encounters use terrain.33.

Sources: `lib/edit/terrain.txt`, `src/cmd/world/cmd-interact.c`, `src/cmd/movement/cmd-movement.c`.

## Locked door

`terrain.36`

Level: **Extended**.

Priority: **38** (higher appears first).

Archive compatibility entry. New encounters use `terrain.33` for this terrain family.

**1. Info**

A locked door needs a successful unlocking attempt before you can open it. Attempts spend turns. Bashing is another possibility, but it makes noise and can alert enemies.

Trigger: Legacy lesson retained for saved progress and archive review. New encounters use terrain.33.

Sources: `lib/edit/terrain.txt`, `src/cmd/world/cmd-interact.c`, `src/cmd/movement/cmd-movement.c`.

## Locked door

`terrain.37`

Level: **Extended**.

Priority: **38** (higher appears first).

Archive compatibility entry. New encounters use `terrain.33` for this terrain family.

**1. Info**

A locked door needs a successful unlocking attempt before you can open it. Attempts spend turns. Bashing is another possibility, but it makes noise and can alert enemies.

Trigger: Legacy lesson retained for saved progress and archive review. New encounters use terrain.33.

Sources: `lib/edit/terrain.txt`, `src/cmd/world/cmd-interact.c`, `src/cmd/movement/cmd-movement.c`.

## Locked door

`terrain.38`

Level: **Extended**.

Priority: **38** (higher appears first).

Archive compatibility entry. New encounters use `terrain.33` for this terrain family.

**1. Info**

A locked door needs a successful unlocking attempt before you can open it. Attempts spend turns. Bashing is another possibility, but it makes noise and can alert enemies.

Trigger: Legacy lesson retained for saved progress and archive review. New encounters use terrain.33.

Sources: `lib/edit/terrain.txt`, `src/cmd/world/cmd-interact.c`, `src/cmd/movement/cmd-movement.c`.

## Locked door

`terrain.39`

Level: **Extended**.

Priority: **38** (higher appears first).

Archive compatibility entry. New encounters use `terrain.33` for this terrain family.

**1. Info**

A locked door needs a successful unlocking attempt before you can open it. Attempts spend turns. Bashing is another possibility, but it makes noise and can alert enemies.

Trigger: Legacy lesson retained for saved progress and archive review. New encounters use terrain.33.

Sources: `lib/edit/terrain.txt`, `src/cmd/world/cmd-interact.c`, `src/cmd/movement/cmd-movement.c`.

## Jammed doors

`terrain.40`

Level: **Extended**.

Priority: **38** (higher appears first).

**1. Info**

A jammed door cannot be opened normally. Inspect Bash or another available way through. Breaking a door takes time and makes noise; another route may be safer.

Trigger: Known nearby terrain in this feature family, after grouping equivalent strengths or positive remaining-use counts; exhausted forges have separate lessons.

Sources: `lib/edit/terrain.txt`, `src/cmd/world/cmd-interact.c`, `src/cmd/movement/cmd-movement.c`.

## Jammed door

`terrain.41`

Level: **Extended**.

Priority: **38** (higher appears first).

Archive compatibility entry. New encounters use `terrain.40` for this terrain family.

**1. Info**

A jammed door cannot be opened normally. Inspect Bash or another available way through. Breaking a door takes time and makes noise; another route may be safer.

Trigger: Legacy lesson retained for saved progress and archive review. New encounters use terrain.40.

Sources: `lib/edit/terrain.txt`, `src/cmd/world/cmd-interact.c`, `src/cmd/movement/cmd-movement.c`.

## Jammed door

`terrain.42`

Level: **Extended**.

Priority: **38** (higher appears first).

Archive compatibility entry. New encounters use `terrain.40` for this terrain family.

**1. Info**

A jammed door cannot be opened normally. Inspect Bash or another available way through. Breaking a door takes time and makes noise; another route may be safer.

Trigger: Legacy lesson retained for saved progress and archive review. New encounters use terrain.40.

Sources: `lib/edit/terrain.txt`, `src/cmd/world/cmd-interact.c`, `src/cmd/movement/cmd-movement.c`.

## Jammed door

`terrain.43`

Level: **Extended**.

Priority: **38** (higher appears first).

Archive compatibility entry. New encounters use `terrain.40` for this terrain family.

**1. Info**

A jammed door cannot be opened normally. Inspect Bash or another available way through. Breaking a door takes time and makes noise; another route may be safer.

Trigger: Legacy lesson retained for saved progress and archive review. New encounters use terrain.40.

Sources: `lib/edit/terrain.txt`, `src/cmd/world/cmd-interact.c`, `src/cmd/movement/cmd-movement.c`.

## Jammed door

`terrain.44`

Level: **Extended**.

Priority: **38** (higher appears first).

Archive compatibility entry. New encounters use `terrain.40` for this terrain family.

**1. Info**

A jammed door cannot be opened normally. Inspect Bash or another available way through. Breaking a door takes time and makes noise; another route may be safer.

Trigger: Legacy lesson retained for saved progress and archive review. New encounters use terrain.40.

Sources: `lib/edit/terrain.txt`, `src/cmd/world/cmd-interact.c`, `src/cmd/movement/cmd-movement.c`.

## Jammed door

`terrain.45`

Level: **Extended**.

Priority: **38** (higher appears first).

Archive compatibility entry. New encounters use `terrain.40` for this terrain family.

**1. Info**

A jammed door cannot be opened normally. Inspect Bash or another available way through. Breaking a door takes time and makes noise; another route may be safer.

Trigger: Legacy lesson retained for saved progress and archive review. New encounters use terrain.40.

Sources: `lib/edit/terrain.txt`, `src/cmd/world/cmd-interact.c`, `src/cmd/movement/cmd-movement.c`.

## Jammed door

`terrain.46`

Level: **Extended**.

Priority: **38** (higher appears first).

Archive compatibility entry. New encounters use `terrain.40` for this terrain family.

**1. Info**

A jammed door cannot be opened normally. Inspect Bash or another available way through. Breaking a door takes time and makes noise; another route may be safer.

Trigger: Legacy lesson retained for saved progress and archive review. New encounters use terrain.40.

Sources: `lib/edit/terrain.txt`, `src/cmd/world/cmd-interact.c`, `src/cmd/movement/cmd-movement.c`.

## Jammed door

`terrain.47`

Level: **Extended**.

Priority: **38** (higher appears first).

Archive compatibility entry. New encounters use `terrain.40` for this terrain family.

**1. Info**

A jammed door cannot be opened normally. Inspect Bash or another available way through. Breaking a door takes time and makes noise; another route may be safer.

Trigger: Legacy lesson retained for saved progress and archive review. New encounters use terrain.40.

Sources: `lib/edit/terrain.txt`, `src/cmd/world/cmd-interact.c`, `src/cmd/movement/cmd-movement.c`.

## Pile of rubble

`terrain.49`

Level: **Extended**.

Priority: **38** (higher appears first).

**1. Info**

Rubble blocks a route until cleared. Tunnelling can take several turns and make noise. Prepare a digging tool and check for enemies before starting.

Trigger: Feature 49 is on the player square or visibly adjacent and marked; secret/unrevealed terrain is excluded.

Sources: `lib/edit/terrain.txt`, `src/cmd/world/cmd-interact.c`, `src/cmd/movement/cmd-movement.c`.

## Quartz vein

`terrain.51`

Level: **Extended**.

Priority: **38** (higher appears first).

**1. Info**

Quartz is solid rock. Digging through it requires a suitable tool and can take repeated noisy turns. Inspect the tunnelling action before committing.

Trigger: Feature 51 is on the player square or visibly adjacent and marked; secret/unrevealed terrain is excluded.

Sources: `lib/edit/terrain.txt`, `src/cmd/world/cmd-interact.c`, `src/cmd/movement/cmd-movement.c`.

## Granite wall

`terrain.63`

Level: **Extended**.

Priority: **38** (higher appears first).

**1. Info**

Granite blocks ordinary movement. Tunnelling may open a route if you have enough digging strength and a suitable tool. Repeated attempts spend time and make noise.

Trigger: Feature 63 is on the player square or visibly adjacent and marked; secret/unrevealed terrain is excluded.

Sources: `lib/edit/terrain.txt`, `src/cmd/world/cmd-interact.c`, `src/cmd/movement/cmd-movement.c`.

## Exhausted forge

`terrain.64`

Level: **Extended**.

Priority: **38** (higher appears first).

**1. Info**

This forge has no uses remaining and cannot make another item. Look for a usable forge if you want to continue smithing.

Trigger: Feature 64 is on the player square or visibly adjacent and marked; secret/unrevealed terrain is excluded.

Sources: `lib/edit/terrain.txt`, `src/cmd/world/cmd-interact.c`, `src/cmd/movement/cmd-movement.c`.

## Forge

`terrain.65`

Level: **Extended**.

Priority: **38** (higher appears first).

**1. Info**

This forge has limited uses. Inspect the remaining count and your smithing proposal before spending them. The proposal lists difficulty, materials, time and other costs.

Trigger: Known nearby terrain in this feature family, after grouping equivalent strengths or positive remaining-use counts; exhausted forges have separate lessons.

Sources: `lib/edit/terrain.txt`, `src/cmd/world/cmd-interact.c`, `src/cmd/movement/cmd-movement.c`.

## Forge

`terrain.66`

Level: **Extended**.

Priority: **38** (higher appears first).

Archive compatibility entry. New encounters use `terrain.65` for this terrain family.

**1. Info**

This forge has limited uses. Inspect the remaining count and your smithing proposal before spending them. The proposal lists difficulty, materials, time and other costs.

Trigger: Legacy lesson retained for saved progress and archive review. New encounters use terrain.65.

Sources: `lib/edit/terrain.txt`, `src/cmd/world/cmd-interact.c`, `src/cmd/movement/cmd-movement.c`.

## Forge

`terrain.67`

Level: **Extended**.

Priority: **38** (higher appears first).

Archive compatibility entry. New encounters use `terrain.65` for this terrain family.

**1. Info**

This forge has limited uses. Inspect the remaining count and your smithing proposal before spending them. The proposal lists difficulty, materials, time and other costs.

Trigger: Legacy lesson retained for saved progress and archive review. New encounters use terrain.65.

Sources: `lib/edit/terrain.txt`, `src/cmd/world/cmd-interact.c`, `src/cmd/movement/cmd-movement.c`.

## Forge

`terrain.68`

Level: **Extended**.

Priority: **38** (higher appears first).

Archive compatibility entry. New encounters use `terrain.65` for this terrain family.

**1. Info**

This forge has limited uses. Inspect the remaining count and your smithing proposal before spending them. The proposal lists difficulty, materials, time and other costs.

Trigger: Legacy lesson retained for saved progress and archive review. New encounters use terrain.65.

Sources: `lib/edit/terrain.txt`, `src/cmd/world/cmd-interact.c`, `src/cmd/movement/cmd-movement.c`.

## Forge

`terrain.69`

Level: **Extended**.

Priority: **38** (higher appears first).

Archive compatibility entry. New encounters use `terrain.65` for this terrain family.

**1. Info**

This forge has limited uses. Inspect the remaining count and your smithing proposal before spending them. The proposal lists difficulty, materials, time and other costs.

Trigger: Legacy lesson retained for saved progress and archive review. New encounters use terrain.65.

Sources: `lib/edit/terrain.txt`, `src/cmd/world/cmd-interact.c`, `src/cmd/movement/cmd-movement.c`.

## Exhausted enchanted forge

`terrain.70`

Level: **Extended**.

Priority: **38** (higher appears first).

**1. Info**

This enchanted forge has no uses remaining and cannot make another item. Look for a usable forge if you want to continue smithing.

Trigger: Feature 70 is on the player square or visibly adjacent and marked; secret/unrevealed terrain is excluded.

Sources: `lib/edit/terrain.txt`, `src/cmd/world/cmd-interact.c`, `src/cmd/movement/cmd-movement.c`.

## Enchanted forge

`terrain.71`

Level: **Extended**.

Priority: **38** (higher appears first).

**1. Info**

This enchanted forge has limited uses. Inspect the remaining count and your smithing proposal before spending them. It grants +3 effective Smithing while you work here, allowing more difficult items.

Trigger: Known nearby terrain in this feature family, after grouping equivalent strengths or positive remaining-use counts; exhausted forges have separate lessons.

Sources: `lib/edit/terrain.txt`, `src/cmd/world/cmd-interact.c`, `src/cmd/movement/cmd-movement.c`.

## Enchanted forge

`terrain.72`

Level: **Extended**.

Priority: **38** (higher appears first).

Archive compatibility entry. New encounters use `terrain.71` for this terrain family.

**1. Info**

This enchanted forge has limited uses. Inspect the remaining count and your smithing proposal before spending them. It grants +3 effective Smithing while you work here, allowing more difficult items.

Trigger: Legacy lesson retained for saved progress and archive review. New encounters use terrain.71.

Sources: `lib/edit/terrain.txt`, `src/cmd/world/cmd-interact.c`, `src/cmd/movement/cmd-movement.c`.

## Enchanted forge

`terrain.73`

Level: **Extended**.

Priority: **38** (higher appears first).

Archive compatibility entry. New encounters use `terrain.71` for this terrain family.

**1. Info**

This enchanted forge has limited uses. Inspect the remaining count and your smithing proposal before spending them. It grants +3 effective Smithing while you work here, allowing more difficult items.

Trigger: Legacy lesson retained for saved progress and archive review. New encounters use terrain.71.

Sources: `lib/edit/terrain.txt`, `src/cmd/world/cmd-interact.c`, `src/cmd/movement/cmd-movement.c`.

## Enchanted forge

`terrain.74`

Level: **Extended**.

Priority: **38** (higher appears first).

Archive compatibility entry. New encounters use `terrain.71` for this terrain family.

**1. Info**

This enchanted forge has limited uses. Inspect the remaining count and your smithing proposal before spending them. It grants +3 effective Smithing while you work here, allowing more difficult items.

Trigger: Legacy lesson retained for saved progress and archive review. New encounters use terrain.71.

Sources: `lib/edit/terrain.txt`, `src/cmd/world/cmd-interact.c`, `src/cmd/movement/cmd-movement.c`.

## Enchanted forge

`terrain.75`

Level: **Extended**.

Priority: **38** (higher appears first).

Archive compatibility entry. New encounters use `terrain.71` for this terrain family.

**1. Info**

This enchanted forge has limited uses. Inspect the remaining count and your smithing proposal before spending them. It grants +3 effective Smithing while you work here, allowing more difficult items.

Trigger: Legacy lesson retained for saved progress and archive review. New encounters use terrain.71.

Sources: `lib/edit/terrain.txt`, `src/cmd/world/cmd-interact.c`, `src/cmd/movement/cmd-movement.c`.

## Exhausted forge Orodruth

`terrain.76`

Level: **Extended**.

Priority: **38** (higher appears first).

**1. Info**

This forge Orodruth has no uses remaining and cannot make another item. Look for a usable forge if you want to continue smithing.

Trigger: Feature 76 is on the player square or visibly adjacent and marked; secret/unrevealed terrain is excluded.

Sources: `lib/edit/terrain.txt`, `src/cmd/world/cmd-interact.c`, `src/cmd/movement/cmd-movement.c`.

## Forge Orodruth

`terrain.77`

Level: **Extended**.

Priority: **38** (higher appears first).

**1. Info**

This forge Orodruth has limited uses. Inspect the remaining count and your smithing proposal before spending them. It grants +7 effective Smithing while you work here, allowing more difficult items.

Trigger: Known nearby terrain in this feature family, after grouping equivalent strengths or positive remaining-use counts; exhausted forges have separate lessons.

Sources: `lib/edit/terrain.txt`, `src/cmd/world/cmd-interact.c`, `src/cmd/movement/cmd-movement.c`.

## Forge Orodruth

`terrain.78`

Level: **Extended**.

Priority: **38** (higher appears first).

Archive compatibility entry. New encounters use `terrain.77` for this terrain family.

**1. Info**

This forge Orodruth has limited uses. Inspect the remaining count and your smithing proposal before spending them. It grants +7 effective Smithing while you work here, allowing more difficult items.

Trigger: Legacy lesson retained for saved progress and archive review. New encounters use terrain.77.

Sources: `lib/edit/terrain.txt`, `src/cmd/world/cmd-interact.c`, `src/cmd/movement/cmd-movement.c`.

## Forge Orodruth

`terrain.79`

Level: **Extended**.

Priority: **38** (higher appears first).

Archive compatibility entry. New encounters use `terrain.77` for this terrain family.

**1. Info**

This forge Orodruth has limited uses. Inspect the remaining count and your smithing proposal before spending them. It grants +7 effective Smithing while you work here, allowing more difficult items.

Trigger: Legacy lesson retained for saved progress and archive review. New encounters use terrain.77.

Sources: `lib/edit/terrain.txt`, `src/cmd/world/cmd-interact.c`, `src/cmd/movement/cmd-movement.c`.

## Up staircase

`terrain.80`

Level: **Extended**.

Priority: **38** (higher appears first).

**1. Info**

An up staircase normally goes one level shallower. If minimum depth prevents that, it can return you at the same depth or deeper instead. Oaths can forbid using it. Read the confirmation; leaving always generates another map.

Trigger: Feature 80 is on the player square or visibly adjacent and marked; secret/unrevealed terrain is excluded.

Sources: `lib/edit/terrain.txt`, `src/cmd/world/cmd-interact.c`, `src/cmd/movement/cmd-movement.c`.

## Down staircase

`terrain.81`

Level: **Extended**.

Priority: **38** (higher appears first).

**1. Info**

A down staircase goes one level deeper and generates a new map. Collect anything you want to keep and check unfinished local objectives before descending.

Trigger: Feature 81 is on the player square or visibly adjacent and marked; secret/unrevealed terrain is excluded.

Sources: `lib/edit/terrain.txt`, `src/cmd/world/cmd-interact.c`, `src/cmd/movement/cmd-movement.c`.

## Up shaft

`terrain.82`

Level: **Extended**.

Priority: **38** (higher appears first).

**1. Info**

An up shaft can ascend farther than an ordinary staircase, but minimum depth can leave you at the same depth or deeper instead. Oath restrictions still apply. Read the destination before leaving; the map is newly generated.

Trigger: Feature 82 is on the player square or visibly adjacent and marked; secret/unrevealed terrain is excluded.

Sources: `lib/edit/terrain.txt`, `src/cmd/world/cmd-interact.c`, `src/cmd/movement/cmd-movement.c`.

## Down shaft

`terrain.83`

Level: **Extended**.

Priority: **38** (higher appears first).

**1. Info**

A down shaft descends farther than an ordinary staircase. Read the destination before confirming, and prepare for a newly generated level at greater depth.

Trigger: Feature 83 is on the player square or visibly adjacent and marked; secret/unrevealed terrain is excluded.

Sources: `lib/edit/terrain.txt`, `src/cmd/world/cmd-interact.c`, `src/cmd/movement/cmd-movement.c`.

## Shallow water

`terrain.84`

Level: **Extended**.

Priority: **38** (higher appears first).

**1. Info**

Moving into, through or out of shallow water costs 50% more movement time and gives -3 Stealth from splashing. Standing still has no extra movement cost. Water breaks your scent trail, but enemies can still track you by sight or sound.

**2. Info**

Each water square entered on foot has a 0.5% disease risk. Standing still adds no infection roll. A successful leap avoids contact and the water splash, though landing still makes noise. Disease needs a potion of Healing or Miruvor; rest does not cure it.

Trigger: Feature 84 is on the player square or visibly adjacent and marked; secret/unrevealed terrain is excluded.

Sources: `lib/edit/terrain.txt`, `src/cave/cave-water.c`, `src/player/effects.c`, `src/melee/melee-movement-resolution.c`.

## Imprisonment — known effect

`effect.191`

Level: **Extended**.

Priority: **34** (higher appears first).

**1. Info**

Closes and locks doors in line of sight. Inspect the room first: changing doors can affect your own retreat as well as enemies.

Trigger: Item kind 191 is aware and publicly encountered; no forced use.

Sources: `lib/edit/object.txt`, `src/use-obj.c`.

## Freedom — known effect

`effect.192`

Level: **Extended**.

Priority: **34** (higher appears first).

**1. Info**

Reveals nearby doors and traps, attempts to open doors and destroy traps, and can close chasms. Check which features actually changed before moving through the area.

Trigger: Item kind 192 is aware and publicly encountered; no forced use.

Sources: `lib/edit/object.txt`, `src/use-obj.c`.

## Light — known effect

`effect.193`

Level: **Extended**.

Priority: **34** (higher appears first).

**1. Info**

Lights a radius of 7 squares and the current room, and stuns light-sensitive creatures in the affected area.

Trigger: Item kind 193 is aware and publicly encountered; no forced use.

Sources: `lib/edit/object.txt`, `src/use-obj.c`.

## Sanctity — known effect

`effect.195`

Level: **Extended**.

Priority: **34** (higher appears first).

**1. Info**

Cleanses a chosen item from equipment, Pack or floor, removing curses where the rules allow. Curse Breaking also permits breaking qualifying jinxed affixes. Select a real eligible target before committing.

Trigger: Item kind 195 is aware and publicly encountered; no forced use.

Sources: `lib/edit/object.txt`, `src/use-obj.c`.

## Understanding — known effect

`effect.196`

Level: **Extended**.

Priority: **34** (higher appears first).

**1. Info**

Identifies a chosen item. Cancelling the selection does not consume the use. The result applies to that selected item.

Trigger: Item kind 196 is aware and publicly encountered; no forced use.

Sources: `lib/edit/object.txt`, `src/use-obj.c`.

## Revelations — known effect

`effect.197`

Level: **Extended**.

Priority: **34** (higher appears first).

**1. Info**

Reveals terrain within a radius of 10 plus your Will. Alchemy increases a Gem of Revelation's range by 50%. Mapped terrain does not reveal every creature occupying it.

Trigger: Item kind 197 is aware and publicly encountered; no forced use.

Sources: `lib/edit/object.txt`, `src/use-obj.c`.

## Treasures — known effect

`effect.198`

Level: **Extended**.

Priority: **34** (higher appears first).

**1. Info**

Detects items within a radius of 10 plus your Will. Alchemy increases a Gem of Treasures' range by 50%. Check their locations on the map; the items remain where they were.

Trigger: Item kind 198 is aware and publicly encountered; no forced use.

Sources: `lib/edit/object.txt`, `src/use-obj.c`.

## Foes — known effect

`effect.199`

Level: **Extended**.

Priority: **34** (higher appears first).

**1. Info**

Detects creatures within a radius of 10 plus your Will. Alchemy increases a Gem of Foes' range by 50%. Detection reveals presence, not every attack or trait of each creature.

Trigger: Item kind 199 is aware and publicly encountered; no forced use.

Sources: `lib/edit/object.txt`, `src/use-obj.c`.

## Slumber — known effect

`effect.200`

Level: **Extended**.

Priority: **34** (higher appears first).

**1. Info**

Attempts to make monsters in line of sight sleepy. The effect is resisted; it does not promise that every foe will stop acting.

Trigger: Item kind 200 is aware and publicly encountered; no forced use.

Sources: `lib/edit/object.txt`, `src/use-obj.c`.

## Majesty — known effect

`effect.201`

Level: **Extended**.

Priority: **34** (higher appears first).

**1. Info**

Attempts to frighten monsters in line of sight. Fear changes morale; whether it takes effect depends on the target and the effect check.

Trigger: Item kind 201 is aware and publicly encountered; no forced use.

Sources: `lib/edit/object.txt`, `src/use-obj.c`.

## Self Knowledge — known effect

`effect.202`

Level: **Extended**.

Priority: **34** (higher appears first).

**1. Info**

Shows your current status effects and character traits. Each use can also reveal one active curse. It does not remove that curse.

Trigger: Item kind 202 is aware and publicly encountered; no forced use.

Sources: `lib/edit/object.txt`, `src/use-obj.c`.

## Warding — known effect

`effect.203`

Level: **Extended**.

Priority: **34** (higher appears first).

**1. Info**

Creates a glyph of warding where the placement rules permit. The glyph makes crossing difficult for opponents; it is not an impenetrable wall.

Trigger: Item kind 203 is aware and publicly encountered; no forced use.

Sources: `lib/edit/object.txt`, `src/use-obj.c`.

## Dismay — known effect

`effect.204`

Level: **Extended**.

Priority: **34** (higher appears first).

**1. Info**

Attempts to confuse monsters in line of sight. Check what actually happened before relying on a target being confused.

Trigger: Item kind 204 is aware and publicly encountered; no forced use.

Sources: `lib/edit/object.txt`, `src/use-obj.c`.

## Recharging — known effect

`effect.206`

Level: **Extended**.

Priority: **34** (higher appears first).

**1. Info**

Restores charges to a chosen staff. Channeling doubles the amount restored by a Gem of Recharging. Check the staff and its charges before confirming; cancelling the selection preserves the use.

Trigger: Item kind 206 is aware and publicly encountered; no forced use.

Sources: `lib/edit/object.txt`, `src/use-obj.c`.

## Summoning — known effect

`effect.210`

Level: **Extended**.

Priority: **34** (higher appears first).

**1. Info**

Summons 1 to 4 creatures at the level's stairs where placement is possible. This can add dangerous enemies to your escape route. Use only when you want the summons.

Trigger: Item kind 210 is aware and publicly encountered; no forced use.

Sources: `lib/edit/object.txt`, `src/use-obj.c`.

## Shadows — known effect

`effect.211`

Level: **Extended**.

Priority: **34** (higher appears first).

**1. Info**

Darkens a radius of 7 squares and the current room. For that round's stealth checks it adds your Will to Stealth. Darkness also changes what you can see.

Trigger: Item kind 211 is aware and publicly encountered; no forced use.

Sources: `lib/edit/object.txt`, `src/use-obj.c`.

## Terror — known effect

`effect.240`

Level: **Extended**.

Priority: **34** (higher appears first).

**1. Info**

Attempts to frighten creatures affected by the horn. Choose the direction and check your Voice cost. Its sound may alert other creatures.

Trigger: Item kind 240 is aware and publicly encountered; no forced use.

Sources: `lib/edit/object.txt`, `src/use-obj.c`.

## Thunder — known effect

`effect.241`

Level: **Extended**.

Priority: **34** (higher appears first).

**1. Info**

Attempts to stun affected creatures. A stun attempt is not guaranteed to stop every enemy; inspect the actual result before advancing.

Trigger: Item kind 241 is aware and publicly encountered; no forced use.

Sources: `lib/edit/object.txt`, `src/use-obj.c`.

## Force — known effect

`effect.242`

Level: **Extended**.

Priority: **34** (higher appears first).

**1. Info**

Attempts to push enemies back and stun them. A blocked push can still stagger and stun a target. Aim carefully: the blast spends Voice, makes a loud noise and can push enemies into terrain hazards.

Trigger: Item kind 242 is aware and publicly encountered; no forced use.

Sources: `lib/edit/object.txt`, `src/use-obj.c`.

## Blasting — known effect

`effect.243`

Level: **Extended**.

Priority: **34** (higher appears first).

**1. Info**

Can shatter stone. It can be aimed along a compass direction, up or down through the normal targeting choices. Inspect the area and cost before committing a destructive effect.

Trigger: Item kind 243 is aware and publicly encountered; no forced use.

Sources: `lib/edit/object.txt`, `src/use-obj.c`.

## Warning — known effect

`effect.250`

Level: **Extended**.

Priority: **34** (higher appears first).

**1. Info**

Raises a loud warning and challenges nearby foes. It draws attention; do not use it merely because another horn would have been helpful.

Trigger: Item kind 250 is aware and publicly encountered; no forced use.

Sources: `lib/edit/object.txt`, `src/use-obj.c`.

## Miruvor — known effect

`effect.313`

Level: **Extended**.

Priority: **34** (higher appears first).

**1. Info**

Cures stun, confusion, hallucination, poison, blindness and fear; halves current bleeding; restores 20 + floor(20% of maximum Health) and all Voice. Medicine equipment can increase the healing. Cures disease and restores all attribute points lost to it, leaving unrelated drain unchanged.

Trigger: Item kind 313 is aware and publicly encountered; no forced use.

Sources: `lib/edit/object.txt`, `src/use-obj.c`.

## Orcish Liquor — known effect

`effect.315`

Level: **Extended**.

Priority: **34** (higher appears first).

**1. Info**

Removes fear and heals 6 + floor(8% of maximum Health), but can add 2d4 stun severity, subject to stun protection. Medicine equipment can increase healing. The drawback matters even when healing is useful.

Trigger: Item kind 315 is aware and publicly encountered; no forced use.

Sources: `lib/edit/object.txt`, `src/use-obj.c`.

## Esgalduin — known effect

`effect.316`

Level: **Extended**.

Priority: **34** (higher appears first).

**1. Info**

Grants +10 Perception for 20d4 turns and restores one quarter of maximum Voice. The bonus is temporary.

Trigger: Item kind 316 is aware and publicly encountered; no forced use.

Sources: `lib/edit/object.txt`, `src/use-obj.c`.

## Clarity — known effect

`effect.317`

Level: **Extended**.

Priority: **34** (higher appears first).

**1. Info**

Cures stun, confusion and hallucination, and ends rage. It is a condition remedy, not a general Health potion.

Trigger: Item kind 317 is aware and publicly encountered; no forced use.

Sources: `lib/edit/object.txt`, `src/use-obj.c`.

## Healing — known effect

`effect.318`

Level: **Extended**.

Priority: **34** (higher appears first).

**1. Info**

Halves current bleeding and restores 15 + floor(16% of maximum Health). Medicine equipment can increase healing. Remaining bleeding can still cause later damage. Cures disease and restores all attribute points lost to it, leaving unrelated drain unchanged.

Trigger: Item kind 318 is aware and publicly encountered; no forced use.

Sources: `lib/edit/object.txt`, `src/use-obj.c`.

## Voice — known effect

`effect.319`

Level: **Extended**.

Priority: **34** (higher appears first).

**1. Info**

Restores all Voice. It does not restore Health or cure conditions.

Trigger: Item kind 319 is aware and publicly encountered; no forced use.

Sources: `lib/edit/object.txt`, `src/use-obj.c`.

## true Sight — known effect

`effect.320`

Level: **Extended**.

Priority: **34** (higher appears first).

**1. Info**

Cures blindness and hallucination and grants resistance to both plus improved invisible-creature detection for 10d4 turns.

Trigger: Item kind 320 is aware and publicly encountered; no forced use.

Sources: `lib/edit/object.txt`, `src/use-obj.c`.

## Antidote — known effect

`effect.321`

Level: **Extended**.

Priority: **34** (higher appears first).

**1. Info**

Removes current poison. It does not restore the Health poison has already taken.

Trigger: Item kind 321 is aware and publicly encountered; no forced use.

Sources: `lib/edit/object.txt`, `src/use-obj.c`.

## Quickness — known effect

`effect.322`

Level: **Extended**.

Priority: **34** (higher appears first).

**1. Info**

Grants +1 speed for 10d4 turns. This can offset slowing without removing the slow condition.

Trigger: Item kind 322 is aware and publicly encountered; no forced use.

Sources: `lib/edit/object.txt`, `src/use-obj.c`.

## Elemental Resistance — known effect

`effect.323`

Level: **Extended**.

Priority: **34** (higher appears first).

**1. Info**

Grants temporary fire and cold resistance for 20d4 turns. It does not grant poison resistance.

Trigger: Item kind 323 is aware and publicly encountered; no forced use.

Sources: `lib/edit/object.txt`, `src/use-obj.c`.

## Strength — known effect

`effect.327`

Level: **Extended**.

Priority: **34** (higher appears first).

**1. Info**

If Strength is drained by at least 3 points, this use restores 3 points instead of granting the temporary buff. Otherwise it grants +3 temporary Strength for 20d4 turns; when that buff expires it restores up to 3 drained points. Check the current drain before choosing it.

Trigger: Item kind 327 is aware and publicly encountered; no forced use.

Sources: `lib/edit/object.txt`, `src/use-obj.c`, `src/player/effects.c`.

## Dexterity — known effect

`effect.328`

Level: **Extended**.

Priority: **34** (higher appears first).

**1. Info**

If Dexterity is drained by at least 3 points, this use restores 3 points instead of granting the temporary buff. Otherwise it grants +3 temporary Dexterity for 20d4 turns; when that buff expires it restores up to 3 drained points. Check the current drain before choosing it.

Trigger: Item kind 328 is aware and publicly encountered; no forced use.

Sources: `lib/edit/object.txt`, `src/use-obj.c`, `src/player/effects.c`.

## Constitution — known effect

`effect.329`

Level: **Extended**.

Priority: **34** (higher appears first).

**1. Info**

If Constitution is drained by at least 3 points, this use restores 3 points instead of granting the temporary buff. Otherwise it grants +3 temporary Constitution for 20d4 turns; when that buff expires it restores up to 3 drained points. Check the current drain before choosing it.

Trigger: Item kind 329 is aware and publicly encountered; no forced use.

Sources: `lib/edit/object.txt`, `src/use-obj.c`, `src/player/effects.c`.

## Grace — known effect

`effect.330`

Level: **Extended**.

Priority: **34** (higher appears first).

**1. Info**

If Grace is drained by at least 3 points, this use restores 3 points instead of granting the temporary buff. Otherwise it grants +3 temporary Grace for 20d4 turns; when that buff expires it restores up to 3 drained points. Check the current drain before choosing it.

Trigger: Item kind 330 is aware and publicly encountered; no forced use.

Sources: `lib/edit/object.txt`, `src/use-obj.c`, `src/player/effects.c`.

## Slowness — known effect

`effect.343`

Level: **Extended**.

Priority: **34** (higher appears first).

**1. Info**

Applies -1 speed for 10d4 turns. Avoid drinking it when you need to escape.

Trigger: Item kind 343 is aware and publicly encountered; no forced use.

Sources: `lib/edit/object.txt`, `src/use-obj.c`.

## Poison — known effect

`effect.344`

Level: **Extended**.

Priority: **34** (higher appears first).

**1. Info**

Inflicts 5d4 poison severity. Poison damage then happens through the condition's damage ticks.

Trigger: Item kind 344 is aware and publicly encountered; no forced use.

Sources: `lib/edit/object.txt`, `src/use-obj.c`.

## Blindness — known effect

`effect.345`

Level: **Extended**.

Priority: **34** (higher appears first).

**1. Info**

Can blind you for 10d4 turns, subject to the protection checks. More light does not cure blindness.

Trigger: Item kind 345 is aware and publicly encountered; no forced use.

Sources: `lib/edit/object.txt`, `src/use-obj.c`.

## Confusion — known effect

`effect.346`

Level: **Extended**.

Priority: **34** (higher appears first).

**1. Info**

Can confuse you for 5d4 turns, subject to the protection checks. Confusion interferes with directions and aiming.

Trigger: Item kind 346 is aware and publicly encountered; no forced use.

Sources: `lib/edit/object.txt`, `src/use-obj.c`.

## Awkwardness — known effect

`effect.348`

Level: **Extended**.

Priority: **34** (higher appears first).

**1. Info**

Drains Dexterity by 1. This is attribute drain rather than a short-lived bonus wearing off.

Trigger: Item kind 348 is aware and publicly encountered; no forced use.

Sources: `lib/edit/object.txt`, `src/use-obj.c`.

## Disconnection — known effect

`effect.350`

Level: **Extended**.

Priority: **34** (higher appears first).

**1. Info**

Drains Grace by 1. Restoration or a matching restoration effect addresses the drain.

Trigger: Item kind 350 is aware and publicly encountered; no forced use.

Sources: `lib/edit/object.txt`, `src/use-obj.c`.

## Rage — known effect

`effect.380`

Level: **Extended**.

Priority: **34** (higher appears first).

**1. Info**

Grants rage for 10d4 turns: +1 Strength/Constitution, -1 Dexterity/Grace, a special melee attack and fear resistance. Rage restricts awareness and Stealth. It also supplies ordinary herb nourishment.

Trigger: Item kind 380 is aware and publicly encountered; no forced use.

Sources: `lib/edit/object.txt`, `src/use-obj.c`.

## Sustenance — known effect

`effect.381`

Level: **Extended**.

Priority: **34** (higher appears first).

**1. Info**

Provides about 2,000 ordinary turns of nourishment. Actual consumption rate can change with your current modifiers.

Trigger: Item kind 381 is aware and publicly encountered; no forced use.

Sources: `lib/edit/object.txt`, `src/use-obj.c`.

## Terror — known effect

`effect.382`

Level: **Extended**.

Priority: **34** (higher appears first).

**1. Info**

If fear protection does not prevent the effect, causes fear for 10d4 turns and speed for 5d4 turns. Preventing the fear also prevents this speed benefit. It provides ordinary herb nourishment.

Trigger: Item kind 382 is aware and publicly encountered; no forced use.

Sources: `lib/edit/object.txt`, `src/use-obj.c`.

## Healing — known effect

`effect.383`

Level: **Extended**.

Priority: **34** (higher appears first).

**1. Info**

Halves bleeding and heals 11 + floor(12% of maximum Health), increased by Medicine equipment. It also provides ordinary herb nourishment. It does not cure disease.

Trigger: Item kind 383 is aware and publicly encountered; no forced use.

Sources: `lib/edit/object.txt`, `src/use-obj.c`.

## Restoration — known effect

`effect.384`

Level: **Extended**.

Priority: **34** (higher appears first).

**1. Info**

Restores every attribute by up to 3 drained points. It also provides ordinary herb nourishment. It does not remove unrelated equipment penalties. It does not cure disease or restore disease penalties.

Trigger: Item kind 384 is aware and publicly encountered; no forced use.

Sources: `lib/edit/object.txt`, `src/use-obj.c`.

## Emptiness — known effect

`effect.385`

Level: **Extended**.

Priority: **34** (higher appears first).

**1. Info**

Reduces nourishment by about 1,000 ordinary turns. This can worsen hunger; it is not a remedy.

Trigger: Item kind 385 is aware and publicly encountered; no forced use.

Sources: `lib/edit/object.txt`, `src/use-obj.c`.

## Visions — known effect

`effect.386`

Level: **Extended**.

Priority: **34** (higher appears first).

**1. Info**

Causes hallucination for 80d4 turns but removes blindness. This tradeoff is different from a clean sight remedy.

Trigger: Item kind 386 is aware and publicly encountered; no forced use.

Sources: `lib/edit/object.txt`, `src/use-obj.c`.

## Entrancement — known effect

`effect.387`

Level: **Extended**.

Priority: **34** (higher appears first).

**1. Info**

Can entrance you for 10d4 turns. While entranced you cannot choose ordinary actions. Save it unless you intend to accept that risk.

Trigger: Item kind 387 is aware and publicly encountered; no forced use.

Sources: `lib/edit/object.txt`, `src/use-obj.c`.

## Weakness — known effect

`effect.388`

Level: **Extended**.

Priority: **34** (higher appears first).

**1. Info**

Drains Strength by 1 and provides ordinary herb nourishment. Nourishment does not cancel its attribute drawback.

Trigger: Item kind 388 is aware and publicly encountered; no forced use.

Sources: `lib/edit/object.txt`, `src/use-obj.c`.

## Sickness — known effect

`effect.389`

Level: **Extended**.

Priority: **34** (higher appears first).

**1. Info**

Always causes disease: -1 Constitution on infection, then -1 to a random attribute every 50 player turns until cured. Rest does not cure it. A potion of Healing or Miruvor cures disease and restores its attribute penalties. Provides ordinary herb nourishment.

Trigger: Item kind 389 is aware and publicly encountered; no forced use.

Sources: `lib/edit/object.txt`, `src/use-obj.c`.

## Piece of Dark Bread — known effect

`effect.399`

Level: **Extended**.

Priority: **34** (higher appears first).

**1. Info**

Provides about 1,500 ordinary turns of nourishment.

Trigger: Item kind 399 is aware and publicly encountered; no forced use.

Sources: `lib/edit/object.txt`, `src/use-obj.c`.

## Strip of Dried Meat — known effect

`effect.400`

Level: **Extended**.

Priority: **34** (higher appears first).

**1. Info**

Provides about 2,000 ordinary turns of nourishment, with a 20% chance of disease: -1 Constitution on infection, then -1 to a random attribute every 50 player turns. Rest does not cure it; potions of Healing or Miruvor do.

Trigger: Item kind 400 is aware and publicly encountered; no forced use.

Sources: `lib/edit/object.txt`, `src/use-obj.c`.

## Fragment of Lembas — known effect

`effect.401`

Level: **Extended**.

Priority: **34** (higher appears first).

**1. Info**

Provides about 3,000 ordinary turns of nourishment and restores 1 drained Grace.

Trigger: Item kind 401 is aware and publicly encountered; no forced use.

Sources: `lib/edit/object.txt`, `src/use-obj.c`.

## Throwing weapon: ready

`item.throwing.ready`

Level: **Normal**.

Priority: **32** (higher appears first).

**1. Action**

Open Inventory, select the throwing weapon, and choose Ready to move it to your Harness. Reaching into the Pack takes three turns and attacks can interrupt it. Skip if you want to keep your current arrangement.

Required action: `ready`; subject: `throwing`.

Trigger: The relevant item is available and the ready action has a legal current target and all required handling resources.

Sources: `src/tutorial/tutorial-game.c`, `src/player/player-active-weapon.c`, `src/cmd/combat/cmd-ranged.c`.

## Throwing weapon: active

`item.throwing.active`

Level: **Normal**.

Priority: **32** (higher appears first).

**1. Action**

Open Change Active and choose the readied throwing weapon. Check which weapon and shield will be active. Confirm the change to finish this practice, or Skip to keep your current setup.

Required action: `change-active`; subject: `throwing`.

Trigger: The relevant item is available and the active action has a legal current target and all required handling resources.

Sources: `src/tutorial/tutorial-game.c`, `src/player/player-active-weapon.c`, `src/cmd/combat/cmd-ranged.c`.

## Throwing weapon: use

`item.throwing.use`

Level: **Normal**.

Priority: **32** (higher appears first).

**1. Action**

Aim at a currently legal visible hostile and throw the weapon. Check the path, range and oath first. A committed miss still counts; cancellation does not. Skip to save ammunition or choose a different tactic.

Required action: `throw`; subject: `throwing`.

Trigger: The relevant item is available and the use action has a legal current target and all required handling resources.

Sources: `src/tutorial/tutorial-game.c`, `src/player/player-active-weapon.c`, `src/cmd/combat/cmd-ranged.c`.

## Bow: ready

`item.bow.ready`

Level: **Normal**.

Priority: **32** (higher appears first).

**1. Action**

Open Inventory, select the bow, and choose Ready to move it to your Harness. Reaching into the Pack takes three turns and attacks can interrupt it. Skip if you want to keep your current arrangement.

Required action: `ready`; subject: `bow`.

Trigger: The relevant item is available and the ready action has a legal current target and all required handling resources.

Sources: `src/tutorial/tutorial-game.c`, `src/player/player-active-weapon.c`, `src/cmd/combat/cmd-ranged.c`.

## Bow: active

`item.bow.active`

Level: **Normal**.

Priority: **32** (higher appears first).

**1. Action**

Open Change Active and choose the readied bow. Check which weapon and shield will be active. Confirm the change to finish this practice, or Skip to keep your current setup.

Required action: `change-active`; subject: `bow`.

Trigger: The relevant item is available and the active action has a legal current target and all required handling resources.

Sources: `src/tutorial/tutorial-game.c`, `src/player/player-active-weapon.c`, `src/cmd/combat/cmd-ranged.c`.

## Bow: use

`item.bow.use`

Level: **Normal**.

Priority: **32** (higher appears first).

**1. Action**

Aim at a currently legal visible hostile and fire one arrow. Check the path, range and oath first. A committed miss still counts; cancellation does not. Skip to save ammunition or choose a different tactic.

Required action: `fire`; subject: `bow`.

Trigger: The relevant item is available and the use action has a legal current target and all required handling resources.

Sources: `src/tutorial/tutorial-game.c`, `src/player/player-active-weapon.c`, `src/cmd/combat/cmd-ranged.c`.

## Select active arrows

`item.arrows.active`

Level: **Normal**.

Priority: **32** (higher appears first).

**1. Action**

Select a different available arrow stack for your active bow. Changing only arrows is free; changing other active equipment follows its own cost rules.

Required action: `change-active`; subject: `arrows`.

Trigger: A compatible alternate arrow stack is available and a bow is active.

Sources: `src/player/player-active-weapon.c`.

## Equip armour

`item.armour.equip`

Level: **Normal**.

Priority: **30** (higher appears first).

**1. Action**

Choose Equip on known, uncursed armour that fits an empty slot. Check Protection, Evasion, penalties and your oath before confirming. Skip if you prefer your current arrangement.

Required action: `equip`; subject: `armour`.

Trigger: Known uncursed armour fits an empty equipment slot and its oath permits equipping. The chosen item is checked again when the real action is committed.

Sources: `src/cmd/item/cmd-item-core.c`, `src/tutorial/tutorial-game.c`.

## Pack storage

`storage.pack`

Level: **Normal**.

Priority: **40** (higher appears first).

**1. Info**

The Pack has its own volume limit. Weight also contributes to your overall load. Read both limits when selecting what to carry; a small but heavy object and a bulky light object create different constraints.

Trigger: The corresponding public state transition or explicit player action has occurred; storage.pack.

Sources: `src/tutorial/tutorial-world.c`, `src/tutorial/tutorial-game.c`.

## Harness capacity

`storage.harness`

Level: **Normal**.

Priority: **40** (higher appears first).

**1. Info**

The Harness has a separate volume limit from the Pack. Readied gear can be reached under its own action rules. Moving an item between storage locations does not create unlimited total carrying capacity.

Trigger: The corresponding public state transition or explicit player action has occurred; storage.harness.

Sources: `src/tutorial/tutorial-world.c`, `src/tutorial/tutorial-game.c`.

## Quiver capacity

`storage.quiver`

Level: **Normal**.

Priority: **40** (higher appears first).

**1. Info**

The Quiver holds arrows under its own capacity rules. Excess arrows may need Pack space. A stack can be picked up partially when only part fits; inspect the quantity actually collected.

Trigger: The corresponding public state transition or explicit player action has occurred; storage.quiver.

Sources: `src/tutorial/tutorial-world.c`, `src/tutorial/tutorial-game.c`.

## Carried weight

`storage.weight`

Level: **Normal**.

Priority: **40** (higher appears first).

**1. Info**

Carried weight is separate from storage volume. Heavy loads can slow you, even if there is space for more items. Check the displayed burden and consider leaving spare heavy gear behind.

Trigger: The corresponding public state transition or explicit player action has occurred; storage.weight.

Sources: `src/tutorial/tutorial-world.c`, `src/tutorial/tutorial-game.c`.

## Reaching into the Pack

`storage.pack_access`

Level: **Normal**.

Priority: **40** (higher appears first).

**1. Info**

This Pack action takes three turns to finish. Enemies can act during those turns, and an attack or your cancellation can interrupt it. Ready urgent gear before combat when possible.

Trigger: The corresponding public state transition or explicit player action has occurred; storage.pack_access.

Sources: `src/tutorial/tutorial-game.c`.

## Pack access interrupted

`storage.pack_interrupted`

Level: **Normal**.

Priority: **40** (higher appears first).

**1. Info**

An attack interrupted the Pack action before it finished. Check the item's current location and the message history. You may need to move away from danger before trying again.

Trigger: The corresponding public state transition or explicit player action has occurred; storage.pack_interrupted.

Sources: `src/tutorial/tutorial-game.c`.

## Only part of the stack fits

`storage.partial_pickup`

Level: **Normal**.

Priority: **40** (higher appears first).

**1. Info**

Only part of the stack fitted in the available storage. Check the quantity collected and the items still on the floor. Free space or choose another storage location if you want to take more.

Trigger: The corresponding public state transition or explicit player action has occurred; storage.partial_pickup.

Sources: `src/tutorial/tutorial-game.c`.

## Resting spends turns

`world.rest`

Level: **Normal**.

Priority: **40** (higher appears first).

**1. Info**

Rest repeats turns until its goal is met or something interrupts it. Enemies and the minimum-depth timer continue. Poison, bleeding and starvation stop ordinary Health recovery; singing stops Voice recovery. Treat disease before a long rest.

Trigger: The corresponding public state transition or explicit player action has occurred; world.rest.

Sources: `src/tutorial/tutorial-game.c`.

## Running and repeated movement

`world.run`

Level: **Normal**.

Priority: **40** (higher appears first).

**1. Info**

Running repeats movement until you stop it or something interrupts it. Enemies and the minimum-depth timer continue between steps. Inspect your route before starting a run.

Trigger: The corresponding public state transition or explicit player action has occurred; world.run.

Sources: `src/tutorial/tutorial-game.c`.

## A known trap

`world.trap`

Level: **Normal**.

Priority: **40** (higher appears first).

**1. Info**

You have found a trap. Seeing it does not disable it. Choose a route around it, inspect Disarm and its risk, or use a legal leap if your hero has Leaping. If it already triggered, check your Health and conditions before acting.

Trigger: The corresponding public state transition or explicit player action has occurred; world.trap.

Sources: `src/tutorial/tutorial-game.c`.

## At a forge

`world.forge`

Level: **Normal**.

Priority: **40** (higher appears first).

**1. Info**

A forge has limited uses. Open Smithing while at it to inspect possible items and their costs. The preview shows difficulty, materials, time and forge uses before you commit. An exhausted forge cannot make another item.

Trigger: The corresponding public state transition or explicit player action has occurred; world.forge.

Sources: `src/tutorial/tutorial-game.c`.

## Fuel is running low

`world.light_low`

Level: **Normal**.

Priority: **70** (higher appears first).

**1. Info**

Your equipped light is running low on fuel. Find a compatible refill or ready a replacement before it burns out. Refuelling or changing equipment spends time, so prepare before meeting enemies.

Trigger: The corresponding public state transition or explicit player action has occurred; world.light_low.

Sources: `src/tutorial/tutorial-world.c`, `src/tutorial/tutorial-game.c`.

## Your light has gone out

`world.light_out`

Level: **Normal**.

Priority: **70** (higher appears first).

**1. Info**

Your equipped light has run out of fuel. Refuel it or equip a replacement if possible. The remembered map remains visible, but it does not show every threat in the dark.

Trigger: The corresponding public state transition or explicit player action has occurred; world.light_out.

Sources: `src/tutorial/tutorial-world.c`, `src/tutorial/tutorial-game.c`.

## Train a skill

`advancement.skills`

Level: **Normal**.

Priority: **40** (higher appears first).

**1. Decision**

The next base skill rank costs 100 XP times the new rank: rank 1 costs 100 XP, rank 2 costs another 200 XP, and so on. Attribute and equipment bonuses are separate. Review the proposed purchase before spending XP.

Trigger: The corresponding public state transition or explicit player action has occurred; advancement.skills.

Sources: `src/tutorial/tutorial-game.c`.

## A song is available

`advancement.song`

Level: **Normal**.

Priority: **40** (higher appears first).

**1. Decision**

You have a song to consider. Read its effect and Voice cost in Songs, then choose when to start singing. Singing spends time and prevents Voice recovery until you stop. Check any oath that forbids it.

Trigger: The corresponding public state transition or explicit player action has occurred; advancement.song.

Sources: `src/tutorial/tutorial-game.c`.

## Quest progress

`quest.progress`

Level: **Normal**.

Priority: **40** (higher appears first).

**1. Info**

{detail} Check Quests for the remaining objective and any restrictions. Reaching a target may still leave a return visit to collect the reward.

Trigger: The corresponding public state transition or explicit player action has occurred; quest.progress.

Sources: `src/tutorial/tutorial-world.c`, `src/tutorial/tutorial-game.c`.

## Quest completed

`quest.completed`

Level: **Normal**.

Priority: **40** (higher appears first).

**1. Info**

{detail} Check Quests for the result and reward. If the objective is complete but the reward is unclaimed, follow its return instruction. A newly unlocked oath still needs its own choice.

Trigger: The corresponding public state transition or explicit player action has occurred; quest.completed.

Sources: `src/tutorial/tutorial-world.c`, `src/tutorial/tutorial-game.c`.

## Quest failed

`quest.failed`

Level: **Normal**.

Priority: **70** (higher appears first).

**1. Info**

{detail} Check Quests for the reason and consequences. The action that caused failure has already happened. Another hero in this Tale may have different opportunities.

Trigger: The corresponding public state transition or explicit player action has occurred; quest.failed.

Sources: `src/tutorial/tutorial-world.c`, `src/tutorial/tutorial-game.c`.

## A curse is revealed

`tale.curse`

Level: **Normal**.

Priority: **70** (higher appears first).

**1. Info**

{detail} A Tale curse has been revealed. Read its effect and current stack in Known Curses. It can affect more than this hero, unlike an ordinary cursed piece of equipment.

Trigger: The corresponding public state transition or explicit player action has occurred; tale.curse.

Sources: `src/tutorial/tutorial-world.c`, `src/tutorial/tutorial-game.c`.

## A blessing is available

`tale.blessing`

Level: **Normal**.

Priority: **70** (higher appears first).

**1. Decision**

{detail} Read the blessing's benefit, cost and limits before choosing it. You can compare options without spending blessing points.

Trigger: The corresponding public state transition or explicit player action has occurred; tale.blessing.

Sources: `src/tutorial/tutorial-world.c`, `src/tutorial/tutorial-game.c`.

## An oath was broken

`tale.oath_break`

Level: **Normal**.

Priority: **70** (higher appears first).

**1. Info**

{detail} Your action broke the oath. Check the lost benefit and any revealed curse, then plan around the new state. Another hero in the Tale may also face the consequences.

Trigger: The corresponding public state transition or explicit player action has occurred; tale.oath_break.

Sources: `src/tutorial/tutorial-world.c`, `src/tutorial/tutorial-game.c`.

## A Silmaril

`tale.silmaril`

Level: **Normal**.

Priority: **70** (higher appears first).

**1. Info**

{detail} You have obtained a Silmaril. Check the main quest and prepare your route out of Angband. You must still reach the surface with it to complete the escape.

Trigger: The corresponding public state transition or explicit player action has occurred; tale.silmaril.

Sources: `src/tutorial/tutorial-world.c`, `src/tutorial/tutorial-game.c`.

## Leaving Angband

`tale.escape`

Level: **Normal**.

Priority: **70** (higher appears first).

**1. Info**

{detail} You escaped. Review the recovered Silmarils, score and remaining Tale objective. An escape completes this hero's expedition, while the Tale may continue with another hero.

Trigger: The corresponding public state transition or explicit player action has occurred; tale.escape.

Sources: `src/game/game-lifecycle.c`, `src/tutorial/tutorial-game.c`, `src/tutorial/tutorial-world.c`.

## A hero has fallen

`tale.death`

Level: **Normal**.

Priority: **70** (higher appears first).

**1. Info**

{detail} This hero has died. The Tale can continue with another hero unless its loss condition has been reached. Check what progress and consequences carry forward. Encountered tutorials remain in this Tale's archive.

Trigger: The corresponding public state transition or explicit player action has occurred; tale.death.

Sources: `src/game/game-lifecycle.c`, `src/tutorial/tutorial-game.c`, `src/tutorial/tutorial-world.c`.

## A truce

`tale.truce`

Level: **Normal**.

Priority: **70** (higher appears first).

**1. Info**

{detail} A truce is in effect. Read its terms before attacking or disturbing the agreement. Some actions can end the truce and expose you to immediate danger.

Trigger: The corresponding public state transition or explicit player action has occurred; tale.truce.

Sources: `src/tutorial/tutorial-world.c`, `src/tutorial/tutorial-game.c`.

## Roomy halls

`world.partition.1`

Level: **Extended**.

Priority: **38** (higher appears first).

**1. Info**

{detail} Rooms and connecting passages offer doorways where you can limit approaching enemies. Find a retreat route before crossing an open room.

Trigger: The player enters and publicly discovers partition kind 1.

Sources: `src/tutorial/tutorial-world.c`, `src/externs.h`, `src/level-generation/level-generation-internal.h`, `src/tutorial/tutorial-game.c`.

## Caves

`world.partition.2`

Level: **Extended**.

Priority: **38** (higher appears first).

**1. Info**

{detail} Natural caves have irregular routes and sight lines. Check visible exits and nearby cover before approaching enemies.

Trigger: The player enters and publicly discovers partition kind 2.

Sources: `src/tutorial/tutorial-world.c`, `src/externs.h`, `src/level-generation/level-generation-internal.h`, `src/tutorial/tutorial-game.c`.

## Ruined halls

`world.partition.3`

Level: **Extended**.

Priority: **38** (higher appears first).

**1. Info**

{detail} Ruined halls contain broken structures and irregular passages. Check which doorways still close and where the open routes lead.

Trigger: The player enters and publicly discovers partition kind 3.

Sources: `src/tutorial/tutorial-world.c`, `src/externs.h`, `src/level-generation/level-generation-internal.h`, `src/tutorial/tutorial-game.c`.

## Labyrinth

`world.partition.4`

Level: **Extended**.

Priority: **38** (higher appears first).

**1. Info**

{detail} Labyrinths have narrow, winding passages. Keep track of intersections and a route back to known ground. The map helps you review explored passages.

Trigger: The player enters and publicly discovers partition kind 4.

Sources: `src/tutorial/tutorial-world.c`, `src/externs.h`, `src/level-generation/level-generation-internal.h`, `src/tutorial/tutorial-game.c`.

## Chasm region

`world.partition.5`

Level: **Extended**.

Priority: **38** (higher appears first).

**1. Info**

{detail} Chasms divide this area. A fall can hurt you and send you deeper. Check each crossing and any leap requirements before moving.

Trigger: The player enters and publicly discovers partition kind 5.

Sources: `src/tutorial/tutorial-world.c`, `src/externs.h`, `src/level-generation/level-generation-internal.h`, `src/tutorial/tutorial-game.c`.

## Great Cave

`world.partition.6`

Level: **Extended**.

Priority: **38** (higher appears first).

**1. Info**

{detail} A great cave offers long sight lines and many approach routes. Distant enemies may see you too; use visible cover and keep room to retreat.

Trigger: The player enters and publicly discovers partition kind 6.

Sources: `src/tutorial/tutorial-world.c`, `src/externs.h`, `src/level-generation/level-generation-internal.h`, `src/tutorial/tutorial-game.c`.

## Fire Cave

`world.partition.fire`

Level: **Extended**.

Priority: **48** (higher appears first).

**1. Info**

{detail} Inside this cave, you lose one layer of fire, fear and stun resistance. Check your current totals before meeting enemies or crossing hazards. The cave theme tells you the environment, not which unseen creatures are present.

Trigger: A discovered great cave has the corresponding public elemental cave type.

Sources: `src/tutorial/tutorial-world.c`, `src/externs.h`, `src/tutorial/tutorial-game.c`.

## Cold Cave

`world.partition.cold`

Level: **Extended**.

Priority: **48** (higher appears first).

**1. Info**

{detail} Inside this cave, you lose one layer of cold, fear and stun resistance. Check your current totals before meeting enemies or crossing hazards. The cave theme tells you the environment, not which unseen creatures are present.

Trigger: A discovered great cave has the corresponding public elemental cave type.

Sources: `src/tutorial/tutorial-world.c`, `src/externs.h`, `src/tutorial/tutorial-game.c`.

## Poison Cave

`world.partition.poison`

Level: **Extended**.

Priority: **48** (higher appears first).

**1. Info**

{detail} Inside this cave, you lose one layer of poison, fear and stun resistance. Check your current totals before meeting enemies or crossing hazards. The cave theme tells you the environment, not which unseen creatures are present.

Trigger: A discovered great cave has the corresponding public elemental cave type.

Sources: `src/tutorial/tutorial-world.c`, `src/externs.h`, `src/tutorial/tutorial-game.c`.

## Tulkas the Strong

`quest.1`

Level: **Extended**.

Priority: **65** (higher appears first).

**1. Decision**

{detail} Hunt the creature named in Quests, then return to Tulkas for the reward. Check the target's learned attacks before confronting it.

Trigger: Quest 1 is publicly offered or accepted; only its revealed text is supplied in context.

Sources: `src/tutorial/tutorial-world.c`, `lib/edit/quest.txt`, `src/quest/quest-status.c`.

## Aulë the Smith

`quest.2`

Level: **Extended**.

Priority: **65** (higher appears first).

**1. Decision**

{detail} Forge a work that meets Aulë's current requirements, then collect the reward. Read the objective and smithing proposal before spending materials. Leaving this level before the reward is granted abandons the quest.

Trigger: Quest 2 is publicly offered or accepted; only its revealed text is supplied in context.

Sources: `src/tutorial/tutorial-world.c`, `lib/edit/quest.txt`, `src/quest/quest-status.c`.

## Mandos the Doomsman

`quest.3`

Level: **Extended**.

Priority: **65** (higher appears first).

**1. Decision**

{detail} Follow Mandos' objective concerning the bound spirit, then collect the reward. Check Quests for the exact target. Leaving this level before receiving the reward abandons the quest.

Trigger: Quest 3 is publicly offered or accepted; only its revealed text is supplied in context.

Sources: `src/tutorial/tutorial-world.c`, `lib/edit/quest.txt`, `src/quest/quest-status.c`.

## Nienna, Lady of Pity

`quest.4`

Level: **Extended**.

Priority: **65** (higher appears first).

**1. Decision**

{detail} Find the downward stair without taking a life, then follow the reward instructions before leaving. Avoid effects that can kill, including automatic follow-up attacks. Leaving the level before receiving the reward abandons this trial.

Trigger: Quest 4 is publicly offered or accepted; only its revealed text is supplied in context.

Sources: `src/tutorial/tutorial-world.c`, `lib/edit/quest.txt`, `src/quest/quest-status.c`.

## Oromë, the Great Hunter

`quest.5`

Level: **Extended**.

Priority: **65** (higher appears first).

**1. Decision**

{detail} The hunt counts wolves, spiders, serpents and vampires separately. Check each live total in Quests; killing more of one group does not fill another group's requirement.

Trigger: Quest 5 is publicly offered or accepted; only its revealed text is supplied in context.

Sources: `src/tutorial/tutorial-world.c`, `lib/edit/quest.txt`, `src/quest/quest-status.c`.

## Varda, Lady of the Stars

`quest.6`

Level: **Extended**.

Priority: **65** (higher appears first).

**1. Decision**

{detail} Check Quests for the revealed destination and time limit. Plan your descent before leaving the current level; unexplored locations remain unknown.

Trigger: Quest 6 is publicly offered or accepted; only its revealed text is supplied in context.

Sources: `src/tutorial/tutorial-world.c`, `lib/edit/quest.txt`, `src/quest/quest-status.c`.

## Unpredictable movement

`monster.rf1_rand_25`

Level: **Extended**.

Priority: **8** (higher appears first).

**1. Info**

This creature sometimes makes a random move. Its next step may differ from the route you expect, so check its position after each turn.

Trigger: A visible creature has this flag in learned lore (l_list), not merely its hidden race definition: RF1_RAND_25.

Sources: `src/tutorial/tutorial-world.c`, `src/defines.h`, `src/monster/monster-recall.c`, `src/melee/melee-process.c`.

## Erratic movement

`monster.rf1_rand_50`

Level: **Extended**.

Priority: **8** (higher appears first).

**1. Info**

This creature often makes random moves. It can still attack and use its abilities; leave room for an unexpected step.

Trigger: A visible creature has this flag in learned lore (l_list), not merely its hidden race definition: RF1_RAND_50.

Sources: `src/tutorial/tutorial-world.c`, `src/defines.h`, `src/monster/monster-recall.c`, `src/melee/melee-process.c`.

## A unique creature

`monster.rf1_unique`

Level: **Extended**.

Priority: **8** (higher appears first).

**1. Info**

This creature is unique and has its own attacks and defences. Read its learned description before confronting it; ordinary members of a similar group may behave differently.

Trigger: A visible creature has this flag in learned lore (l_list), not merely its hidden race definition: RF1_UNIQUE.

Sources: `src/tutorial/tutorial-world.c`, `src/defines.h`, `src/monster/monster-recall.c`, `src/melee/melee-process.c`.

## A peaceful creature

`monster.rf1_peaceful`

Level: **Extended**.

Priority: **8** (higher appears first).

**1. Info**

This creature is peaceful. Move toward it to use the available interaction instead of treating it as an ordinary enemy.

Trigger: A visible creature has this flag in learned lore (l_list), not merely its hidden race definition: RF1_PEACEFUL.

Sources: `src/tutorial/tutorial-world.c`, `src/defines.h`, `src/monster/monster-recall.c`, `src/melee/melee-process.c`.

## No ordinary melee blows

`monster.rf1_never_blow`

Level: **Extended**.

Priority: **8** (higher appears first).

**1. Info**

This creature does not make ordinary physical blows. Check its other learned abilities: ranged attacks or surrounding effects can still make it dangerous.

Trigger: A visible creature has this flag in learned lore (l_list), not merely its hidden race definition: RF1_NEVER_BLOW.

Sources: `src/tutorial/tutorial-world.c`, `src/defines.h`, `src/monster/monster-recall.c`, `src/melee/melee-process.c`.

## An immobile creature

`monster.rf1_never_move`

Level: **Extended**.

Priority: **8** (higher appears first).

**1. Info**

This creature does not move normally. Check the range of its attacks before approaching; it can still threaten squares within reach.

Trigger: A visible creature has this flag in learned lore (l_list), not merely its hidden race definition: RF1_NEVER_MOVE.

Sources: `src/tutorial/tutorial-world.c`, `src/defines.h`, `src/monster/monster-recall.c`, `src/melee/melee-process.c`.

## Movement out of sight

`monster.rf1_hidden_move`

Level: **Extended**.

Priority: **8** (higher appears first).

**1. Info**

This creature moves while outside your view. Losing sight of it can let it reposition, so do not assume it stayed on its last seen square.

Trigger: A visible creature has this flag in learned lore (l_list), not merely its hidden race definition: RF1_HIDDEN_MOVE.

Sources: `src/tutorial/tutorial-world.c`, `src/defines.h`, `src/monster/monster-recall.c`, `src/melee/melee-process.c`.

## Critical immunity

`monster.rf1_no_crit`

Level: **Extended**.

Priority: **8** (higher appears first).

**1. Info**

This creature has no vulnerable areas for critical hits. Higher Attack still helps you hit, but critical bonus dice do not apply. Compare ordinary damage and other known bonuses.

Trigger: A visible creature has this flag in learned lore (l_list), not merely its hidden race definition: RF1_NO_CRIT.

Sources: `src/tutorial/tutorial-world.c`, `src/defines.h`, `src/monster/monster-recall.c`, `src/melee/melee-process.c`.

## Critical resistance

`monster.rf1_res_crit`

Level: **Extended**.

Priority: **8** (higher appears first).

**1. Info**

This creature halves critical bonus dice, rounding down. For example, one critical die becomes none and two become one. Ordinary damage and other bonus dice are separate.

Trigger: A visible creature has this flag in learned lore (l_list), not merely its hidden race definition: RF1_RES_CRIT.

Sources: `src/tutorial/tutorial-world.c`, `src/defines.h`, `src/monster/monster-recall.c`, `src/melee/melee-process.c`.

## A mindless creature

`monster.rf2_mindless`

Level: **Extended**.

Priority: **8** (higher appears first).

**1. Info**

This creature follows simple, aggressive tactics. Do not rely on ordinary morale pressure to make it retreat. Check its separate resistances before choosing a status effect.

Trigger: A visible creature has this flag in learned lore (l_list), not merely its hidden race definition: RF2_MINDLESS.

Sources: `src/tutorial/tutorial-world.c`, `src/defines.h`, `src/monster/monster-recall.c`, `src/melee/melee-process.c`.

## An intelligent foe

`monster.rf2_smart`

Level: **Extended**.

Priority: **8** (higher appears first).

**1. Info**

This enemy can choose tactical positions and react to the fight. Expect it to use its learned attacks and available terrain rather than always charging straight toward you.

Trigger: A visible creature has this flag in learned lore (l_list), not merely its hidden race definition: RF2_SMART.

Sources: `src/tutorial/tutorial-world.c`, `src/defines.h`, `src/monster/monster-recall.c`, `src/melee/melee-process.c`.

## A territorial foe

`monster.rf2_territorial`

Level: **Extended**.

Priority: **8** (higher appears first).

**1. Info**

This creature does not pursue around corners in the usual way. Breaking its line of approach can help you disengage; watch where it actually stops.

Trigger: A visible creature has this flag in learned lore (l_list), not merely its hidden race definition: RF2_TERRITORIAL.

Sources: `src/tutorial/tutorial-world.c`, `src/defines.h`, `src/monster/monster-recall.c`, `src/melee/melee-process.c`.

## A short-sighted creature

`monster.rf2_short_sighted`

Level: **Extended**.

Priority: **8** (higher appears first).

**1. Info**

This creature detects you only at close range through its normal perception checks. Keep your distance when you want to pass unnoticed.

Trigger: A visible creature has this flag in learned lore (l_list), not merely its hidden race definition: RF2_SHORT_SIGHTED.

Sources: `src/tutorial/tutorial-world.c`, `src/defines.h`, `src/monster/monster-recall.c`, `src/melee/melee-process.c`.

## An invisible creature

`monster.rf2_invisible`

Level: **Extended**.

Priority: **8** (higher appears first).

**1. Info**

This creature is difficult to see normally. True Sight or detection can help you locate it. Check whether you can actually target it before committing an attack.

Trigger: A visible creature has this flag in learned lore (l_list), not merely its hidden race definition: RF2_INVISIBLE.

Sources: `src/tutorial/tutorial-world.c`, `src/defines.h`, `src/monster/monster-recall.c`, `src/melee/melee-process.c`.

## A luminous creature

`monster.rf2_glow`

Level: **Extended**.

Priority: **8** (higher appears first).

**1. Info**

This creature glows on its own square. That can reveal its position, even if its surroundings remain dark. It may have separate effects on nearby light.

Trigger: A visible creature has this flag in learned lore (l_list), not merely its hidden race definition: RF2_GLOW.

Sources: `src/tutorial/tutorial-world.c`, `src/defines.h`, `src/monster/monster-recall.c`, `src/melee/melee-process.c`.

## Enemy Cruel Blow

`monster.rf2_cruel_blow`

Level: **Extended**.

Priority: **8** (higher appears first).

**1. Info**

A strong critical hit from this creature can confuse you. Check your conditions after being hit; Clarity or Miruvor can cure confusion.

Trigger: A visible creature has this flag in learned lore (l_list), not merely its hidden race definition: RF2_CRUEL_BLOW.

Sources: `src/tutorial/tutorial-world.c`, `src/defines.h`, `src/monster/monster-recall.c`, `src/melee/melee-process.c`.

## Enemy Exchange Places

`monster.rf2_exchange_places`

Level: **Extended**.

Priority: **8** (higher appears first).

**1. Info**

This creature can exchange places in combat. A blocked route may suddenly open or close, so recheck positions after it moves.

Trigger: A visible creature has this flag in learned lore (l_list), not merely its hidden race definition: RF2_EXCHANGE_PLACES.

Sources: `src/tutorial/tutorial-world.c`, `src/defines.h`, `src/monster/monster-recall.c`, `src/melee/melee-process.c`.

## A multiplying creature

`monster.rf2_multiply`

Level: **Extended**.

Priority: **8** (higher appears first).

**1. Info**

This creature can reproduce. More can appear while you spend turns nearby. Consider dealing with the source or leaving before the group grows.

Trigger: A visible creature has this flag in learned lore (l_list), not merely its hidden race definition: RF2_MULTIPLY.

Sources: `src/tutorial/tutorial-world.c`, `src/defines.h`, `src/monster/monster-recall.c`, `src/melee/melee-process.c`.

## Enemy regeneration

`monster.rf2_regenerate`

Level: **Extended**.

Priority: **8** (higher appears first).

**1. Info**

This creature recovers Health especially quickly. Long pauses can undo your damage, so watch its condition when re-engaging.

Trigger: A visible creature has this flag in learned lore (l_list), not merely its hidden race definition: RF2_REGENERATE.

Sources: `src/tutorial/tutorial-world.c`, `src/defines.h`, `src/monster/monster-recall.c`, `src/melee/melee-process.c`.

## Enemy Riposte

`monster.rf2_riposte`

Level: **Extended**.

Priority: **8** (higher appears first).

**1. Info**

A melee attack that misses badly can give this enemy a free counterattack. Improve your accuracy or consider another approach before repeatedly attacking.

Trigger: A visible creature has this flag in learned lore (l_list), not merely its hidden race definition: RF2_RIPOSTE.

Sources: `src/tutorial/tutorial-world.c`, `src/defines.h`, `src/monster/monster-recall.c`, `src/melee/melee-process.c`.

## Enemy Flanking

`monster.rf2_flanking`

Level: **Extended**.

Priority: **8** (higher appears first).

**1. Info**

This enemy can attack while stepping between squares beside you. A movement action near you can therefore include a melee hit.

Trigger: A visible creature has this flag in learned lore (l_list), not merely its hidden race definition: RF2_FLANKING.

Sources: `src/tutorial/tutorial-world.c`, `src/defines.h`, `src/monster/monster-recall.c`, `src/melee/melee-process.c`.

## A surrounding cloud

`monster.rf2_cloud_surround`

Level: **Extended**.

Priority: **8** (higher appears first).

**1. Info**

This creature produces an effect around itself. Check the learned effect and keep enough distance to avoid standing in it unnecessarily.

Trigger: A visible creature has this flag in learned lore (l_list), not merely its hidden race definition: RF2_CLOUD_SURROUND.

Sources: `src/tutorial/tutorial-world.c`, `src/defines.h`, `src/monster/monster-recall.c`, `src/melee/melee-process.c`.

## A flying creature

`monster.rf2_flying`

Level: **Extended**.

Priority: **8** (higher appears first).

**1. Info**

This creature can fly over chasms and avoid contact with water, ice and poisonous seep. A gap in your walking route may not block it. Lava heat can still harm flyers without fire resistance.

Trigger: A visible creature has this flag in learned lore (l_list), not merely its hidden race definition: RF2_FLYING.

Sources: `src/tutorial/tutorial-world.c`, `src/defines.h`, `src/monster/monster-recall.c`, `src/melee/melee-process.c`.

## Passing through doors

`monster.rf2_pass_door`

Level: **Extended**.

Priority: **8** (higher appears first).

**1. Info**

This creature can pass under doors. Closing a door will not keep it out.

Trigger: A visible creature has this flag in learned lore (l_list), not merely its hidden race definition: RF2_PASS_DOOR.

Sources: `src/tutorial/tutorial-world.c`, `src/defines.h`, `src/monster/monster-recall.c`, `src/melee/melee-process.c`.

## Unlocking doors

`monster.rf2_unlock_door`

Level: **Extended**.

Priority: **8** (higher appears first).

**1. Info**

This creature can unlock doors. A lock may delay it, but you need another plan if it keeps approaching.

Trigger: A visible creature has this flag in learned lore (l_list), not merely its hidden race definition: RF2_UNLOCK_DOOR.

Sources: `src/tutorial/tutorial-world.c`, `src/defines.h`, `src/monster/monster-recall.c`, `src/melee/melee-process.c`.

## Opening doors

`monster.rf2_open_door`

Level: **Extended**.

Priority: **8** (higher appears first).

**1. Info**

This creature can open doors. Closing one can break sight or buy time, but it is not a lasting barrier.

Trigger: A visible creature has this flag in learned lore (l_list), not merely its hidden race definition: RF2_OPEN_DOOR.

Sources: `src/tutorial/tutorial-world.c`, `src/defines.h`, `src/monster/monster-recall.c`, `src/melee/melee-process.c`.

## Bashing doors

`monster.rf2_bash_door`

Level: **Extended**.

Priority: **8** (higher appears first).

**1. Info**

This creature can break through doors. Watch for the barrier to fail and keep another retreat route ready.

Trigger: A visible creature has this flag in learned lore (l_list), not merely its hidden race definition: RF2_BASH_DOOR.

Sources: `src/tutorial/tutorial-world.c`, `src/defines.h`, `src/monster/monster-recall.c`, `src/melee/melee-process.c`.

## Passing through walls

`monster.rf2_pass_wall`

Level: **Extended**.

Priority: **8** (higher appears first).

**1. Info**

This creature can move through walls. Ordinary rock will not contain it; check nearby squares even when a wall blocks your own route.

Trigger: A visible creature has this flag in learned lore (l_list), not merely its hidden race definition: RF2_PASS_WALL.

Sources: `src/tutorial/tutorial-world.c`, `src/defines.h`, `src/monster/monster-recall.c`, `src/melee/melee-process.c`.

## Destroying walls

`monster.rf2_kill_wall`

Level: **Extended**.

Priority: **8** (higher appears first).

**1. Info**

This creature can destroy walls. A blocked route can open as it approaches, changing sight lines and escape paths.

Trigger: A visible creature has this flag in learned lore (l_list), not merely its hidden race definition: RF2_KILL_WALL.

Sources: `src/tutorial/tutorial-world.c`, `src/defines.h`, `src/monster/monster-recall.c`, `src/melee/melee-process.c`.

## Tunnelling through walls

`monster.rf2_tunnel_wall`

Level: **Extended**.

Priority: **8** (higher appears first).

**1. Info**

This creature can tunnel through walls. Listen for digging and recheck routes as the terrain changes.

Trigger: A visible creature has this flag in learned lore (l_list), not merely its hidden race definition: RF2_TUNNEL_WALL.

Sources: `src/tutorial/tutorial-world.c`, `src/defines.h`, `src/monster/monster-recall.c`, `src/melee/melee-process.c`.

## Crushing other creatures

`monster.rf2_kill_body`

Level: **Extended**.

Priority: **8** (higher appears first).

**1. Info**

This creature can kill weaker monsters blocking its path. Another enemy may not keep it away from you for long.

Trigger: A visible creature has this flag in learned lore (l_list), not merely its hidden race definition: RF2_KILL_BODY.

Sources: `src/tutorial/tutorial-world.c`, `src/defines.h`, `src/monster/monster-recall.c`, `src/melee/melee-process.c`.

## Taking floor items

`monster.rf2_take_item`

Level: **Extended**.

Priority: **8** (higher appears first).

**1. Info**

This creature can pick up floor items. Consider collecting something useful before it reaches the item, if doing so is safe.

Trigger: A visible creature has this flag in learned lore (l_list), not merely its hidden race definition: RF2_TAKE_ITEM.

Sources: `src/tutorial/tutorial-world.c`, `src/defines.h`, `src/monster/monster-recall.c`, `src/melee/melee-process.c`.

## Destroying floor items

`monster.rf2_kill_item`

Level: **Extended**.

Priority: **8** (higher appears first).

**1. Info**

This creature can destroy items in its path. A floor item may be lost if you leave it in the approaching creature's route.

Trigger: A visible creature has this flag in learned lore (l_list), not merely its hidden race definition: RF2_KILL_ITEM.

Sources: `src/tutorial/tutorial-world.c`, `src/defines.h`, `src/monster/monster-recall.c`, `src/melee/melee-process.c`.

## Retreating when power is low

`monster.rf2_low_mana_run`

Level: **Extended**.

Priority: **8** (higher appears first).

**1. Info**

This creature may retreat when it runs low on power. Watch its position: a retreat does not mean it has been defeated.

Trigger: A visible creature has this flag in learned lore (l_list), not merely its hidden race definition: RF2_LOW_MANA_RUN.

Sources: `src/tutorial/tutorial-world.c`, `src/defines.h`, `src/monster/monster-recall.c`, `src/melee/melee-process.c`.

## Enemy Charge

`monster.rf2_charge`

Level: **Extended**.

Priority: **8** (higher appears first).

**1. Info**

This creature can make a stronger attack after moving toward you. A straight approach gives it a chance to charge.

Trigger: A visible creature has this flag in learned lore (l_list), not merely its hidden race definition: RF2_CHARGE.

Sources: `src/tutorial/tutorial-world.c`, `src/defines.h`, `src/monster/monster-recall.c`, `src/melee/melee-process.c`.

## Bane against Elves

`monster.rf2_elfbane`

Level: **Extended**.

Priority: **8** (higher appears first).

**1. Info**

This creature has a racial Bane against Elves. Its relevant skill contests can be stronger against a matching hero.

Trigger: A visible creature has this flag in learned lore (l_list), not merely its hidden race definition: RF2_ELFBANE.

Sources: `src/tutorial/tutorial-world.c`, `src/defines.h`, `src/monster/monster-recall.c`, `src/melee/melee-process.c`.

## Enemy Knock Back

`monster.rf2_knock_back`

Level: **Extended**.

Priority: **8** (higher appears first).

**1. Info**

This enemy can push you backward. Keep clear of chasms, lava, traps and other hazards behind your square.

Trigger: A visible creature has this flag in learned lore (l_list), not merely its hidden race definition: RF2_KNOCK_BACK.

Sources: `src/tutorial/tutorial-world.c`, `src/defines.h`, `src/monster/monster-recall.c`, `src/melee/melee-process.c`.

## Enemy Crippling Shot

`monster.rf2_crippling`

Level: **Extended**.

Priority: **8** (higher appears first).

**1. Info**

A critical ranged hit from this creature can slow you. Check both damage and the Slow condition after a hit.

Trigger: A visible creature has this flag in learned lore (l_list), not merely its hidden race definition: RF2_CRIPPLING.

Sources: `src/tutorial/tutorial-world.c`, `src/defines.h`, `src/monster/monster-recall.c`, `src/melee/melee-process.c`.

## Enemy Opportunist

`monster.rf2_opportunist`

Level: **Extended**.

Priority: **8** (higher appears first).

**1. Info**

Moving away from this adjacent enemy can give it a free attack. A retreat may still be worthwhile; compare the destination with the risk of that hit.

Trigger: A visible creature has this flag in learned lore (l_list), not merely its hidden race definition: RF2_OPPORTUNIST.

Sources: `src/tutorial/tutorial-world.c`, `src/defines.h`, `src/monster/monster-recall.c`, `src/melee/melee-process.c`.

## Enemy Zone of Control

`monster.rf2_zone_of_control`

Level: **Extended**.

Priority: **8** (higher appears first).

**1. Info**

Moving from one square beside this enemy to another can give it a free attack. Check the whole step when moving around it.

Trigger: A visible creature has this flag in learned lore (l_list), not merely its hidden race definition: RF2_ZONE_OF_CONTROL.

Sources: `src/tutorial/tutorial-world.c`, `src/defines.h`, `src/monster/monster-recall.c`, `src/melee/melee-process.c`.

## A light-sensitive creature

`monster.rf3_hurt_lite`

Level: **Extended**.

Priority: **8** (higher appears first).

**1. Info**

Bright light penalises this creature. Some light effects can also stun or hurt it. Check the illumination on its actual square, not just your lamp's fuel.

Trigger: A visible creature has this flag in learned lore (l_list), not merely its hidden race definition: RF3_HURT_LITE.

Sources: `src/tutorial/tutorial-world.c`, `src/defines.h`, `src/monster/monster-recall.c`, `src/melee/melee-process.c`.

## A creature of stone

`monster.rf3_stone`

Level: **Extended**.

Priority: **8** (higher appears first).

**1. Info**

This creature has a stone body. Shattering effects can damage or weaken it in ways that ordinary creatures resist.

Trigger: A visible creature has this flag in learned lore (l_list), not merely its hidden race definition: RF3_STONE.

Sources: `src/tutorial/tutorial-world.c`, `src/defines.h`, `src/monster/monster-recall.c`, `src/melee/melee-process.c`.

## Fire vulnerability

`monster.rf3_hurt_fire`

Level: **Extended**.

Priority: **8** (higher appears first).

**1. Info**

This creature is especially vulnerable to fire. Fire attacks deal extra damage, and a fire brand can add more bonus dice against it.

Trigger: A visible creature has this flag in learned lore (l_list), not merely its hidden race definition: RF3_HURT_FIRE.

Sources: `src/tutorial/tutorial-world.c`, `src/defines.h`, `src/monster/monster-recall.c`, `src/melee/melee-process.c`.

## Cold vulnerability

`monster.rf3_hurt_cold`

Level: **Extended**.

Priority: **8** (higher appears first).

**1. Info**

This creature is especially vulnerable to cold. Cold attacks deal extra damage, and a cold brand can add more bonus dice against it.

Trigger: A visible creature has this flag in learned lore (l_list), not merely its hidden race definition: RF3_HURT_COLD.

Sources: `src/tutorial/tutorial-world.c`, `src/defines.h`, `src/monster/monster-recall.c`, `src/melee/melee-process.c`.

## An armed creature

`monster.rf3_has_weapon`

Level: **Extended**.

Priority: **8** (higher appears first).

**1. Info**

This creature uses forged weapons. Song of Shattering can weaken its weapon attacks if the effect succeeds.

Trigger: A visible creature has this flag in learned lore (l_list), not merely its hidden race definition: RF3_HAS_WEAPON.

Sources: `src/tutorial/tutorial-world.c`, `src/defines.h`, `src/monster/monster-recall.c`, `src/melee/melee-process.c`.

## An armoured creature

`monster.rf3_has_armour`

Level: **Extended**.

Priority: **8** (higher appears first).

**1. Info**

This creature wears substantial armour. Strong damage or critical hits may be needed to get through it. Song of Shattering can weaken its armour if the effect succeeds.

Trigger: A visible creature has this flag in learned lore (l_list), not merely its hidden race definition: RF3_HAS_ARMOUR.

Sources: `src/tutorial/tutorial-world.c`, `src/defines.h`, `src/monster/monster-recall.c`, `src/melee/melee-process.c`.

## Lightning resistance

`monster.rf3_res_elec`

Level: **Extended**.

Priority: **8** (higher appears first).

**1. Info**

This creature is immune to lightning damage and the extra dice of a lightning brand. Ordinary weapon damage can still affect it.

Trigger: A visible creature has this flag in learned lore (l_list), not merely its hidden race definition: RF3_RES_ELEC.

Sources: `src/tutorial/tutorial-world.c`, `src/defines.h`, `src/monster/monster-recall.c`, `src/melee/melee-process.c`, `src/spell/spell-projection-effects.c`.

## Fire resistance

`monster.rf3_res_fire`

Level: **Extended**.

Priority: **8** (higher appears first).

**1. Info**

This creature is immune to fire damage, lava and the extra dice of a fire brand. Choose another damage type or ordinary weapon damage.

Trigger: A visible creature has this flag in learned lore (l_list), not merely its hidden race definition: RF3_RES_FIRE.

Sources: `src/tutorial/tutorial-world.c`, `src/defines.h`, `src/monster/monster-recall.c`, `src/melee/melee-process.c`, `src/spell/spell-projection-effects.c`.

## Cold resistance

`monster.rf3_res_cold`

Level: **Extended**.

Priority: **8** (higher appears first).

**1. Info**

This creature is immune to cold damage and the extra dice of a cold brand. Choose another damage type or ordinary weapon damage.

Trigger: A visible creature has this flag in learned lore (l_list), not merely its hidden race definition: RF3_RES_COLD.

Sources: `src/tutorial/tutorial-world.c`, `src/defines.h`, `src/monster/monster-recall.c`, `src/melee/melee-process.c`, `src/spell/spell-projection-effects.c`.

## Poison resistance

`monster.rf3_res_pois`

Level: **Extended**.

Priority: **8** (higher appears first).

**1. Info**

This creature is immune to poisoning and the extra poison effect of a poison brand. Ordinary weapon damage can still affect it.

Trigger: A visible creature has this flag in learned lore (l_list), not merely its hidden race definition: RF3_RES_POIS.

Sources: `src/tutorial/tutorial-world.c`, `src/defines.h`, `src/monster/monster-recall.c`, `src/melee/melee-process.c`, `src/spell/spell-projection-effects.c`.

## Resists slowing

`monster.rf3_no_slow`

Level: **Extended**.

Priority: **8** (higher appears first).

**1. Info**

This creature is extremely resistant to slowing: magical slowing checks face an additional 100 resistance. Choose another tactic rather than counting on Slow.

Trigger: A visible creature has this flag in learned lore (l_list), not merely its hidden race definition: RF3_NO_SLOW.

Sources: `src/tutorial/tutorial-world.c`, `src/defines.h`, `src/monster/monster-recall.c`, `src/melee/melee-process.c`, `src/spell/spell-projection-effects.c`.

## Resists fear

`monster.rf3_no_fear`

Level: **Extended**.

Priority: **8** (higher appears first).

**1. Info**

This creature resists ordinary morale pressure and gains +100 resistance against magical fear. Frightening it is usually impractical; plan another way to escape or defeat it.

Trigger: A visible creature has this flag in learned lore (l_list), not merely its hidden race definition: RF3_NO_FEAR.

Sources: `src/tutorial/tutorial-world.c`, `src/defines.h`, `src/monster/monster-recall.c`, `src/melee/melee-process.c`, `src/spell/spell-projection-effects.c`.

## Cannot be stunned

`monster.rf3_no_stun`

Level: **Extended**.

Priority: **8** (higher appears first).

**1. Info**

This creature cannot be stunned. A stunning effect may have other uses, but stunning this target will not help.

Trigger: A visible creature has this flag in learned lore (l_list), not merely its hidden race definition: RF3_NO_STUN.

Sources: `src/tutorial/tutorial-world.c`, `src/defines.h`, `src/monster/monster-recall.c`, `src/melee/melee-process.c`, `src/spell/spell-projection-effects.c`.

## Resists confusion

`monster.rf3_no_conf`

Level: **Extended**.

Priority: **8** (higher appears first).

**1. Info**

This creature is extremely resistant to confusion: magical confusion checks face an additional 100 resistance. Choose another tactic rather than relying on it becoming confused.

Trigger: A visible creature has this flag in learned lore (l_list), not merely its hidden race definition: RF3_NO_CONF.

Sources: `src/tutorial/tutorial-world.c`, `src/defines.h`, `src/monster/monster-recall.c`, `src/melee/melee-process.c`, `src/spell/spell-projection-effects.c`.

## Resists sleep

`monster.rf3_no_sleep`

Level: **Extended**.

Priority: **8** (higher appears first).

**1. Info**

This creature is extremely resistant to sleep: sleep checks face an additional 100 resistance. Do not rely on a slumber effect to stop it.

Trigger: A visible creature has this flag in learned lore (l_list), not merely its hidden race definition: RF3_NO_SLEEP.

Sources: `src/tutorial/tutorial-world.c`, `src/defines.h`, `src/monster/monster-recall.c`, `src/melee/melee-process.c`, `src/spell/spell-projection-effects.c`.

## Enemy archery

`monster.rf4_arrow1`

Level: **Extended**.

Priority: **8** (higher appears first).

**1. Info**

This creature has a shortbow arrow attack. Lines of fire and nearby cover matter even when it is not adjacent.

Trigger: A visible creature has this flag in learned lore (l_list), not merely its hidden race definition: RF4_ARROW1.

Sources: `src/tutorial/tutorial-world.c`, `src/defines.h`, `src/monster/monster-recall.c`, `src/melee/melee-process.c`.

## Enemy longbow

`monster.rf4_arrow2`

Level: **Extended**.

Priority: **8** (higher appears first).

**1. Info**

This creature has a longbow arrow attack. Distance alone does not prevent ranged damage; inspect the line of fire.

Trigger: A visible creature has this flag in learned lore (l_list), not merely its hidden race definition: RF4_ARROW2.

Sources: `src/tutorial/tutorial-world.c`, `src/defines.h`, `src/monster/monster-recall.c`, `src/melee/melee-process.c`.

## Thrown boulders

`monster.rf4_boulder`

Level: **Extended**.

Priority: **8** (higher appears first).

**1. Info**

This creature can throw boulders. Check cover and the actual attack result; being outside melee range does not make you safe.

Trigger: A visible creature has this flag in learned lore (l_list), not merely its hidden race definition: RF4_BOULDER.

Sources: `src/tutorial/tutorial-world.c`, `src/defines.h`, `src/monster/monster-recall.c`, `src/melee/melee-process.c`.

## Fire breath

`monster.rf4_brth_fire`

Level: **Extended**.

Priority: **8** (higher appears first).

**1. Info**

This creature can breathe fire. Inspect fire resistance, relevant Protection, cover and the observed area before choosing a route.

Trigger: A visible creature has this flag in learned lore (l_list), not merely its hidden race definition: RF4_BRTH_FIRE.

Sources: `src/tutorial/tutorial-world.c`, `src/defines.h`, `src/monster/monster-recall.c`, `src/melee/melee-process.c`.

## Cold breath

`monster.rf4_brth_cold`

Level: **Extended**.

Priority: **8** (higher appears first).

**1. Info**

This creature can breathe cold. Inspect cold resistance, applicable Protection and the line of attack.

Trigger: A visible creature has this flag in learned lore (l_list), not merely its hidden race definition: RF4_BRTH_COLD.

Sources: `src/tutorial/tutorial-world.c`, `src/defines.h`, `src/monster/monster-recall.c`, `src/melee/melee-process.c`.

## Poison breath

`monster.rf4_brth_pois`

Level: **Extended**.

Priority: **8** (higher appears first).

**1. Info**

This creature can breathe poison. Resistance reduces damage differently from an Antidote curing existing poison.

Trigger: A visible creature has this flag in learned lore (l_list), not merely its hidden race definition: RF4_BRTH_POIS.

Sources: `src/tutorial/tutorial-world.c`, `src/defines.h`, `src/monster/monster-recall.c`, `src/melee/melee-process.c`.

## Dark breath

`monster.rf4_brth_dark`

Level: **Extended**.

Priority: **8** (higher appears first).

**1. Info**

This creature can breathe darkness. Light and dark resistance have their own rules; ordinary physical Protection is not automatically applicable.

Trigger: A visible creature has this flag in learned lore (l_list), not merely its hidden race definition: RF4_BRTH_DARK.

Sources: `src/tutorial/tutorial-world.c`, `src/defines.h`, `src/monster/monster-recall.c`, `src/melee/melee-process.c`.

## Earthquakes

`monster.rf4_earthquake`

Level: **Extended**.

Priority: **8** (higher appears first).

**1. Info**

This creature can cause an earthquake. The terrain and available routes can change; recheck the map after the actual event.

Trigger: A visible creature has this flag in learned lore (l_list), not merely its hidden race definition: RF4_EARTHQUAKE.

Sources: `src/tutorial/tutorial-world.c`, `src/defines.h`, `src/monster/monster-recall.c`, `src/melee/melee-process.c`.

## Calling for help

`monster.rf4_shriek`

Level: **Extended**.

Priority: **8** (higher appears first).

**1. Info**

This creature can shriek for help. Noise can alert other foes. A creature currently alone in view may soon gain support.

Trigger: A visible creature has this flag in learned lore (l_list), not merely its hidden race definition: RF4_SHRIEK.

Sources: `src/tutorial/tutorial-world.c`, `src/defines.h`, `src/monster/monster-recall.c`, `src/melee/melee-process.c`.

## A stunning screech

`monster.rf4_screech`

Level: **Extended**.

Priority: **8** (higher appears first).

**1. Info**

This creature can make a loud screech and stun you. Noise and the resulting condition are separate consequences.

Trigger: A visible creature has this flag in learned lore (l_list), not merely its hidden race definition: RF4_SCREECH.

Sources: `src/tutorial/tutorial-world.c`, `src/defines.h`, `src/monster/monster-recall.c`, `src/melee/melee-process.c`.

## Creating darkness

`monster.rf4_darkness`

Level: **Extended**.

Priority: **8** (higher appears first).

**1. Info**

This creature can darken the area around you. Recheck what is currently visible; remembered terrain is not proof that enemies have stayed in place.

Trigger: A visible creature has this flag in learned lore (l_list), not merely its hidden race definition: RF4_DARKNESS.

Sources: `src/tutorial/tutorial-world.c`, `src/defines.h`, `src/monster/monster-recall.c`, `src/melee/melee-process.c`.

## Forgetting the map

`monster.rf4_forget`

Level: **Extended**.

Priority: **8** (higher appears first).

**1. Info**

This creature can make you forget mapped terrain. This affects remembered knowledge, separate from current sight.

Trigger: A visible creature has this flag in learned lore (l_list), not merely its hidden race definition: RF4_FORGET.

Sources: `src/tutorial/tutorial-world.c`, `src/defines.h`, `src/monster/monster-recall.c`, `src/melee/melee-process.c`.

## Causing fear

`monster.rf4_scare`

Level: **Extended**.

Priority: **8** (higher appears first).

**1. Info**

This creature can frighten you. Fear interferes with melee and ranged aiming. Inspect a known remedy after the condition actually appears.

Trigger: A visible creature has this flag in learned lore (l_list), not merely its hidden race definition: RF4_SCARE.

Sources: `src/tutorial/tutorial-world.c`, `src/defines.h`, `src/monster/monster-recall.c`, `src/melee/melee-process.c`.

## Causing confusion

`monster.rf4_conf`

Level: **Extended**.

Priority: **8** (higher appears first).

**1. Info**

This creature can confuse you. Direction and aiming choices become unreliable while confused; inspect current conditions before committing.

Trigger: A visible creature has this flag in learned lore (l_list), not merely its hidden race definition: RF4_CONF.

Sources: `src/tutorial/tutorial-world.c`, `src/defines.h`, `src/monster/monster-recall.c`, `src/melee/melee-process.c`.

## Causing entrancement

`monster.rf4_hold`

Level: **Extended**.

Priority: **8** (higher appears first).

**1. Info**

This creature can entrance you. A successful trance prevents ordinary actions until it ends. Avoid assuming you can drink a remedy after losing the ability to act.

Trigger: A visible creature has this flag in learned lore (l_list), not merely its hidden race definition: RF4_HOLD.

Sources: `src/tutorial/tutorial-world.c`, `src/defines.h`, `src/monster/monster-recall.c`, `src/melee/melee-process.c`.

## Causing slowing

`monster.rf4_slow`

Level: **Extended**.

Priority: **8** (higher appears first).

**1. Info**

This creature can slow you. Compare your current speed and escape route; a temporary speed bonus may offset the penalty without removing it.

Trigger: A visible creature has this flag in learned lore (l_list), not merely its hidden race definition: RF4_SLOW.

Sources: `src/tutorial/tutorial-world.c`, `src/defines.h`, `src/monster/monster-recall.c`, `src/melee/melee-process.c`.

## Hatching spiders

`monster.rf4_hatch_spider`

Level: **Extended**.

Priority: **8** (higher appears first).

**1. Info**

This creature can hatch spiders. Additional enemies can alter surrounding pressure and block routes.

Trigger: A visible creature has this flag in learned lore (l_list), not merely its hidden race definition: RF4_HATCH_SPIDER.

Sources: `src/tutorial/tutorial-world.c`, `src/defines.h`, `src/monster/monster-recall.c`, `src/melee/melee-process.c`.

## Dimming light

`monster.rf4_dim`

Level: **Extended**.

Priority: **8** (higher appears first).

**1. Info**

This creature can dim your light. Check the darkening effect separately from fuel and blindness.

Trigger: A visible creature has this flag in learned lore (l_list), not merely its hidden race definition: RF4_DIM.

Sources: `src/tutorial/tutorial-world.c`, `src/defines.h`, `src/monster/monster-recall.c`, `src/melee/melee-process.c`.

## An enemy binding song

`monster.rf4_sng_binding`

Level: **Extended**.

Priority: **8** (higher appears first).

**1. Info**

This enemy's Song of Binding can slow you and close or lock doors. Check your escape route and Slow condition. Song of Silence weakens the opposing song.

Trigger: A visible creature has this flag in learned lore (l_list), not merely its hidden race definition: RF4_SNG_BINDING.

Sources: `src/tutorial/tutorial-world.c`, `src/defines.h`, `src/monster/monster-recall.c`, `src/melee/melee-process.c`, `src/player/player-song-monster.c`.

## An enemy piercing song

`monster.rf4_sng_piercing`

Level: **Extended**.

Priority: **8** (higher appears first).

**1. Info**

This enemy's Song of Piercing can reveal your location to it through an opposed Will check. Distance and Song of Silence help you resist the search.

Trigger: A visible creature has this flag in learned lore (l_list), not merely its hidden race definition: RF4_SNG_PIERCING.

Sources: `src/tutorial/tutorial-world.c`, `src/defines.h`, `src/monster/monster-recall.c`, `src/melee/melee-process.c`, `src/player/player-song-monster.c`.

## An enemy oath song

`monster.rf4_sng_oaths`

Level: **Extended**.

Priority: **8** (higher appears first).

**1. Info**

This enemy's Song of Oaths can summon Oathwraiths. Leaving it singing can add more pursuers. Song of Silence weakens the summoning song.

Trigger: A visible creature has this flag in learned lore (l_list), not merely its hidden race definition: RF4_SNG_OATHS.

Sources: `src/tutorial/tutorial-world.c`, `src/defines.h`, `src/monster/monster-recall.c`, `src/melee/melee-process.c`, `src/player/player-song-monster.c`.

## Bane against Dwarves

`monster.rf4_dwarfbane`

Level: **Extended**.

Priority: **8** (higher appears first).

**1. Info**

This creature has a racial Bane against Dwarves. Relevant contests can be harder for a matching hero.

Trigger: A visible creature has this flag in learned lore (l_list), not merely its hidden race definition: RF4_DWARFBANE.

Sources: `src/tutorial/tutorial-world.c`, `src/defines.h`, `src/monster/monster-recall.c`, `src/melee/melee-process.c`.

## Bane against Men

`monster.rf4_edainbane`

Level: **Extended**.

Priority: **8** (higher appears first).

**1. Info**

This creature has a racial Bane against Men. Relevant contests can be harder for a matching hero.

Trigger: A visible creature has this flag in learned lore (l_list), not merely its hidden race definition: RF4_EDAINBANE.

Sources: `src/tutorial/tutorial-world.c`, `src/defines.h`, `src/monster/monster-recall.c`, `src/melee/melee-process.c`.

## Thrown webs

`monster.rf4_throw_web`

Level: **Extended**.

Priority: **8** (higher appears first).

**1. Info**

This creature can throw a web onto your square. Escaping can take time; check your current terrain before repeatedly trying to move.

Trigger: A visible creature has this flag in learned lore (l_list), not merely its hidden race definition: RF4_THROW_WEB.

Sources: `src/tutorial/tutorial-world.c`, `src/defines.h`, `src/monster/monster-recall.c`, `src/melee/melee-process.c`.

## Rallying allies

`monster.rf4_rally`

Level: **Extended**.

Priority: **8** (higher appears first).

**1. Info**

This creature can restore the courage of fleeing allies. Watch for enemies returning to the fight after they retreat.

Trigger: A visible creature has this flag in learned lore (l_list), not merely its hidden race definition: RF4_RALLY.

Sources: `src/tutorial/tutorial-world.c`, `src/defines.h`, `src/monster/monster-recall.c`, `src/melee/melee-process.c`.

## Bane against Noldor

`monster.rf4_noldorbane`

Level: **Extended**.

Priority: **8** (higher appears first).

**1. Info**

This creature has a Bane against Noldor. Its relevant skill contests can be stronger against a matching hero.

Trigger: A visible creature has this flag in learned lore (l_list), not merely its hidden race definition: RF4_NOLDORBANE.

Sources: `src/tutorial/tutorial-world.c`, `src/defines.h`, `src/monster/monster-recall.c`, `src/melee/melee-process.c`.

## Bane against Sindar

`monster.rf4_sindarbane`

Level: **Extended**.

Priority: **8** (higher appears first).

**1. Info**

This creature has a Bane against Sindar. Its relevant skill contests can be stronger against a matching hero.

Trigger: A visible creature has this flag in learned lore (l_list), not merely its hidden race definition: RF4_SINDARBANE.

Sources: `src/tutorial/tutorial-world.c`, `src/defines.h`, `src/monster/monster-recall.c`, `src/melee/melee-process.c`.

## Orc lore

`monster.rf3_orc`

Level: **Extended**.

Priority: **8** (higher appears first).

**1. Info**

This creature belongs to the Orc group. Check the targets named by your weapon slays, Bane ability, quests and oath; those effects may depend on creature type. Its other traits must be learned separately.

Trigger: A visible creature has this flag in learned lore (l_list), not merely its hidden race definition: RF3_ORC.

Sources: `src/tutorial/tutorial-world.c`, `src/defines.h`, `src/monster/monster-recall.c`, `src/melee/melee-process.c`.

## Troll lore

`monster.rf3_troll`

Level: **Extended**.

Priority: **8** (higher appears first).

**1. Info**

This creature belongs to the Troll group. Check the targets named by your weapon slays, Bane ability, quests and oath; those effects may depend on creature type. Its other traits must be learned separately.

Trigger: A visible creature has this flag in learned lore (l_list), not merely its hidden race definition: RF3_TROLL.

Sources: `src/tutorial/tutorial-world.c`, `src/defines.h`, `src/monster/monster-recall.c`, `src/melee/melee-process.c`.

## Serpent lore

`monster.rf3_serpent`

Level: **Extended**.

Priority: **8** (higher appears first).

**1. Info**

This creature belongs to the Serpent group. Check the targets named by your weapon slays, Bane ability, quests and oath; those effects may depend on creature type. Its other traits must be learned separately.

Trigger: A visible creature has this flag in learned lore (l_list), not merely its hidden race definition: RF3_SERPENT.

Sources: `src/tutorial/tutorial-world.c`, `src/defines.h`, `src/monster/monster-recall.c`, `src/melee/melee-process.c`.

## Dragon lore

`monster.rf3_dragon`

Level: **Extended**.

Priority: **8** (higher appears first).

**1. Info**

This creature belongs to the Dragon group. Check the targets named by your weapon slays, Bane ability, quests and oath; those effects may depend on creature type. Its other traits must be learned separately.

Trigger: A visible creature has this flag in learned lore (l_list), not merely its hidden race definition: RF3_DRAGON.

Sources: `src/tutorial/tutorial-world.c`, `src/defines.h`, `src/monster/monster-recall.c`, `src/melee/melee-process.c`.

## Rauko lore

`monster.rf3_rauko`

Level: **Extended**.

Priority: **8** (higher appears first).

**1. Info**

This creature belongs to the Rauko group. Check the targets named by your weapon slays, Bane ability, quests and oath; those effects may depend on creature type. Its other traits must be learned separately.

Trigger: A visible creature has this flag in learned lore (l_list), not merely its hidden race definition: RF3_RAUKO.

Sources: `src/tutorial/tutorial-world.c`, `src/defines.h`, `src/monster/monster-recall.c`, `src/melee/melee-process.c`.

## Undead lore

`monster.rf3_undead`

Level: **Extended**.

Priority: **8** (higher appears first).

**1. Info**

This creature belongs to the Undead group. Check the targets named by your weapon slays, Bane ability, quests and oath; those effects may depend on creature type. Its other traits must be learned separately.

Trigger: A visible creature has this flag in learned lore (l_list), not merely its hidden race definition: RF3_UNDEAD.

Sources: `src/tutorial/tutorial-world.c`, `src/defines.h`, `src/monster/monster-recall.c`, `src/melee/melee-process.c`.

## Spider lore

`monster.rf3_spider`

Level: **Extended**.

Priority: **8** (higher appears first).

**1. Info**

This creature belongs to the Spider group. Check the targets named by your weapon slays, Bane ability, quests and oath; those effects may depend on creature type. Its other traits must be learned separately.

Trigger: A visible creature has this flag in learned lore (l_list), not merely its hidden race definition: RF3_SPIDER.

Sources: `src/tutorial/tutorial-world.c`, `src/defines.h`, `src/monster/monster-recall.c`, `src/melee/melee-process.c`.

## Wolf lore

`monster.rf3_wolf`

Level: **Extended**.

Priority: **8** (higher appears first).

**1. Info**

This creature belongs to the Wolf group. Check the targets named by your weapon slays, Bane ability, quests and oath; those effects may depend on creature type. Its other traits must be learned separately.

Trigger: A visible creature has this flag in learned lore (l_list), not merely its hidden race definition: RF3_WOLF.

Sources: `src/tutorial/tutorial-world.c`, `src/defines.h`, `src/monster/monster-recall.c`, `src/melee/melee-process.c`.

## Man lore

`monster.rf3_man`

Level: **Extended**.

Priority: **8** (higher appears first).

**1. Info**

This creature belongs to the Man group. Check the targets named by your weapon slays, Bane ability, quests and oath; those effects may depend on creature type. Its other traits must be learned separately.

Trigger: A visible creature has this flag in learned lore (l_list), not merely its hidden race definition: RF3_MAN.

Sources: `src/tutorial/tutorial-world.c`, `src/defines.h`, `src/monster/monster-recall.c`, `src/melee/melee-process.c`.

## Elf lore

`monster.rf3_elf`

Level: **Extended**.

Priority: **8** (higher appears first).

**1. Info**

This creature belongs to the Elf group. Check the targets named by your weapon slays, Bane ability, quests and oath; those effects may depend on creature type. Its other traits must be learned separately.

Trigger: A visible creature has this flag in learned lore (l_list), not merely its hidden race definition: RF3_ELF.

Sources: `src/tutorial/tutorial-world.c`, `src/defines.h`, `src/monster/monster-recall.c`, `src/melee/melee-process.c`.

## Giant lore

`monster.rf3_giant`

Level: **Extended**.

Priority: **8** (higher appears first).

**1. Info**

This creature belongs to the Giant group. Check the targets named by your weapon slays, Bane ability, quests and oath; those effects may depend on creature type. Its other traits must be learned separately.

Trigger: A visible creature has this flag in learned lore (l_list), not merely its hidden race definition: RF3_GIANT.

Sources: `src/tutorial/tutorial-world.c`, `src/defines.h`, `src/monster/monster-recall.c`, `src/melee/melee-process.c`.

## Cat lore

`monster.rf3_cat`

Level: **Extended**.

Priority: **8** (higher appears first).

**1. Info**

This creature belongs to the Cat group. Check the targets named by your weapon slays, Bane ability, quests and oath; those effects may depend on creature type. Its other traits must be learned separately.

Trigger: A visible creature has this flag in learned lore (l_list), not merely its hidden race definition: RF3_CAT.

Sources: `src/tutorial/tutorial-world.c`, `src/defines.h`, `src/monster/monster-recall.c`, `src/melee/melee-process.c`.

## Horror lore

`monster.rf3_horror`

Level: **Extended**.

Priority: **8** (higher appears first).

**1. Info**

This creature belongs to the Horror group. Check the targets named by your weapon slays, Bane ability, quests and oath; those effects may depend on creature type. Its other traits must be learned separately.

Trigger: A visible creature has this flag in learned lore (l_list), not merely its hidden race definition: RF3_HORROR.

Sources: `src/tutorial/tutorial-world.c`, `src/defines.h`, `src/monster/monster-recall.c`, `src/melee/melee-process.c`.

## Vampire lore

`monster.rf3_vampire`

Level: **Extended**.

Priority: **8** (higher appears first).

**1. Info**

This creature belongs to the Vampire group. Check the targets named by your weapon slays, Bane ability, quests and oath; those effects may depend on creature type. Its other traits must be learned separately.

Trigger: A visible creature has this flag in learned lore (l_list), not merely its hidden race definition: RF3_VAMPIRE.

Sources: `src/tutorial/tutorial-world.c`, `src/defines.h`, `src/monster/monster-recall.c`, `src/melee/melee-process.c`.

## Heavy stun

`status.heavy_stun`

Level: **Normal**.

Priority: **84** (higher appears first).

**1. Info**

Stun severity is at least 50, so every skill is reduced by 4. Above 100, you lose the ability to act. Clarity or Miruvor can remove stun while you can still use an item.

Trigger: The corresponding public status-pane state first becomes active with its owning ability or action requirements satisfied: heavy_stun.

Sources: `src/sdl/ui/sdl-panes.c`, `src/tutorial/tutorial-game.c`, `src/player/player-song-duels.c`, `src/cmd/combat/cmd-combat.c`.

## Knocked out

`status.knocked_out`

Level: **Normal**.

Priority: **96** (higher appears first).

**1. Info**

Stun severity is above 100. You cannot act until it falls enough for you to recover. Continue closes the explanation; remedies become available once you can act again.

Trigger: The corresponding public status-pane state first becomes active with its owning ability or action requirements satisfied: knocked_out.

Sources: `src/sdl/ui/sdl-panes.c`, `src/tutorial/tutorial-game.c`, `src/player/player-song-duels.c`, `src/cmd/combat/cmd-combat.c`.

## Full

`status.full`

Level: **Normal**.

Priority: **33** (higher appears first).

**1. Info**

You have enough nourishment for now. Save food for later unless you need an herb's other effect. Full describes your food reserve, not your Health or Voice.

Trigger: The corresponding public status-pane state first becomes active with its owning ability or action requirements satisfied: full.

Sources: `src/sdl/ui/sdl-panes.c`, `src/tutorial/tutorial-game.c`, `src/player/player-song-duels.c`, `src/cmd/combat/cmd-combat.c`.

## Singing temporarily blocked

`status.song_lockout_timer`

Level: **Extended**.

Priority: **33** (higher appears first).

**1. Info**

A song duel has temporarily prevented starting another song. Wait for the displayed lockout to expire through normal game time. A Voice potion restores Voice but does not remove this lockout.

Trigger: The corresponding public status-pane state first becomes active with its owning ability or action requirements satisfied: song_lockout_timer.

Sources: `src/sdl/ui/sdl-panes.c`, `src/tutorial/tutorial-game.c`, `src/player/player-song-duels.c`, `src/cmd/combat/cmd-combat.c`.

## Losing ground in a song duel

`status.song_contest_player_stacks`

Level: **Extended**.

Priority: **84** (higher appears first).

**1. Info**

Your opponent is gaining ground in the song duel. Losing can drain an attribute and briefly prevent singing. Check the current duel state and consider ending the song before the contest is lost.

Trigger: The corresponding public status-pane state first becomes active with its owning ability or action requirements satisfied: song_contest_player_stacks.

Sources: `src/sdl/ui/sdl-panes.c`, `src/tutorial/tutorial-game.c`, `src/player/player-song-duels.c`, `src/cmd/combat/cmd-combat.c`.

## Climbing

`status.climbing`

Level: **Extended**.

Priority: **33** (higher appears first).

**1. Info**

You are climbing. Completing the climb spends time, and enemies can act. Check the destination and nearby threats when you regain control.

Trigger: The corresponding public status-pane state first becomes active with its owning ability or action requirements satisfied: climbing.

Sources: `src/sdl/ui/sdl-panes.c`, `src/tutorial/tutorial-game.c`, `src/player/player-song-duels.c`, `src/cmd/combat/cmd-combat.c`.

## Leaping

`status.leaping`

Level: **Extended**.

Priority: **33** (higher appears first).

**1. Info**

You are in mid-leap. The landing is part of the committed action. An obstructed landing can leave you on the crossed square, so check the terrain and enemies after the leap resolves.

Trigger: The corresponding public status-pane state first becomes active with its owning ability or action requirements satisfied: leaping.

Sources: `src/sdl/ui/sdl-panes.c`, `src/tutorial/tutorial-game.c`, `src/player/player-song-duels.c`, `src/cmd/combat/cmd-combat.c`.

## Knocked back

`status.knocked_back`

Level: **Extended**.

Priority: **33** (higher appears first).

**1. Info**

You have been pushed to another square. Check the terrain underfoot and nearby enemies immediately; displacement can put you in a trap or other hazard.

Trigger: The corresponding public status-pane state first becomes active with its owning ability or action requirements satisfied: knocked_back.

Sources: `src/sdl/ui/sdl-panes.c`, `src/tutorial/tutorial-game.c`, `src/player/player-song-duels.c`, `src/cmd/combat/cmd-combat.c`.

## A turn to recover

`status.skip_next_turn`

Level: **Extended**.

Priority: **33** (higher appears first).

**1. Info**

Your last action or an effect requires a recovery turn. Smite is one example. You must wait for that recovery before choosing another ordinary action.

Trigger: The corresponding public status-pane state first becomes active with its owning ability or action requirements satisfied: skip_next_turn.

Sources: `src/sdl/ui/sdl-panes.c`, `src/tutorial/tutorial-game.c`, `src/player/player-song-duels.c`, `src/cmd/combat/cmd-combat.c`.

## Recovering from trance

`status.was_entranced`

Level: **Extended**.

Priority: **33** (higher appears first).

**1. Info**

The trance has just ended. You are briefly protected from being entranced again, giving you an opportunity to act. Choose a safe next step before the protection ends.

Trigger: The corresponding public status-pane state first becomes active with its owning ability or action requirements satisfied: was_entranced.

Sources: `src/sdl/ui/sdl-panes.c`, `src/tutorial/tutorial-game.c`, `src/player/player-song-duels.c`, `src/cmd/combat/cmd-combat.c`.

## Vengeance is primed

`status.vengeance`

Level: **Extended**.

Priority: **33** (higher appears first).

**1. Info**

Taking melee damage has prepared Vengeance: your next melee hit gains one extra damage die. Further damage does not add more dice to this bonus.

Trigger: The corresponding public status-pane state first becomes active with its owning ability or action requirements satisfied: vengeance.

Sources: `src/sdl/ui/sdl-panes.c`, `src/tutorial/tutorial-game.c`, `src/player/player-song-duels.c`, `src/cmd/combat/cmd-combat.c`.

## Focused Attack is ready

`status.focused`

Level: **Extended**.

Priority: **33** (higher appears first).

**1. Info**

Waiting has prepared Focused Attack. Your next attack can gain a bonus equal to half your Perception. Moving or taking another action can lose the opportunity.

Trigger: The corresponding public status-pane state first becomes active with its owning ability or action requirements satisfied: focused.

Sources: `src/sdl/ui/sdl-panes.c`, `src/tutorial/tutorial-game.c`, `src/player/player-song-duels.c`, `src/cmd/combat/cmd-combat.c`.

## Concentration is building

`status.concentration`

Level: **Extended**.

Priority: **33** (higher appears first).

**1. Info**

Repeated attacks against the same enemy build Concentration, adding up to half your Perception to Attack. Switching targets resets the sequence.

Trigger: The corresponding public status-pane state first becomes active with its owning ability or action requirements satisfied: concentration.

Sources: `src/sdl/ui/sdl-panes.c`, `src/tutorial/tutorial-game.c`, `src/player/player-song-duels.c`, `src/cmd/combat/cmd-combat.c`.

## Power Throw is ready

`status.power_throw`

Level: **Extended**.

Priority: **33** (higher appears first).

**1. Info**

Power Throw is ready. Throw a Harness spear or hand axe at an adjacent enemy while striking in melee. The two attacks roll separately; successful damage is combined before one Protection roll. Your melee weapon stays active.

Trigger: The corresponding public status-pane state first becomes active with its owning ability or action requirements satisfied: power_throw.

Sources: `src/sdl/ui/sdl-panes.c`, `src/tutorial/tutorial-game.c`, `src/player/player-song-duels.c`, `src/cmd/combat/cmd-combat.c`.

## You are singing

`status.singing`

Level: **Normal**.

Priority: **33** (higher appears first).

**1. Info**

You are singing the displayed song and, if shown, a minor theme. Songs spend Voice over time, and Voice cannot regenerate while you sing. Stop through the Songs menu when the effect is no longer useful.

Trigger: The corresponding public status-pane state first becomes active with its owning ability or action requirements satisfied: singing.

Sources: `src/sdl/ui/sdl-panes.c`, `src/tutorial/tutorial-game.c`, `src/player/player-song-duels.c`, `src/cmd/combat/cmd-combat.c`.

## In a pit

`status.in_pit`

Level: **Normal**.

Priority: **80** (higher appears first).

**1. Info**

You are in a pit, where your Attack and Evasion are halved. Moving out attempts a climb, which can fail and spend a turn. Check nearby enemies and your Health before trying.

Trigger: The corresponding public status-pane state first becomes active with its owning ability or action requirements satisfied: in_pit.

Sources: `src/sdl/ui/sdl-panes.c`, `src/tutorial/tutorial-game.c`, `src/player/player-song-duels.c`, `src/cmd/combat/cmd-combat.c`, `src/cmd/movement/cmd-run.c`.

## Caught in a web

`status.in_web`

Level: **Normal**.

Priority: **80** (higher appears first).

**1. Info**

You are caught in a web, where your Attack and Evasion are halved. Attempting to move struggles against the web and may take several turns. Leaping cannot free you from it.

Trigger: The corresponding public status-pane state first becomes active with its owning ability or action requirements satisfied: in_web.

Sources: `src/sdl/ui/sdl-panes.c`, `src/tutorial/tutorial-game.c`, `src/player/player-song-duels.c`, `src/cmd/combat/cmd-combat.c`, `src/cmd/movement/cmd-run.c`.

## Standing in sunlight

`status.sunlight`

Level: **Extended**.

Priority: **33** (higher appears first).

**1. Info**

You are standing in sunlight. It illuminates the square independently of your own light and can hinder light-sensitive enemies.

Trigger: The corresponding public status-pane state first becomes active with its owning ability or action requirements satisfied: sunlight.

Sources: `src/sdl/ui/sdl-panes.c`, `src/tutorial/tutorial-game.c`, `src/player/player-song-duels.c`, `src/cmd/combat/cmd-combat.c`.

## Cursed rolls

`status.cursed`

Level: **Normal**.

Priority: **84** (higher appears first).

**1. Info**

This curse makes you use the worse of two rolls in affected Attack, Evasion and skill contests. Check the source and any available remedy. A cursed item that resists removal is a separate kind of curse.

Trigger: The corresponding public status-pane state first becomes active with its owning ability or action requirements satisfied: cursed.

Sources: `src/sdl/ui/sdl-panes.c`, `src/tutorial/tutorial-game.c`, `src/player/player-song-duels.c`, `src/cmd/combat/cmd-combat.c`.

## Running

`status.running`

Level: **Normal**.

Priority: **33** (higher appears first).

**1. Info**

Running repeats movement until interrupted. Every step spends time, and enemies can act between steps. Inspect your route before starting another run.

Trigger: The corresponding public status-pane state first becomes active with its owning ability or action requirements satisfied: running.

Sources: `src/sdl/ui/sdl-panes.c`, `src/tutorial/tutorial-game.c`, `src/player/player-song-duels.c`, `src/cmd/combat/cmd-combat.c`.

## Smithing in progress

`status.smithing`

Level: **Extended**.

Priority: **33** (higher appears first).

**1. Info**

Smithing is underway. Work continues for the time shown by the accepted proposal. If interrupted, check the unfinished work and remaining costs before resuming.

Trigger: The corresponding public status-pane state first becomes active with its owning ability or action requirements satisfied: smithing.

Sources: `src/sdl/ui/sdl-panes.c`, `src/tutorial/tutorial-game.c`, `src/player/player-song-duels.c`, `src/cmd/combat/cmd-combat.c`.

## Fletching in progress

`status.fletching`

Level: **Extended**.

Priority: **33** (higher appears first).

**1. Info**

Arrow crafting is underway. Each completed arrow uses its normal time and materials. If interrupted, check how many arrows were finished before starting again.

Trigger: The corresponding public status-pane state first becomes active with its owning ability or action requirements satisfied: fletching.

Sources: `src/sdl/ui/sdl-panes.c`, `src/tutorial/tutorial-game.c`, `src/player/player-song-duels.c`, `src/cmd/combat/cmd-combat.c`.

## Resting

`status.resting`

Level: **Normal**.

Priority: **33** (higher appears first).

**1. Info**

Rest spends turns to recover. Poison, bleeding and starvation stop ordinary Health regeneration; singing stops Voice regeneration. Disease continues while resting and must be cured with Healing or Miruvor.

Trigger: The corresponding public status-pane state first becomes active with its owning ability or action requirements satisfied: resting.

Sources: `src/sdl/ui/sdl-panes.c`, `src/tutorial/tutorial-game.c`, `src/player/player-song-duels.c`, `src/cmd/combat/cmd-combat.c`.

## Repeating a command

`status.repeat`

Level: **Normal**.

Priority: **33** (higher appears first).

**1. Info**

Your last command is set to repeat. Each repetition spends its normal time, so enemies can act. Cancel it when the situation changes.

Trigger: The corresponding public status-pane state first becomes active with its owning ability or action requirements satisfied: repeat.

Sources: `src/sdl/ui/sdl-panes.c`, `src/tutorial/tutorial-game.c`, `src/player/player-song-duels.c`, `src/cmd/combat/cmd-combat.c`.

## A mortal wound

`status.mortal_wound`

Level: **Normal**.

Priority: **97** (higher appears first).

**1. Info**

Bleeding severity is above 100. The next damage tick will cost more than 20 Health, and ordinary Health regeneration is blocked. Healing herbs or potions and Miruvor halve bleeding; one use may leave substantial bleeding. Song of Staunching can stop it.

Trigger: The public status predicate becomes active: cut > 100.

Sources: `src/sdl/ui/sdl-panes.c`, `src/tutorial/tutorial-game.c`, `src/dungeon/dungeon-player.c`.

## On the run

`status.on_the_run`

Level: **Normal**.

Priority: **55** (higher appears first).

**1. Info**

You are escaping with a Silmaril. Check the main objective and your route to the surface. This is the escape phase of the expedition; automatic running is a separate movement command.

Trigger: The public status predicate becomes active: on_the_run.

Sources: `src/sdl/ui/sdl-panes.c`, `src/tutorial/tutorial-game.c`, `src/dungeon/dungeon-player.c`.

## Approach quietly

`combat.stealth`

Level: **Normal**.

Priority: **40** (higher appears first).

**1. Info**

{subject} has not become alert to you. Stealth mode makes you harder to notice but slows most actions. Distance and cover also help. Use it before approaching when staying unnoticed matters.

**2. Action**

Enter Stealth mode for this practice. You can turn it off later when speed matters more. Skip if you prefer a different approach.

Required action: `stealth`.

Trigger: After the first-creature introduction, a visible nonadjacent hostile is below alertness; no visible adjacent or alert foe makes the approach unsafe, and Stealth can be enabled.

Sources: `src/tutorial/tutorial-game.c`, `src/player/player-bonuses.c`.

## Shallow water

`world.water`

Level: **Normal**.

Priority: **45** (higher appears first).

**1. Info**

Moving into, through or out of water is slower and makes a splash. Each water square entered on foot has a small disease risk. Plan a dry route when you need speed or stealth. A successful leap avoids water contact, but click-to-travel wades.

Trigger: Known shallow water is on the current square or visibly adjacent; the observation expires when none remains nearby.

Sources: `src/tutorial/tutorial-game.c`, `src/cave/cave-water.c`, `src/player/effects.c`.

## Keep clear of lava

`world.lava`

Level: **Normal**.

Priority: **90** (higher appears first).

**1. Info**

Lava is deadly. Ground contact without enough effective fire resistance kills you immediately; resistance still leaves severe damage on entry and while you remain there. Leaping also causes heat damage. Inspect the square for your current risk before choosing any crossing.

Trigger: Known molten lava is on the current square or visibly adjacent; the observation expires when none remains nearby.

Sources: `src/tutorial/tutorial-game.c`, `src/cave/cave-lava.c`, `src/player/player-bonuses.c`.

## Fighting on ice

`world.ice`

Level: **Normal**.

Priority: **55** (higher appears first).

**1. Info**

While standing on ice, you have -2 Melee, -2 Archery and -2 Evasion. Movement takes its usual time. Dry ground gives you better footing in a fight. Fire melts ice into water; cold freezes water into ice.

Trigger: Known solid ice is on the current square or visibly adjacent; the observation expires when none remains nearby.

Sources: `src/tutorial/tutorial-game.c`, `src/cave/cave-water.c`, `src/player/player-bonuses.c`.

## Poisonous seep

`world.poison`

Level: **Normal**.

Priority: **80** (higher appears first).

**1. Info**

Entering poisonous seep on foot, or spending turns on it, adds poison. Leave it before stopping to heal or use an Antidote, or another dose can poison you again. Poison resistance reduces exposure; a successful leap avoids contact.

Trigger: Known poisonous seep is on the current square or visibly adjacent; the observation expires when none remains nearby.

Sources: `src/tutorial/tutorial-game.c`, `src/cave/cave-poison.c`, `src/player/player-bonuses.c`.

## Molten lava

`terrain.85`

Level: **Extended**.

Priority: **38** (higher appears first).

**1. Info**

Ground contact is immediately lethal unless you have positive effective fire resistance on that square. With one, two or three net resistance layers, lava deals 40, 30 or 24 Health per exposure. Armour does not reduce this damage. Fire caves and vulnerability can cancel resistance layers.

**2. Info**

Lava hurts on entry and on later turns spent in it. Leaping counts as one extra resistance layer in the air, so crossing still burns you. Inspect the destination damage preview; an interrupted leap can leave you in the lava.

Trigger: The known feature is on the player square or visibly adjacent; hidden terrain is not disclosed.

Sources: `src/tutorial/tutorial-game.c`, `lib/edit/terrain.txt`, `src/cave/cave-lava.c`, `src/player/player-bonuses.c`.

## Solid ice

`terrain.86`

Level: **Extended**.

Priority: **38** (higher appears first).

**1. Info**

Grounded heroes on ice receive -2 Melee, -2 Archery and -2 Evasion. Grounded monsters also lose 2 Attack and Evasion. Movement takes its usual time. Airborne creatures avoid the footing penalty.

**2. Info**

Fire melts ice into shallow water, and cold freezes water into ice. This includes elemental effects and hits with the corresponding weapon brands. Recheck the terrain after an elemental attack: water has different movement and disease risks.

Trigger: The known feature is on the player square or visibly adjacent; hidden terrain is not disclosed.

Sources: `src/tutorial/tutorial-game.c`, `lib/edit/terrain.txt`, `src/cave/cave-water.c`, `src/player/player-bonuses.c`, `src/melee/melee-util.c`.

## Poisonous seep

`terrain.87`

Level: **Extended**.

Priority: **38** (higher appears first).

**1. Info**

Contact with poisonous seep adds 6 poison severity before resistance and applicable Protection. Poison caves can increase exposure by reducing resistance. New doses occur on entry and on later turns spent in the seep, even while resting or using an item.

**2. Info**

Poison deals damage over time and prevents ordinary Health regeneration. Move to clean ground before using Antidote or Miruvor, or another exposure can poison you again. A successful airborne crossing avoids contact.

Trigger: The known feature is on the player square or visibly adjacent; hidden terrain is not disclosed.

Sources: `src/tutorial/tutorial-game.c`, `lib/edit/terrain.txt`, `src/cave/cave-poison.c`, `src/spell/spell-projection-effects.c`, `src/player/player-bonuses.c`.
