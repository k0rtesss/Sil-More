# Movement Input Rework Plan

Status: draft on April 15, 2026.

## Goal
- Replace movement's dependency on legacy keymaps and macro-trigger strings.
- Stop encoding movement as command bytes plus embedded digits such as `;8`,
  `.6`, or `/4`.
- Move keyboard movement configuration into normal settings data, with direct
  key assignment and explicit modifier chords (`Ctrl`, `Shift`, `Alt`).
- Keep gameplay behavior the same unless the plan explicitly changes it. This
  is an input architecture change, not a movement mechanics rewrite.

## Alignment With The UI Terminal Extermination Plan
This plan is a vertical slice of the active
`docs/ui_render_replacement_plan.md` mission, specifically:
- Live Remnant Family 1: legacy input and command loop
- Execution Order A: expand the audit first
- Execution Order B: replace the live input stack

That means the movement rewrite must help delete terminal-era ownership rather
than hide it.

Non-negotiable constraints from the extermination plan:
- do not add new gameplay-facing compatibility wrappers around `inkey()`,
  `request_command()`, `flush()`, or byte queues
- do not add a second command stack beside `src/app/*`
- do not preserve SDL movement handling by synthesizing new macro-trigger text
- do not add new settings APIs that still fundamentally store movement as
  command chars or keymap action strings
- any temporary translator must live behind `src/app/*` or a narrow platform
  translation seam, and it must reduce live debt rather than create a new
  permanent layer

## Current Problems
- `request_command()` still resolves keyboard input by looking up
  `keymap_act[mode][key]` and injecting string actions back through
  `inkey_next` in `src/util-input.c`.
- `target_dir()` still derives direction by scanning action strings for digits,
  so movement meaning is hidden inside strings like `;1`, `.8`, or `/5` in
  `src/targeting.c`.
- The shipped keyboard defaults are stored as legacy keymap actions in
  `lib/pref/pref.prf`,
  including mode toggles such as `hjkl_movement` and `angband_keyset`.
- SDL directional modifier handling still synthesizes macro-trigger byte
  sequences like `^_Sx38\r` in `src/main-sdl.c` and `lib/pref/pref-sdl.prf`.
- The settings UI labelled as "keybinds" is only a front-end for
  `keymap_act[]`, and movement defaults are still defined as legacy actions
  like `;1` in `src/cmd/ui/cmd-ui-settings.c`.
- Core command dispatch still switches on old command chars such as `';'`,
  `'.'`, `'/'`, and `'z'` in `src/dungeon.c`.

## Target State
Movement input should be action-based, not macro-based.

### 1. Structured actions
- Introduce explicit movement action ids such as:
  - `MOVE_DIR`
  - `RUN_DIR`
  - `INTERACT_DIR`
  - `WAIT`
  - `REST`
- Direction is stored as structured data, not parsed back out of a string.
- The movement layer should never need to understand `;`, `.`, `/`, `z`, or
  digit-mining rules.

### 2. Structured bindings
- Add a keyboard binding table with entries shaped like:
  - input context
  - physical key or scancode
  - required modifiers
  - forbidden modifiers
  - bound action id
  - optional direction payload
- Bindings should allow:
  - one primary and one secondary binding per action
  - presets
  - conflict detection
  - direct capture in the settings UI

### 3. Settings-backed configuration
- Keyboard bindings should live in `sil_sdl.json` beside controller and touch
  bindings, not in `user.prf` dumps.
- `hjkl_movement` and `angband_keyset` should be retired as movement-mode
  selectors.
- Replace them with presets such as:
  - `Modern Arrows`
  - `Modern WASD+QEZC`
  - `Vi Keys`
  - `Classic Sil`

### 4. Modifier chords are first-class
- `Shift`, `Ctrl`, and `Alt` should be represented structurally in the binding
  record, not tunneled through macro trigger text.
- Recommended default movement semantics:
  - plain direction: walk
  - `Shift + direction`: run
  - `Ctrl + direction`: interact or alter in that direction
  - `Alt + direction`: available as a user-assignable directional modifier
    action, not hardcoded through macro text
- `Wait` and `Rest` should be normal actions with normal bindings, for example
  `Numpad5`, `.`, or `Shift + Numpad5`, depending on preset.

