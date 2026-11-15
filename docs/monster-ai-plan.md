# Monster AI implementation plan

## Physical sight and stealth recognition are separate (2026-11-15)

**Shooters firing from darkness are intended gameplay.** The
`visual_recognition` setting controls the light-dependent visual bonus to a
monster's perception-versus-stealth check. Failing to receive that bonus does
not blind the monster. Disguise's reduction of that bonus also does not blind
an already-alert enemy. This supersedes the earlier recognition-limited combat
range described in historical audit notes below.

Use `monster_has_visual_perception_bonus()` only for that stealth-roll bonus
and its stealth-vision indicators. Use `monster_has_sight()` for physical LOS,
MAX_SIGHT and short sight, and `monster_ai_can_see_player()` for an alerted
enemy's current combat target. Unwary monsters must still pass detection before
physical LOS can supply a combat target; successfully heard sounds and scent
remain separate investigation evidence.

No light/recognition gate belongs in attack legality, combat spacing, retreat,
sensory pursuit, squad firing posts, command authority, reactions, or support
targeting. An alerted orc archer in an unlit room must take a legal retreat and
shoot when it has a clear lane. Walls, doors, distance, short sight, alertness,
status restrictions and projectile-blocking actors still matter. Tests cover
all named templates' legal attack masks with the setting on/off, the actual
stealth-roll bonus, failed detection, and headless dark combat encounters.

Archer post previews also obey actual minimum/maximum missile ranges. A clear
lane closer than the preferred spacing is usable. When an allied melee screen
blocks the bow, the local search can expand with its combat spacing, up to a
17-by-17 area of personally visible cells and sixteen paid steps. Ordinary
movement retains its 9-by-9/two-step bounds. This prevents archers circling a
blocked preferred-distance ring when the open lane lies outside that ring.

Dry missile-position routes cannot enter deep water, where mandatory escape
would cancel the route and return the archer to the same shore. A dry archer
with no usable retreat can fight in melee. Native bow retreat uses the same
fallback for ordinary/unique and visible/hidden actors; the player's ability
to see the monster does not change that choice. Shallow water, flying routes,
normal pursuit crossings and escape from water already occupied remain legal.

## Additional compound gameplay coverage (2026-11-15)

The follow-up headless audit adds 765 accepted scenario cases: 264 concealed or
recovering catalogue encounters, 264 mixed-party terrain/song/orientation cases,
48 sensory cases, 39 action-clock/knowledge-expiry cases, 64 demonstrated or
unused technique cases, 16 otherwise identical ability-absence controls, 64
distinct test-generated mazes and six active-crowd cases. All player actions use
the real SDL queue; observations and technique diagnostics are read-only.

The maze controller replans from the character's position, its declared starting
map and currently visible terrain, and answers visible native prompts. Active
groups of 40–240 monsters exercise the sixteen-squad/eight-member bounds. The
mass-loss case preserves actor identities through compaction and permits the
legitimate rout after twenty commanders die. Twelve speed families show the
expected haste/base/slow action ordering. Loud physical noises are investigated
without revealing the hidden player, and unused abilities leave enemy actions
unchanged in sixteen paired comparisons.

Entry points are `check_monster_ai_stress_gameplay.py`,
`check_monster_ai_reactions_gameplay.py`, `check_monster_ai_maze_gameplay.py` and
`check_monster_ai_capacity_gameplay.py` under `scripts/`. The reactions driver
accepts completed control reports through `--reference-report` for its optional
`--negative-equivalence` comparison. Exact traces, source/object fingerprints,
corrected fixture controls and the final audit are under
`scripts/output/monster-ai-round9/`. This expansion changes the test harness,
without changing production AI statistics, rules or save data.

## Blocked firing-lane detours (2026-11-15)

A personally recognized player behind a melee screen can require more than the
ordinary two-action tactical lookahead. Missile actors with a blocked ray now
search up to eight steps within the existing actor-centred 9-by-9 area for a
usable firing post. A paid route to that post can take precedence over repeated
small improvements among blocked squares. Assembly and regroup plans retain
their existing behavior; route legality, poison accounting and reaction costs
still apply. This adds no monster statistics, attack frequency or save fields.

The same 38-turn SDL-input encounter changed from zero to 17 arrows for the
previously blocked archer. Twelve orientation/seed variants pass, and the real
engine regression checks the movement route and read-only preview behavior.
The broader audit covers the combat catalogue, long mixed encounters, native
terrain prompts, save/reload and logging equivalence. Evidence and exact inputs
are retained under `scripts/output/monster-ai-round8/`; the reusable entry point
is `scripts/check_monster_ai_audit_gameplay.py`.

## Deep-water archery corrections (2026-11-13)

An archer with no adjacent dry square can now follow a bounded local route
through deep water to a bank. It pays the existing swimming movement cost;
equal-length exits favor its established combat distance using its own sensory
anchor. Ordinary melee crossing retains its forward pursuit.

Squad, tactical and native archer retreat positions now reject deep water as a
grounded firing post. This also prevents returning to water immediately after
escaping when the opposite bank is harmful or warned. A cornered archer can
resume melee. Shallow-water shooting and flight retain their existing rules.

Headless SDL coverage includes every combat-capable template, old support and
spell behaviors, water widths/orientations, unavailable exits, dangerous banks,
confusion, Challenge, flooding with a protected dry route, and live save/load.
The chosen melee blow/effect and ordinary/reaction/Smite choice are available in
DEBUG diagnostics without consuming RNG. Evidence is under
`scripts/output/monster-ai-round7/`; no persistent format or combat-stat change.

## Coordinated archer corrections (2026-11-13)

