# Water, Lava, Poison, and Ice in Sil-More

Give the four terrains different tactical purposes. **Water should change movement and pursuit. Lava should reshape safe routes and reward displacement. Poison should make time spent in a position costly. Ice should make footing and defensive positioning matter.** These roles fit Sil-More's existing emphasis on stealth, contested movement, limited recovery, and choosing which fights to take.

The recommended first prototype uses shallow water, molten lava, poisonous ground, and solid ice. Deep water, thin ice, spreading clouds, and elaborate elemental combinations belong to later experiments. A terrain succeeds when crossing it, holding its edge, or making an enemy occupy it can each be a reasonable decision in different circumstances.

All Sil-More rules and numerical values proposed below are **design recommendations, not implemented or playtested behavior**. Comparisons distinguish documented game behavior from the design lessons drawn from it. The evidence covers six games and the local Sil-More working tree on branch `help`, based on commit `4205c767` with existing uncommitted changes, inspected on 9 September 2026. External development-branch sources are snapshots, not claims about every released edition.

## 1. Recommended identities

| Terrain | Main decision | Starting prototype | How the player can benefit |
|---|---|---|---|
| Shallow water | Cross quickly enough to escape, or preserve speed and quietness? | Movement touching water takes 150% normal movement energy; a splash reduces that action's Stealth score by 3; water does not retain a scent trail | Delay ground pursuers; interrupt scent tracking after breaking sight and hearing; choose a short crossing over a long detour |
| Molten lava | Spend health on a dangerous crossing, or control the safe approaches? | 12 raw fire damage per contact/exposure; existing resistance and applicable elemental protection reduce it; local light | Knock enemies into it; cross a narrow channel with protection; fight on a bank that limits surrounding attackers |
| Poisonous ground | How long is this position worth occupying? | Each exposure adds 4 raw poison through the existing player poison mitigation route; ordinary movement speed | Take a short route at a known cost; drive susceptible enemies from a choke point; gain safer ground through poison resistance |
| Solid ice | Is this exposed position worth the loss of footing? | Normal movement speed; grounded occupants have -2 Evasion; no routine random slipping | Fight from a dry edge against an enemy on ice; cross quickly while avoiding prolonged defence there |

These are starting constants for comparison, not final balance. The first three columns describe the proposed ordinary human-sized ground actor. Flight and explicit terrain adaptations should have named exceptions. Poison's current player/monster timing difference requires particular care, discussed below.

The most useful design criteria are: a distinct decision, understandable consequences before commitment, a way to use the terrain against enemies, useful counterplay without a mandatory equipment check, and compatibility with Sil-More's existing movement abilities. Visual identity matters because it lets the player recognize those decisions immediately.

## 2. Evidence from other games

| Game and evidence boundary | Documented mechanic | Design lesson for Sil-More |
|---|---|---|
| **Brogue / Brogue Community Edition**; original developer guidance and CE development source | Dangerous terrain affects exploration and movement safety. Lava emits light; the CE source also distinguishes ice and melting ice over water.[^1][^2][^3] | Terrain can organize an entire room. Warnings and safe routes are part of the mechanic. |
| **Dungeon Crawl Stone Soup 0.33.1**; tagged in-game descriptions | Shallow water hinders non-adapted creatures; deep water and lava have traversal restrictions. Toxic bog damages and poisons even poison-resistant creatures.[^4] | Define which bodies can use a terrain. Clearly distinguish a stronger named variant from an ordinary hazard. |
| **The Battle for Wesnoth**; project documentation | Movement cost and chance to be hit depend on terrain and unit movement type. Aquatic units can defend water much better than land units.[^5][^6] | Terrain can alter relative advantage without dealing damage. |
| **Divinity: Original Sin 2's engine**; Larian surface API | Water and blood can freeze, melt, or electrify; contamination creates poison; ignition affects oil and poison. Ground and cloud layers are distinct.[^7] | A few transformations can multiply tactical options, but each adds rules and presentation work. |
| **Into the Breach**; direct developer interviews, plus a community mechanics reference | Displacement can kill enemies in water. Developer interviews emphasize manipulable positions and readable choices; the mechanics guide documents ice breaking into water.[^8][^9][^10] | A hazard becomes a weapon when the player controls position. Map geometry must leave room for counterplay. |
| **Shattered Pixel Dungeon**; developer source snapshot | Grounded actors in water can lose Burning and Ooze; toxic gas damages non-immune occupants as its field updates.[^11][^12][^13] | Terrain can be a recovery resource. A persistent cloud creates a different decision from a contact surface. |

