#!/usr/bin/env python3
"""Prepare or upload Sil-More Steam depots with SteamPipe.

The default action is safe and local: validate the supplied deployment folders,
write SteamPipe VDF files, and create a receipt. Use ``--preview`` to run
SteamCMD with a preview build, or ``--upload`` to upload a real build.

This script deliberately accepts no password. SteamCMD can use its saved login
state, or it will prompt for the password and Steam Guard code when
``--account`` is supplied.
"""

from __future__ import annotations

import argparse
from datetime import datetime, timezone
import json
import os
from pathlib import Path
import shlex
import subprocess
import re
import sys


ROOT = Path(__file__).resolve().parents[1]
DEFAULT_APP_ID = 5442160
DEFAULT_WINDOWS_DEPOT_ID = 5442161
DEFAULT_LINUX_DEPOT_ID = 5442162

# Local runtime state, developer artifacts, and optional source assets must not
# become part of a Steam depot even if they exist in a deployment folder.
FILE_EXCLUSIONS = (
    "steam_appid.txt",
    "log.txt",
    "generation.txt",
    "*.raw",
    "*.sav*",
    "*.log",
    "*.db",
    "*.sqlite*",
    "*.gitignore",
    "*.gitkeep",
    "*.keepalive",
    "*.tmp",
    "*.bak",
    "*.dmp",
    "*.pdb",
    "*.wav",
    "*.WAV",
    "lib/apex/*",
    "lib/bone/*",
    "lib/data/*",
    "lib/save/*",
    "lib/user/*",
    "lib/edit/misc/*",
    "lib/xtra/sound/packs/*",
    "lib/xtra/font/InputMono-Bold.ttf",
)


def project_version() -> str:
    """Read the four-part game version from the source tree."""

    text = (ROOT / "src" / "defines.h").read_text(encoding="utf-8")
    values = []
    for part in ("MAJOR", "MINOR", "PATCH", "EXTRA"):
        match = re.search(
            rf"^\s*#define\s+VERSION_{part}\s+(\d+)", text, re.MULTILINE
        )
        if not match:
            raise ValueError(f"VERSION_{part} is missing from src/defines.h")
        values.append(match.group(1))
    return ".".join(values)


def resolve_repo_path(value: Path) -> Path:
    """Resolve a CLI path relative to the repository, not the caller's cwd."""

    if value.is_absolute():
        return value.expanduser().resolve()
    return (ROOT / value).expanduser().resolve()


def vdf_path(path: Path) -> str:
    """Return a portable, quoted VDF path."""

    value = path.resolve().as_posix().replace('"', '\\"')
    return f'"{value}"'


def vdf_value(value: object) -> str:
    """Return a quoted VDF scalar."""

    text = str(value).replace('"', '\\"')
    return f'"{text}"'


def validate_content_root(kind: str, root: Path) -> None:
    """Reject a missing or obviously wrong Steam deployment folder."""

    if not root.is_dir():
        raise ValueError(f"{kind} content root does not exist: {root}")

    required = {
        "windows": ("sil-more.exe", "steam_api64.dll", "lib"),
        "linux": ("sil-more", "sil-more.sh", "libsteam_api.so", "lib"),
    }[kind]
    missing = [name for name in required if not (root / name).exists()]
    if missing:
        raise ValueError(
            f"{kind} content root is missing required entries: "
            + ", ".join(missing)
        )

    app_id_files = list(root.rglob("steam_appid.txt"))
    if app_id_files:
        paths = ", ".join(str(path.relative_to(root)) for path in app_id_files)
        raise ValueError(
            f"Remove development Steam App ID file(s) before upload: {paths}"
        )


def depot_vdf(depot_id: int, content_root: Path) -> str:
    """Render a root-layout depot build script."""

    lines = [
        '"DepotBuild"',
        "{",
        f"    \"DepotID\" {vdf_value(depot_id)}",
        f"    \"ContentRoot\" {vdf_path(content_root)}",
        '    "FileMapping"',
        "    {",
        '        "LocalPath" "*"',
        '        "DepotPath" "."',
        '        "Recursive" "1"',
        "    }",
    ]
    for pattern in FILE_EXCLUSIONS:
        lines.append(f"    \"FileExclusion\" {vdf_value(pattern)}")
    lines.extend(["}", ""])
    return "\n".join(lines)


