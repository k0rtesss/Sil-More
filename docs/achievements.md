# Achievements

Achievements work locally in every build. Open **Main menu > Achievements**
(keyboard shortcut **v** inside that menu). The list uses the existing book
layout, including wrapping, page navigation, scrolling, touch, and controller
controls. New unlocks appear in the message history and the popup notification.

| Stable key / Steam API name | Name | Unlock condition |
| --- | --- | --- |
| `INTO_THE_DEEP` | Into the Deep | Reach 500 feet (dungeon depth 10). |
| `LIGHT_IN_DARKNESS` | A Light in the Darkness | Successfully cut a Silmaril from Morgoth's Iron Crown, including when a full inventory makes it drop to the floor. |
| `BACK_TO_STARLIGHT` | Back to the Starlight | Escape through the Gates carrying at least one Silmaril. |
| `THREE_LIGHTS_IN_EXILE` | Three Lights in Exile | Escape carrying all three Silmarils. |
| `THE_SHADOW_FALLS` | The Shadow Falls | Defeat Morgoth's final form. |
| `THE_QUIET_ROAD` | The Quiet Road | Escape with a Silmaril with zero player-attributed kills in a fully tracked run. |
| `WORD_UNBROKEN` | Word Unbroken | Escape with a Silmaril and a selected, unbroken oath. |
| `FORGED_BY_YOUR_HAND` | Forged by Your Hand | Complete and receive a smithing project, including a completed reforge. |
| `A_NAME_WORTH_REMEMBERING` | A Name Worth Remembering | Complete and receive a custom artefact. |
| `SONG_BENEATH_THE_MOUNTAIN` | Song Beneath the Mountain | Escape with a Silmaril, at least 20 trained Song, and at least 100 paid turns spent singing. |
| `THERE_AND_BACK_AGAIN` | There and Back Again | Enter Morgoth's actual throne room and reach the Gates alive. A Silmaril is not required. |
| `BY_A_THREAD` | By a Thread | A damaging hit leaves exactly 1 HP; then kill an enemy before fully healing or leaving the floor. |
| `LIGHT_OF_THE_ELDAR` | Light of the Eldar | Kill an undead enemy with a player-origin light attack, including a song of light. |
| `THE_LONG_TALE` | The Long Tale | Win a Tale in which at least two distinct character templates completed adventures. |

Ordinary scored adventures qualify, including the contextual gameplay
tutorial. Wizard/debug runs, active cheats, recorded cheating, and the separate
practice tutorial (`game_type != 0`) cannot earn achievements. Previously
earned achievements remain visible while playing these modes.

## Local progress and synchronization

Progress lives in `ANGBAND_DIR_APEX/achievements.json`, outside character and
Tale files. This is `<user-root>/meta/achievements.json` in standard builds and
`lib/apex/achievements.json` in portable builds. Death, a new character, and a new
Tale do not clear achievements. Character saves now store run-wide kill,
singing and comeback history in save format
**0.9.8.31**. Earlier saves still load, with safe defaults for the new fields.
The separate achievement file keeps schema version 1 and preserves all
previously unlocked achievement keys.

Unlocks are written immediately through a temporary file, flushed, closed, and
renamed over the previous file. Failed writes retain the previous file and the
in-memory unlock, and retry periodically. An unreadable, corrupt, oversized,
or unsupported-version file is preserved without overwriting it; the viewer
reports that progress could not be saved. Back up and repair/move that file if
this occurs. Unknown fields and unknown achievement keys in version 1 files
survive writes.

The local file doubles as the durable platform retry queue: every session
reconciles it with available services. Platform failures never revoke local
achievements or interrupt an adventure. Remote unlocks are merged into local
progress without replaying the local unlock notification. Dates show when this
device first recorded an unlock; imported platform unlocks use the import date.

This is one local profile per OS/app data directory. Its progress is merged
with the platform account currently using that profile. There is no new game
account, account selection UI, or direct Steam-to-Google cloud service. Moving
the same local file between installations carries these milestones with it.

Existing normal character saves can establish **Into the Deep** from their
recorded maximum depth on entering the dungeon. They can earn all event-based
milestones going forward, but **The Quiet Road** requires a new character whose
entire kill history was tracked. Old saves never assume that an unknown kill
count means zero. Singing starts accumulating from the upgrade onward.