### Brogue: the room is part of the encounter

Brogue's developer guidance says levels can generally be explored without entering the major dangerous terrains. This makes risky crossings a tool for the player rather than an unavoidable admission fee. Its CE movement code refuses a normal voluntary move into known lethal lava under the relevant conditions; confusion and temporary protections are handled separately.[^1][^3]

The CE terrain definitions combine lava with light and include separate melting-ice states. Those are important examples of terrain conveying both opportunity and changing risk.[^2] The transferable lesson is to design a bank, bridge, escape route, and warning together. Importing only lava damage would capture little of that design.

### Dungeon Crawl: adaptation changes the map

In the explicitly checked 0.33.1 descriptions, non-swimmers wade slowly and can fumble melee attacks in shallow water, while suitable bodies avoid those penalties. Deep water limits access by size, swimming ability, or flight. Toxic bog is explicitly harsher than ordinary poison resistance would suggest.[^4]

For Sil-More, creature adaptation is a strong idea; occasional wasted attacks are a weaker fit. A deterministic movement cost is easier to assess during an escape. Ordinary poison tiles should also respect existing poison protection; resistance-piercing bog would need its own name, appearance, and explanation.

### Wesnoth: a dangerous tile need not deal damage

Wesnoth defines movement costs and defence separately for each movement type. Its documentation distinguishes defence, which changes the chance of being hit, from resistance, which changes damage after a hit. Water can therefore be excellent ground for an aquatic unit and poor ground for a land unit.[^5][^6]

This is the strongest precedent for the proposed Sil-More ice. A small, visible Evasion penalty makes the same enemy more dangerous depending on where the fight happens. There is no need for another damage-over-time effect. The implication is that banks and dry islands deserve as much attention as the hazardous tiles themselves.

### Divinity: interactions need a limited vocabulary

Larian's API documents explicit transformations rather than treating every surface as unrelated: freeze and melt, electrify, contaminate, and ignite. It also separates ground surfaces from clouds.[^7] This supports a small interaction vocabulary for Sil-More, beginning with water becoming ice and ice becoming water.

The recommendation is selective. Divinity's rule that poison ignites is a fantasy convention, not an obligation to make every poisonous material combustible. Sil-More should identify a volatile gas separately if it needs that behavior. A ground coating and a gas should also differ in how flight, contact, and dispersal affect them.

### Into the Breach: positioning gives hazards offensive value

Justin Ma describes weapons becoming more useful when players learn to use their displacement, including pushing an enemy into water. In another developer interview, Ma and Matthew Davis explain why unreadable danger saturation and positions that cannot be manipulated were problems during development.[^8][^9]

The community mechanics reference describes damaging ice until it becomes water. It is a supporting account of the original game's rules, not a verified inventory of every later update.[^10] Sil-More can borrow the broader principle confidently: terrain should reward controlling the enemy's destination. Instant environmental kills deserve much more caution in a game where the player has one persistent character.

### Shattered Pixel Dungeon: water can be something to seek

The checked Burning and Ooze implementations remove those effects when their actor is grounded in water, with explicit timing conditions. The toxic-gas implementation damages non-immune occupants during field evolution.[^11][^12][^13]

This suggests two distinct opportunities. Water can provide relief, creating a reason to move toward it, while gas creates pressure to leave an area. Sil-More should adopt such benefits only when the corresponding status exists and its timing is explained. It should not promise that water removes internal poison merely because it can wash away an external coating.

## 3. What Sil-More already contributes

