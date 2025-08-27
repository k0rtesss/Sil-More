---
applyTo: '**'
---

# Sil-More Project Memory & Knowledge Base

## Project Overview
Sil-Morë is a Tolkien-themed ASCII roguelike game forked from Sil, which itself descends from Angband. The game features stealth mechanics, Tolkien lore integration, and a complex quest system involving the Valar (Tulkas, Aule, Mandos).

## CRITICAL BUGS DISCOVERED AND FIXES NEEDED

## CRITICAL BUGS DISCOVERED AND FIXES NEEDED

### FIXED ISSUE 1: Quest Vault Placement Strategy
**RESOLVED**: Quest vault placement now uses forced forge strategy
- **Root Cause**: Quest vaults used random placement (50 attempts) with 4-cell padding, while forced forge used strategic center placement with better location selection
- **Key Differences Found**:
  - Forced forge: Single strategic location (map center), fails level if can't place
  - Quest vaults: 50 random locations, continues if fails
  - Both used same 4-cell padding, but quest vaults often couldn't find space
  - Quest vault templates include both small (11x11) and large (39x9) vaults
- **Solution Implemented**: 
  - Quest vaults now use center-focused placement like forced forge
  - Added `place_room_forced()` with reduced padding (1-cell instead of 4-cell)
  - Strategic fallback locations near map center
  - Enhanced logging for debugging
- **Files Modified**: src/generate.c (quest vault placement logic)

### FIXED ISSUE 2: Metarun Quest Persistence Logic
**RESOLVED**: Quest states now only persist in metarun when fully REWARDED
- **Root Cause**: Metarun completion was triggered by SUCCESS state (state 3) instead of REWARDED state (state 4/5)
- **Problem**: If player completed quest but didn't get reward (or died), quest was still marked as "metarun completed", preventing re-attempts in new runs
- **Quest State Reference**:
  - Aule: 0=NOT_STARTED, 1=FORGE_PRESENT, 2=ACTIVE, 3=SUCCESS, 5=REWARDED
  - Mandos: 0=NOT_STARTED, 1=GIVER_PRESENT, 2=ACTIVE, 3=SUCCESS, 4=REWARDED  
  - Tulkas: 0=NOT_STARTED, 1=GIVER_PRESENT, 2=COMPLETE, 4=REWARDED
- **Solution**: Modified `metarun_check_and_update_quests()` to only mark quests as metarun-completed when REWARDED
- **Files Modified**: src/metarun.c (quest completion logic)

### FIXED ISSUE 3: Two Critical Metarun Quest Bugs - RESOLVED
**BUG 1**: Quest states persist when loading dead saves - **FIXED**
- Problem: Dead characters were having quest progress restored from metarun
- Solution: Modified birth.c to only restore quest states for living characters
- Added check: `if (character_loaded && !p_ptr->is_dead)` instead of just `if (character_loaded)`

**BUG 2**: metarun_is_quest_completed() checks ALL metaruns - **FIXED**  
- Problem: Function checked all metaruns instead of current metarun only
- Log showed: "Found quest 0x4 completed in metarun[1]" when checking from metarun[2]
- Solution: Modified function to only check `metaruns[current_run]` instead of looping through all metaruns
- Now correctly prevents quest spawning only within the same metarun

**FILES MODIFIED**:
- src/metarun.c: Fixed metarun_is_quest_completed() to check current metarun only
- src/birth.c: Fixed quest state restoration to skip dead characters

### FIXED ISSUE 3: Metarun Quest Completion Logic
Resolved: `metarun_is_quest_completed()` now scans all metaruns (verified in `metarun.c`). Primary persistence bug source shifted to write‑back ordering (see Fix Summary below).

### FIXED ISSUE 5: Forge Forcing Logic - COMPLETELY FIXED ✅

**PROBLEM IDENTIFIED AND RESOLVED**: Forge forcing was happening on levels beyond the target levels (2, 6, 10) due to incorrect comparison operator.

**Root Cause Analysis**:
- The guaranteed forge logic used `<=` comparison: `next_guaranteed_forge_level <= p_ptr->depth`
- This caused forge forcing to activate at level 2 AND ALL DEEPER LEVELS, instead of just at levels 2, 6, and 10
- Example: On level 15 with `fixed_forge_count=2`, `next_guaranteed_forge_level=10`, so `10 <= 15` = true
- Result: `force_forge=true` was incorrectly set on level 15, affecting quest vault generation and normal vault placement

**Complete Solution Implemented**:
```c
// Fixed logic - only force at exact levels
int next_guaranteed_forge_level = 2 + (p_ptr->fixed_forge_count * 4);
is_guaranteed_forge_level = (next_guaranteed_forge_level == p_ptr->depth);
```

