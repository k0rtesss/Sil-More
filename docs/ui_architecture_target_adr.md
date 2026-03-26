# ADR: UI Session, Snapshot, And Event Boundary

Status: accepted for UI0 scaffolding on March 27, 2026.

## Context
The current UI boundary is still terminal-first:

- gameplay code blocks in `inkey()`
- overlay flows depend on `screen_save()` / `screen_load()`
- dungeon rendering still emits `Term_*` commands directly
- `platform-ui.h` still exposes SDL-shaped names into core-facing code

That boundary prevents an SDL scene stack, keeps cadence tied to blocking UI
loops, and makes a future web/mobile frontend harder than it needs to be.

## Decision
Adopt a frontend-neutral application boundary under `src/app/` with five public
surfaces:

- `app-session.h`: session lifecycle and advancement
- `app-input.h`: low-level input events plus higher-level intents
- `app-snapshot.h`: renderable state snapshots
- `app-events.h`: compact event records emitted by the core
- `app-host.h`: host callbacks for time, persistence, capabilities, and logging

The `Term` path remains a legacy compatibility frontend during migration. It is
not the long-term API that new UI features should target.

## Boundary Rules
- `sil-core` stays authoritative for gameplay, persistence, content parsing,
  interaction state, snapshots, and emitted events.
- Frontends own frame cadence, layout, scene stacks, animations, and physical
  input binding.
- Public boundary types stay plain C and SDL-free.
- The session driver advances the core until a wait reason is reached rather
  than letting platform code block inside gameplay loops.

Target usage shape:

```c
app_submit_input(session, &input);
app_advance_until_waiting(session);
const app_snapshot *snapshot = app_get_snapshot(session);
const app_event_span *events = app_drain_events(session);
```

## Input Model
Two input layers are first-class:

- legacy low-level key/event injection for the compatibility frontend
- higher-level intent submission for SDL scene work and later web/mobile hosts

This keeps keymaps and macro behavior working while new scenes move to intent
driven flows.

## Output Model
Two output layers are first-class:

- snapshots for full renderer state
- event spans for animation and frontend bookkeeping

Snapshots describe what to render. Events describe what changed. Frontends may
animate from events, but the snapshot remains authoritative.

## Consequences
- New UI architecture work should prefer `src/app/*` even before the SDL scene
  stack is complete.
- Legacy additions must stay inside already-tagged migration surfaces listed in
  [`ui_migration_inventory.md`](./ui_migration_inventory.md).
- `platform-ui.h`, `inkey()`, `screen_save()` / `screen_load()`, and direct
  `Term_*` render/control calls are treated as measurable legacy debt.
- UI0 establishes a baseline gate with
  [`tools/ui_debt_audit.py`](../tools/ui_debt_audit.py) and
  [`tests/ui_debt_audit_baseline.json`](../tests/ui_debt_audit_baseline.json).

## Non-Goals
- rewriting gameplay systems for style reasons
- introducing SDL or toolkit types into the new public boundary
- removing the legacy frontend before snapshots, events, and wait states exist
