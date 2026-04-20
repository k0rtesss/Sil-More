#!/usr/bin/env python3

# Copyright (C) 2025-2026 Sil-More contributors
#
# This file is part of Sil-More.
#
# Sil-More is free software; you can redistribute it and/or modify it
# under the terms of the GNU General Public License version 2 as
# published by the Free Software Foundation.
#
# Sil-More is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See LICENSE.md
# for more details.

import subprocess
from pathlib import Path

def main():
    in_path = Path('scripts/output/new_vaults.txt')
    if not in_path.exists():
        print('Missing scripts/output/new_vaults.txt; run tools/find_new_vaults.py first')
        return 1
    ids = [l.strip() for l in in_path.read_text(encoding='utf-8').splitlines() if l.strip()]
    if not ids:
        print('No ids to render')
        return 0
    cmd = [str(Path.cwd() / 'src' / '.venv' / 'Scripts' / 'python.exe'), 'tools/vaultviz.py', 'render']
    for id_ in ids:
        cmd.extend(['--vault', id_])
    cmd.extend(['--styles', 'all', '--scale', '2', '--out-dir', 'scripts/output/vaultviz_new', '--quiet'])
    print('Running: ' + ' '.join(cmd))
    subprocess.run(cmd)
    return 0

if __name__ == '__main__':
    raise SystemExit(main())