**Behavior After Fix**:
- ✅ Forge forcing only occurs at levels 2, 6, and 10 exactly
- ✅ Quest vault generation on level 15 sees `force_forge=false` correctly
- ✅ Normal vault generation works properly without inappropriate forge restrictions

**Files Modified**: `src/generate.c` - Fixed guaranteed forge level logic from `<=` to `==` comparison

### FIXED ISSUE 7: Quest Ability Display and Multiple Quest Spawning - RESOLVED ✅

**PROBLEM 1**: Mandos' Doom ability not properly updating display when clearing confusion
- **Root Cause**: Direct assignment `p_ptr->confused = 0` bypassed proper status clearing functions
- **Solution**: Use proper status functions `set_confused(0)`, `set_afraid(0)`, `set_stun(0)`, etc. that trigger display updates
- **Files Modified**: `src/xtra1.c` - Fixed status effect clearing in Mandos ability

**PROBLEM 2**: Tulkas and Mandos spawning simultaneously on same level
- **Root Cause**: Quest reservation (`quest_reserved[0]`) was set too late - only when pending states were applied at end of level generation
- **Solution**: Set `quest_reserved[0] = 1` immediately when quest vault is placed, preventing other quests from spawning during same generation
- **Files Modified**: `src/generate.c` - Added immediate quest reservation when quest vaults are placed

**PROBLEM 3**: Quest vault visibility and padding (In Progress)
- **Analysis**: Quest vaults use 1-cell padding vs normal vaults' 2-cell padding. User requested padding normalization.
- **Current Status**: Need to determine if quest vaults should match normal vault padding (2-cell) or if padding should be standardized to 1-cell

**System Behavior After Fixes**:
1. ✅ Mandos' Doom properly clears confusion with immediate display update
2. ✅ Only one quest can spawn per level (quest reservation works immediately)
3. 🔄 Quest vault placement and visibility being reviewed

### FIXED ISSUE 6: Quest Reward Abilities - CORRECTED IMPLEMENTATION ✅

**CORRECTED IMPLEMENTATION**: Quest reward abilities have been fixed to work according to their intended design.

**Abilities Implementation Corrected**:

