cmake_minimum_required(VERSION 3.18)
project(sil-more C)

# Project language standard
set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
include(CTest)
include(CheckCSourceCompiles)
find_package(Python3 QUIET COMPONENTS Interpreter)

set(SIL_BUILD_WITH_SDL_SOURCES_DEFAULT OFF)
# On macOS, Homebrew ships sdl3 / sdl3_image / sdl3_ttf but no sdl3_mixer,
# and its sdl3 lags the version our vendored SDL_mixer pins. Default APPLE
# (macOS + iOS) to the source build alongside Windows/Android.
if(WIN32 OR ANDROID OR APPLE)
  set(SIL_BUILD_WITH_SDL_SOURCES_DEFAULT ON)
endif()

option(LOG_MODE_TIMESTAMP "Create new log with timestamp each time" OFF)
option(STEAMDECK_SUPPORT "Build for Steam Deck" OFF)
option(SIL_USE_LOCAL_DATA "Store generated data inside the game directory (debug builds)" OFF)
option(SIL_BUILD_WITH_SDL_SOURCES "Build SDL dependencies from pinned sources under external/ (recommended for Windows and Android)" ${SIL_BUILD_WITH_SDL_SOURCES_DEFAULT})
option(SIL_STRICT_WARNINGS "Enable stricter warning settings for development builds" OFF)
option(SIL_ENABLE_ASAN "Enable AddressSanitizer on supported compilers" OFF)
option(SIL_ENABLE_UBSAN "Enable UndefinedBehaviorSanitizer on supported compilers" OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE "Release")
endif()

if(ANDROID)
  # Ensure Android logging symbols are resolvable for SDL and dependent shared libs.
  set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -llog")
endif()

# Detect iOS when the CMake system name is set (for example via
# -DCMAKE_SYSTEM_NAME=iOS).
if(CMAKE_SYSTEM_NAME STREQUAL "iOS")
  set(SIL_IOS ON)
  if(NOT CMAKE_OSX_DEPLOYMENT_TARGET)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "15.0" CACHE STRING "Minimum iOS version")
  endif()
endif()

if(SIL_IOS AND SIL_USE_LOCAL_DATA)
  message(WARNING "SIL_USE_LOCAL_DATA is not writable in an iOS app bundle; forcing it OFF so saves/configs use SDL_GetPrefPath().")
  set(SIL_USE_LOCAL_DATA OFF CACHE BOOL "Store generated data inside the game directory (debug builds)" FORCE)
endif()

set(SIL_EXTERNAL_DIR "${CMAKE_CURRENT_SOURCE_DIR}/external")

set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_SOURCE_DIR}/src/defines.h"
)
file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/src/defines.h" SIL_VERSION_STRING_LINE
  REGEX "^#define[ \t]+VERSION_STRING[ \t]+\"[^\"]+\""
)
if(NOT SIL_VERSION_STRING_LINE)
  message(FATAL_ERROR "Could not read VERSION_STRING from src/defines.h")
endif()
string(REGEX REPLACE
  "^#define[ \t]+VERSION_STRING[ \t]+\"([^\"]+)\".*$"
  "\\1"
  SIL_VERSION_STRING
  "${SIL_VERSION_STRING_LINE}"
)

# Configure SDL_mixer when built from the vendored submodule: enable only the
# codecs we actually use (WAV + Ogg Vorbis via stb_vorbis) and keep the build
# self-contained so it works on every platform without system codec packages.
macro(_sil_configure_vendored_sdl_mixer)
  set(SDLMIXER_INSTALL OFF CACHE BOOL "" FORCE)
  set(SDLMIXER_TESTS OFF CACHE BOOL "" FORCE)
  set(SDLMIXER_EXAMPLES OFF CACHE BOOL "" FORCE)
  set(SDLMIXER_STRICT OFF CACHE BOOL "" FORCE)
  set(SDLMIXER_DEPS_SHARED OFF CACHE BOOL "" FORCE)
  set(SDLMIXER_VENDORED ON CACHE BOOL "" FORCE)
  set(SDLMIXER_AIFF OFF CACHE BOOL "" FORCE)
  set(SDLMIXER_VOC OFF CACHE BOOL "" FORCE)
  set(SDLMIXER_AU OFF CACHE BOOL "" FORCE)
  set(SDLMIXER_FLAC OFF CACHE BOOL "" FORCE)
  set(SDLMIXER_GME OFF CACHE BOOL "" FORCE)
  set(SDLMIXER_MOD OFF CACHE BOOL "" FORCE)
  set(SDLMIXER_MP3 OFF CACHE BOOL "" FORCE)
  set(SDLMIXER_MIDI OFF CACHE BOOL "" FORCE)
  set(SDLMIXER_OPUS OFF CACHE BOOL "" FORCE)
  set(SDLMIXER_WAVPACK OFF CACHE BOOL "" FORCE)
  set(SDLMIXER_WAVE ON CACHE BOOL "" FORCE)
  set(SDLMIXER_VORBIS_STB ON CACHE BOOL "" FORCE)
  set(SDLMIXER_VORBIS_VORBISFILE OFF CACHE BOOL "" FORCE)
  set(SDLMIXER_VORBIS_TREMOR OFF CACHE BOOL "" FORCE)
