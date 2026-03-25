cmake_minimum_required(VERSION 3.18)
project(sil-more C)

# Project language standard
set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(LOG_MODE_TIMESTAMP "Create new log with timestamp each time" OFF)
option(STEAMDECK_SUPPORT "Build for Steam Deck" OFF)
option(SIL_USE_LOCAL_DATA "Store generated data inside the game directory (debug builds)" OFF)
option(SIL_BUILD_WITH_SDL_SOURCES "Build SDL dependencies from source under external/ (useful for Android)" OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE "Release")
endif()

if(ANDROID)
  # Ensure Android logging symbols are resolvable for SDL and dependent shared libs.
  set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -llog")
endif()

if (SIL_BUILD_WITH_SDL_SOURCES)
  set(SIL_EXTERNAL_DIR "${CMAKE_CURRENT_SOURCE_DIR}/external")

  if (EXISTS "${SIL_EXTERNAL_DIR}/SDL/CMakeLists.txt")
    add_subdirectory("${SIL_EXTERNAL_DIR}/SDL" "${CMAKE_BINARY_DIR}/_deps/SDL")
  else()
    message(FATAL_ERROR "SIL_BUILD_WITH_SDL_SOURCES=ON but external/SDL is missing. Run 'git submodule update --init --recursive' from the repo root or disable SIL_BUILD_WITH_SDL_SOURCES.")
  endif()

  if (EXISTS "${SIL_EXTERNAL_DIR}/SDL_image/CMakeLists.txt")
    add_subdirectory("${SIL_EXTERNAL_DIR}/SDL_image" "${CMAKE_BINARY_DIR}/_deps/SDL_image")
  else()
    message(FATAL_ERROR "SIL_BUILD_WITH_SDL_SOURCES=ON but external/SDL_image is missing. Run 'git submodule update --init --recursive' from the repo root or disable SIL_BUILD_WITH_SDL_SOURCES.")
  endif()

  if (EXISTS "${SIL_EXTERNAL_DIR}/SDL_ttf/CMakeLists.txt")
    add_subdirectory("${SIL_EXTERNAL_DIR}/SDL_ttf" "${CMAKE_BINARY_DIR}/_deps/SDL_ttf")
  else()
    message(FATAL_ERROR "SIL_BUILD_WITH_SDL_SOURCES=ON but external/SDL_ttf is missing. Run 'git submodule update --init --recursive' from the repo root or disable SIL_BUILD_WITH_SDL_SOURCES.")
  endif()

  if (ANDROID AND TARGET SDL3-shared)
    # Needed for __android_log_print / __android_log_write in SDL Android backend.
    target_link_libraries(SDL3-shared PRIVATE log)
  endif()
else()
  find_package(SDL3 REQUIRED)
  find_package(SDL3_ttf REQUIRED)
  find_package(SDL3_image REQUIRED)
endif()

add_subdirectory(src/log)

# Transitional subsystem groupings for the unstable refactor port.
# Keep the compiled source set unchanged until each split lands.
set(SIL_MORE_SOURCES_CORE
    src/blitz.c
    src/birth.c
    src/cave.c
    src/cJSON.c
    src/dungeon.c
    src/format.c
    src/game-event.c
    src/item_set.c
    src/quest.c
    src/randart.c
    src/rng.c
    src/runtime-cli.c
    src/squelch.c
    src/supplies.c
    src/tables.c
    src/util.c
    src/variable.c
    src/wizard1.c
    src/wizard2.c
    src/z-term.c
)

set(SIL_MORE_SOURCES_CMD
    src/cmd4.c
    src/cmd/combat/cmd-combat-rolls.c
    src/cmd/combat/cmd-combat.c
    src/cmd/combat/cmd-ranged.c
    src/cmd/item/cmd-fletchery.c
    src/cmd/item/cmd-identify.c
    src/cmd/item/cmd-item-activate.c
    src/cmd/item/cmd-item-core.c
    src/cmd/item/cmd-item-drop.c
    src/cmd/item/cmd-item-utility.c
    src/cmd/item/cmd-pickup.c
    src/cmd/monster/cmd-monster-alert.c
    src/cmd/movement/cmd-movement.c
    src/cmd/movement/cmd-search.c
    src/cmd/movement/cmd-travel.c
    src/cmd/ui/cmd-ui-abilities.c
    src/cmd/ui/cmd-ui-character.c
    src/cmd/ui/cmd-ui-knowledge.c
    src/cmd/ui/cmd-ui-look.c
    src/cmd/ui/cmd-ui-main-menu.c
    src/cmd/ui/cmd-ui-nearby.c
    src/cmd/ui/cmd-ui-object-display.c
    src/cmd/ui/cmd-ui-query.c
    src/cmd/ui/cmd-ui-settings.c
    src/cmd/world/cmd-interact.c
)