1. **Aule's Forge** (`SPC_AULE`):
   - ✅ **Granted correctly**: Sets both `have_ability` and `active_ability` on quest completion
   - ✅ **Blocks Masterpiece purchase**: Prevents purchasing inferior smithing ability
   - ✅ **Works like Masterpiece + 2**: Allows smithing items beyond skill level by adding base skill + 2 extra difficulty points
   - ✅ **Efficient skill drain**: Drains only 1 smithing skill for every 2 excess difficulty points (vs Masterpiece's 1:1 ratio)
   - ✅ **Actually drains skill**: Uses the same `smithing_cost.drain` system as Masterpiece to consume skill points
   - **Usage**: Added alongside existing Masterpiece checks in smithing functions

2. **Mandos' Doom** (`SPC_MANDOS`):
   - ✅ **Granted correctly**: Sets both `have_ability` and `active_ability` on quest completion
   - ✅ **Mental immunity**: Grants +100 resistance to fear, hallucination, stun, and confusion
   - ✅ **Effect clearing**: Automatically clears fear, hallucination, entrancement, rage, stun, and confusion each turn
   - **Usage**: Applied in `calc_bonuses()` function in `xtra1.c`

**Code Implementation**:
- Aule's Forge now properly works alongside Masterpiece ability using the same skill drain system
- Added Aule checks to `calculate_smithing_cost()` and `too_difficult()` functions  
- Skill drain calculation: `smithing_cost.drain += (excess + 1) / 2` for efficient 1:2 ratio
- Mandos' Doom includes full confusion immunity and clearing
- Debug logging added to verify ability activation

**Testing Requirements**:
- **Aule's Forge**: Test by attempting high-difficulty smithing projects - should allow Masterpiece effect +2 with efficient drain and actual skill consumption
- **Mandos' Doom**: Test by encountering mental effects - should be immune to all mental effects including confusion

**Files Modified**: `src/xtra1.c`, `src/cmd4.c`, `lib/edit/ability.txt`, `src/generate.c` - Corrected ability implementations and forge forcing logic

**PROBLEM IDENTIFIED AND RESOLVED**: Quest vaults were being successfully placed but lost during level regeneration because quest states persisted between generation attempts.

**Root Cause Analysis**: 
- Quest vault placed in first attempt → quest state set to GIVER_PRESENT → level generation fails due to connectivity
- During regeneration, quest state persisted → quest vault logic sees "quest already active" → skips quest vault placement
- Result: Quest vault disappears from final level despite being successfully placed initially

**Complete Solution Implemented**:

1. **Enhanced Pending Quest State System**: Already existed to defer quest state changes until successful generation
2. **Added Quest State Reset Function**: `reset_quest_vault_states()` resets quest states from previous failed attempts
3. **Integrated Reset Logic**: Called at start of each generation attempt to provide clean slate
4. **Safe Level-Specific Reset**: Only resets quest states that were set at current level (prevents interference with legitimate quests)
5. **Quest Reservation Reset**: Also resets quest_reserved[0] when quest states are reset

**Key Code Changes in `src/generate.c`**:
```c
// New function to reset quest states from failed attempts
static void reset_quest_vault_states(void) {
    bool reset_reservation = false;
    
    if (p_ptr->aule_quest == AULE_QUEST_FORGE_PRESENT && p_ptr->aule_level == p_ptr->depth) {
        p_ptr->aule_quest = AULE_QUEST_NOT_STARTED;
        p_ptr->aule_level = 0;
        reset_reservation = true;
    }
    
    if (p_ptr->mandos_quest == MANDOS_QUEST_GIVER_PRESENT && p_ptr->mandos_level == p_ptr->depth) {
        p_ptr->mandos_quest = MANDOS_QUEST_NOT_STARTED;
        p_ptr->mandos_level = 0;
        reset_reservation = true;
    }
    
    if (reset_reservation && p_ptr->quest_reserved[0]) {
        p_ptr->quest_reserved[0] = 0;
    }
}

// Called at start of each generation attempt:
reset_pending_quest_states();
reset_quest_vault_states();
```

**Fix Verification**: 
- Log messages show successful quest state reset: "Quest vault regeneration: Resetting Mandos quest from GIVER_PRESENT to NOT_STARTED"
- Quest vault logic now sees clean state on regeneration attempts
- Quest vaults can be properly re-placed during level regeneration
- Legitimate quests on other levels remain unaffected

**System Behavior After Fix**:
1. ✅ Quest vault placed → pending state recorded → level fails → states reset
2. ✅ Regeneration attempt → clean quest state → quest vault can be placed again  
3. ✅ Level succeeds → pending states applied → quest properly activated
4. ✅ Legitimate quests on other levels protected by level-specific reset logic

**Result**: Quest vaults now persist through level regeneration correctly, fixing the disappearing quest vault bug.

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
1. **Quest Vault Placement**: Implemented forced placement strategy with center-focused location selection and reduced padding (1-cell vs 4-cell)
2. **Metarun Quest Persistence**: Fixed to only mark quests as metarun-completed when REWARDED (not SUCCESS), allowing re-attempts if player didn't get reward
3. **Quest Vault Level Regeneration**: Fixed level regeneration discarding quest vaults due to insufficient room count - reduced minimum room requirement when quest vault present
4. **Verification**: Enhanced logging active across quest, metarun, and vault systems

## Files Modified in Latest Quest Fixes
- src/generate.c: Quest vault placement strategy (forced placement + reduced padding) + level regeneration fix (dynamic room requirements)
- src/metarun.c: Quest completion persistence logic (REWARDED-only marking)

## User Preferences & Context
- Working on Windows with Cygwin build environment
- Focused on quest system reliability and persistence
- Requires proper metarun functionality for quest completion tracking
- Needs vault visibility issues resolved for quest accessibility

## Latest Fixes Applied

### FIXED ISSUE 8: Tulkas Quest Reservation - RESOLVED ✅

**PROBLEM**: Tulkas spawning did not consistently set quest reservation, allowing multiple quests to spawn simultaneously.
- **Root Cause**: Main Tulkas spawn logic at line 4762 in generate.c set quest state but not quest_reserved[0]
- **Secondary Path**: Fallback spawn logic at line 4786 correctly set both quest state and reservation
- **Result**: Tulkas could spawn via main path without blocking other quests from spawning

**Solution Applied**:
```c
// Added quest reservation to main Tulkas spawn path
if (place_monster_one(try_y, try_x, R_IDX_TULKAS, true, true, NULL))
{
    p_ptr->tulkas_quest = TULKAS_QUEST_GIVER_PRESENT;
    p_ptr->quest_reserved[0] = 1; /* Mark any quest spawned */
    tulkas_spawned = true;
    // logging...
}
```

**System Behavior After Fix**:
- ✅ Tulkas spawning immediately reserves quest slot via both spawn paths
- ✅ Quest vault placement uses pending system (no immediate reservation)
- ✅ Only one quest can be active per level (proper reservation pattern)

**Files Modified**: `src/generate.c` - Added quest reservation to main Tulkas spawn logic

### Build Process Updated
```bash
cd c:\Users\efrem\Documents\GitHub\sil-qh\src
C:\Soft\cygwin\bin\bash.exe -l -c "cd /cygdrive/c/Users/efrem/Documents/GitHub/sil-qh/src && make -f Makefile.cyg -j8"
cd c:\Users\efrem\Documents\GitHub\sil-qh
Copy-Item src\sil.exe . -Force
```
