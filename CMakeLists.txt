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
    src/item_set.c
    src/quest.c
    src/randart.c
    src/rng.c
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
    src/cmd/ui/cmd-ui-look.c
    src/cmd/ui/cmd-ui-object-display.c
    src/cmd/ui/cmd-ui-query.c
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
    src/melee1.c
    src/melee2.c
    src/monster-death.c
    src/monster1.c
    src/monster2.c
    src/spells1.c
    src/spells2.c
)

set(SIL_MORE_SOURCES_PLAYER
    src/player/ability_log.c
    src/player/encumbrance.c
    src/player/identification.c
    src/player/killer.c
    src/player/weapon_stats.c
    src/player-status.c
    src/player-xp.c
    src/targeting.c
    src/thrall_quest.c
    src/xtra1.c
    src/xtra2.c
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

set(SIL_MORE_SOURCES_FRONTEND
    src/files.c
    src/main.c
    src/main-sdl.c
    src/pane.c
    src/signals.c
    src/ui/colors.c
    src/ui/ui-file-viewer.c
    src/ui/ui-help.c
    src/ui/story_font.c
)

set(SIL_MORE_SOURCES_PLATFORM
    src/fs/io_sdl.c
    src/fs/path.c
    src/sdl-config.c
    src/sdl-sound.c
    src/sound-config.c
)

set(SIL_MORE_SOURCES_LOG
    src/log/bootstrap.c
    src/log/fatal.c
)

set(SIL_MORE_SOURCES_SUPPORT
    src/support/strl.c
)

set(SIL_MORE_SOURCES
    ${SIL_MORE_SOURCES_CORE}
    ${SIL_MORE_SOURCES_CMD}
    ${SIL_MORE_SOURCES_INIT}
    ${SIL_MORE_SOURCES_LEVEL_GENERATION}
    ${SIL_MORE_SOURCES_DROP}
    ${SIL_MORE_SOURCES_OBJECT}
    ${SIL_MORE_SOURCES_MONSTER_AND_COMBAT}
    ${SIL_MORE_SOURCES_PLAYER}
    ${SIL_MORE_SOURCES_SAVE_AND_META}
    ${SIL_MORE_SOURCES_SCORE}
    ${SIL_MORE_SOURCES_FRONTEND}
    ${SIL_MORE_SOURCES_PLATFORM}
    ${SIL_MORE_SOURCES_LOG}
    ${SIL_MORE_SOURCES_SUPPORT}
)

if(ANDROID)
  add_library(sil-more SHARED ${SIL_MORE_SOURCES})
else()
  add_executable(sil-more ${SIL_MORE_SOURCES})
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

# add_executable(sil-more ${SIL_MORE_SOURCES})

target_compile_options(sil-more PRIVATE
  $<$<C_COMPILER_ID:GNU,Clang,AppleClang>:-Wall -Wextra>
  $<$<C_COMPILER_ID:MSVC>:/W4>
)

target_include_directories(sil-more PRIVATE src)
if(MINGW)
  target_link_options(sil-more PRIVATE -mwindows)
endif()

target_link_libraries(sil-more PRIVATE SDL3::SDL3 SDL3_ttf::SDL3_ttf SDL3_image::SDL3_image log)

# Add compile definitions after target is created
if (LOG_MODE_TIMESTAMP)
  target_compile_definitions(sil-more PUBLIC LOG_MODE_TIMESTAMP)
endif()

if (STEAMDECK_SUPPORT)
    target_compile_definitions(sil-more PUBLIC STEAMDECK_SUPPORT)
endif()

if (SIL_USE_LOCAL_DATA)
    target_compile_definitions(sil-more PUBLIC SIL_USE_LOCAL_DATA)
endif()
