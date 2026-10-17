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

Cloud synchronization is automatic while signed in to Play Games. Before native
progress is loaded at launch, the game checks cloud progress. A fresh installation
downloads an existing profile; a device whose local profile still matches its
last synchronized version can also load changes made on another device. The
startup check times out after 15 seconds without a provider response. Choosing
**Play offline** explicitly suspends automatic sync until the next launch; local
saving continues.

Successful saves, background saves, finalized runs and session close capture the
complete profile on the game thread, then upload an immutable copy asynchronously.
Newer checkpoints replace queued older ones. Failed requests retry while the app
is active and after resume; a successful offline SDK commit may be queued locally
until Google reconnects. A download discovered during play waits for the next
launch; it never replaces files beneath a running game.

The device remembers the account and a fingerprint of the last common profile in
`.play-cloud-sync`, outside the synchronized directories. Fingerprints compare
file names and contents, independently of ZIP compression and timestamps. If both
devices changed progress, or the Play Games account changes, automatic sync keeps
the existing profiles and asks for a choice at launch. A conflict found during
play displays a notice that the choice will be offered at the next launch. A local
progress reset does not automatically restore or overwrite the previous cloud profile.

There is no cloud entry in the main menu. If startup finds damaged cloud data,
an invalid local synchronization record, an oversized local profile, or a previous
local reset, it offers recovery at that point. **Continue locally** preserves the
existing profiles; **Restore from cloud** explicitly loads the selected complete
profile; **Back up this device** can repair a cloud copy after confirmation when
there is a valid nonempty local checkpoint. Provider conflicts present the available
versions for a choice, and a selected version is validated before resolving the
provider conflict. Automatic sync never opens a manual sign-in prompt; an explicit
recovery action can request sign-in.

Restore validates the complete archive before queuing it and keeps a recovery copy
of the previous local profile. It applies both progress directories before native
loading, with rollback for an interrupted replacement.
Recovery copies stay outside the synchronized roots as `.play-cloud-backup-*`.
Temporary save/lock files are excluded from uploads.

Google uploads are capped at **3 MiB compressed**, with a 64 MiB expanded limit
and 10,000-file validation limit. Oversized or corrupt archives are rejected
without replacing local progress. The checked desktop profile compressed to
226,338 bytes. Measure another profile with:

```powershell
python scripts/check_cloud_saves.py --profile '<profile root>'
python scripts/check_cloud_sync.py
python scripts/check_cloud_profile_paths.py
```

Steam and Google Play keep separate provider storage. This integration does not
automatically synchronize a Steam account with a Google account.

## Release checks

Keep Play's Data safety declaration and the public privacy policy consistent
with the Play Games SDK and optional Saved Games. Local archive/rollback tests,
native path migration and synchronization fixtures, platform-boundary achievement fixtures, and signed
build checks do not substitute for testing two real accounts/devices. Verify
live sign-in, an earned achievement, automatic upload/launch restore, manual
recovery, cancellation, offline retries, account switching, a conflict and a local
progress reset using the distributed beta.