def app_vdf(
    app_id: int,
    description: str,
    build_output: Path,
    depot_scripts: list[tuple[int, Path]],
    preview: bool,
    set_live: str | None,
) -> str:
    """Render an app build that can contain one or both OS depots."""

    lines = [
        '"AppBuild"',
        "{",
        f"    \"AppID\" {vdf_value(app_id)}",
        f"    \"Desc\" {vdf_value(description)}",
        f"    \"BuildOutput\" {vdf_path(build_output)}",
        f"    \"Preview\" {vdf_value(1 if preview else 0)}",
    ]
    if set_live:
        lines.append(f"    \"SetLive\" {vdf_value(set_live)}")
    lines.extend(['    "Depots"', "    {"])
    for depot_id, script_path in depot_scripts:
        lines.append(f"        {vdf_value(depot_id)} {vdf_path(script_path)}")
    lines.extend(["    }", "}", ""])
    return "\n".join(lines)


def find_steamcmd(explicit: Path | None, sdk_root: Path | None) -> Path:
    """Find the SteamCMD shipped with the Steamworks SDK."""

    if explicit:
        path = resolve_repo_path(explicit)
        if not path.is_file():
            raise ValueError(f"SteamCMD executable was not found: {path}")
        return path

    if sdk_root is None:
        env_root = os.environ.get("STEAMWORKS_SDK_ROOT")
        sdk_root = Path(env_root) if env_root else ROOT / "external" / "steamworks_sdk" / "sdk"
    root = resolve_repo_path(sdk_root)
    if os.name == "nt":
        candidates = [root / "tools" / "ContentBuilder" / "builder" / "steamcmd.exe"]
    else:
        candidates = [root / "tools" / "ContentBuilder" / "builder_linux" / "steamcmd.sh"]

    for candidate in candidates:
        if candidate.is_file():
            return candidate
    raise ValueError(
        "SteamCMD was not found. Pass --steamcmd or --steamworks-sdk; "
        "expected the Steamworks SDK ContentBuilder builder directory."
    )


def display_command(command: list[str]) -> str:
    """Format a command without ever displaying a password."""

    return " ".join(shlex.quote(part) for part in command)


def build_output_path(version: str, explicit: Path | None) -> Path:
    if explicit:
        return resolve_repo_path(explicit)
    stamp = datetime.now(timezone.utc).strftime("%Y%m%d-%H%M%S")
    return ROOT / "scripts" / "output" / f"steam-upload-{version}-{stamp}"


def prepare(args: argparse.Namespace) -> tuple[Path, Path, dict[str, object]]:
    """Validate content, write VDFs, and return the app VDF and receipt."""

    version = args.version or project_version()
    output = build_output_path(version, args.build_output)
    vdf_dir = output / "vdf"
    steam_output = output / "steampipe-output"
    vdf_dir.mkdir(parents=True, exist_ok=True)
    steam_output.mkdir(parents=True, exist_ok=True)

    depot_scripts: list[tuple[int, Path]] = []
    roots: dict[str, Path] = {}
    if args.platform in ("windows", "both"):
        windows_root = resolve_repo_path(args.content_root)
        validate_content_root("windows", windows_root)
        path = vdf_dir / f"depot_build_{args.windows_depot_id}.vdf"
        path.write_text(depot_vdf(args.windows_depot_id, windows_root), encoding="utf-8")
        depot_scripts.append((args.windows_depot_id, path))
        roots["windows"] = windows_root

    if args.platform in ("linux", "both"):
        if args.linux_content_root is None:
            raise ValueError(
                "--linux-content-root is required for --platform linux or both"
            )
        linux_root = resolve_repo_path(args.linux_content_root)
        validate_content_root("linux", linux_root)
        path = vdf_dir / f"depot_build_{args.linux_depot_id}.vdf"
        path.write_text(depot_vdf(args.linux_depot_id, linux_root), encoding="utf-8")
        depot_scripts.append((args.linux_depot_id, path))
        roots["linux"] = linux_root

    if args.windows_depot_id == args.linux_depot_id:
        raise ValueError("Windows and Linux depot IDs must be different")

    preview = not args.upload
    if args.platform == "windows":
        default_description = f"Sil-More {version} Steam Windows x64"
    elif args.platform == "linux":
        default_description = f"Sil-More {version} Steam native Linux x86-64"
    else:
        default_description = f"Sil-More {version} Steam Windows x64 + native Linux x86-64"
    description = args.description or default_description
    app_path = vdf_dir / f"app_build_{args.app_id}.vdf"
    app_path.write_text(
        app_vdf(
            args.app_id,
            description,
            steam_output,
            depot_scripts,
            preview,
            args.set_live,
        ),
        encoding="utf-8",
    )

    receipt: dict[str, object] = {
        "app_id": args.app_id,
        "version": version,
        "platform": args.platform,
        "preview": preview,
        "windows_depot_id": args.windows_depot_id,
        "linux_depot_id": args.linux_depot_id,
        "content_roots": {key: str(value) for key, value in roots.items()},
        "app_build_vdf": str(app_path),
        "depot_build_vdfs": [str(path) for _, path in depot_scripts],
        "steam_pipe_output": str(steam_output),
        "set_live": args.set_live,
        "file_exclusions": list(FILE_EXCLUSIONS),
        "created_utc": datetime.now(timezone.utc).isoformat(),
    }
    (output / "prepare-receipt.json").write_text(
        json.dumps(receipt, indent=2) + "\n", encoding="utf-8"
    )
    return app_path, output, receipt