Headless gameplay exposed three additional movement failures. Aggressive enemies
affected by Challenge now advance for melee instead of retaining firing, guard,
or assembly jobs while the song suppresses their ranged actions. A resisted
mana-drain check also retains a melee approach; ending the song restores ordinary
ranged spacing. The song's existing morale, resistance and mana rules are retained.

Missile posts require the archer's physical sight range as well as a clear ray.
Members receive their commander's shared sighting even when no final post is
reachable within the bounded search, allowing an ordinary approach. Ranged actors
also no longer collect melee surrounding/Flanking rewards that previously kept
them in an adjacent ring despite an available firing position.

Regressions cover the production scoring and range decisions and real SDL
GameControl replays: strong/weak Challenge, voluntary stopping and Voice
exhaustion, mixed morale, doorways, crowded squads, two captains, cross-kin
authority, withdrawal cover and moving targets.

Further varied encounters reproduced a warning/positioning loop: combat movement
could return an archer to announced lava, then world behavior immediately moved
it back out. Shared terrain scoring now rejects announced lava, chasm, deep water,
rubble and acid where they are harmful, using the escape rule's existing flight
and immunity distinctions. Route costs enforce this too, so ordinary pursuit can
take a safe detour. The virtual player endpoint remains an attack. Safe warnings
and ordinary shallow-water movement are retained. The same 30-turn corridor
replay changes from five hazard re-entries and ten wasted moves to zero
re-entries/moves, with 17 versus 22 arrows (each run also has one rally).

## Commander and song lifecycle corrections (2026-11-13)

Further lifecycle tests found two related gaps. A temporary commander's death
must cancel its orders while retaining each survivor's own spent regroup/assembly
budget, so reacquiring leadership cannot restart the same retreat condition.
Disguise pacification, recognition history and same-turn attacker flags, together
with Revealing hints, now follow their creature during monster-list compaction.
Removed/reused slots are cleared even when Disguise buffers have not been
initialized. These are runtime bookkeeping corrections with no save-format or
combat-stat changes.

## Dragon scent within a lair (2026-11-13)

Territorial dragons may now follow an acquired scent, including the existing
bounded search after that trail ends, within six squares of their saved home.
This reuses the territorial world-work radius. Existing vault membership and
partition boundaries also apply. A sound, shared sighting, or old visual target
alone does not grant this exception. Other territorial races retain their rule;
ordinary wolf/cat tracking and visible combat keep their existing behavior.
The home coordinates already persist in monster routines, so no save format
change is needed.

## Commander and soldier coordination (2026-09-25)

`monster_squad_prepare()` in `src/melee/melee-movement-tactics.c` now makes one
shared plan before the first eligible action of each `process_monsters()` pass;
empty scheduler polls do not consume planning budgets. Leadership is authored in
`lib/edit/monster.txt` as `L:grade:doctrine:kin[:additional command kin]`.
Rally and Escort no longer determine eligibility. There are 105 explicit
profiles, including 50 non-unique types capable of leading a compatible group.
Morgoth retains his separate behavior.

| Grade | Selection and behavior | Examples |
|---|---|---|
| NONE | May follow a compatible leader, cannot lead | Spider hatchlings, phantoms |
| ORDINARY | Elected when no higher grade is present; separate approach/firing positions | Orc soldiers, warriors, archers; ordinary trolls |
| LEADER | Preferred over ordinary members; can assign a flanker, ranged guard or withdrawal cover | Orc champions, Easterling warriors/spies, troll guards |
| COMMANDER | Highest grade; can briefly assemble approaching melee soldiers or regroup a battered squad | Orc captains, Fankil, Gothmog |

Within a grade, a viable incumbent outranks every newcomer. Level breaks ties
only when electing a new leader. Existing members are retained before recruiting
new allies. Death, incapacity, loss of contact/sight, or a higher-grade arrival
can change leadership; current HP and small score changes cannot reshuffle it.

- Each commander coordinates up to eight members, including itself, within six
  squares and line of sight. Short-sighted members require contact within two.
  At most sixteen squads are planned per pass.
- Command kin are explicit, independent of incidental shared wandering bands.
  Dragons can command dragons, never trolls by virtue of level. Extra authority
  is permitted only for tactical commanders: Gothmog and Turkano may direct
  orcs/trolls alongside raukar; Fankil may direct raukar alongside Men. Hostile
  relations and explicit conflicting allegiances still prevent recruitment.
- PACK profiles (wolves/werewolves and spiders) use pursuit and surrounding,
  without soldier guard assignments or assembly plans. Ordinary wolves can
  elect an alpha despite lacking SMART; mindless creatures remain excluded.
- Experienced tactical leaders assign at most one guard, prioritizing cover
  for a visible wounded fleeing ally over a ranged guard, and one flanker when
  the available soldiers allow it. Roles influence the production movement score.
- Commanders can assemble melee soldiers spread by at least three squares,
  while the nearest is still at least four squares from the player, for up to
  two player turns. Archers and the commander are excluded from this spread
  calculation. If at least half the squad is at half health or less, the
  commander may order up to three turns of retreat toward a more distant,
  cohesive position. A persistent condition cannot renew its own wait budget.
- The commander must currently recognize the player. Soldiers can follow its
  shared sighting without personal sight, but orders stop when contact, sight,
  alertness, or morale fails. Confusion, stun, skipped/recovery turns, social
  disputes, Song of Disguise and the truce exclude coordination.
- A bounded search of observed, empty, legal routes assigns separate positions.
  Ranged positions are assigned first; melee soldiers favor contact and opposite
  sides while avoiding the reserved firing corridors. Existing useful positions
  and unchanged orders receive modest preference to reduce reshuffling.