The current feature definitions contain floor, chasms, doors, traps, rubble, forges, and stairs, but no dedicated water, lava, poison-surface, or ice feature IDs. Elemental names in style text are not proof of tile mechanics. See [terrain definitions](C:/dev/Sil-More/lib/edit/terrain.txt:53) and [feature constants](C:/dev/Sil-More/src/defines.h:1267).

Several existing systems make these additions promising:

- **Movement has offensive value.** Flanking grants an attack when stepping between adjacent positions; Controlled Retreat rewards a particular movement history; Sprinting depends on sustained movement; Leaping crosses selected hazards. Terrain can change which route supports a build without replacing those abilities. See [ability definitions](C:/dev/Sil-More/lib/edit/ability.txt:310).
- **Stealth includes both noise and scent.** Player processing updates the noise flow and scent trail. Scent is deposited across a small neighborhood, not just under the player's feet. Water can create a specifically Sil-More escape tool, but cutting the trail requires handling that spread. See [player processing](C:/dev/Sil-More/src/dungeon/dungeon-player.c:972) and [scent deposition](C:/dev/Sil-More/src/cave/cave-flow.c:308).
- **Poison already creates delayed cost.** The player has a 0–100 poison counter. Each relevant player update deals one fifth of the counter rounded up; its normal decay subtracts that amount. Poison also prevents natural health regeneration. See [counter bounds](C:/dev/Sil-More/src/player/effects.c:239), [damage](C:/dev/Sil-More/src/dungeon/dungeon-player.c:1003), [regeneration](C:/dev/Sil-More/src/dungeon/dungeon-player.c:1096), and [decay](C:/dev/Sil-More/src/dungeon/dungeon-player.c:1239).
- **Elemental caves already penalize resistances.** Big caves reduce fear and stun resistance, and fire, ice, and poison caves reduce the corresponding elemental resistance. Adding damaging tiles to those caves compounds their danger. See [environmental bonuses](C:/dev/Sil-More/src/player/player-bonuses.c:749).

The live elemental formula is especially important. With a positive effective resistance value, damage is `floor(raw_damage * 2 / (2 + resistance_stacks))`. One net resistance stack therefore reduces raw damage to approximately two thirds, not one half. A single net vulnerability doubles damage. Applicable elemental protection is then subtracted on the pure fire/poison routes. See [damage helper](C:/dev/Sil-More/src/spell/spell-damage.c:2436) and [poison application](C:/dev/Sil-More/src/spell/spell-damage.c:2649).

These facts favor modest, explicit terrain values. A tile balanced on an ordinary floor may be much more dangerous inside an elemental cave. Preview its effect at the destination, including the destination's cave penalties.

## 4. Water: movement, pursuit, and relief

### Recommended first behavior

Use **shallow water that everyone can wade through**. A grounded movement whose origin or destination is water costs 150% of its normal movement energy, applied once even if both squares are wet. Attacking or using an item while stationary retains its normal action cost. Charging the exit step as well avoids making the bank transition an unexplained exception.

Each such movement produces a splash: apply -3 to that action's effective Stealth score. This is an event penalty, not a permanent reduction while standing still. It works within the existing hearing system; it should not reveal the player to the whole level. There should be no additional armour-weight penalty in the first version, because equipment already affects build and movement choices.

Water does not retain fresh scent, and entering it clears scent on that water square. While the player is in water, do not stamp fresh scent onto nearby banks. Existing land tracks remain. A tracker may search the last known bank, hear splashes, or see the player; water does not erase its memory or guarantee escape. The surrounding scent stencil must not silently reconnect a trail across a narrow stream.

### The decision this creates

A stream makes escape harder in the immediate exchange but can help end pursuit later. A player who crosses in full view of a fast enemy may be caught. A player who breaks sight, follows a bend in the stream, and exits farther away may lose a scent tracker. That is a useful tension between short-term exposure and longer-term concealment.

Water also creates a defensible bank. A pursuing land monster loses movement time crossing it while the player attacks from dry ground. Flying and explicitly aquatic creatures should bypass the movement and splash penalties, which makes those enemies tactically distinct. Aquatic affinity need not make a creature stronger in every respect; simply moving normally is already an advantage.

### Good later additions