endmacro()

if (SIL_BUILD_WITH_SDL_SOURCES)

  if(SIL_IOS OR ANDROID)
    set(SDLTTF_VENDORED ON CACHE BOOL "Use vendored freetype/harfbuzz for mobile" FORCE)
  endif()

  if(SIL_IOS)
    set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
    set(SDL_SHARED OFF CACHE BOOL "" FORCE)
    set(SDL_STATIC ON CACHE BOOL "" FORCE)
    set(SDL3IMAGE_SHARED OFF CACHE BOOL "" FORCE)
    set(SDL3IMAGE_STATIC ON CACHE BOOL "" FORCE)
    set(SDL3TTF_SHARED OFF CACHE BOOL "" FORCE)
    set(SDL3TTF_STATIC ON CACHE BOOL "" FORCE)
  endif()

  if (EXISTS "${SIL_EXTERNAL_DIR}/SDL/CMakeLists.txt")
    add_subdirectory("${SIL_EXTERNAL_DIR}/SDL" "${CMAKE_BINARY_DIR}/_deps/SDL")
  else()
    message(FATAL_ERROR "SIL_BUILD_WITH_SDL_SOURCES=ON but external/SDL is missing. Run 'git submodule update --init --recursive' from the repo root or disable SIL_BUILD_WITH_SDL_SOURCES.")
  endif()

  if(SIL_IOS)
    set(_sdl3_shim_dir "${CMAKE_BINARY_DIR}/_sdl3_shim")
    file(MAKE_DIRECTORY "${_sdl3_shim_dir}")
    file(WRITE "${_sdl3_shim_dir}/SDL3Config.cmake"
      "set(SDL3_FOUND TRUE)\n"
      "set(SDL3_VERSION \"${SDL_VERSION}\")\n"
      "set(SDL3_SDL3-static_FOUND TRUE)\n"
      "set(SDL3_Headers_FOUND TRUE)\n"
      "# Targets already defined by add_subdirectory\n"
    )
    file(WRITE "${_sdl3_shim_dir}/SDL3ConfigVersion.cmake"
      "set(PACKAGE_VERSION \"${SDL_VERSION}\")\n"
      "set(PACKAGE_VERSION_COMPATIBLE TRUE)\n"
      "set(PACKAGE_VERSION_EXACT FALSE)\n"
    )
    set(SDL3_DIR "${_sdl3_shim_dir}" CACHE PATH "" FORCE)
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

  if (EXISTS "${SIL_EXTERNAL_DIR}/SDL_mixer/CMakeLists.txt")
    _sil_configure_vendored_sdl_mixer()
    add_subdirectory("${SIL_EXTERNAL_DIR}/SDL_mixer" "${CMAKE_BINARY_DIR}/_deps/SDL_mixer")
  else()
    message(FATAL_ERROR "SIL_BUILD_WITH_SDL_SOURCES=ON but external/SDL_mixer is missing. Run 'git submodule update --init --recursive' from the repo root or disable SIL_BUILD_WITH_SDL_SOURCES.")
  endif()

  if (ANDROID AND TARGET SDL3-shared)
    # Needed for __android_log_print / __android_log_write in SDL Android backend.
    target_link_libraries(SDL3-shared PRIVATE log)
  endif()