- Orders influence the existing tactical movement score; its poison budgets,
  reaction costs, terrain rules and final movement legality remain authoritative.
  Archers hold a safe assigned position with a firing line. Spells and melee
  attacks still use the ordinary action scheduler and costs.
- `monster_type.squad` is runtime-only. Plans are reconstructed after loading;
  deletion clears a commander's orders and compaction repairs its index. No new
  save record, monster flag, race ID, combat stats, or generation rule is introduced.
  The race-template record has new command metadata; its raw cache is regenerated
  automatically when its size changes. Invalid/duplicate leadership records,
  unknown tokens, and unauthorized cross-kin declarations are rejected.
- Visible soldiers may report following orders after a successful move, using
  the existing message cooldown and only when the commander is also visible.

Validation: focused assignment/contact/corridor/lifecycle tests, stable elections,
grade succession, cross-kin boundaries, pack behavior, guards, retreat and assembly
budgets; full Fankil and ordinary-orc scheduler fixtures including commander
death and champion succession; actual commander withdrawal through get/make/process
move; real template parsing, invalid records, raw-cache loading and old-size cache
regeneration. Standard Windows build, combat/learning suites, legacy monster-record
roundtrips and template-version checks pass. Encounter difficulty and visual feel
still require interactive playtesting.

## Earlier adaptive AI implementation record

Updated 2026-09-11. Implemented against branch `0.9.8`, including the monster, ability, and poison baseline now committed as `d67d8ded`.

Implementation is in `src/monster/monster-ai.c`, `monster-senses.c`, `monster-tactics.c`, the existing melee decision/movement modules, shared light/projection helpers, and the real player action and damage routes. Save version is now **0.9.8.6**. Morgoth keeps a separate copy of his previous tactical movement and his ordinary song-selection gate.

Automated validation covers learning, bounded sensory pursuit, movement and shadow tactics, casting/melee decisions, poison action order, real new-monster encounters, and versioned saves. The parallel Windows build passes with eight workers; the unrestricted default exhausted compiler memory during the full rebuild. Interactive encounter balance has not been playtested. The sections below retain the agreed design and validation requirements.

Completed checks: `check_monster_ai.ps1` (574), `check_monster_ai_combat.ps1` (74), `check_monster_abilities.ps1` (310), `check_monster_senses.ps1` (76), `check_monster_learning.ps1`, `check_monster_poison.py`, `check_monster_ai_integration.py`, `check_monster_scent_save.py`, `check_poison_terrain.py`, `check_ice_combat.py`, `check_lava.py`, template version validation, and scoped whitespace checks. The integration tests use isolated temporary data, not player saves.

## 1. Scope and retained decisions

Improve ordinary and unique monsters through terrain-aware movement, useful formations, deliberate ability use, and reactions to witnessed player tactics. Extend the existing hazard and tactical movement implementation rather than building a second movement system.

- Preserve current monster stats, damage, spell frequencies, mana recovery, IDs, GUIDs, allocation rules, and authored encounters. In particular, preserve Carcharoth's current 2d12 bite.
- Leave Morgoth's tactical profile, pursuit, phases, crown, and truce behavior alone. Shared, demonstrated correctness fixes remain in scope, with explicit Morgoth regressions.
- Intelligent monsters learn from their own observations and limited nearby warnings. They cannot inspect hidden player equipment, unshown abilities, exact HP, resistance values, or poison counters.
- Ordinary thinking creatures adapt modestly; SMART creatures use the full tactical evaluation. MINDLESS creatures retain basic pursuit and self-preservation without player-model learning.
- Serpents disregard allied collateral when choosing an otherwise legal area attack. This is a collateral policy, not a new MINDLESS flag: preserve their actual attacks, casting chances, costs, and self-preservation.
- Restore useful scent for existing wolves/hounds and cats; add dragons, including Lhamthanc. Do not add serpent scent.
- Shadow-emitting Balrogs should cover allies when the actual combined lighting effect helps. Include Dúron and other shadow emitters through the same capability.
- Counterplay must have costs: movement, positioning, exposure, mana, lost passive bonuses, or recovery. Avoid absolute shutdowns of player abilities.
- No new floor, forge, travel route, monster ability, or balance retuning is part of this AI implementation.

## 2. Current baseline to preserve

### Poison and pools already exist

Monster poison is pending damage, capped at 100. At the beginning of an eligible monster action, pool contact is applied first, then `ceil(poison / 5)` damage is dealt and removed from the counter. This runs before ordinary AI, sleeping, or skipped-action handling. Player pool contact and poison ticking occur within the completed-player-action route, so contact does not promise an extra full turn before damage.

Monster `RES_POIS` prevents new poison completely. Flight prevents pool contact, but does not grant immunity to poison brands or projections. Poison suppresses ordinary HP regeneration, including that of regenerating monsters.

- Monster poison brands add half of positive post-protection physical damage, rounded up. Bow and ammunition brands combine into one dose.
- Poison projections add delayed damage instead of immediate monster HP damage.
- Poison melee against the player uses its existing elemental bonus and post-protection damage route. Pure poison, breath, and pool exposure use the pure-poison resistance/protection route. Do not apply one resistance formula to every attack.
- A pool has raw contact dose 6. Player resistance, opposition, poison protection, and the destination poisonous-cave modifier affect actual exposure.
- A grounded monster entering from dry land receives entry exposure and another exposure at the beginning of its next action. Existing AI budgets 12 for that commitment and 6 for subsequent pool steps.
- Ordinary and mindless monsters already seek land; immune or flying creatures do not need to escape pool contact. Existing crossing logic avoids reversing a survivable crossing every turn.
- Pursuit, fleeing, and local tactical paths already include poison budgets. Lethal voluntary entry is rejected where appropriate; movement preferences already penalize pools.
- Pools are distinct from water: they do not have water's movement surcharge or scent washout. Poison pools and ice currently retain ordinary scent.
- Existing elemental transformations are ice plus fire into water and water plus cold into ice. Breaths do not create poison pools, and pools are not currently frozen, burned away, or cleansed.