An optional second-stage benefit is one additional effective fire-resistance stack **only while physically wading**. It would create a choice between enduring the next fire attack in water and moving onto better escape ground. Display it as a terrain contribution to effective resistance; do not add an invisible lingering Wet timer. Extinguishing an eventual Burning status is also sensible if that status is introduced.

Deep water should wait. It introduces swimming, drowning, equipment burden, trapped actors, and reliable exit rules. Inventory rust, destroyed supplies, thirst, drinking, and fishing offer little to the first tactical prototype. A narrow, consequential stream provides more value than a large compulsory swimming section.

## 5. Lava: routes, displacement, and visible danger

### Recommended first behavior

Use **molten lava with a high, predictable health cost**. Start with 12 raw fire damage for one exposure, mitigated through the existing elemental rules and relevant protection. It should be dangerous enough that ordinary pathfinding avoids it, but have no special unconditional instant-death rule.

Before protection, that example becomes 12 damage with no net resistance, 8 with one net resistance stack, 6 with two, and 24 with one net vulnerability. These values demonstrate the current formula; they are not a recommendation that every character should be able to survive a crossing. A weakened character can still die from one contact.

Lava should emit local light, initially within two squares where walls do not block it. That exposes approaching creatures and affects stealth and existing darkness interactions. Do not add an invisible adjacent heat-damage aura. If a future volcanic vent damages nearby squares, show those squares and its timing as a separate hazard.

Contact harms an actor once on entry. Remaining there through another completed action causes another exposure. An entry action must not accidentally receive both its entry dose and a duplicate stationary dose. A forced entry also causes contact damage; an actual later re-entry is another exposure. Walking onto safe ground causes no extra exit dose. This is a proposed event contract that must be reconciled with the actor scheduler before implementation.

### The decision this creates

The useful shapes are channels, bends, banks, and interrupted pools. A one-square channel may offer an expensive shortcut away from a threatening group. A bridge offers safe movement but exposes a predictable approach. Lava beside a fight increases the value of Knock Back and makes the enemy's displacement attacks more threatening.

Enemies need to understand that choice. A susceptible monster should generally use a bridge, fight at the bank, or choose another approach. A resistant creature might cross when doing so is worthwhile. Flight avoids contact; ordinary fire resistance reduces damage but does not automatically grant permission to stand indefinitely in molten rock. Any genuinely lava-adapted creature needs an explicit trait.

### Necessary limits

Auto-travel and held movement should stop before known lava. A deliberate entry should show the expected damage range after destination-specific mitigation, with a stronger warning if it can kill. Refusing the move should spend no turn. A repeat input must not dismiss the warning and take multiple steps.

For the first version, terrain contact should damage actors without routinely destroying their carried equipment. Floor drops from a lava death should relocate to an eligible nearby safe square; essential quest objects must always have a recoverable fallback. Lava should be a combat resource, not a reason to lose the run's objective through an unrelated drop rule.

## 6. Poison: a position with an accumulating cost

### Recommended first behavior

Name the first tile **poisonous seep** or **poisonous ground**. Treat it as a thin contact surface, not waist-deep sludge and not an airborne cloud. It has ordinary movement speed and adds 4 raw poison per exposure through the player poison-resistance and applicable protection calculation. It does not also deal a separate immediate terrain hit.

Use the same entry-versus-staying distinction as lava. Taking a dose now has consequences after leaving because it feeds the existing counter. Do not clear internal poison by moving into clean water. Recovery comes through the existing poison decay and appropriate cures.

For an isolated unmitigated dose of 4, starting at zero and assuming no other effects, the existing decay produces four updates of 1 damage. A dose of 12 produces damage of 3, 2, 2, 1, 1, 1, 1, 1: twelve damage across eight updates. In both examples regeneration remains suppressed while poison is present. At the counter cap or with additional doses and cures, the arithmetic changes.

### The decision this creates

Poison makes a good fighting position temporary. A doorway is usually useful because it limits the number of attackers. A poisoned doorway asks whether avoiding surrounding enemies is worth accumulating damage and delaying recovery. A small seep between two routes can be worth crossing to escape, while standing in the middle to trade blows is expensive.