else()
  find_package(SDL3 REQUIRED)
  find_package(SDL3_ttf REQUIRED)
  find_package(SDL3_image REQUIRED)
  find_package(SDL3_mixer QUIET)
  if(NOT SDL3_mixer_FOUND)
    if (EXISTS "${SIL_EXTERNAL_DIR}/SDL_mixer/CMakeLists.txt")
      _sil_configure_vendored_sdl_mixer()
      add_subdirectory("${SIL_EXTERNAL_DIR}/SDL_mixer" "${CMAKE_BINARY_DIR}/_deps/SDL_mixer")
    else()
      message(FATAL_ERROR "SDL3_mixer was not found as an installed package and external/SDL_mixer is missing. Run 'git submodule update --init --recursive' to build it from source, or install SDL3_mixer system-wide.")
    endif()
  endif()
endif()

add_subdirectory(src/log)

# Transitional subsystem groupings for the unstable refactor port.
# Keep the compiled source set unchanged until each split lands.
set(SIL_MORE_SOURCES_CORE
    src/cave/cave-awareness.c
    src/cave/cave-geometry.c
    src/cave/cave-visuals.c
    src/cave/cave.c
    src/cave/cave-map-ui.c
    src/cave/cave-state.c
)

set(SIL_MORE_SOURCES_CMD
    src/cmd/debug/wizard1.c
    src/cmd/debug/wizard2.c
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
    src/cmd/ui/cmd-ui-abilities-details.c
    src/cmd/ui/cmd-ui-abilities-scenes.c
    src/cmd/ui/cmd-ui-abilities-songs.c
    src/cmd/ui/cmd-ui-character.c
    src/cmd/ui/cmd-ui-info.c
    src/cmd/ui/cmd-ui-knowledge-browser.c
    src/cmd/ui/cmd-ui-knowledge-supplies.c
    src/cmd/ui/cmd-ui-knowledge.c
    src/cmd/ui/cmd-ui-look.c
    src/cmd/ui/cmd-ui-main-menu.c
    src/cmd/ui/cmd-ui-main-menu-help.c
    src/cmd/ui/cmd-ui-nearby.c
    src/cmd/ui/cmd-ui-object-display.c
    src/cmd/ui/cmd-ui-query.c
    src/cmd/ui/cmd-ui-settings.c
    src/cmd/ui/cmd-ui-settings-controller.c
    src/cmd/ui/cmd-ui-settings-keybinds.c
    src/cmd/ui/cmd-ui-settings-panes.c
    src/cmd/ui/cmd-ui-settings-touch.c
    src/cmd/ui/cmd-ui-settings-visuals.c
    src/cmd/world/cmd-interact-chest.c
    src/cmd/world/cmd-interact-chest-core.c
    src/cmd/world/cmd-interact-chest-skeleton-loot.c
    src/cmd/world/cmd-interact-chest-skeleton-note-hints.c
    src/cmd/world/cmd-interact-chest-skeleton-note.c
    src/cmd/world/cmd-interact-door.c
    src/cmd/world/cmd-interact-terrain.c
    src/cmd/world/cmd-interact.c
)

set(SIL_MORE_SOURCES_INIT
    src/init/init-info.c
    src/init/init-flags.c
    src/init/init2.c
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
)

set(SIL_MORE_SOURCES_LEVEL_GENERATION
    src/level-generation/gen-log.c
    src/level-generation/level-generation-big-cave.c
    src/level-generation/level-generation-connectivity.c
    src/level-generation/level-generation-connectivity-partitions.c
    src/level-generation/level-generation-connectivity-placement.c
    src/level-generation/level-generation-layout-info.c
    src/level-generation/level-generation-layout-anchors.c
    src/level-generation/level-generation-layout.c
    src/level-generation/level-generation-legendary.c
    src/level-generation/level-generation-layout-morgoth.c
    src/level-generation/level-generation-layout-partitions.c
    src/level-generation/level-generation-quests.c
    src/level-generation/level-generation-rooms.c
    src/level-generation/level-generation-rooms-vault-advanced.c
    src/level-generation/level-generation-rooms-special.c
    src/level-generation/level-generation-rooms-vaults.c
    src/level-generation/level-generation-screen.c
    src/level-generation/level-generation-state.c
    src/level-generation/level-generation-terrain.c
    src/level-generation/level-generation.c
)

set(SIL_MORE_SOURCES_DROP
    src/drop/drop-system-catalog.c
    src/drop/drop-system.c
    src/drop/drop-system-difficulty.c
    src/drop/drop-system-selection.c
)