### New ability infrastructure already exists

The implementation already tracks completed actions, movement history, consecutive melee attacks, Vengeance, and Smite recovery. Extend these records and their scheduler boundaries; do not introduce competing history updates.

The baseline save version was **0.9.8.5**: monster poison arrived in `.4`, and stateful abilities/lore in `.5`. The implemented persistent AI/scent extension uses **0.9.8.6**, with version-gated older reads.

The nine new uniques are defined. Ringwion, Angacirca, Langon, and Dúron use normal allocation. Lhamthanc and Fankil have authored encounters. Helcamo, Ondotur, and Nambatur remain reserved without their future encounters. Their AI can be exercised through development fixtures without enabling new locations.

## 3. Decision model and knowledge

Use one pipeline: witnessed event → individual memory → permitted tactical context → legal action candidates → utility evaluation → execution.

### Observation rules

- Distinguish accuracy, armour, elemental resistance, status resistance, and demonstrated reactions. A miss or fully absorbed hit is not proof of elemental immunity.
- Track signed evidence from −3 to +3, with at most one update per feature per player action. A clearly witnessed reaction can establish the corresponding capability; ambiguous outcomes remain uncertain.
- Initial expiry budgets: defensive evidence 40 player actions, demonstrated reactions 20, short movement patterns 8. These are implementation defaults to validate in playtesting.
- Use actual monster perception, not `m_ptr->ml`, player monster lore, health bars, or UI messages as a substitute for perception.
- Own HP, poison, mana, passive history, and recovery are legitimate planning inputs. Player equivalents require an observable, coarse inference.
- Do not attribute later poison ticks to a particular attacker: current monster poison has no stored source. Emit bounded observations at a witnessed application/outcome; add explicit causal state only if a concrete learning requirement needs it.

### Nearby warnings

Existing audible Shriek/Rally/warning actions may carry up to two fresh, useful observations to appropriate nearby allies. Lower received confidence by one and limit remaining lifetime to half the source lifetime. Repeated copies cannot raise confidence or extend the original evidence indefinitely.

Respect affiliation, audibility, and Silence. Do not add free warning actions or transmit an unseen player's current coordinates. Sharing knowledge is separate from the spell's existing effect: **Shriek alerts existing monsters; Rally adds temporary morale to eligible living Orcs, Men, and Raukar.** Preserve Rally's present morale calculation and reach unless separately changing its mechanics.

### Action and search limits

- Compare melee/blow choice, eligible ranged attacks, support, movement, continued song, and deliberate waiting in one decision.
- Use at most an actor-centred radius-4 local search (81 cells) and one additional own-action lookahead for ordinary tactical choices. Existing longer pursuit flows remain available.
- Commit to a short objective for at most three own actions, and recheck legality and hazards each action. Allow at most two consecutive deliberate waits outside existing guardian/territorial behavior.
- Require a meaningful improvement before repositioning; Flanking can use a lower threshold because moving itself can attack. Preserve variation among options within roughly 10% of the best score.
- Previews must not consume RNG, mana, actions, lore, or world state. Never temporarily move the real monster or player to evaluate an option.

### Casting opportunity

Roll the existing casting chance once per eligible own action. An intelligent monster may retain one successful opportunity, using it immediately or during its next two eligible own actions. There is no stacking, extra roll, extra cast, or waived mana cost; a new success cannot refresh an older reserve indefinitely. Expiry and hard casting prohibitions discard it. Mindless creatures and Morgoth retain the corrected single-roll behavior without this reserve.

Smite is an explicit melee commitment, not an RF4 casting opportunity. Songs retain their actual automatic pulse, mana, startup, and switching behavior; selecting an active song can produce an additional existing pulse and is not automatically a redundant action.

## 4. Terrain, formations, and poison strategy

### Movement and formation

Score terrain according to the moving monster's actual immunity, flight, movement rules, and abilities. Cold resistance does not make ice sure-footed; flight does not make lava safe. Use water/ice/fire mechanics already implemented by the engine.

Choose useful approach and surrounding squares: distribute melee attackers across legal adjacent positions, keep ranged lanes clear, leave useful retreat routes for allies, and avoid repeatedly swapping or clogging doorways. A surrounded player should result from reachable positions and paid movement, not shared omniscience or extra moves.

For monsters with the actual displacement ability, score legal knockback/exchange outcomes against visible chasms, traps, fire, water, ice, and poison. Preview the execution geometry, including the straight and alternate knockback destinations and the appropriate reaction attack. Do not grant a generic push action to monsters that lack one. Preserve primary-blow restrictions on knockback.

Known player defenses reduce the estimated payoff of a hazard; unknown defenses remain uncertain. Trap exploitation requires legitimate trap knowledge, not an inspection of hidden traps. Re-evaluate after terrain transformation or forced movement.

### Extend poison planning