Poison resistance gives access to more usable positions. It should reduce ordinary seep exposure using the existing rules. Do not make basic green tiles ignore resistance just to keep them threatening. More severe terrain can exist later, but it should announce its exceptional rule as clearly as DCSS's toxic bog does.[^4]

### Monsters and clouds need explicit decisions

The current monster projection route treats poison differently: poison-resistant monsters take zero poison damage, and ordinary poison uses the monster damage route rather than the player's delayed counter. See [monster poison handling](C:/dev/Sil-More/src/spell/spell-projection-effects.c:773). Therefore, adding player poison terrain does not automatically create equivalent delayed poison on monsters.

For the first implementation, preserve this existing asymmetry deliberately: susceptible monsters receive direct poison damage per exposure, resistant monsters receive none, and player doses become delayed poison. Monster recall and terrain inspection should explain the immunity. Tune against the fact that enemies pay immediately while the player can use a cure. A shared delayed monster-poison system would be a larger follow-up, with persistent-state and balance implications.

Poison clouds should be a later overlay with finite lifetime and visible spread. Flight should bypass a contact seep but not automatically bypass an airborne cloud. A poisonous dragon leaving a short-lived cloud could force movement; permanently painting every breath footprint green would risk exhausting all usable space. Ordinary poison need not ignite. Reserve explosions for a visibly distinct volatile gas.

## 7. Ice: footing and exposure

### Recommended first behavior

Use **solid ice over supporting ground**, with -2 Evasion for grounded occupants and normal movement speed. Apply the modifier to player and susceptible monster defence, and show it in combat explanations. It should affect the actual defended attack, not merely the displayed character total.

Walking onto ice is deterministic. There is no generic chance to fall, lose an entire turn, drift into lava, or move in a random direction. A nominally small slipping chance becomes frequent over a long journey: at an independent 10% per step, ten steps give about a 65% chance of at least one slip. That is a major repeated control penalty, not a rare flourish.

Keep Flanking, Controlled Retreat, and ordinary attacks working. The exposed destination is the cost of using them on ice. Do not simultaneously slow movement, reduce accuracy, disable abilities, and add cold damage. Such stacking would turn ice into a list of reasons to avoid participating in the room.

### The decision this creates

The player wants to keep a dry defensive square while making a foe stand on ice. Knock Back can help establish that arrangement. An ice-covered approach can be a fast escape route because it does not slow movement, but the player is easier to hit if intercepted there. An ice-adapted monster reverses the apparent advantage.

The Evasion penalty should remain -2 unless a specific footing adaptation removes it. Cold resistance protects against cold damage; it does not give traction. Likewise, fire resistance does not make a character immune to being pushed. Separating physical footing from elemental protection keeps the rules understandable.

### Better advanced versions

Thin ice is a separate future terrain with visible integrity: intact, cracked, broken. Show the supporting water and what breaking it will do. A crossing that breaks under an enemy or a heavy blow can be interesting if the state is legible and the player has a response. It should not be a hidden random collapse on the only exit.

Directional sliding could be an optional special action or rare terrain variant, with the endpoint previewed. Ordinary movement should not acquire involuntary sliding merely to make ice look active. For the first release, an Evasion modifier and good encounter geometry are enough.

## 8. Elemental interactions worth adding

Start with four stable terrains, then add the following small set only when actors can deliberately cause the relevant effects. These are proposed Sil-More rules, not a transcription of another game's interaction table.

| Cause | Target | Recommended result | Scope |
|---|---|---|---|
| Explicit cold effect | Shallow water | Solid ice; suspend water's movement and splash effects | First transformation experiment |
| Explicit fire effect | Ice | Restore shallow water if it covered water; restore floor if it covered dry ground | First transformation experiment; preserve the underlying material |
| Wading in water | Incoming fire | Optional extra effective fire-resistance stack while grounded there | Add only after water's pursuit role is assessed |
| Water | Internal poison counter | No cure | Baseline rule |
| Fire | Ordinary poison seep | No explosion | Baseline rule; reserve combustion for a named volatile material |
| Strong deliberate cooling | Lava | Potentially a small, visibly stable crust or bridge | Later; requires limits, lifetime rules, and connectivity checks |
| Poison breath | Dry floor | A small temporary cloud or patch | Later; cap area and duration |
| Cold effect | Thin ice | Repair visible integrity | Later, alongside the complete thin-ice rules |