set(SIL_MORE_SOURCES_OBJECT
    src/object/item_set.c
    src/object/object-allocation.c
    src/object/object-desc.c
    src/object/object-display.c
    src/object/object-flags.c
    src/object/object-flavor.c
    src/object/object-info.c
    src/object/object-info-lore.c
    src/object/object-inventory.c
    src/object/object-knowledge.c
    src/object/object-list.c
    src/object/object-make.c
    src/object/object-place.c
    src/object/object-randart.c
    src/object/object-randart-generation.c
    src/object/object-slot.c
    src/object/object-ui-display.c
    src/object/object-ui-enhanced.c
    src/object/object-ui-identify.c
    src/object/object-ui-select.c
    src/object/object-use.c
    src/object/object-util.c
    src/object/supplies.c
)

set(SIL_MORE_SOURCES_MONSTER_AND_COMBAT
    src/melee/melee-attack.c
    src/melee/melee-attack-ranged.c
    src/melee/melee-combat-display.c
    src/melee/melee-movement.c
    src/melee/melee-movement-path.c
    src/melee/melee-movement-resolution.c
    src/melee/melee-process.c
    src/melee/melee-util.c
    src/monster/monster-death.c
    src/monster/monster1.c
    src/monster/monster-message.c
    src/monster/monster2.c
    src/monster/monster-spawn.c
)

set(SIL_MORE_SOURCES_SPELL
    src/spell/spell-damage.c
    src/spell/spell-elemental-materials.c
    src/spell/spell-detection.c
    src/spell/spell-monster.c
    src/spell/spell-projection-effects.c
    src/spell/spell-projection.c
    src/spell/spell-teleport.c
    src/spell/spell-terrain.c
    src/spell/spell-utility.c
    src/spell/spell-visuals.c
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
    src/player/player-status.c
    src/player/player-xp.c
    src/player/encumbrance.c
    src/player/identification.c
    src/player/killer.c
    src/player/weapon_stats.c
)

set(SIL_MORE_SOURCES_QUEST
    src/quest/quest.c
    src/quest/quest-niena.c
    src/quest/quest-orome.c
    src/quest/quest-rewards.c
    src/quest/thrall_quest.c
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
    src/metarun/metarun-meta-state.c
    src/metarun/metarun-monster-events.c
    src/metarun/metarun.c
    src/metarun/metarun-persistence.c
    src/metarun/metarun-score.c
    src/metarun/metarun-state.c
    src/metarun/metarun-ui-history.c
    src/metarun/metarun-ui-stats.c
)

set(SIL_MORE_SOURCES_SCORE
    src/score/score_io.c
    src/score/score_entry.c
    src/score/score_logic.c
    src/score/score_guid.c
    src/score/score_artefact.c
    src/score/score_runs.c
    src/score/score_ui.c
    src/score/score_ui-run-history.c
)

set(SIL_MORE_SOURCES_FS
    src/fs/file.c
    src/fs/load-dungeon.c
    src/fs/load-notes-inventory.c
    src/fs/load-player.c
    src/fs/load.c
    src/fs/resource.c
    src/fs/save-dungeon.c
    src/fs/save-notes-inventory.c
    src/fs/save-player.c
    src/fs/save.c
    src/fs/savefile-name.c
)

set(SIL_MORE_SOURCES_RUNTIME
    src/runtime/blitz.c
    src/runtime/runtime-cli.c
    src/runtime/runtime-dungeon.c
    src/runtime/runtime-dungeon-maintenance.c
    src/runtime/runtime-dungeon-presentation.c
    src/runtime/runtime-dungeon-session.c
    src/runtime/runtime-exit.c
    src/runtime/runtime-game.c
    src/runtime/runtime-state.c
)

set(SIL_MORE_SOURCES_APP_BOUNDARY
    src/app/app-command.c
    src/app/app-movement.c
    src/app/app-interaction.c
    src/app/app-ui-command.c
    src/app/app-scene-birth.c
    src/app/app-scene-birth-blitz.c
    src/app/app-scene-birth-setup.c
    src/app/app-scene-birth-traits.c
    src/app/app-scene-birth-ui.c
    src/app/app-ui.c
    src/app/app-events.c
    src/app/app-host.c
    src/app/app-host-bridge.c
    src/app/app-scene-bootstrap.c
    src/app/app-scene-dungeon.c
    src/app/app-scene-menu.c
    src/app/app-session.c
    src/app/app-wire.c
)

