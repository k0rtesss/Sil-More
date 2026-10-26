# Vertical gameplay

The resident dungeon remains the current floor plus one floor above and below.
An extra destination may exist while completing a long fall or shaft journey.
Older ordinary floors are discarded. Returning generates fresh surroundings,
constrained by resident neighbors and the fixed great-vault/quest sites. Those
sites keep their progress in a sparse archive; distant ordinary floor maps are
not archived. Each depth also streams horizontal partitions using a shared
world origin; see [Endless dungeon partitions](endless-world.md).

## Water and volcanic passages

Inspect a passage to learn its direction and risk. Stand on its entrance and
use the usual descend/ascend commands. The player-square popup, radial menu,
context action, touch buttons and controller bindings expose the same commands.
Quest confirmations, oaths and minimum-depth restrictions still apply.

* A descending stream is reversible. Downstream movement is easier; upstream
  movement costs more energy.
* Rapids and waterfalls can sweep a grounded creature downstream. An action
  produces at most one current displacement and one floor transfer. Waterfalls
  inflict impact damage based on height and receiving terrain. Deep water halves
  impact and shallow water reduces it by a quarter; neither removes the risk.
  Bridges, ice, rock and lava remain hard landings.
* Lake-cavern openings reveal a limited view of the receiving floor. Separate
  basins have separate surfaces; an upper opening is not walkable lower water.
* Submerged passages require a deliberate dive. Air decreases on paid actions,
  recovers in shallows and causes drowning damage when exhausted. Carried weight
  reduces the initial air budget. Air is visible in status displays.
* Lava falls retain normal lava contact rules. Some volcanic flows are finite:
  they drain into a roofed conduit, expose hot rock, then become reversible dry
  lava-tube passages after cooling.

Water depth and current strength are independent. Flight avoids grounded
current displacement. Nearby grounded monsters use the same current/drop rules;
occupied landings and allocation limits never delete an actor or carried items.
Lightweight floor stacks at a flowing outlet can be carried into its resident
receiving floor. Named artefacts and notes are protected from this transport.

## Cistern Fortress

Vault templates 525 and 526 form the upper Sluice Keep and lower Settling
Galleries. They share transformed placement, reciprocal stairs and discovery
history. Two dry approach galleries and the service staircase remain available.
The upper sluice releases stored water into the lower reservoir after a delay.
The lower race can continue as a descending stream.

An upper reward lies beneath the cistern water. Draining it exposes the reward;
building the wet vault does not consume its reward attempt. The normal drop
policy applies, and its shared award bit prevents another attempt after eviction.

Open or close a visible sluice with the ordinary Open/context action. The gate
is also a real door: destroying it leaves it broken open and releases its finite
reservoir. Gate state, basin volumes and in-flight packets belong to the shared
region. Rebuilding a discarded half cannot recreate its authored chest/treasure
or award discovery experience again while its counterpart retains that history.

## Reservoirs and time

The hydraulic model uses bounded storage and explicit source/outlet capacities.
Maintained natural inflow balances normal outflow. Blocking a supplied outlet
raises its basin; finite sluices and late lava flows spend stored volume.
Transit has a game-time delay. A pending packet passes to the receiving half
before its sender is evicted and is credited once.
Finite connection capacities reflect their strength: a large sluice releases
faster than its narrow downstream race, so the receiving pool temporarily rises
before draining back to its normal level.

Nearby rising basins warn before prolonged high water and impending overflow.
Warnings also originate physical sound events, remain quiet on repeated samples,
and become available again after the basin returns to its normal level.

Freezing an outlet stops its flow; thawing does not create extra water. External
flood/eruption terrain changes contribute their represented volume to a nearby
region once. Walls contain the local basin footprint. Water/lava interaction
consumes both supplies, produces hot rock and steam, and can be more dangerous
where surrounding rock confines the steam. Menus and animation advance no time.

## Looking and fighting through openings

The current floor remains the actionable foreground. Lower terrain is drawn at
normal tile size with reduced light inside an actual opening. Banks retain their
original art; bridges and current-floor actors remain above the recess. Hidden
traps, secret features, blindness, darkness and restricted views are respected.
Hover and Look describe what lies above/below without exposing the cached map.
Falls and ceiling inflows animate on the existing visual clock and use local
acoustic propagation.

Use above/below aiming with the normal bow command while at an opening. For an
adjacent opening, its context menu offers **Shoot through opening**; **Ctrl-G**
provides the keyboard command. Only real open rays and visible targets qualify.
Shots spend ordinary ammunition and energy. Nearby creatures on the adjoining
floor advance on the shared clock, can return physical ranged attacks and can
fly through an unobstructed opening.

## Authoring and validation

Paired vaults are explicit-only type 12 templates. `Y:companion:layer` links
upper layer 0 and lower layer 1. `P:role:row:column` marks gates, basins,
spillways, outlets, overlooks, stairs, approaches and submerged rewards. Both
layers must share
dimensions and reciprocal metadata; all eight transforms use one stored frame.
In vault terrain rows, `_` is shallow water, `"` is deep water, and a backtick
continues to mean lava. Role 9 marks the reward exposed by draining a pool.

Save format 1.1.0.6 adds resident region/vault metadata. Earlier saves receive
empty vertical metadata and keep their existing terrain and stairs.

Focused checks:

* `scripts/check_vertical_vaults.py`: production template parser, transforms,
  reciprocal stairs and authored dry circulation.
* `scripts/check_vertical_cache.py`: real generation, eviction/reconstruction,
  shared history, v5/v6 save lanes, malformed records, aperture visibility and
  ownership transfers, including preserved monster feuds and diplomacy. Targeted
  `--case links`, `--case holes` and `--case upkeep` cover long-fall columns,
  reciprocal stairs for every resident peer, isolated shaft geometry, and item
  placement without premature view/lore/quest updates during generation.
* `scripts/check_vertical_water.py`: currents, flow budgets, blocking, mixing,
  lava cooling, diving, destruction and every natural feature's generation.
* `scripts/check_vertical_render.py` and `check_vertical_ui.py`: production SDL
  layers and controls/status behavior, with explicitly injected core boundaries.
* `tools/test-vertical-combat.c` and `test-vertical-actors.c`: production combat
  and monster-current paths with isolated engine boundaries.

These fixtures establish engine and rendering behavior. Physical-device input
and encounter balance require separate gameplay testing.