1. Preserve the existing escape priority and crossing behavior. Evaluate ongoing own poison and future exposure before choosing to remain stationary, cast, support, or enter Smite recovery.
2. Prefer a safe exit when continued exposure is dangerous, even if leaving loses Concentration, Blocking, a shadow position, or an attack. A poisoned regenerator must not wait in a pool expecting to heal.
3. Use a pure action-order preview for final local candidates: entry exposure, action-start exposure, cap at 100, poison ticks, and mandatory recovery. Keep broad pathfinding conservative where exact simulation would be expensive.
4. Test alternative routes that merge: a cheaper route with more poison must not erase the only survivable longer route. Use bounded non-dominated cost/exposure alternatives where a single label loses viable paths.
5. Poison-resistant and flying monsters can use pools as safe contact positions or crossing lanes. Already-poisoned flyers still suffer their existing poison.
6. When witnessed evidence suggests a player is already under poison pressure, weigh immediate physical damage, control, pursuit, and preventing a safe recovery against another dose. Account for diminishing value near the cap only to the extent the monster can infer it; never read the player's actual counter or HP.
7. Intelligent area attackers consider allies' immunity, current own-known condition where legitimately available, and delayed collateral exposure. Serpents ignore allied collateral as agreed; dragons use their intelligent profile.
8. Poison pools retain scent under current mechanics. Do not use a nonexistent cleansing/freezing/burning interaction in route selection.

### Shadow support

Use the actual negative-light emitter, radius, distance falloff, line of sight, and overlapping contributions. `GLOW` alone is not a shadow capability. Evaluate marginal improvement from moving this emitter, rather than assuming that stacking darkness always helps.

Balrogs may hold a position that covers engaged allies or their approach. Dúron can combine useful cover with mobile melee. Shadow spiders use the same lighting evaluation with their own combat roles.

Darkness can relieve light-hating allies, but it can also harm allies whose combat effectiveness depends on illuminating the player. Subtract that cost and exposure to poison or other hazards. Do not darken the entire level, use the player-view-limited lighting buffer as omniscient truth, or move merely to increase an irrelevant darkness number.

Extract shared, pure light-contribution helpers for execution and preview. Preserve the actual emitter-center `GLOW` behavior, visibility ordering, and later lighting clamps. Keep support anchors local and short-lived.

## 5. Ability-aware choices and player counterplay

### New monster abilities

| Ability | Existing rule to preserve | Planned decision consequence |
|---|---|---|
| Concentration | Consecutive stationary ordinary melee, including misses, builds +1 attack per prior action up to half Perception; Ringwion caps at +3. Movement, waiting, casting, displacement, and lost actions break the chain. Reactions neither receive the bonus nor build the chain. | Prefer a useful sustained engagement; compare the next attack bonus with repositioning, control, and escape. Do not wait to build concentration. |
| Sprinting | Four completed compatible voluntary movement actions grant +1 speed only when speed after haste/slow is below 4; it never lowers a faster monster. Failure, winding routes, attacks, casts, waits, skips, and displacement break the chain. | Prefer a useful straight pursuit/interception or messenger route. Break the chain for urgent danger or a valuable warning. Do not run in circles to charge it. |
| Dodging | Previous completed movement grants +3 evasion. | Value a legal move that also improves position or performs Flanking; price the lost attack and incoming movement reactions. |
| Blocking | Being stationary grants extra shield dice only. Easterling warriors have 3d4 normally and 4d4 while blocking. | Hold useful front-line squares and protect archers; advance when the defensive square no longer contributes. Do not retain their removed Flanking profile. |
| Vengeance | Positive incoming melee damage arms one nonstacking bonus die. A landed melee hit consumes it even if protection absorbs the result; a miss retains it. | Value a credible retaliatory hit without deliberately absorbing suicidal damage. Ranged damage and poison ticks do not arm it. Preserve Carcharoth's 2d12 base bite. |
| Smite | Eligible ordinary melee currently automatically spends 10 mana before the hit roll, maximizes resulting damage dice on a hit, and forces one full recovery action. Misses still pay. Reactions remain disabled through recovery. | Add an explicit ordinary-blow versus Smite choice. Commit only when the hit opportunity and recovery position justify it, including pending poison and escape needs. Preserve immediate execution; add no wind-up, cooldown, damage, or mana changes. |
| Zone of Control | Requires the monster's prior action to be stationary and the existing reaction prerequisites; reacts to adjacent-to-adjacent movement. | Angacirca should hold a useful control square. Leaving adjacency is a different reaction case; avoid treating all player movement as a free attack. |

Existing ability getters can reveal monster lore through `observe_ability()`. Split pure calculation from revelation before using them in AI previews. Player knowledge of monster RF5 flags is not monster knowledge of player abilities.

### Existing active abilities

- **Melee alternatives:** expose every defined blow to selection, including third and later alternatives. Intelligent monsters compare physical damage, observed elemental effectiveness, wounds, disarm, and control. Simple selection retains primary weight 2 and each alternative weight 1. Do not invent attacks.
- **Breaths and projectiles:** evaluate actual reach, line geometry, collateral, observed defenses, and a legal physical alternative. Fire-resistant players should make an intelligent dragon consider its claws or bite, not magically gain a different breath.
- **Slow, fear, confusion, holding, webs:** value the real duration/control opportunity and supported follow-up. Reduce repeated ineffective use from observed evidence. Preserve distinct save and extension behavior; repeated resistance is evidence, not certainty from one failure.
- **Rally:** prefer useful morale restoration or support for engaged eligible allies over an empty cry. Preserve current effect semantics.
- **Shriek:** prefer situations with reachable recipients that can use the alert or warning; summon nothing new.
- **Songs:** compare existing pulse, control, summoning, pursuit, and mana effects. Keep automatic pulses distinct from deliberate song actions. Gorthaur's Oaths can summon oathwraiths; Shelob has no spell list to invent.
- **Disarm, wounds, stat drains, shatter, batter, exchange:** score the actual attack and terrain consequences with witnessed player context. Keep status effects, reaction costs, and execution prerequisites distinct.

### Responding to the player