set(SIL_MORE_SOURCES_FRONTEND
    src/ui/targeting.c
    src/ui/targeting-interactive.c
    src/ui/colors.c
    src/ui/ui-character-dump.c
    src/ui/ui-character-screen.c
    src/ui/ui-death.c
    src/ui/ui-file-viewer.c
    src/ui/ui-help.c
    src/ui/ui-browser-shell.c
    src/ui/ui-information-scene.c
    src/ui/ui-look-sidebar.c
    src/ui/ui-narrative.c
    src/ui/ui-semantic-scene.c
    src/ui/ui-touch-mouse-tutorial.c
    src/ui/ui-story.c
    src/ui/ui-status.c
    src/ui/story_font.c
)

set(SIL_MORE_SOURCES_PLATFORM
    src/main-sdl.c
    src/platform/sdl-init.c
    src/sdl-layout.c
    src/sdl-render.c
    src/sdl-scene-bootstrap.c
    src/sdl-scene-dungeon.c
    src/sdl-scene-menu.c
    src/sdl-menu/sdl-menu-pointer.c
    src/sdl-menu/sdl-map-pointer.c
    src/sdl-menu/sdl-player-action-menu.c
    src/sdl-menu/sdl-round-movement.c
    src/sdl-menu/sdl-scene-menu-base.c
    src/sdl-menu/sdl-scene-menu-browser.c
    src/sdl-menu/sdl-scene-menu-chrome.c
    src/sdl-menu/sdl-gamepad-labels.c
    src/sdl-menu/sdl-scene-menu-pages.c
    src/sdl-scene.c
    src/sdl-story-font.c
    src/sdl-touch.c
    src/platform/sdl-touch-controls.c
    src/sdl-ui-style.c
    src/pane.c
    src/fs/io_sdl.c
    src/fs/path.c
    src/init/init-paths.c
    src/platform-frame.c
    src/platform-signals.c
    src/platform-time.c
    src/log/bootstrap.c
    src/platform/sdl-app-options.c
    src/sdl-config.c
    src/platform/sdl-touch-config.c
    src/platform/sdl-config-defaults.c
    src/sdl-sound.c
    src/sound-config.c
)

set(SIL_MORE_SOURCES_APP
    src/main.c
)

set(SIL_MORE_SOURCES_LOG
    src/log/fatal.c
)

set(SIL_MORE_SOURCES_SUPPORT
    src/support/cJSON.c
    src/support/angle.c
    src/support/game-tables.c
    src/support/input.c
    src/support/message.c
    src/support/prompt.c
    src/support/format.c
    src/support/editing-buffer.c
    src/support/reliability-checks.c
    src/support/rng.c
    src/support/strl.c
    src/support/text-output.c
    src/support/utf8.c
    src/support/util.c
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
    ${SIL_MORE_SOURCES_RUNTIME}
    ${SIL_MORE_SOURCES_APP_BOUNDARY}
    ${SIL_MORE_SOURCES_FRONTEND}
    ${SIL_MORE_SOURCES_LOG}
    ${SIL_MORE_SOURCES_SUPPORT}
)

add_library(sil-build-config INTERFACE)
target_compile_options(sil-build-config INTERFACE
  $<$<C_COMPILER_ID:GNU,Clang,AppleClang>:-Wall -Wextra>
  $<$<C_COMPILER_ID:MSVC>:/W4>
)
target_include_directories(sil-build-config INTERFACE
  src
)

add_library(sil-strict-build-config INTERFACE)
target_compile_options(sil-strict-build-config INTERFACE
  $<$<C_COMPILER_ID:GNU,Clang,AppleClang>:-Werror -Wshadow -Wformat=2 -Wstrict-prototypes -Wmissing-prototypes>
  $<$<C_COMPILER_ID:MSVC>:/WX>
)

