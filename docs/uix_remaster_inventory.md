# Sil-More UIX Remaster Inventory

Status: initial implementation spec for the semantic-pixel UIX remaster.

## Product Direction

Sil-More UI should feel like a modern remaster of an old-school roguelike:
the game keeps pixel art, compact density, direct commands, and serious
First Age atmosphere, but menus should no longer look like terminal-era
placeholders. Mouse, touch, keyboard, and gamepad must operate the same
semantic widgets.

Chosen defaults:
- Input model: unified input across mouse, touch, keyboard, and gamepad.
- Visual model: Pixel-Lore Modern, using crisp pixel art, restrained stone,
  parchment, metal, and dark-glass accents.
- Movement model: staged smart movement, starting with adjacent click/tap and
  inspect, then safe path preview, then full click-to-travel.

## Shared UIX Rules

- Every repeated control has a stable semantic widget id, role, action, focus
  order, optional tooltip, and pointer/touch hit target.
- Hover-only information is forbidden. The same detail must be available by
  mouse hover, keyboard/gamepad focus, and touch long-press.
- Touch-first controls target 48 logical px where space permits. Dense text
  rows must not fall below a 24 logical px hit target unless an equivalent
  larger action exists.
- Main dungeon scale remains integer-scaled. UI overlays use logical pixels and
  may scale independently through menu density and platform display scale.
- New UI-facing work stays behind `src/app/*`, `src/ui/*`, or SDL frontend
  render/input seams. Do not reintroduce terminal APIs, `inkey()` call sites,
  `screen_save/load`, or direct `Term_*` surfaces.

## Out-Of-Game Flat Screens

Screens:
- Welcome, bootstrap/loading, main navigation, story, death/victory, tomb.
- Halls of Mandos, run history, run detail, metarun history.
- Character selection, birth, oath/stat allocation, blitz setup, character
  sheet, dump/export prompts.

Target design:
- Use interactive hub layouts: illustrated header or background, card/list
  region, detail preview, and stable footer actions.
- Character creation uses comparison cards, trait tags, stat bars, warnings,
  portrait/crest art, and a preview sheet. Existing mutation points remain in
  the birth flow until final confirmation.
- Halls/run history use sortable lists, filters, run timeline/detail panes,
  trophy markers, and clickable monster/item/artefact recall.

## In-Game Overlays

Screens:
- Top/bottom strips, left rail, inventory, equipment, floor items, item
  selector, object info, identify/recharge prompts.
- Look, targeting, nearby list, map/minimap, monster/object recall, message
  log, hint log, combat history, spell/item submenus, smithing.

Target design:
- Overlays are semantic panels with hit-tested rows, footer actions, tabs,
  context actions, optional pin state, and future drag/resize handles.
- Inventory/equipment preserve current layout semantics, floor `-)` shortcut,
  list cycling, and comparison behavior while gaining pointer/touch activation.
- Look and targeting expose direct map/entity hit testing: hover/focus/tap-hold
  inspects, adjacent click/tap interacts, and right-click/long-press recalls.

## Knowledge And Data Screens

Screens:
- Knowledge root/browser, supplies, help, file viewer, settings, keybinds,
  controller, visuals, quest status, quest typewriter, metarun stats/history,
  blessings/curses, smithing recipes/options, debug/spoiler browsers.

Target design:
- One shared browser shell handles tabs, search, sort/filter, list-detail,
  scroll, sticky actions, and predictable focus order.
- Rich detail content can include icons, compact tables, stat bars, lore text,
  and recall links, but all data remains generated from existing game state.

## Implementation Checklist

- Foundation: interactive metadata in `app_ui_scene`, SDL hit registry, pointer
  event submission, and key-bridge activation for legacy wait loops.
- Next: replace key-bridge consumers with semantic intents in high-traffic
  menus, starting with welcome, main menu, inventory/equipment, item selector,
  look/targeting, and knowledge browser.
- Then: add hover/focus state, tooltip/long-press detail, visual remaster
  tokens, draggable overlays, resizable panes, live integer scale controls, and
  persisted layout state in `sil_sdl.json`.
- Validation: `py -3 tools\ui_debt_audit.py --check`,
  `py -3 tools\modernization_audit.py --details`, relevant `ctest`, and the
  Windows SDL build path.