| Witnessed player behavior | Planned response and limitation |
|---|---|
| Flanking | Deny easy orbit paths through useful formation, cover, and existing control abilities; do not make every monster endlessly orbit. |
| Controlled Retreat | Recognize the actual stationary-then-retreat sequence and avoid a predictable stand/retreat/chase loop; use ranged pressure, another approach, or a bounded wait where useful. |
| Opportunist / Zone of Control / Polearm | Price the specific movement that triggers the observed reaction. Do not combine their different trigger conditions. |
| Riposte | Prefer more reliable hits, ranged pressure, support, or position when appropriate; preserve uncertainty about unobserved accuracy and armour. |
| Charge / knockback / Exchange Places | Evaluate actual direction history, landing squares, terrain, and the corresponding reaction rules. |
| Kiting, Sprinting, Skirmishing | Intercept at reachable junctions, take cover, maintain useful missile range, and avoid futile chasing. |
| Focus / Concentration | React to observed preparation or sustained attacks; account for the fact that player waiting does not itself reset player Concentration. |
| Impale | Avoid the first two occupied squares on the same eight-way ray from the player, from either position; distant or offset lines do not count. |
| Whirlwind / Rage | Remember sweeping attacks separately and avoid crowding the whole ring beside the player, including opposite sides; allow a useful step out of that ring. |
| Follow Through | Remember the kill-triggered follow-up separately. Any other enemy beside the player can be next; the positioning penalty grows when an ally is visibly wounded. |
| Frightening weapon strikes | Record the actual recipients of the existing slay/brand morale penalty as personal weapon-fear evidence. Preserve the morale-driven stance and fleeing rules. |
| Stealth, Vanish, ambush | Use legitimate last-known position, perceived sound, and eligible scent; never follow hidden current coordinates. |
| Elemental/status resistance | Shift toward an existing effective attack or support action after credible observations. Keep poison melee and pure poison mechanics distinct. |
| Player songs | Respond to their actual distance, control, and combat effects; ordinary damage does not automatically interrupt singing. |

Stat-only and crafting abilities do not require invented monster counters. Ability names alone are not observable evidence.

## 6. Monster profiles

Profiles combine capabilities and roles rather than adding a unique-only AI branch for every attack. These profiles change choices, not stats or available actions.

### Ordinary monsters

| Family or role | Profile |
|---|---|
| Wolves/hounds and cats | Local scent tracking, pack approach and surrounding; retain family-specific scent age limits and actual exchange/flanking capabilities. |
| Orc and human melee groups | Form a useful front line, leave missile lanes, react modestly to witnessed counters; only actual SMART/ability flags enable stronger tactics. |
| Easterling warriors | Stationary shield front line with Blocking; move to restore useful contact, not to use removed Flanking. |
| Archers and boulder throwers | Clear shots, useful range, cover, and safe relocation; avoid blocking allies or relying on shots that cannot reach. |
| Controllers and singers | Follow-up value, real recipients, observed resistance, mana, and ongoing song effects. |
| Dragons | Intelligent breath/melee/control choice, appropriate ally collateral, and restored dragon scent. Flying and breathless dragons retain those distinctions. |
| Serpents | Existing attacks and legal range, reckless allied AoE collateral, basic self-preservation, no scent. |
| Shadow/light-hating creatures | Actual marginal lighting benefit and safe cover; negative light is not universally beneficial to allies. |
| Poison attackers / poison-resistant creatures | Delayed-damage pressure or safe pool positioning as their actual capabilities permit. |
| Regenerators, flyers, heavy movers | Evaluate their real regeneration suppression, contact immunity, terrain access, and displacement opportunities. |
| Mindless, immobile, and territorial creatures | Preserve their movement and territory restrictions. No learned player model for mindless creatures; existing basic hazard escape still applies. |

### Existing uniques

| Monster(s) | Retained and updated profile |
|---|---|
| Gorgol, Orcobal, Othrod, Ulfang | Escort/commander coordination and useful available Rally; Othrod's disarm and Ulfang's Opportunist keep distinct triggers. |
| Baugon, the Merciless | Use the renamed ID-76 Orc captain profile, existing archery/spells and escorts. Do not retain the old Boldog name or confuse him with Angacirca. |
| Uldor; Umuiyan | Crippling ranged pressure and fast archery respectively; useful lanes and responses to observed approaches. |
| Balcmeg and Lug | Complementary reachable melee positions and pressure rather than both choosing the same slot. |
| Brodda | Territorial undead control and all defined drain/blow alternatives, including the currently unreachable third blow. |
| Duruin, Belegwath, Turkano, Vallach, Gothmog | Actual shadow support plus their existing fire, melee, disarm, escort, speed, and territory distinctions. |
| Delthaur; Lungorthin | Their actual fear and breath opportunities respectively, with appropriate terrain and allied safety. |
| Gilim and Nan | Boulder/melee and displacement opportunities; account for darkness that can impair their combat. |
| Dagorhir | Regeneration, batter/knockback, and escort pressure; poisoning prevents a false wait-to-regenerate strategy. |
| Tevildo and Oikeroi | Cat scent, coordinated melee/exchange, and legal reaction-aware repositioning. |
| Maeglin | Riposte and Cruel Blow duelist; accurate useful engagement, not gratuitous movement. |
| Scatha, Smaug, Glaurung | Dragon scent and their existing breath/control/melee choices; Smaug's flight affects contact and movement. |
| Gostir and the Green Great Dragon | Dragon scent and existing melee/control. They do not gain breath attacks. |
| Ancalagon | Flying fire-dragon profile, observed-defense adaptation, collateral evaluation, and dragon scent. |
| Draugluin | Canine scent and pack pressure. |
| Carcharoth | Canine scent, poison bite pressure, speed, and one charged Vengeance die; retain base 2d12. |
| Thuringwethil | Flight/speed, wounds and existing drains, with exposure-aware engagement. |
| Shelob; Ungoliant | Actual shadow and poison/control opportunities; Shelob remains a melee creature, while Ungoliant uses her existing dark breath. |
| Gorthaur | Canine pursuit, existing wolves/Hold/Oaths, and useful control/summoning decisions. |