if (SIL_ENABLE_ASAN OR SIL_ENABLE_UBSAN)
  if (MSVC)
    message(FATAL_ERROR "SIL_ENABLE_ASAN/SIL_ENABLE_UBSAN are only configured for GNU/Clang-style toolchains.")
  endif()

  set(SIL_SANITIZER_FLAGS "")
  if (SIL_ENABLE_ASAN)
    list(APPEND SIL_SANITIZER_FLAGS -fsanitize=address -fno-omit-frame-pointer)
  endif()
  if (SIL_ENABLE_UBSAN)
    list(APPEND SIL_SANITIZER_FLAGS -fsanitize=undefined -fno-sanitize-recover=undefined)
  endif()

  string(JOIN " " SIL_SANITIZER_FLAGS_STRING ${SIL_SANITIZER_FLAGS})
  set(_sil_saved_required_flags "${CMAKE_REQUIRED_FLAGS}")
  set(_sil_saved_required_link_options "${CMAKE_REQUIRED_LINK_OPTIONS}")
  set(CMAKE_REQUIRED_FLAGS "${CMAKE_REQUIRED_FLAGS} ${SIL_SANITIZER_FLAGS_STRING}")
  set(CMAKE_REQUIRED_LINK_OPTIONS ${SIL_SANITIZER_FLAGS})
  check_c_source_compiles("int main(void) { return 0; }" SIL_SANITIZER_RUNTIME_AVAILABLE)
  set(CMAKE_REQUIRED_FLAGS "${_sil_saved_required_flags}")
  set(CMAKE_REQUIRED_LINK_OPTIONS "${_sil_saved_required_link_options}")

  if (SIL_SANITIZER_RUNTIME_AVAILABLE)
    target_compile_options(sil-build-config INTERFACE
      $<$<C_COMPILER_ID:GNU,Clang,AppleClang>:${SIL_SANITIZER_FLAGS}>
    )
    target_link_options(sil-build-config INTERFACE
      $<$<C_COMPILER_ID:GNU,Clang,AppleClang>:${SIL_SANITIZER_FLAGS}>
    )
  else()
    message(WARNING
      "Requested sanitizer flags (${SIL_SANITIZER_FLAGS_STRING}) are unavailable on this host; "
      "the preset will continue without sanitizer instrumentation."
    )
  endif()
endif()

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
target_link_libraries(sil-core PUBLIC
  sil-build-config
  log
)
# Some UI/settings sources still live in sil-core and include SDL-facing
# migration headers directly, so the core target needs SDL usage requirements
# during compilation on source-built/mobile configurations.
target_link_libraries(sil-core PRIVATE
  SDL3::SDL3
  SDL3_ttf::SDL3_ttf
  SDL3_image::SDL3_image
  SDL3_mixer::SDL3_mixer
)

if (SIL_STRICT_WARNINGS)
  target_link_libraries(sil-core PUBLIC sil-strict-build-config)
endif()