### 5. One action model across devices
- Keyboard, controller, and touch should bind to the same action ids.
- Gamepad and touch may keep device-specific settings screens, but the bound
  target should be a shared action enum, not raw chars plus
  `GAMEPAD_BIND_SHIFT` hacks.
- This aligns with the existing `app_input` and `app_intent` boundary in
  `src/app/app-input.h`.

## Recommended Architecture
The cleanest path is a staged replacement of the live input stack, not another
layer of macro glue.

### Phase 0. Expand the input debt audit for this slice
Deliverables:
- extend the UI debt audit or add a companion audit for:
  - movement-related `inkey`
  - `request_command`
  - `flush(`
  - movement macro-trigger synthesis in SDL
  - movement keymap defaults in `pref.prf`
  - movement macro defaults in `pref-sdl.prf`
- check in a baseline before movement code starts changing

Exit when:
- the repo can measure whether the movement rewrite is actually deleting live
  terminal-model debt

### Phase 1. Add the semantic movement command service behind `src/app/*`
Deliverables:
- add named semantic movement command types and binding structs
- add an app-facing command-input service for movement acquisition
- add keyboard binding storage to SDL config
- add default movement presets

Implementation notes:
- The semantic service should be owned by `src/app/*` or a narrow adjacent
  core-facing seam, not by a new `util-input`-style global helper.
- Keep this layer plain C and SDL-free where practical.
- SDL should provide physical key events; binding resolution should be a normal
  lookup, not byte-sequence synthesis.
- The service should produce semantic command payloads such as action plus
  direction plus modifiers, not command chars plus embedded digits.
- Missing movement bindings at startup should be filled with the configured
  default preset, not imported from legacy keymaps.
- Old movement keymaps and macro text should simply stop being authoritative
  for movement.

Exit when:
- the game can load, save, reset, and display keyboard movement bindings
  without writing `A:` / `C:` movement entries
- settings UI can capture real chords like `Shift+Up` or `Ctrl+H`
- movement command acquisition has a named semantic API instead of direct
  dependence on command-byte assembly

### Phase 2. Replace SDL movement translation without macro synthesis
Deliverables:
- stop calling `sdl_send_macro_key()` for movement combos
- stop depending on `pref-sdl.prf` for directional modifier behavior
- resolve keyboard and gamepad directional input into semantic commands or app
  intents with direction payloads

Implementation notes:
- It is acceptable to keep legacy raw-key bridging alive for unrelated command
  families while movement migrates, but movement itself must stop depending on
  the bridge.
- Movement should be the first family moved off raw legacy bytes because that
  directly advances Execution Order B from the extermination plan.
- Keyboard, controller, and touch should converge on the same semantic action
  ids here rather than preserving different device-specific movement semantics.

Exit when:
- pressing `Shift+Up` produces a structured `RUN_DIR north` action
- pressing `Ctrl+Left` produces a structured `INTERACT_DIR west` action
- no movement path relies on `^_Sx..` or `^_Cx..` macro trigger text

### Phase 3. Replace movement command consumption
Deliverables:
- teach movement consumers to use semantic action id plus direction directly
- stop routing movement through `request_command()` plus `target_dir()` digit
  extraction
- remove movement's need for gameplay-facing `flush()` calls in normal flow

Primary targets:
- `src/util-input.c`
- `src/targeting.c`
- `src/dungeon.c`
- `src/cmd/movement/cmd-movement.c`
- `src/cmd/world/cmd-interact.c`

Implementation notes:
- `get_rep_dir()` and `get_aim_dir()` should stop asking for a char and then
  translating it indirectly.
- Direction prompts should accept the same configured directional bindings as
  top-level movement.
- If an intermediate translator is needed, it should be a narrow semantic
  translator that shrinks `request_command()` ownership instead of preserving
  byte-queue semantics as a new permanent layer.
- `src/dungeon.c` should move toward semantic command acquisition for movement
  rather than continuing to special-case movement inside the old command-byte
  switch forever.

Exit when:
- movement, run, alter, wait, and rest no longer depend on legacy action
  strings
- `target_dir()` is no longer required for movement resolution
- movement is no longer a gameplay caller of the live legacy input stack