Player-origin poison damage retains its source through save/resume, so a delayed
poison kill disqualifies a pacifist run. Monster and environmental poison kills
do not. Poison's total damage and decay are unchanged. Comeback windows survive
save/resume on the same floor, and end on full healing, lethal damage or leaving
the floor. Free menu actions do not advance singing progress.

**The Long Tale** records completed ordinary adventures by race/template in the
current Tale's local progress. Repeated finalization or Eru resurrection of the
same template cannot count twice. Living autosave snapshots and Blitz games do
not count. Only an actual Tale victory awards it; bookkeeping for finished Tales
is removed. Historical completed adventures from before this feature are not
inferred from death totals or old score snapshots. Defeated Tales also discard
their bookkeeping. Profiles written above the original 64 KiB limit can now
load up to 4 MiB; writes use compact JSON and never exceed that read limit.
Optional Tale history has a 48 KiB budget. If older abandoned records fill it,
the oldest records outside the current Tale are discarded; this can lose partial
Long Tale progress for an older unfinished Tale, but never removes earned
unlocks or invents completed adventures. The current Tale's character set is
retained. Unknown extension fields remain preserved; if they alone prevent a
profile from fitting, saving reports failure and retains the previous file.

## Steam setup

Build with the optional Steamworks integration described in
[steam-build.md](steam-build.md). In the Steamworks dashboard for the configured
App ID, create and **publish** the fourteen achievements using the exact API names
in the table above. Supply their names, descriptions, and unlocked/locked icons.
These are ordinary one-shot achievements; no Steam stat definitions are needed
for progress tracking. Keep the API names stable after release.

The adapter reads the Steam client's preloaded achievement cache, calls
`SetAchievement`, and submits batches with `StoreStats`. A successful
`UserStatsStored_t` callback confirms an upload. Failures and missing callbacks
retry with a one-minute delay; unacknowledged cached `SetAchievement` changes
are kept pending. Calls are pumped during idle input waits. The in-game viewer
also offers the Steam achievements overlay when Steam is active.

The standalone executable has no Steam SDK requirement. A Steam build whose
Steam initialization fails continues with local achievements.

