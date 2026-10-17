# Cloud saves

The progress profile contains `save/` (characters) and `meta/` (Tales, scores,
custom artefacts and achievements). Standard builds place both directories
under the same user root. Screen layout, sound settings, generated caches and
logs are outside these synchronized roots.

Desktop builds migrate missing `achievements.json` and `artefacts.db` from the
old installation `lib/apex/` directory to the user's `meta/` directory. Existing
destination files take precedence, and the original files are preserved.
Portable builds keep their existing local-data paths.

## Steam: Windows and Linux

App **5442160** uses Steam Auto-Cloud after the game exits, and downloads the
profile before launch. Sign in to the same Steam account on both systems, use
the current game version, and let Steam finish syncing before switching devices.

| Root | Subdirectory | Pattern | Recursive |
| --- | --- | --- | --- |
| WinSavedGames | `sil-more/save` | `*` | No |
| WinSavedGames | `sil-more/meta` | `*` | Yes |

The Linux root override maps `WinSavedGames` to `LinuxHome`, without an added
path. Both roots have no OS restriction, which lets the same files synchronize
between Windows (`%USERPROFILE%/Saved Games/sil-more/`) and Linux
(`~/sil-more/`). Device-specific preferences remain local. Dynamic Cloud Sync
is off; the game does not implement its suspend/resume protocol.

The configured per-user quota is **100,000,000 bytes and 500 files**. The checked
profile on October 17, 2026 contained 27 files totaling 1,371,105 bytes. This is
a measurement of that profile, not an unlimited-history guarantee.

## Google Play

The `play` flavor uses Play Games Services project **699016262526**, Android
package `com.silmore.myapp`, and Saved Games snapshot `sil_more_progress_v1`.
The sideload flavor has no Google SDK or cloud action. Publish the OAuth consent
configuration and Play Games project, enable Saved Games, and register the
Google Play app-signing certificate in the Android OAuth credential. Upload-key
signing is still required for the AAB sent to Play Console.

Open **Main menu > Cloud saves** (`w`) and choose **Save to cloud** or **Restore
from cloud**. These are manual whole-profile operations. The native game first
checkpoints progress and pauses while the provider dialog is open. Replacing a
different cloud profile requires confirmation; provider conflicts present the
available versions for a choice. A successful offline SDK commit may be queued
locally until Google reconnects.

Restore validates the complete archive before queuing it, keeps a recovery copy
of the previous local profile, and restarts the app. It applies both progress
directories before native loading, with rollback for an interrupted replacement.
Recovery copies stay outside the synchronized roots as `.play-cloud-backup-*`.
Temporary save/lock files are excluded from uploads.

Google uploads are capped at **3 MiB compressed**, with a 64 MiB expanded limit
and 10,000-file validation limit. Oversized or corrupt archives are rejected
without replacing local progress. The checked desktop profile compressed to
226,338 bytes. Measure another profile with:

```powershell
python scripts/check_cloud_saves.py --profile '<profile root>'
python scripts/check_cloud_profile_paths.py
```

Steam and Google Play keep separate provider storage. This integration does not
automatically synchronize a Steam account with a Google account.

## Release checks

Keep Play's Data safety declaration and the public privacy policy consistent
with the Play Games SDK and optional Saved Games. Local archive/rollback tests,
native path migration tests, platform-boundary achievement fixtures, and signed
build checks do not substitute for testing two real accounts/devices. Verify
live sign-in, an earned achievement, an explicit cloud save/restore, cancellation,
offline behavior and a conflict using the distributed beta.