Avoid automatic surface creation from every branded melee hit. That would turn an equipment property into a large, largely unavoidable terrain-generation system. Explicit projections, consumables, or special attacks are easier to predict and price. If a cold attack freezes water, it should target a bounded area rather than propagate through an entire connected lake.

Persistence also needs a clear rule. A prototype can keep transformed shallow water frozen until melted explicitly. A timed variant must display its remaining safe duration and provide a visible melting state. Never make an apparently safe route disappear silently while the player is crossing it.

## 9. Encounters that justify the tiles

| Encounter | Layout and enemies | Intended decisions |
|---|---|---|
| **The flooded bend** | A shallow stream curves behind a wall; a scent-tracking pursuer follows | Accept slower, louder steps to interrupt tracking after breaking sight; choose where to leave the stream |
| **The forge channel** | A narrow lava channel has a bridge and dry banks; enemies can approach both ends | Hold the bridge, spend protection on a shortcut, or maneuver for Knock Back |
| **The poisoned doorway** | A short seep occupies a strong doorway; a second entrance offers worse geometry | Pay poison to limit attackers, retreat before the counter grows, or accept a more open fight |
| **The frozen approach** | An ice lane leads past several dry islands; a melee enemy advances | Preserve a dry defensive square, concede it to escape, or push the foe onto ice |
| **The divided hunting ground** | Water divides a chamber with both flying and ground enemies | The water slows one pursuer but gives no safety from the other; prioritize accordingly |
| **The contested cache** | Optional treasure sits beyond a short hazard crossing, with a recoverable return route | Spend a consumable or health now, postpone the attempt, or leave the reward |

Each encounter should be useful with only one terrain before combining several. Begin with one main terrain and, at most, one supporting interaction in a room. Broad fields of permanently bad ground produce repeated avoidance rather than changing tactics.

Primary routes between arrival points, stairs, and required objectives need a survivable path without a particular resistance item or movement skill. Optional treasure can justify a harder requirement, provided that both the cost and the return route are visible. Terrain also needs to remain fair during the escape portion of the game, not just on descent.

Do not solve hazard placement with a universal tile-density percentage alone. A one-tile patch in a doorway can matter more than a lake in an unused corner. Validate chokepoints, line of sight, diagonal movement, dry fighting positions, and alternatives. The number of consequential choices is more useful than the number of colored squares.

## 10. Implementation implications

This is a system change rather than four entries in a tile atlas. The following routes need shared rules:

| Route | Why it matters | Current starting point |
|---|---|---|
| Player movement and Leaping | Cost, contact, run interruption, takeoff and landing safety | [cmd-run.c](C:/dev/Sil-More/src/cmd/movement/cmd-run.c:33) |
| Forced movement and exchanges | Hazards must apply when walking is bypassed; displacement must not create free movement attacks | [Knock Back](C:/dev/Sil-More/src/cmd/combat/cmd-combat.c:2367), [exchange consequences](C:/dev/Sil-More/src/melee/melee-movement-resolution.c:228) |
| Monster placement and path selection | A legal square may still be damaging or tactically poor | [monster terrain legality](C:/dev/Sil-More/src/melee/melee-util.c:32) |
| Poison and periodic effects | Avoid duplicate entry ticks; preserve and explain the existing update cadence | [player damage processing](C:/dev/Sil-More/src/dungeon/dungeon-player.c:1001) |
| Noise and scent | Water must affect actual detection and pursuit | [scent update](C:/dev/Sil-More/src/cave/cave-flow.c:308) |
| Terrain transformation and lighting | Passability, projections, visibility, remembered terrain, and darkness resistance must agree | [cave_set_feat](C:/dev/Sil-More/src/cave/cave-flow.c:733) |
| Generation, spawning, and drops | Required paths and essential objects must remain usable | [empty-floor predicates](C:/dev/Sil-More/src/defines.h:3598) and level-generation modules |
| Saves and terrain data | Stable feature IDs, stored underlying terrain, and any timers must survive loading | [save terrain](C:/dev/Sil-More/src/fs/save-dungeon.c:131), [load terrain](C:/dev/Sil-More/src/fs/load-dungeon.c:280) |

