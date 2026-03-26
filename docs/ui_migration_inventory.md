# UI Migration Inventory

This UI0 inventory tags the main migration surfaces so later stages can move
work without reopening the same boundary question each time.

## Inventory
| Module set | Tag | First focus | Notes |
| --- | --- | --- | --- |
| `src/app/*` | early boundary | UI0-UI2 | Reserved neutral boundary surface for session, input, snapshot, event, and host APIs. |
| `src/main.c`, `src/runtime/runtime-game.c`, `src/runtime-cli.c`, `src/init2.c`, `src/fs/*` | early boundary | UI1-UI2 | Own outer-loop control, host wiring, runtime paths, and persistence seams needed to make the core externally drivable. |
| `src/cave.c`, `src/ui/ui-status.c`, `src/util-message.c`, `src/ui/ui-look-sidebar.c`, `src/melee/melee-combat-display.c` | early snapshot | UI3 | Already own most map, status, top-line, and pane data needed for the first dungeon snapshot/event pass. |
| `src/main-sdl.c`, `src/sdl-layout.c`, `src/sdl-render.c`, `src/sdl-touch.c`, `src/sdl-story-font.c`, `src/sdl-config.c`, `src/sdl-sound.c`, `src/pane.c` | early frontend-only | UI4 | SDL scene/render/layout ownership belongs here, not in gameplay modules. |
| `src/util-input.c`, `src/util-prompt.c`, `src/object/object-ui-select.c`, `src/targeting.c`, `src/cmd/ui/cmd-ui-look.c`, `src/cmd/item/*` | mid-stage interaction extraction | UI5 | These still fuse gameplay state with blocking terminal control flow; move them after the session/wait model is real. |
| `src/cmd/ui/cmd-ui-settings.c`, `src/ui/ui-help.c`, `src/ui/ui-file-viewer.c`, `src/score/score_ui.c`, `src/quest/quest-ui.c`, `src/ui/ui-story.c`, `src/ui/ui-death.c`, `src/ui/ui-character-screen.c` | early frontend-only | UI6 | These are mostly informational or presentation-heavy scenes once the frontend can query the underlying data. |
| `src/z-term.c`, `src/platform-ui.h` | early frontend-only | UI7 | Legacy compatibility anchor. Keep stable during UI0-UI6 and isolate only after the neutral boundary exists. |
| `src/birth.c`, `src/ui/smithing/ui-smithing-screen.c`, `src/metarun.c`, `src/blitz.c` | late bespoke flow | UI5-UI7 | Large custom workflows with nontrivial prompt/state choreography; migrate after generic interaction primitives are proven. |

## Standing Rule
- New UI-facing work should land behind `src/app/*`.
- If a change cannot use the new boundary yet, keep it inside a module already
  listed above and treat the touched API as legacy debt, not as a pattern to
  copy elsewhere.
- Run [`tools/ui_debt_audit.py`](../tools/ui_debt_audit.py) after UI
  architecture changes to confirm counts did not grow.