### Phase 4. Retire movement-specific legacy surfaces
Deliverables:
- remove movement keymap defaults from `pref.prf`
- remove movement macro defaults from `pref-sdl.prf`
- remove `hjkl_movement` and `angband_keyset` as movement architecture choices
- keep optional preset names that preserve those layouts without preserving the
  old implementation

Exit when:
- movement is configured entirely from settings data
- the settings UI no longer talks about keysets or keymaps for movement
- `;1` / `.1` / `/1` no longer appear as active movement definitions in code
  or shipped defaults

## Parallel Slices
The execution model should be parallel-by-slice after the semantic interface is
defined. Each slice owns a disjoint write set and should avoid widening its
scope.

### Slice 0. Audit And Guardrails
Goal:
- make the movement slice measurable before implementation begins

Status:
- completed in the working tree on April 15, 2026 via the
  `movement_input` audit family in `tools/ui_debt_audit.py`

Write set:
- `tools/ui_debt_audit.py`
- debt baselines under `tests/`
- `docs/movement_input_rework_plan.md`
- `docs/ui_render_replacement_plan.md` only if the shared audit wording needs
  to be updated

Outputs:
- movement-specific audit checks for input-stack debt
- a checked-in baseline for the input family this rewrite is meant to reduce

Baseline snapshot from
`py -3 tools/ui_debt_audit.py --audit movement_input --details`:
- movement-related `inkey()` waits: 3 files / 13 matches
- movement `request_command()` ownership: 3 files / 4 matches
- movement-related `flush()` usage: 5 files / 17 matches
- SDL directional macro bridge symbols: 2 files / 17 matches
- movement action defaults in `pref.prf`: 1 file / 84 matches
- movement macro defaults in `pref-sdl.prf`: 1 file / 36 matches

Checked-in guardrail:
- `tests/ui_debt_audit_baseline.json` now carries the `movement_input`
  baseline alongside the existing `ui0` audit so the existing audit check can
  fail on regressions without adding a second test entry point.

Dependencies:
- none

### Slice 1. Semantic Movement Service
Goal:
- define the app-facing movement command and binding model once, so other
  slices can build on it in parallel

Write set:
- `src/app/*` files needed for movement command or binding types
- new movement-command service files if introduced under `src/app/`

Outputs:
- semantic movement action ids
- direction payload model
- binding record model
- app-facing lookup or submission API for movement commands

Dependencies:
- starts after Slice 0 baseline exists

Rules:
- this slice owns the public interface
- later slices consume that interface but should not redesign it piecemeal

### Slice 2. Settings, Config, And Migration
Goal:
- move keyboard movement bindings into structured settings

Write set:
- `src/sdl-config.c`
- `src/sdl-config.h`
- `src/cmd/ui/cmd-ui-settings.c`

Outputs:
- JSON-backed keyboard movement bindings
- preset load or reset flow

Dependencies:
- starts after Slice 1 interface is frozen

Rules:
- do not reintroduce `.prf` dumps as the primary movement settings format
- do not expand scope into SDL event translation or gameplay command
  consumption

### Slice 3. SDL Device Translation
Goal:
- translate keyboard, gamepad, and touch directional input into semantic
  movement commands without macro synthesis

Write set:
- `src/main-sdl.c`
- `src/sdl-touch.c`
- `src/platform-input.h`
- `src/ui/ui-help.c` or other input-label surfaces if needed for display

Outputs:
- no movement use of `sdl_send_macro_key()`
- shared semantic translation for keyboard, controller, and touch movement

Dependencies:
- starts after Slice 1 interface is frozen

Rules:
- keep unrelated non-movement command families on the legacy path if needed
- do not touch movement settings storage or gameplay consumers unless blocked

### Slice 4. Gameplay Movement Consumers
Goal:
- make gameplay consume semantic movement commands directly and stop depending
  on movement-specific legacy input assembly

Write set:
- `src/dungeon.c`
- `src/targeting.c`
- `src/util-input.c`
- `src/externs.h`
- `src/cmd/movement/cmd-movement.c`
- `src/cmd/world/cmd-interact.c`

Outputs:
- movement, run, alter, wait, and rest consumed from semantic action plus
  direction payload
- reduced ownership of `request_command()` and `target_dir()` for movement

