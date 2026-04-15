# UI Terminal Extermination Plan

Status: complete on April 15, 2026.

## Outcome
- The SDL runtime no longer routes control flow through `TERM_XTRA_*`.
- The SDL runtime no longer stores or redraws from terminal-style shadow
  buffers, dirty spans, hook tables, or compatibility key queues.
- Character dumps no longer embed miniature screenshot output captured from
  faux screen cells.
- The `terminal_kernel` audit baseline is locked at zero and now guards
  against reintroducing terminal-era SDL internals.

## Completed Slices
### A. Expand the audit
- `tools/ui_debt_audit.py` now measures:
  - `TERM_XTRA_*`
  - SDL hook-table symbols
  - SDL compatibility key-queue symbols
  - SDL shadow-buffer or dirty-row symbols
  - miniature screenshot mirror symbols
- `tests/ui_debt_audit_baseline.json` records the zero-debt end state.

### B. Remove `TERM_XTRA_*` and hook-driven control flow
- Direct SDL and `platform_frame_*` helpers own event pumping, clear, delay,
  present, and react behavior.
- No `TERM_XTRA_*` symbol remains in active SDL code.
- No SDL view-state hook table remains.

### C. Remove shadow buffers and dirty-row redraw
- `src/sdl-main-internal.h` no longer defines SDL shadow-buffer state.
- `src/sdl-render.c` no longer replays row or cell compatibility buffers.
- Full view refresh now rebuilds semantic snapshot state and refreshes
  supporting panes through the semantic window-scene path.

### D. Rewrite exports
- `src/files.c` no longer owns miniature screenshot buffers or snapshot
  sampling helpers.
- The dungeon death path no longer captures a miniature screenshot.
- Character dumps no longer emit screenshot sections derived from compatibility
  screen-cell mirrors.

### E. Refresh baseline and docs
- The audit notes and baseline now describe a zero-debt reintroduction
  guardrail rather than live remaining debt.
- This document now records the completed terminal-free SDL state.

## Zero-Debt Invariants
- `py -3 tools/ui_debt_audit.py --audit terminal_kernel` reports zero matches
  for every metric.
- `py -3 tools/ui_debt_audit.py --check` passes against the checked-in
  baseline.
- The existing `terminal_model` audit remains at zero.
- Any remaining references to grids, rows, columns, or cells are gameplay-space
  or scene-layout concepts, not terminal UI compatibility state.

## Validation
- Run `py -3 tools/ui_debt_audit.py --details`.
- Run `py -3 tools/ui_debt_audit.py --check`.
- Build with `.\build-cmake.bat` or `cmake --build build-standard --parallel`.
- Smoke-test:
  - command input, including repeat counts and keymaps
  - redraw and resize behavior across main and supporting panes
  - story pages, help pages, and browser or document views
  - inventory, equipment, monster recall, and monster list panes
  - character dump generation