One particularly important distinction is **can occupy**, **can traverse safely**, and **wants to traverse**. The current monster helper allows most non-wall squares after special cases. New passable lava would otherwise become a legal ordinary destination without good hazard reasoning. Conversely, making lava a wall would incorrectly affect line of sight and projectiles.

Use the existing actor scheduling convention deliberately. Player poison presently advances in the completed player-action processing route; it is not safe to describe it as a new universal real-time clock. Verify haste, slow movement, stationary actions, forced entries, paralysis, and entering a hazard during another actor's action. Changing poison cadence globally would be a separate balance change.

Existing floor predicates often test specific IDs or wall flags. Appending feature IDs preserves historical numbers but does not automatically make them work with summoning, floor drops, escape routes, or targeting. The current template ends at feature 83; choose subsequent IDs only after auditing parser limits and classification assumptions. New persistent timers or substrate fields also require explicit save compatibility handling.

## 11. Presentation and testing

Terrain inspection should state the effective rule for the current actor. Examples:

- **Shallow water:** Movement takes 50% longer. Moving through it gives -3 Stealth for that action. Water does not retain scent.
- **Molten lava:** 12 raw fire damage on contact and on each further action spent here. Show the current actor's resulting damage range separately.
- **Poisonous seep:** Exposure adds poison after resistance and protection. Poison continues to harm you after leaving and prevents natural health recovery.
- **Ice:** -2 Evasion while grounded here. Movement speed is unchanged.

Hazards need distinct shapes or textures as well as colors. An actor standing on a hazard must not completely conceal it. Distinguish remembered terrain from visible terrain when a transformation may have occurred. Damage messages and the combat display should attribute the terrain contribution clearly.

Start with four hand-built encounter rooms, then combine them with procedural generation. Compare the same encounter layouts across a stealth character, a light-armour movement character, and a heavy-armour character, with and without one relevant resistance. Include ordinary and elemental-cave contexts; the latter can change the result sharply.

The important measurements are how often players deliberately enter a terrain, exploit it against an enemy, change a route because of it, or misunderstand its consequences. Track health spent, extra movement energy, recovery delay, and unavoidable damage near required paths. These observations can distinguish a useful tradeoff from a compulsory tax.

For functional verification, include walking, held input, controller and touch routes, Leaping, Knock Back, exchanges, monster immunity, floor drops, and save/load while on a hazard. Specifically test that one entry does not charge twice and that a newly created surface beneath an actor has a defined first exposure. These are proposed checks; no runtime or hardware verification has been performed for this study.

## 12. Priority and confidence

**First prototype:** the four identities and starting rules, actor-aware pathing, inspection, deliberate-entry warnings, and four useful encounter shapes. Water's scent behavior is part of its intended value, so it needs real pursuit verification. Poison can preserve the existing documented player/monster asymmetry initially.

**Next:** bounded water-to-ice and ice-to-water transformations, optional water fire protection, and a small number of explicit terrain adaptations. Add one interaction at a time and verify that it creates an additional choice.

**Later:** deep water, thin ice, temporary clouds, controllable lava crusts, or intentional sliding. These are distinct design features with their own UI and balance requirements.

Confidence is high that the four proposed roles are distinct and fit the inspected systems. Confidence is moderate that all four should ship together; lava and poison need the most careful assessment against cave vulnerability and recovery. Confidence in the numerical constants is low until encounter testing. The evidence supports the design patterns, not a claim that these particular values are balanced.

## Sources

All links were consulted on 9 September 2026. Tagged source is version-specific. Links to `master` describe the inspected development snapshot and can change. Interview evidence describes design intent at publication; community references are identified separately.

