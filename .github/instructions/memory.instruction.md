---
applyTo: '**'
---

# Sil-More Project Memory & Knowledge Base- Makefile.cocoa - macOS with Cocoa interface

**Recommended Windows Build Process**: Use Cygwin with the launch target:
```bash
C:\Soft\cygwin\bin\bash.exe -l -c "cd /cygdrive/c/Users/efrem/Documents/GitHub/sil-qh/src && make -f Makefile.cyg launch"
```

**IMPORTANT BUILD NOTES**:
- Use `make launch` target which includes: movebin (moves exe to root) + exec (runs from root)
- Game MUST be run from root directory (/sil-qh) not from src directory  
- Renaming the .exe breaks ini file loading - keep original name
- lib folder must be in root directory for proper game data access

## Quest Logic Refactoring - Critical Issues & Solutionsject Overview
Sil-Morë is a Tolkien-themed ASCII roguelike game forked from Sil, which itself descends from Angband. The game features stealth mechanics, Tolkien lore integration, and a complex quest system involving the Valar (Tulkas, Aule, Mandos).

## CRITICAL BUGS DISCOVERED AND FIXES NEEDED

### URGENT ISSUE 1: Quest Vault Partial Visibility
**ROOT CAUSE IDENTIFIED**: Debug visibility forcing code was causing partial rendering
- The DEBUG_QUEST_VAULT mode was force-marking vault areas with `CAVE_MARK|CAVE_SEEN|CAVE_GLOW`
- This artificial visibility forcing interfered with natural game visibility rules
- Partial visibility occurred when debug code conflicted with normal rendering

**FIX IMPLEMENTED**: Removed forced visibility marking in DEBUG_QUEST_VAULT mode
- Commented out the line: `for (int ry = y1; ry <= y2; ++ry) for (int rx = x1; rx <= x2; ++rx) cave_info[ry][rx] |= (CAVE_MARK|CAVE_SEEN|CAVE_GLOW);`
- Quest vaults now follow natural visibility rules (player must discover them normally)
- Debugging still captures vault layout without forcing artificial visibility 

### URGENT ISSUE 2: Quest State Persistence Broken  
**ROOT CAUSE IDENTIFIED**: Quest states saved separately from metarun completion tracking
- Player quest states (p_ptr->mandos_quest) saved in character save files  
- Metarun quest completion flags saved separately in metarun files
- New characters don't restore quest states from metarun completion tracking
- Result: Quest states reset to 0 despite metarun tracking completion

**FIX IMPLEMENTED**: Added `metarun_restore_quest_states()` function called after character loading
- Function checks metarun completion flags and restores appropriate quest states
- Sets completed quests to REWARDED state for new characters in the same metarun
- Added comprehensive logging for quest state restoration process
- Added function to metarun.h header and called from load.c after character data loaded

### FIXED ISSUE 3: Metarun Quest Completion Logic  
**STATUS**: Resolved - `metarun_is_quest_completed()` now scans all metaruns (verified in `metarun.c`). Persistence bug fixed with write-back ordering.

### URGENT ISSUE 3: Quest Vault Visibility Problem - UNDER INVESTIGATION
**PROBLEM**: Quest vaults place successfully but are sometimes invisible to players
- Log shows successful placement but user screenshot shows no vault
- Enhanced debugging added and comprehensive analysis performed

**ROOT CAUSE ANALYSIS COMPLETED**:
1. **Vault placement is successful** - Logs confirm proper construction
2. **CAVE_ICKY protection works** - Corridors respect vault boundaries
3. **Room registration works** - Quest vaults properly added to dungeon room list
4. **Debug mode force-reveals vault** - Should make them always visible

**LIKELY CAUSES IDENTIFIED**:
- **Connectivity issues**: Quest vaults may be isolated from main dungeon despite corridor generation
- **Visibility flag issues**: Vault area may not be properly marked as seen/known to player
- **Rendering problems**: Vault exists but display system doesn't show it

**DEBUGGING ENHANCEMENTS IMPLEMENTED**:
- Added comprehensive quest vault state tracking
- Enhanced logging for vault visibility and connectivity status  
- Added post-generation validation for quest vault accessibility
- Force-marking quest vault areas as visible in debug mode
- Detailed vault layout dumping and change tracking

**NEXT STEPS**:
- Run test games to collect enhanced debug logs
- Verify vault connectivity to main dungeon network
- Check if corridors properly connect to quest vault room entrances
- Investigate potential display/rendering issues

## Architecture & Build System

### Core Structure
- **Primary Language**: C (C11 compatible)
- **Main Header**: src/angband.h - Include this in all source files
- **Version**: 0.8.6 (see defines.h for version constants)
- **Platform Support**: Windows, macOS, Linux/Unix, DOS