API references: [Steam user stats](https://partner.steamgames.com/doc/api/ISteamUserStats),
[Steam overlay](https://partner.steamgames.com/doc/api/ISteamFriends#ActivateGameOverlay).

## Google Play setup

The Android `play` flavor includes Play Games Services v2 (`22.1.0`); `sideload`
has no Play Games dependency and keeps the same local achievement experience.
Unconfigured Play builds also work locally and do not initialize Play Games.

1. Configure the game's Play Games Services project in Play Console. Link the
   Android application ID and the signing certificate SHA-1 of each build you
   will test/distribute, including the **Play App Signing** certificate for Play
   delivery. Register test accounts while the service is unpublished.
2. Create fourteen **standard**, one-shot achievements with the conditions in the
   table above. Supply the Console metadata, points, and icons, and publish the
   Play Games configuration when releasing it.
3. Supply the project and achievement IDs to Gradle, either as project
   properties or environment variables. The project ID is the numeric **Games
   services project ID**, distinct from the Android application ID. Achievement
   IDs are the opaque values generated by Play Console, distinct from Steam
   API names. The project ID and all fourteen achievement IDs must be supplied
   together, and all achievement IDs must be distinct. Gradle derives the
   catalogue from the native enum to keep resource names and indexes aligned.

```powershell
$env:SIL_MORE_PLAY_GAMES_PROJECT_ID = '<Games services project ID>'
$env:SIL_MORE_PLAY_ACH_INTO_THE_DEEP = '<Console achievement ID>'
$env:SIL_MORE_PLAY_ACH_LIGHT_IN_DARKNESS = '<Console achievement ID>'
$env:SIL_MORE_PLAY_ACH_BACK_TO_STARLIGHT = '<Console achievement ID>'
$env:SIL_MORE_PLAY_ACH_THREE_LIGHTS_IN_EXILE = '<Console achievement ID>'
$env:SIL_MORE_PLAY_ACH_THE_SHADOW_FALLS = '<Console achievement ID>'
$env:SIL_MORE_PLAY_ACH_THE_QUIET_ROAD = '<Console achievement ID>'
$env:SIL_MORE_PLAY_ACH_WORD_UNBROKEN = '<Console achievement ID>'
$env:SIL_MORE_PLAY_ACH_FORGED_BY_YOUR_HAND = '<Console achievement ID>'
$env:SIL_MORE_PLAY_ACH_A_NAME_WORTH_REMEMBERING = '<Console achievement ID>'
$env:SIL_MORE_PLAY_ACH_SONG_BENEATH_THE_MOUNTAIN = '<Console achievement ID>'
$env:SIL_MORE_PLAY_ACH_THERE_AND_BACK_AGAIN = '<Console achievement ID>'
$env:SIL_MORE_PLAY_ACH_BY_A_THREAD = '<Console achievement ID>'
$env:SIL_MORE_PLAY_ACH_LIGHT_OF_THE_ELDAR = '<Console achievement ID>'
$env:SIL_MORE_PLAY_ACH_THE_LONG_TALE = '<Console achievement ID>'
# Run the existing Play bundle/release build with these values in its environment.
```

The IDs are build configuration, not secrets. No OAuth secret or service
account key belongs in the app. An incomplete or invalid configuration fails
Gradle configuration instead of silently disabling part of the catalogue.

The Play-only Application initializes the SDK when configuration is complete.
Automatic profile creation prompts are suppressed. Authentication checks and
provider tasks run asynchronously on Android's UI thread; the native game
thread polls a snapshot. Uploads use `unlockImmediate`, and failures retry
with bounded backoff (5 seconds to 5 minutes). Remote achievement loads restore
unlocks on a new installation. Lifecycle/account generations reject stale
responses. The **Google Play achievements** action may explicitly request
sign-in and open Google's achievements screen; gameplay never forces that UI.

Keep the JNI methods in `proguard-rules.pro`: release builds look them up by
name. Play-only manifest entries and dependencies must stay out of sideload.

API references: [Android authentication](https://developer.android.com/games/pgs/android/android-signin),
[Android achievements](https://developer.android.com/games/pgs/android/achievements),
[AchievementsClient](https://developers.google.com/android/reference/com/google/android/gms/games/AchievementsClient).

## Validation

```powershell
.\build-incremental.ps1 -Target standard
.\build-incremental.ps1 -Target steam
python scripts/check_achievements.py
python scripts/check_achievement_steam.py
python scripts/check_achievement_viewer.py
python scripts/check_achievement_events.py
python scripts/check_achievement_save.py
python scripts/check_android_achievement_catalogue.py --gradle
python scripts/check_steamworks.py
$env:JAVA_HOME = 'C:/Program Files/BellSoft/LibericaJDK-17'
cd android
.\gradlew.bat :app:compileSideloadDebugJavaWithJavac :app:compilePlayDebugJavaWithJavac --console=plain
.\gradlew.bat :app:externalNativeBuildPlayDebug -PSIL_MORE_ANDROID_ABIS=arm64-v8a --console=plain
```

The focused checks use temporary profiles and SDK-boundary doubles. They do
not authenticate to Steam/Google or modify real achievements. Before release,
test real published IDs on authorized accounts: each gameplay trigger; an
offline unlock followed by reconnection/restart; an existing remote unlock
with a fresh local profile; sign-in cancellation; Android pause/resume; and
the local viewer with keyboard, controller, and portrait/landscape touch.

## Adding another achievement

Append a stable enum entry and catalogue row in `src/achievements.{h,c}`; never
reorder existing indexes. Add the corresponding Play ID resource/configuration
and Java ID array. Gradle and both native bridge masks derive from the enum;
the catalogue check verifies Java's order against it.
Hook a successful production gameplay event after its outcome is known. Keep
eligibility, local persistence, notifications, and platform synchronization in
the shared core. Publish the matching entries in both store dashboards before
shipping a configured release.

The full catalogue above is implemented. The No Armour proposal is excluded
at the user's request. Native book pagination supports up to 64 pages so the
expanded catalogue remains complete with large text and narrow screens.