[^1]: Brian Walker. [Brogue official site and gameplay guidance](https://sites.google.com/site/broguegame/). Undated rolling page with historical release notes. Used for exploration routes, dangerous terrain, and interface expectations.

[^2]: Brogue Community Edition contributors. [Globals.c: terrain and feature definitions](https://raw.githubusercontent.com/tmewett/BrogueCE/master/src/brogue/Globals.c). Development-source snapshot. Used for lava light, ice, and melting-ice states.

[^3]: Brogue Community Edition contributors. [Movement.c: movement safety checks](https://raw.githubusercontent.com/tmewett/BrogueCE/master/src/brogue/Movement.c). Development-source snapshot. Used for refusal of ordinary voluntary movement into known lethal lava under the specified conditions.

[^4]: Dungeon Crawl Stone Soup contributors. [In-game feature descriptions, version 0.33.1](https://raw.githubusercontent.com/crawl/crawl/0.33.1/crawl-ref/source/dat/descript/features.txt). Tagged release source. Used for shallow/deep water, lava, and toxic bog; not presented as the latest release.

[^5]: The Battle for Wesnoth project. [UnitsWML](https://wiki.wesnoth.org/UnitsWML). Living project documentation, last edited 2 August 2026 in the inspected page. Used for movement types, terrain movement costs, and defence definitions.

[^6]: The Battle for Wesnoth project wiki contributors. [Defense and resistance](https://wiki.wesnoth.org/Defense_and_resistance). Undated living explanatory page. Used for the difference between avoidance and mitigation, including aquatic defence examples.

[^7]: Larian Studios. [Divinity Engine Wiki: Osiris/API/TransformSurface](https://docs.larian.game/Osiris/API/TransformSurface). Undated engine documentation. Used for named transformations and separate ground/cloud layers.

[^8]: Steven Messner, interviewing Justin Ma and Matthew Davis. [Everything you need to know about Into the Breach](https://www.pcgamer.com/into-the-breach-preview/). PC Gamer, displayed publication 11 February 2018; article notes an earlier September 2017 publication. Used for direct developer explanation of displacement into water and multipurpose weapons.

[^9]: Alex Wiltshire, interviewing Justin Ma and Matthew Davis. [Reimagining failure in strategy game design in Into the Breach](https://www.gamedeveloper.com/design/reimagining-failure-in-strategy-game-design-in-i-into-the-breach-i-). Game Developer / formerly Gamasutra, 28 February 2018. Used for direct developer testimony about readable choices, controllable geometry, and situations with no counterplay.

[^10]: GameFAQs community strategy guide. [Into the Breach: Game Mechanics](https://gamefaqs.gamespot.com/pc/205477-into-the-breach/faqs/76363/game-mechanics). Original-era guide, indexed as published in 2018; consulted through accessible indexed content. Secondary supporting reference for ice damage and conversion to water; not treated as an exhaustive current-version authority.

[^11]: Evan Debenham / Shattered Pixel Dungeon contributors. [Burning.java](https://raw.githubusercontent.com/00-Evan/shattered-pixel-dungeon/master/core/src/main/java/com/shatteredpixel/shatteredpixeldungeon/actors/buffs/Burning.java). Development-source snapshot. Used for grounded water contact and Burning removal with its timing conditions.

[^12]: Evan Debenham / Shattered Pixel Dungeon contributors. [Ooze.java](https://raw.githubusercontent.com/00-Evan/shattered-pixel-dungeon/master/core/src/main/java/com/shatteredpixel/shatteredpixeldungeon/actors/buffs/Ooze.java). Development-source snapshot. Used for water cleansing an external damaging coating.

[^13]: Evan Debenham / Shattered Pixel Dungeon contributors. [ToxicGas.java](https://raw.githubusercontent.com/00-Evan/shattered-pixel-dungeon/master/core/src/main/java/com/shatteredpixel/shatteredpixeldungeon/actors/blobs/ToxicGas.java). Development-source snapshot. Used for recurring gas-field damage and immunity checks.

Local implementation findings are linked at their point of use. Those are evidence for current Sil-More behavior; proposed mechanics and numerical constants remain original recommendations requiring implementation and playtesting.