### Newly defined uniques

| Monster | Current encounter | Planned strategy |
|---|---|---|
| Ringwion, the Pale Blade (403) | Normal allocation from tier 16 | Cold Riposte/Concentration duelist. Preserve an advantageous stationary attack chain, up to +3; abandon it for lethal hazards or a materially better position. Respect fire vulnerability and ordinary ice footing. |
| Helcamo, the Hoarfrost (404) | Reserved | Territorial cold guardian with Slow. Use a credible Slow opportunity to improve existing melee/control; stop wasting repeated attempts after observed ineffectiveness. No new encounter or ice immunity. |
| Lhamthanc, the Forked Tongue (405) | Vault 521, levels 19–20 | Fast flying territorial dragon with Scare at 15%, claws and a wounding bite. Use legal water shortcuts, fear follow-up and dragon scent. He has no breath or Confusion spell. |
| Angacirca, Reaper of Thralls (406) | Normal allocation from tier 16 | Stationary Zone of Control at useful adjacent squares; select wounding or disarming blows from observed context. Move when the control position no longer matters. |
| Langon, the Rushing Herald (407) | Normal allocation from tier 15 | Sprinting messenger with Shriek, flight, Pass Door, Exchange Places and poison resistance. Preserve useful straight routes; shout when recipients benefit; use safe pool crossings. |
| Dúron, Keeper of the Unlit Ways (408) | Normal allocation from tier 20 | Mobile shadow melee with radius-3 negative light, Flanking and Dodging. Combine lateral attacks, +3 movement evasion and useful allied cover without suicidal hazard steps. |
| Fankil, the Sower of Strife (409) | Vault 522, levels 14–15 | Rally/Scare commander behind two Blocking Easterling warriors and two archers. Value actual morale improvement, firing lanes, and fear follow-up; no fabricated summons or haste. |
| Ondotur, the Buried Lord (410) | Reserved | Slow stone guardian using existing tunnelling and Shatter. Compare useful tunnel routes and their terrain consequences; preserve fire/cold/poison resistance and lack of new ranged powers. |
| Nambatur, Custodian of Grond's Forge (411) | Reserved | Fire-resistant heavy melee with Batter and Smite. Choose immediate Smite only when its chance, mana cost and recovery exposure justify it. Do not schedule a nonexistent forge encounter. |

## 7. Scent restoration

Make sensory evidence drive pursuit: recognized sight, successfully perceived player-origin sound at its recorded origin, local scent, then last-known position and bounded search. Monster-origin noise must not reveal the player's current position. Keep investigation targets separate from retreat, lair, and scripted targets.

- Keep existing wolf/hound age limit 80 and cat limit 40; use 80 for dragons initially. Validate explicit capability mapping against the old glyph behavior before replacing it.
- Awake trackers can acquire a trail on their occupied square before seeing or hearing the player. Scent investigation is not automatic visual identification or permission to attack an unseen location.
- Follow younger, reachable adjacent scent. Resolve equal-age plateaus with a recent-four-cell history; abandon an unproductive trail after eight movement decisions without fresher evidence.
- Search by physically moving within radius 2 of the last confirmed trace for at most eight decisions. Do not scan the whole map for its newest scent.
- Age scent once per completed player action. Preserve the deposition stencil, wall/LOS rules, water and bank discontinuity, leap/airborne gaps, and disconnected teleport arrivals.
- Ice and poison pools retain scent. Melting ice to water clears affected traces; freezing water does not restore an erased trail. Preserve current lava scent behavior.
- Scent does not change sleeping wake rules. Separately, real poison damage already wakes a sleeping monster through `mon_take_hit()`; preserve and test this actual damage route.
- Persist the normalized scent field so save/reload cannot erase pursuit: zero absent, 1–81 representing ages 0–80, with a reconstructed clock. Version-gate the added dungeon record and initialize old saves safely.

## 8. Correctness fixes and interfaces

Fix these before judging tactical balance:

1. Remove the duplicate ranged-chance roll in the stationary fallback. One own action must have one ordinary opportunity roll.
2. Charge Web and Rally their declared non-song mana cost; the current numeric song-boundary shortcut skips both later RF4 entries. Identify songs explicitly.
3. Make Song of Piercing activation independent of whether the player can see its singer. Preserve Morgoth's existing intent and song behavior.
4. Apply hard ability legality before random/single-option shortcuts. Separate self/allied support targeting from player-projectability checks, without removing offensive reach requirements.
5. Make all defined melee alternatives reachable. Preserve attack-specific knockback and reaction constraints.
6. Remove flow construction's unrelated writes to monsters' target coordinates. Updating one flow must not erase another creature's target.
7. Correct scent-sharing handling of absent scent and sender/recipient alerting. Check lighting bounds before accessing candidate grid data.
8. Repair `check_monster_poison.py` legacy fixtures: the current writer includes the newer Vengeance and Smite bytes, but old fixtures only trim the poison-sized suffix. Construct records for each real version explicitly and test sentinel alignment.

Use shared pure descriptions/calculations for attack geometry, light contribution, ability bonuses, and poison action timing. Add a deliberate Smite execution parameter instead of probing the current auto-spending helper. Keep final legality checks in execution even when preview already checked them.

Per-monster AI state should hold bounded observations, a short objective, sensory evidence, recent scent search cells, and the optional casting opportunity. Preserve it through index moves/compaction and saving; reset it for new spawns, replacements, offspring and polymorph as appropriate. Avoid stale source references after index reuse. Preserve existing poison, action-history, Vengeance and Smite state in every lifecycle route.

