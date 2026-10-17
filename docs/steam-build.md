# Steam build guide

Steamworks integration is optional and disabled by default. The ordinary
standard and portable builds work without Valve's SDK. Enabling
`SIL_STEAMWORKS=ON` adds the small C++ adapter around the C engine and links the
Steam API runtime for the target operating system.

The adapter initializes Steam before renderer creation, pumps callbacks during
input waits and animation delays, and shuts down at process exit. If Steam
initialization fails, the game continues without Steam features. Achievements
use the shared local profile and synchronize with Steam; see
[achievement setup](achievements.md) for the catalogue and dashboard configuration.
Steam Cloud uses Auto-Cloud for the shared progress profile; see
[cloud saves](cloud-saves.md) for the published roots and recovery behavior.
Leaderboards are not configured.

## SDK and source checkout

Initialize the pinned SDL sources and their dependencies:

```bash
git submodule update --init --recursive
```

Obtain the Steamworks SDK separately from
[Valve's SDK download page](https://partner.steamgames.com/downloads/list).
The integration was built and checked with SDK **1.65**. Extract it into
`external/steamworks_sdk/`, so this file exists:

```text
external/steamworks_sdk/sdk/public/steam/steam_api.h
```

The SDK directory, build outputs and development `steam_appid.txt` files are
excluded from Git. Commit the game's adapter, build scripts and checks; keep
the downloaded SDK outside the published source. The game retains its existing
[license](../LICENSE.md), and Valve's SDK remains subject to its own terms.

For a different SDK location, set
`-DSTEAMWORKS_SDK_ROOT=<directory-containing-public-and-redistributable_bin>`.
The default App ID is `5442160` (Sil-Morë); override it with
`-DSIL_STEAM_APP_ID=<app-id>` when configuring a different application.

## Windows x64

Use the same MSYS2 prerequisites as the normal Windows build:

```powershell
.\build-cmake.bat steam
```

This builds a separate Release executable in `build-steam/` and stages the game,
SDL dependencies, assets and `steam_api64.dll` in `sil-more-windows-steam/`.
Standard and portable builds continue to set `SIL_STEAMWORKS=OFF`.

For an incremental rebuild after configuration:

```powershell
.\build-incremental.ps1 -Target steam
```

## Native Linux and macOS configuration

Configure on the target operating system after installing its build
dependencies and initializing the SDL submodules:

```bash
cmake -S . -B build-steam-native -DCMAKE_BUILD_TYPE=Release \
  -DSIL_BUILD_WITH_SDL_SOURCES=ON -DSIL_STEAMWORKS=ON
cmake --build build-steam-native --parallel
```

The same option selects `libsteam_api.so` on Linux and
`libsteam_api.dylib` on macOS. For a universal macOS build, also pass
`'-DCMAKE_OSX_ARCHITECTURES=x86_64;arm64'` at the first configure and
`-DSDLTTF_VENDORED=ON` to build the font dependencies for both architectures.
All linked dependencies must support the requested architectures.

CMake copies the selected Steam runtime beside `sil-more` and sets a relative
runtime search path. A release package also needs the game assets and all
SDL/font runtime dependencies. macOS additionally needs an app bundle, signing
and notarization; native macOS builds and execution have not been validated.

## Native Linux Steam release packages

Run the package helper inside an x86-64 Steam Runtime 4 SDK. It requires
Python 3.11+, CMake 3.24+, Ninja, GCC/G++, patchelf, binutils and the initialized
SDL submodules. The Steam Runtime SDK provides the Linux build environment;
the separately obtained Steamworks SDK above supplies the Steam API library.

```bash
python3 scripts/build_linux_steam.py --jobs 8 --output scripts/output/linux-release
```

The helper enables Steamworks, builds the game and SDL dependencies, stages
the native ELF files with relative library paths, and creates a ZIP rooted
for a Linux-only depot plus a tarball containing the deployment folder. It
excludes WAV source audio, sound packs, template work files, the fallback font,
logs, saves and generated caches. Use a fresh output directory for each package.

Set Steam's Linux launch executable to `sil-more.sh` and select Steam Linux
Runtime 4.0. Saves and settings normally live under `~/sil-more/`, outside the
installation. The helper creates local packages; upload and branch activation
are separate release steps.

## Local Steam testing

Start Steam with an account that owns the app. Create `steam_appid.txt` beside
the **game executable**, containing only `5442160`, and launch from that folder
when testing outside the Steam client. For a different App ID, use the same ID
in both the file and the CMake configuration.

Remove the development file from the release package before a depot upload.
Without it, directly launching a Steam-enabled executable can ask Steam to
relaunch the installed app. See
[Valve's API setup documentation](https://partner.steamgames.com/doc/sdk/api).

## PC handheld defaults

Linux and Windows builds identify Steam Deck separately from other recognized
PC handhelds. Steam-enabled builds use SDK 1.65's
[`IsRunningOnSteamHardware`](https://partner.steamgames.com/doc/api/ISteamUtils#IsRunningOnSteamHardware)
for Deck identification, including a Windows build running through Proton.
The fallback reads public manufacturer/model fields from Linux DMI or Windows
SMBIOS. Under Wine/Proton it can also read Linux DMI through the usual `Z:` host
mapping. Detection does not depend on display resolution or an attached gamepad.

The model list covers Deck LCD/OLED, ROG Ally/X/Xbox Ally, Legion Go/Go S/Go 2,
MSI Claw, listed GPD Win models, listed AYANEO handhelds, ONEXPLAYER-branded
models and ZOTAC Gaming Zone. Unknown models keep desktop defaults.

On first launch, recognized handhelds use the Android handheld starting
settings: compact terminal mode, collapsed left panel, no bottom panes,
open quick-access panel, controller Auto mode and mouse control off. They skip
the desktop controller-choice prompt. Existing saved profiles and explicit
input choices are retained. Reset Interface restores the device's defaults.
Touch-only phone orientation/font prompts remain specific to mobile devices.

Set `SIL_HANDHELD=auto`, `desktop`, `handheld` or `steamdeck` before launching
to override classification (for an unrecognized model or support testing).
The default is `auto`. This changes the starting defaults only when there is
no saved SDL config, or when Reset Interface is requested; it does not replace
saved settings. The startup log records the detected class, source and model.

Model identifier references:
[Linux Deck DMI matches](https://github.com/torvalds/linux/blob/master/sound/soc/amd/acp-config.c),
[ROG Ally](https://github.com/hhd-dev/hhd/blob/master/src/hhd/device/rog_ally/__init__.py),
[Legion Go](https://github.com/hhd-dev/hhd/blob/master/src/hhd/device/legion_go/__init__.py),
[GPD Win](https://github.com/hhd-dev/hhd/blob/master/src/hhd/device/gpd/win/__init__.py),
[AYANEO and Zone](https://github.com/hhd-dev/hhd/blob/master/src/hhd/device/generic/const.py).
Windows firmware access uses
[`GetSystemFirmwareTable`](https://learn.microsoft.com/en-us/windows/win32/api/sysinfoapi/nf-sysinfoapi-getsystemfirmwaretable).

## Validation

These checks use the separately installed SDK. They run on Windows with MSYS2,
or on a native host with GCC/Clang, CMake and Ninja:

```bash
python scripts/check_steam_platform_config.py
python scripts/check_steamworks.py
```

After a Windows standard build, `python scripts/check_handheld_startup.py`
checks model matches/exclusions, bounded SMBIOS parsing, Steam detection and
overrides, first launches and saved-profile reloads using private profiles and
dummy SDL drivers. `python scripts/check_desktop_startup_input.py` retains the
desktop prompt checks and exercises handheld input overrides. These fixtures
do not establish physical handheld or Proton input compatibility.

The platform check verifies SDK binary architectures, target selection and
error handling, then generates the production Linux/macOS CMake project using
stand-in SDL targets and host compiler settings. The adapter check compiles
the production bridge against SDK boundary test doubles and exercises
success, initialization failure, relaunch and shutdown. These checks do not
run the native game. Their generated files and logs stay in ignored
`scripts/output/` directories.

Test each release package through the real Steam client, including startup,
graphics, audio, keyboard/mouse/controller input, saves and exit. An OS package
check or an `All OSes` depot filter does not establish that native binaries
are present. Use OS-specific depots for platform binaries and dependencies,
define launch options for the actual executables, and test those builds before
publishing platform support. See
[Steam depot rules](https://partner.steamgames.com/doc/store/application/depots).

## Release source

Build releases from a committed revision and record that revision, SDK version,
toolchain and the pinned submodule revisions alongside the build receipt.
Tag the exact source commit used for each released build and make the matching
source and build scripts available with the release.

A GitHub-generated source archive does not include submodule contents. When
preparing a complete source bundle, include the pinned SDL submodules and their
required dependency sources as well as the game and its build scripts. Keep
Valve's separately obtained SDK, credentials, generated binaries, upload
archives, logs and temporary files out of the Git source commit.