set(SIL_MORE_SOURCES_INIT
    src/init/init-flags.c
    src/init/init-object-bonuses.c
    src/init/init-parse-limits.c
    src/init/init-parse-partition.c
    src/init/init-parse-runtypes.c
    src/init/init-parser-core.c
    src/init/init-style.c
    src/init/init-parse-terrain.c
    src/init/init-parse-object-kind.c
    src/init/init-parse-vault.c
    src/init/init-parse-ability.c
    src/init/init-parse-artefact.c
    src/init/init-parse-names.c
    src/init/init-parse-skeleton-notes.c
    src/init/init-parse-ego.c
    src/init/init-parse-monster.c
    src/init/init-parse-player.c
    src/init/init-parse-stores.c
    src/init/init-parse-curses.c
    src/init/init-parse-major-blessings.c
    src/init/init-parse-flavor.c
    src/init/init-parse-effects.c
    src/init/init-parse-quests.c
    src/init2.c
)

set(SIL_MORE_SOURCES_LEVEL_GENERATION
    src/gen-log.c
    src/level-generation/level-generation-big-cave.c
    src/level-generation/level-generation-connectivity.c
    src/level-generation/level-generation-layout-anchors.c
    src/level-generation/level-generation-layout.c
    src/level-generation/level-generation-layout-morgoth.c
    src/level-generation/level-generation-quests.c
    src/level-generation/level-generation-rooms.c
    src/level-generation/level-generation-screen.c
    src/level-generation/level-generation-state.c
    src/level-generation/level-generation-terrain.c
    src/level-generation/level-generation.c
)

set(SIL_MORE_SOURCES_DROP
    src/drop/drop-system-catalog.c
    src/drop/drop-system-difficulty.c
    src/drop/drop-system-selection.c
    src/drop_system.c
)

set(SIL_MORE_SOURCES_OBJECT
    src/obj-info.c
    src/object/object-allocation.c
    src/object/object-desc.c
    src/object/object-display.c
    src/object/object-flags.c
    src/object/object-flavor.c
    src/object/object-inventory.c
    src/object/object-knowledge.c
    src/object/object-list.c
    src/object/object-make.c
    src/object/object-place.c
    src/object/object-slot.c
    src/object/object-ui-display.c
    src/object/object-ui-enhanced.c
    src/object/object-ui-identify.c
    src/object/object-ui-select.c
    src/object/object-util.c
    src/use-obj.c
)

set(SIL_MORE_SOURCES_MONSTER_AND_COMBAT
    src/melee/melee-attack.c
    src/melee/melee-combat-display.c
    src/melee/melee-movement.c
    src/melee/melee-process.c
    src/melee/melee-util.c
    src/monster-death.c
    src/monster1.c
    src/monster2.c
)

set(SIL_MORE_SOURCES_SPELL
    src/spell/spell-damage.c
    src/spell/spell-detection.c
    src/spell/spell-monster.c
    src/spell/spell-projection.c
    src/spell/spell-teleport.c
    src/spell/spell-terrain.c
    src/spell/spell-utility.c
)

set(SIL_MORE_SOURCES_PLAYER
    src/player/ability_log.c
    src/player/player-abilities.c
    src/player/player-bane.c
    src/player/player-calc.c
    src/player/player-oaths.c
    src/player/player-resources.c
    src/player/player-song-disguise.c
    src/player/player-song-duels.c
    src/player/player-song-effects.c
    src/player/player-songs.c
    src/player/encumbrance.c
    src/player/identification.c
    src/player/killer.c
    src/player/weapon_stats.c
    src/player-status.c
    src/player-xp.c
    src/targeting.c
    src/thrall_quest.c
)

set(SIL_MORE_SOURCES_QUEST
    src/quest/quest-niena.c
    src/quest/quest-orome.c
    src/quest/quest-rewards.c
    src/quest/quest-tulkas.c
    src/quest/quest-ui.c
    src/quest/quest-valar.c
    src/quest/quest-varda.c
)

set(SIL_MORE_SOURCES_SMITHING
    src/smithing/smithing-cost.c
    src/smithing/smithing-difficulty.c
    src/smithing/smithing-item.c
    src/smithing/smithing-materials.c
    src/smithing/smithing-state.c
    src/ui/smithing/ui-smithing-screen.c
)

set(SIL_MORE_SOURCES_SAVE_AND_META
    src/load.c
    src/metarun.c
    src/metarun_legacy.c
    src/save.c
)

set(SIL_MORE_SOURCES_SCORE
    src/score/score_io.c
    src/score/score_logic.c
    src/score/score_guid.c
    src/score/score_artefact.c
    src/score/score_runs.c
    src/score/score_ui.c
)