## 9. Implementation order and validation

### Stage 1 — Correctness and current contracts

Fix opportunity/mana/visibility/flow/melee-selection issues and the stale save fixture. Extract pure helpers and add integrated timing tests. Establish the existing hazard, ability, encounter and Morgoth behavior as the baseline.

### Stage 2 — Fair perception and memory

Replace hidden-coordinate pursuit inputs with evidence targets, restore local scent, add bounded observations and nearby warning sharing, and implement versioned persistence. Test no hidden-state access before introducing stronger adaptation.

### Stage 3 — Ability and poison decisions

Integrate candidate scoring with existing movement/escape logic. Add explicit Smite choice, history-dependent passive value, delayed poison/recovery prediction, deliberate ranged opportunity use, and observed-defense attack alternatives.

### Stage 4 — Group and unique behavior

Add surrounding assignments, reaction-aware movement, hazard displacement, marginal shadow support, ordinary family profiles, and all existing/new unique profiles. Keep reserved encounters disabled.

### Stage 5 — Integrated encounters and tuning

Run the focused suites and normal parallel Windows build, then playtest representative encounters. Tune utility weights and short decision budgets, not monster stats, unless a separate balance change is requested.

Required automated coverage:

- Existing `check_monster_abilities.ps1`, `check_new_monsters.py`, `check_monster_poison.py`, poison-terrain tests, and the monster AI harness; extend their production call-site coverage rather than relying only on copied formulas.
- Real decision → action → movement → reaction sequences: Concentration retention/reset, Blocking under melee/archery/throwing, stationary Zone of Control, Dodging, four-step Sprinting, and forced movement.
- Smite hit and miss, mana payment, mandatory recovery, disabled reactions, poison ticks during recovery, Vengeance consumption, and combinations with statuses and save/reload.
- Poison entry/standing/exit, player full-action timing, flight versus actual immunity, protection and cave-boundary recalculation, brands/projections, cap saturation, lethal and survivable routes, alternative paths at merges, forced exposure, and no oscillation.
- Real `mon_take_hit()` sleeping wakeup, morale/death and Morgoth paths; a stubbed damage function cannot validate those behaviors.
- No learning from hidden equipment, player lore, exact player state, unwitnessed events, or stale entity indices. Repeated warning messages must not manufacture confidence.
- One casting roll, reserve deadline, support without a player firing line, full attack reach/collateral, spell mana, and offscreen song activation.
- Scent acquisition before sight, fresh/old/absent trails, plateaus, water and leap gaps, poison/ice retention, terrain transformation, bounded search, save/reload, dragon inclusion and serpent exclusion.
- Shadow overlap, useful allied cover, allies harmed by darkness, blocked LOS, map edges, moving emitters, and hazardous support positions.
- All new GUIDs and unique limits, two authored vaults in all orientations, fixed Fankil escorts, reserved-monster exclusion, renamed Baugon, and unchanged Morgoth encounter behavior.
- Bounded search work and pure previews: repeated evaluations must not consume randomness or mutate combat, lore, lighting, or terrain.

Manual encounter cases: a mixed group around water/ice/fire/poison; a poison crossing under pursuit; an injured poisoned regenerator; Balrog cover for allies with different light preferences; Ringwion versus mobile and stationary play; Fankil's shield-and-archer escort; Langon's useful sprint/warning choice; Dúron's mobile darkness; Carcharoth's poison/Vengeance retaliation; Lhamthanc's flying scent pursuit; and a development-only Nambatur encounter testing safe and unsafe Smite commitments.

Passing source checks or a build is not proof of balanced or convincing encounter behavior. Record automated results separately from manual playtesting.

### Action transparency

Visible leaders announce a new advance, hold, or regroup order after actually taking an action. A visible ally must also be present before a message identifies group activity. Archers explain useful repositioning and deliberate holds at assigned firing posts. Explanations are suppressed for lost actions, forced displacement, unseen actors, hallucinations, and failed moves. Per-actor cooldowns and separate per-player-turn limits for command and movement messages keep formations readable without a message for every soldier.

Desktop `log.txt` records completed active monster actions at the default DEBUG level: actor/race identity, positions, HP/mana/energy changes, sensory evidence and remembered position, commander, role, plan, job, order validity, command budget, and tactical reason. Ranged records include the committed attack ID and distinguish arrows, boulders, and spell/support actions. Action-start deaths retain the original actor's identity; missing turns show the cause instead of claiming a move. Unchanged sleepers are available at TRACE. `SIL_LOG_LEVEL=TRACE` enables those records; `SIL_LOG_LEVEL=INFO` suppresses per-action diagnostics. Mobile keeps its existing INFO default.

Detailed diagnostics may describe offscreen actors, but player-facing messages use only visible actions and visible allies. Logging and message generation must not roll dice or change decisions. `scripts/check_monster_ai_transparency_gameplay.py` checks real SDL-input encounters, log/action correspondence, visibility-safe messages, exact replay equivalence at different log levels, and live save/reload continuations.

Squad role selection follows the ordinary range selector's specialization threshold: occasional arrows or boulders at 15% frequency or below supplement melee instead of forcing a melee-oriented creature to hold a firing post. Current long-range preferences, bow specialists and creatures unable to strike retain ranged roles. Missile opportunities remain available when melee cannot reach the target.

The bright-player penalty to light-averse morale requires actual physical monster sight, including LOS and range. Stealth recognition does not gate it. An unseen player's lighting cannot lower morale through a wall. Completed-action diagnostics include morale, current player sight, local light and player light so this interaction can be checked against the actual encounter.