def run_steamcmd(args: argparse.Namespace, app_path: Path) -> int:
    steamcmd = find_steamcmd(args.steamcmd, args.steamworks_sdk)
    command = [str(steamcmd)]
    if args.account:
        command.extend(["+login", args.account])
    command.extend(["+run_app_build", str(app_path), "+quit"])
    print("SteamCMD command:")
    print("  " + display_command(command))
    print("SteamCMD will prompt for a password/Steam Guard code if needed.")
    completed = subprocess.run(command, cwd=steamcmd.parent, check=False)
    return completed.returncode


def make_parser() -> argparse.ArgumentParser:
    result = argparse.ArgumentParser(description=__doc__)
    result.add_argument(
        "--platform",
        choices=("windows", "linux", "both"),
        default="windows",
        help="Depot(s) to prepare or upload (default: windows)",
    )
    result.add_argument(
        "--content-root",
        type=Path,
        default=ROOT / "sil-more-windows-steam",
        help="Windows Steam deployment folder",
    )
    result.add_argument(
        "--linux-content-root",
        type=Path,
        help="Clean sil-more-linux-steam folder from build_linux_steam.py",
    )
    result.add_argument("--app-id", type=int, default=DEFAULT_APP_ID)
    result.add_argument(
        "--windows-depot-id", type=int, default=DEFAULT_WINDOWS_DEPOT_ID
    )
    result.add_argument(
        "--linux-depot-id", type=int, default=DEFAULT_LINUX_DEPOT_ID
    )
    result.add_argument("--version", help="Override the version read from src/defines.h")
    result.add_argument("--description", help="SteamPipe build description")
    result.add_argument(
        "--build-output",
        type=Path,
        help="Output folder for VDFs, manifests, logs, and the receipt",
    )
    result.add_argument(
        "--steamworks-sdk",
        type=Path,
        help="Steamworks SDK root containing public/ and tools/",
    )
    result.add_argument(
        "--steamcmd",
        type=Path,
        help="Explicit steamcmd.exe or steamcmd.sh path",
    )
    result.add_argument(
        "--account",
        help="Steam builder account name; never pass the password on the command line",
    )
    action = result.add_mutually_exclusive_group()
    action.add_argument(
        "--preview",
        action="store_true",
        help="Run SteamCMD with Preview=1 after preparing the VDFs",
    )
    action.add_argument(
        "--upload",
        action="store_true",
        help="Run SteamCMD with Preview=0 and upload a real build",
    )
    result.add_argument(
        "--set-live",
        metavar="BRANCH",
        help="Set the uploaded build live on a non-default beta branch",
    )
    return result


def main() -> int:
    argument_parser = make_parser()
    args = argument_parser.parse_args()
    if args.set_live and not args.upload:
        argument_parser.error("--set-live requires --upload")
    if args.set_live and args.set_live.lower() == "default":
        argument_parser.error("--set-live cannot target the default branch")

    try:
        app_path, output, receipt = prepare(args)
    except ValueError as error:
        argument_parser.error(str(error))

    print(f"Prepared SteamPipe files: {output}")
    print(f"  App build: {app_path}")
    print(f"  Platforms: {receipt['platform']}")
    if not (args.preview or args.upload):
        print("No SteamCMD call made. Use --preview to generate manifests or --upload to upload.")
        return 0

    return run_steamcmd(args, app_path)


if __name__ == "__main__":
    sys.exit(main())