set(SIL_MORE_SOURCES_FS
    src/fs/pref-files.c
    src/fs/pref-time.c
    src/fs/savefile-name.c
)

set(SIL_MORE_SOURCES_FRONTEND
    src/files.c
    src/signals.c
    src/ui/colors.c
    src/ui/ui-character-name.c
    src/ui/ui-character-screen.c
    src/ui/ui-death.c
    src/ui/ui-file-viewer.c
    src/ui/ui-help.c
    src/ui/ui-look-sidebar.c
    src/ui/ui-story.c
    src/ui/ui-status.c
    src/ui/story_font.c
)

set(SIL_MORE_SOURCES_PLATFORM
    src/main-sdl.c
    src/pane.c
    src/fs/io_sdl.c
    src/fs/path.c
    src/sdl-config.c
    src/sdl-sound.c
    src/sound-config.c
)

set(SIL_MORE_SOURCES_APP
    src/main.c
)

set(SIL_MORE_SOURCES_LOG
    src/log/bootstrap.c
    src/log/fatal.c
)

set(SIL_MORE_SOURCES_SUPPORT
    src/support/strl.c
)

set(SIL_MORE_SOURCES_CORE_LIBRARY
    ${SIL_MORE_SOURCES_CORE}
    ${SIL_MORE_SOURCES_CMD}
    ${SIL_MORE_SOURCES_INIT}
    ${SIL_MORE_SOURCES_LEVEL_GENERATION}
    ${SIL_MORE_SOURCES_DROP}
    ${SIL_MORE_SOURCES_OBJECT}
    ${SIL_MORE_SOURCES_MONSTER_AND_COMBAT}
    ${SIL_MORE_SOURCES_SPELL}
    ${SIL_MORE_SOURCES_PLAYER}
    ${SIL_MORE_SOURCES_QUEST}
    ${SIL_MORE_SOURCES_SMITHING}
    ${SIL_MORE_SOURCES_SAVE_AND_META}
    ${SIL_MORE_SOURCES_SCORE}
    ${SIL_MORE_SOURCES_FS}
    ${SIL_MORE_SOURCES_FRONTEND}
    ${SIL_MORE_SOURCES_LOG}
    ${SIL_MORE_SOURCES_SUPPORT}
)

add_library(sil-build-config INTERFACE)
target_compile_options(sil-build-config INTERFACE
  $<$<C_COMPILER_ID:GNU,Clang,AppleClang>:-Wall -Wextra>
  $<$<C_COMPILER_ID:MSVC>:/W4>
)
target_include_directories(sil-build-config INTERFACE src)

if (LOG_MODE_TIMESTAMP)
  target_compile_definitions(sil-build-config INTERFACE LOG_MODE_TIMESTAMP)
endif()

if (STEAMDECK_SUPPORT)
  target_compile_definitions(sil-build-config INTERFACE STEAMDECK_SUPPORT)
endif()

if (SIL_USE_LOCAL_DATA)
  target_compile_definitions(sil-build-config INTERFACE SIL_USE_LOCAL_DATA)
endif()

add_library(sil-core STATIC ${SIL_MORE_SOURCES_CORE_LIBRARY})
set_target_properties(sil-core PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_link_libraries(sil-core PUBLIC sil-build-config SDL3::SDL3 log)

# WP50: keep the SDL implementation isolated in its own object target while
# core code talks to it through the narrower platform facade headers.
add_library(sil-platform-sdl OBJECT ${SIL_MORE_SOURCES_PLATFORM})
set_target_properties(sil-platform-sdl PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_link_libraries(sil-platform-sdl PUBLIC
  sil-build-config
  SDL3::SDL3
  SDL3_ttf::SDL3_ttf
  SDL3_image::SDL3_image
  log
)

if(ANDROID)
  add_library(sil-more SHARED ${SIL_MORE_SOURCES_APP})
else()
  add_executable(sil-more ${SIL_MORE_SOURCES_APP})
endif()

if(ANDROID)
  # SDL's Android launcher expects libmain.so by convention.
  set_target_properties(sil-more PROPERTIES OUTPUT_NAME "main")
endif()

# Include Windows resource (.rc) so the executable has an icon on Windows
if(WIN32 AND NOT ANDROID)
  # Prefer the .rc in src/icons which references the Sil.ico file present there.
  target_sources(sil-more PRIVATE ${CMAKE_SOURCE_DIR}/src/icons/sil.rc)
endif()

# add_library(log STATIC src/log/log.c)
target_include_directories(log PUBLIC src/log)

if(MINGW)
  target_link_options(sil-more PRIVATE -mwindows)
endif()

target_link_libraries(sil-more PRIVATE sil-platform-sdl sil-core)