### Build System
Multiple platform-specific Makefiles exist:
- Makefile.std - Unix/Linux systems
- Makefile.cyg - Cygwin on Windows (recommended for Windows development)
- Makefile.win - Windows native
- Makefile.osx - macOS
- Makefile.cocoa - macOS with Cocoa interface

**Recommended Windows Build Process**: Use Cygwin with the command:
`ash
C:\Soft\cygwin\bin\bash.exe -l -c "cd /cygdrive/c/Users/efrem/Documents/GitHub/sil-qh/src && make -f Makefile.cyg -j8 launch"
`

## Quest Logic Refactoring - Critical Issues & Solutions

### UPDATED ISSUE: Metarun Quest Completion Not Persisting
Root Cause Identified: `metarun_mark_quest_completed()` wrote directly to `metaruns[current_run]` but `save_metaruns()` overwrote that array entry with the stale copy from the global `metar` struct (`metaruns[current_run] = metar;`). Result: newly set quest bits were lost on save → reload showed zeros.

Fix Implemented: Function now updates `metar.completed_quests` first, mirrors the bit to the array (optional), then calls `save_metaruns()`. Added trace logging to confirm bitmask after update.

### UPDATED ISSUE: Quest Vault Visibility / Overwrite
Observation: Logs report successful quest vault placement but vault sometimes absent. Hypothesis: Subsequent normal room/vault generation may succeed elsewhere causing player not to encounter original location, or extremely rare later carving could intrude if CAVE_ICKY not set early enough. Current code places quest vault before other rooms and uses `CAVE_ICKY`, so outright overwrites are unlikely; invisibility may stem from failed placement after logging or from player not reaching coordinates.

Mitigation Applied: (a) Additional tracing already present. (b) Confirmed early placement sequence. (c) Future enhancement suggestion: store last quest vault bbox & echo to player on debug mode; optionally add a map reveal cheat flag. (No structural change required yet.)

### FIXED ISSUES 

#### Special Abilities System
- **Fixed**: Special abilities not persisting - Modified calc_bonuses() to preserve S_SPC abilities
- **Fixed**: Special skill showing in menu - Hidden S_SPC in skill display functions
- **Fixed**: Abilities menu showing all slots - Filtered to show only granted abilities

#### Quest State Management  
- **Fixed**: Added MANDOS_QUEST_REWARDED state to prevent repeated reward interactions
- **Fixed**: Aule quest skill check - Changed from total skill to base skill only
- **Fixed**: Quest reservation system to prevent multiple quests per run

## Key Technical Details

### Metarun System
- File: src/metarun.c
- Function: metarun_check_and_update_quests() - Should be called from update_stuff()
- Persistence: save_metaruns() called on game exit
- Quest flags: METARUN_QUEST_TULKAS, METARUN_QUEST_AULE, METARUN_QUEST_MANDOS

### Quest States
- **Mandos**: NOT_STARTED(0)  GIVER_PRESENT(1)  ACTIVE(2)  SUCCESS(3)  REWARDED(4)
- **Aule**: NOT_STARTED(0)  FORGE_PRESENT(1)  ACTIVE(2)  SUCCESS(3)  REWARDED(4)
- **Tulkas**: NOT_STARTED(0)  ACTIVE(1)  COMPLETE(2)  REWARDED(3)

### Quest Vault System
- File: src/generate.c
- Placement: 	ry_quest_vault_type() function
- Collision: solid_rock() and CAVE_ICKY flag system
- Processing: process_quest_vault_area() function

## Files Modified in Quest Refactoring
- src/defines.h: Added MANDOS_QUEST_REWARDED 4
- src/xtra2.c: Fixed Mandos quest state management and reward handling  
- src/metarun.c: Enhanced quest completion detection for all states
- src/generate.c: Fixed Aule quest to check base smithing skill, enhanced debugging
- src/xtra1.c: Fixed special abilities persistence in calc_bonuses()
- src/cmd4.c: Fixed abilities menu filtering

## Fix Summary (Latest)
1. Metarun Persistence: Corrected quest flag save ordering (write live `metar` first) and added idempotent guard.
2. Verification: `metarun_is_quest_completed()` already scans full history; memory doc updated to reflect.
3. Vault Overwrite: No direct overwrite bug found; monitoring continues with existing diagnostics.

## Current Debugging Status
Enhanced logging active across quest, metarun, and vault systems. Persistence fix pending runtime verification across save/load cycles.

## User Preferences & Context
- Working on Windows with Cygwin build environment
- Focused on quest system reliability and persistence
- Requires proper metarun functionality for quest completion tracking
- Needs vault visibility issues resolved for quest accessibility