# WP50: keep the SDL implementation isolated in its own object target while
# core code talks to it through the narrower platform facade headers.
add_library(sil-platform-sdl OBJECT ${SIL_MORE_SOURCES_PLATFORM})
set_target_properties(sil-platform-sdl PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_link_libraries(sil-platform-sdl PUBLIC
  sil-build-config
  SDL3::SDL3
  SDL3_ttf::SDL3_ttf
  SDL3_image::SDL3_image
  SDL3_mixer::SDL3_mixer
  log
)
if (SIL_STRICT_WARNINGS)
  target_link_libraries(sil-platform-sdl PUBLIC sil-strict-build-config)
endif()

if(ANDROID)
  add_library(sil-more SHARED ${SIL_MORE_SOURCES_APP})
elseif(SIL_IOS)
  add_executable(sil-more MACOSX_BUNDLE ${SIL_MORE_SOURCES_APP})
else()
  add_executable(sil-more ${SIL_MORE_SOURCES_APP})
endif()

if(ANDROID)
  # SDL's Android launcher expects libmain.so by convention.
  set_target_properties(sil-more PROPERTIES OUTPUT_NAME "main")
endif()

if(SIL_IOS)
  set_target_properties(sil-more PROPERTIES
    MACOSX_BUNDLE_INFO_PLIST "${CMAKE_SOURCE_DIR}/ios/Info.plist.in"
    MACOSX_BUNDLE_GUI_IDENTIFIER "com.silqh.silmore"
    MACOSX_BUNDLE_BUNDLE_NAME "Sil-More"
    MACOSX_BUNDLE_BUNDLE_VERSION "${SIL_VERSION_STRING}"
    MACOSX_BUNDLE_SHORT_VERSION_STRING "${SIL_VERSION_STRING}"
    XCODE_ATTRIBUTE_TARGETED_DEVICE_FAMILY "1,2"
    XCODE_ATTRIBUTE_ASSETCATALOG_COMPILER_APPICON_NAME "AppIcon"
  )

  set(SIL_IOS_ASSET_CATALOG "${CMAKE_SOURCE_DIR}/ios/Assets.xcassets")
  set_source_files_properties("${SIL_IOS_ASSET_CATALOG}" PROPERTIES
    MACOSX_PACKAGE_LOCATION "Resources"
  )
  target_sources(sil-more PRIVATE "${SIL_IOS_ASSET_CATALOG}")

  file(GLOB SIL_RES_EDIT    CONFIGURE_DEPENDS "lib/edit/*.txt")
  file(GLOB SIL_RES_PREF    CONFIGURE_DEPENDS "lib/pref/*.prf" "lib/pref/*.json")
  file(GLOB SIL_RES_FONT    CONFIGURE_DEPENDS "lib/xtra/font/*.ttf")
  file(GLOB SIL_RES_GRAF    CONFIGURE_DEPENDS "lib/xtra/graf/*.png")
  file(GLOB_RECURSE SIL_RES_SOUND LIST_DIRECTORIES false CONFIGURE_DEPENDS "lib/xtra/sound/*")
  file(GLOB_RECURSE SIL_RES_MUSIC LIST_DIRECTORIES false CONFIGURE_DEPENDS "lib/xtra/music/*")
  file(GLOB SIL_RES_HELP    CONFIGURE_DEPENDS "lib/help/*.txt" "lib/help/*.hlp")

  foreach(_file ${SIL_RES_EDIT})
    set_source_files_properties(${_file} PROPERTIES MACOSX_PACKAGE_LOCATION "Resources/lib/edit")
  endforeach()
  foreach(_file ${SIL_RES_PREF})
    set_source_files_properties(${_file} PROPERTIES MACOSX_PACKAGE_LOCATION "Resources/lib/pref")
  endforeach()
  foreach(_file ${SIL_RES_FONT})
    set_source_files_properties(${_file} PROPERTIES MACOSX_PACKAGE_LOCATION "Resources/lib/xtra/font")
  endforeach()
  foreach(_file ${SIL_RES_GRAF})
    set_source_files_properties(${_file} PROPERTIES MACOSX_PACKAGE_LOCATION "Resources/lib/xtra/graf")
  endforeach()
  foreach(_file ${SIL_RES_SOUND})
    get_filename_component(_dir "${_file}" DIRECTORY)
    file(RELATIVE_PATH _rel_dir "${CMAKE_CURRENT_SOURCE_DIR}/lib/xtra/sound" "${_dir}")
    string(REPLACE "\\" "/" _rel_dir "${_rel_dir}")
    if(_rel_dir STREQUAL ".")
      set(_package_dir "Resources/lib/xtra/sound")
    else()
      set(_package_dir "Resources/lib/xtra/sound/${_rel_dir}")
    endif()
    set_source_files_properties("${_file}" PROPERTIES MACOSX_PACKAGE_LOCATION "${_package_dir}")
  endforeach()
  foreach(_file ${SIL_RES_MUSIC})
    get_filename_component(_dir "${_file}" DIRECTORY)
    file(RELATIVE_PATH _rel_dir "${CMAKE_CURRENT_SOURCE_DIR}/lib/xtra/music" "${_dir}")
    string(REPLACE "\\" "/" _rel_dir "${_rel_dir}")
    if(_rel_dir STREQUAL ".")
      set(_package_dir "Resources/lib/xtra/music")
    else()
      set(_package_dir "Resources/lib/xtra/music/${_rel_dir}")
    endif()
    set_source_files_properties("${_file}" PROPERTIES MACOSX_PACKAGE_LOCATION "${_package_dir}")
  endforeach()
  foreach(_file ${SIL_RES_HELP})
    set_source_files_properties(${_file} PROPERTIES MACOSX_PACKAGE_LOCATION "Resources/lib/help")
  endforeach()

  set(SIL_ALL_RESOURCES
    ${SIL_RES_EDIT} ${SIL_RES_PREF} ${SIL_RES_FONT}
    ${SIL_RES_GRAF} ${SIL_RES_SOUND} ${SIL_RES_MUSIC} ${SIL_RES_HELP}
  )
  target_sources(sil-more PRIVATE ${SIL_ALL_RESOURCES})

  target_compile_definitions(sil-more PRIVATE SIL_IOS)
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

target_link_libraries(sil-more PRIVATE sil-platform-sdl sil-core m)
if (SIL_STRICT_WARNINGS)
  target_link_libraries(sil-more PRIVATE sil-strict-build-config)
endif()

if(BUILD_TESTING)
  add_executable(sil-phase01-tests
    tests/phase0_tests.c
    src/support/reliability-checks.c
  )
  target_link_libraries(sil-phase01-tests PRIVATE sil-build-config)
  add_test(
    NAME sil_phase01_regressions
    COMMAND sil-phase01-tests
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/smithing_parity_cases.txt
  )

  add_executable(sil-ui1-tests
    tests/ui1_tests.c
  )
  target_link_libraries(sil-ui1-tests PRIVATE
    sil-build-config
    sil-core
    sil-platform-sdl
  )
  add_test(
    NAME sil_ui1_scaffolding
    COMMAND sil-ui1-tests
  )
  add_executable(sil-ui8-tests
    tests/ui8_tests.c
  )
  target_link_libraries(sil-ui8-tests PRIVATE
    sil-build-config
    sil-core
    sil-platform-sdl
  )
  add_test(
    NAME sil_ui8_wire
    COMMAND sil-ui8-tests
  )
  add_executable(sil-ui8-demo-packets
    tools/ui8_emit_demo_packets.c
  )
  target_link_libraries(sil-ui8-demo-packets PRIVATE
    sil-build-config
    sil-core
    sil-platform-sdl
  )
  add_test(
    NAME sil_ui8_demo_packets
    COMMAND sil-ui8-demo-packets
      ${CMAKE_CURRENT_BINARY_DIR}/ui8-demo-samples
  )
  if(WIN32)
    set(SIL_TEST_PATH_ENTRIES
      "${CMAKE_CURRENT_BINARY_DIR}/_deps/SDL"
      "${CMAKE_CURRENT_BINARY_DIR}/_deps/SDL_image"
      "${CMAKE_CURRENT_BINARY_DIR}/_deps/SDL_ttf"
      "${CMAKE_CURRENT_BINARY_DIR}/_deps/SDL_mixer"
      "C:/msys64/mingw64/bin"
      "$ENV{PATH}"
    )
    list(JOIN SIL_TEST_PATH_ENTRIES ";" SIL_TEST_RUNTIME_PATH_RAW)
    string(REPLACE ";" "\\;" SIL_TEST_RUNTIME_PATH "${SIL_TEST_RUNTIME_PATH_RAW}")
    set_tests_properties(sil_ui1_scaffolding PROPERTIES
      ENVIRONMENT "PATH=${SIL_TEST_RUNTIME_PATH}"
    )
    set_tests_properties(sil_ui8_wire PROPERTIES
      ENVIRONMENT "PATH=${SIL_TEST_RUNTIME_PATH}"
    )
    set_tests_properties(sil_ui8_demo_packets PROPERTIES
      ENVIRONMENT "PATH=${SIL_TEST_RUNTIME_PATH}"
    )
  endif()

  if(Python3_Interpreter_FOUND)
    add_test(
      NAME sil_ui0_audit
      COMMAND ${Python3_EXECUTABLE}
        ${CMAKE_CURRENT_SOURCE_DIR}/tools/ui_debt_audit.py
        --check
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/ui_debt_audit_baseline.json
    )
    add_test(
      NAME sil_architecture_debt_audit
      COMMAND ${Python3_EXECUTABLE}
        ${CMAKE_CURRENT_SOURCE_DIR}/tools/modernization_audit.py
        --audit
        architecture
        --check
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/modernization_audit_baseline.json
    )
    add_test(
      NAME sil_folder_ownership_audit
      COMMAND ${Python3_EXECUTABLE}
        ${CMAKE_CURRENT_SOURCE_DIR}/tools/modernization_audit.py
        --audit
        folder_ownership
        --check
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/modernization_audit_baseline.json
    )
    add_test(
      NAME sil_source_size_audit
      COMMAND ${Python3_EXECUTABLE}
        ${CMAKE_CURRENT_SOURCE_DIR}/tools/source_size_audit.py
        --check
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/source_size_audit_baseline.json
    )
    add_test(
      NAME sil_smithing_parity
      COMMAND ${Python3_EXECUTABLE}
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/smithing_parity_test.py
    )
  else()
    message(WARNING "Python3 interpreter not found; audit and parity Python tests disabled.")
  endif()
endif()