Dependencies:
- starts after Slice 1 interface is frozen
- should integrate cleanly with Slice 3 once SDL translation is available

Rules:
- if a temporary translator is necessary, keep it narrow and movement-specific
  only as a shrinking bridge
- do not add a second permanent command loop

### Slice 5. Legacy Defaults And Cleanup
Goal:
- remove movement-specific legacy assets and parser ownership once the new path
  is live

Write set:
- `lib/pref/pref.prf`
- `lib/pref/pref-sdl.prf`
- `src/fs/pref-files.c`
- `src/util-convert.c`
- follow-up docs touched by the cleanup

Outputs:
- shipped defaults no longer define movement through keymap or macro text
- movement-specific legacy parser ownership reduced or deleted

Dependencies:
- starts only after Slices 2, 3, and 4 are landed

Rules:
- do not start cleanup while live gameplay still needs the legacy movement
  path

## Parallel Execution Rules
- Slice 0 starts first and remains active until the end for measurement.
- Slice 1 is the interface-defining slice and should land before the others
  spread out.
- Slices 2, 3, and 4 can run in parallel once Slice 1 has frozen the semantic
  movement interface.
- Slice 5 starts last, after the new settings path, SDL translation, and
  gameplay consumers are all live.
- If two slices need the same file, stop and re-cut the boundary instead of
  letting both slices edit it.
- Prefer thin integration commits between slices over broad rebases.

## Settings UX Requirements
- Binding capture must record actual modifier chords, not the post-shift text
  character.
- The UI must display normalized labels such as `Up`, `Shift+Up`, `Ctrl+H`,
  `Alt+Numpad 8`.
- The settings UI must be built as normal semantic settings UI and must not
  add new document-cell or terminal-size coupling while this work lands.
- The UI must detect conflicts before save and offer:
  - swap
  - clear old binding
  - cancel
- Essential actions must remain bound:
  - move in 8 directions
  - wait
  - run in 8 directions
  - interact in 8 directions
- Preset selection should be explicit and reversible.
- Saving should go through the same settings flow already used for controller
  bindings, not a separate macro dump prompt.

## Compatibility Policy
- Do not delete the global macro system in the first pass unless non-movement
  command families are also migrated.
- Movement must stop depending on macros even if the rest of the game still
  supports them temporarily.
- The movement rewrite is allowed to leave unrelated command families on the
  legacy path temporarily, but it is not allowed to create a fresh movement-only
  compatibility bridge that would survive the extermination work.
- Do not add startup import logic for old movement keymaps.
- Unknown or advanced automation macros should remain opt-in legacy behavior
  until a broader input rewrite is planned.

## Validation
- Unit coverage:
  - binding lookup by key plus modifiers
  - conflict detection
  - preset load and reset
- SDL smoke coverage:
  - walk in 8 directions
  - run with `Shift + direction`
  - interact with `Ctrl + direction`
  - wait and rest
  - targeting direction prompts
  - open, close, bash, tunnel, and alter prompts
  - controller and touch bindings still produce the same movement actions
- Regression checks:
  - the expanded extermination audit decreases or stays flat for the input
    family during the work
  - no movement combo requires macro-trigger bytes
  - no movement settings save path writes `A:` / `P:` / `C:` movement records
  - no directional lookup depends on scanning action strings for digits
  - no new gameplay-facing wrappers are added around `inkey()` or
    `request_command()`

## Recommended Order
1. Run Slice 0 first and check in the audit baseline.
2. Land Slice 1 and freeze the semantic movement interface.
3. Run Slices 2, 3, and 4 in parallel with disjoint ownership.
4. Integrate the slices and verify the movement path end to end.
5. Run Slice 5 to delete movement-specific legacy defaults and remaining
   cleanup leftovers.

## Done When
- Movement is bound by settings, not by macro text.
- Modifier chords are explicit data, not encoded byte sequences.
- Direction is carried as structured payload, not extracted from `;8`-style
  strings.
- `hjkl_movement` and `angband_keyset` are no longer live movement
  architecture switches.
- A player can configure movement entirely through the settings UI without
  touching `.prf` files or learning macro syntax.
- The movement rewrite has reduced the active terminal-model debt tracked by
  the extermination plan, rather than shifting that debt into a new wrapper.
