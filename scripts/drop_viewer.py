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

"""
Drop Group Viewer -- shows all eligible drop groups for a given depth.

Groups match the C drop system logic (drop-system-catalog.c / drop-system-selection.c):
  - Normal items: grouped by (tval, sval) -- one group per base item type
  - Ego items:   grouped by (ego_combo, tval, sval) -- one group per ego + base type
  - Artefacts:   each is its own group

Usage:
  python drop_viewer.py [depth]          -- show groups at depth (must be 1-20+)
  python drop_viewer.py                  -- interactive: prompts for depth
  python drop_viewer.py --cat weapon     -- filter to one category
  python drop_viewer.py --min-weight N   -- hide groups with weight below N
  python drop_viewer.py --diff L U       -- filter to difficulty range [L, U]
"""

import sys
import os
import argparse

# Enable UTF-8 output on Windows consoles that support it
if sys.platform == 'win32':
    try:
        import ctypes
        ctypes.windll.kernel32.SetConsoleMode(
            ctypes.windll.kernel32.GetStdHandle(-11), 7)
        sys.stdout.reconfigure(encoding='utf-8', errors='replace')
    except Exception:
        pass

# Allow importing sibling scripts from the same directory
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import calc_artefact_difficulty as _cad
from calc_artefact_difficulty import (
    parse_artefact_file,
    parse_special_file,
    parse_object_file,
    parse_ability_file,
    populate_objects_dict,
    resolve_artefact_name,
    generate_normal_variants,
    generate_special_variants,
    generate_dual_ego_variants,
    calculate_difficulty,
    rarity_from_schedule_py,
    schedule_min_depth_py,
    schedule_max_depth_cap_py,
    clean_obj_name,
    get_tval_name,
)

# ---------------------------------------------------------------------------
# Drop category constants (match C DROP_CAT_* enum)
# ---------------------------------------------------------------------------

DROP_CAT_WEAPON  = 0
DROP_CAT_ARMOR   = 1
DROP_CAT_JEWELRY = 2
DROP_CAT_SUPPLY  = 3
DROP_CAT_NAMES   = ["Weapon", "Armor", "Jewelry", "Supply"]

# Mirrors drop_category_for_kind() in src/drop/drop-system-catalog.c exactly.
# TV_* and SV_* values from src/defines.h.
_TV_ARROW     = 17
_TV_SWORD     = 23
_TV_HAFTED    = 21
_TV_POLEARM   = 22
_TV_BOW       = 19
_TV_DIGGING   = 20
_TV_MAIL      = 37
_TV_SOFT_ARMOR = 36
_TV_SHIELD    = 34
_TV_CLOAK     = 35
_TV_BOOTS     = 30
_TV_GLOVES    = 31
_TV_HELM      = 32
_TV_CROWN     = 33
_TV_RING      = 45
_TV_AMULET    = 40
_TV_LIGHT     = 39
_TV_HORN      = 66   # JEWELRY (not supply)
_TV_POTION    = 75
_TV_STAFF     = 70
_TV_GEM       = 56
_TV_FOOD      = 80   # Herbs
_TV_FLASK     = 77

# TV_LIGHT svals
_SV_LIGHT_TORCH        = 0
_SV_LIGHT_LANTERN      = 1
_SV_LIGHT_LESSER_JEWEL = 2
_SV_LIGHT_MALLORN      = 3
_SV_LIGHT_FEANORIAN    = 8
_SV_LIGHT_SILMARIL     = 9

# TV_DIGGING svals that go to supply (plain shovels/mattocks)
_SV_SHOVEL  = 1
_SV_MATTOCK = 3


def get_drop_category(tval, sval=None):
    """Mirror C drop_category_for_kind() from drop-system-catalog.c."""
    if tval in (_TV_ARROW, _TV_SWORD, _TV_HAFTED, _TV_POLEARM, _TV_BOW):
        return DROP_CAT_WEAPON
    if tval == _TV_DIGGING:
        # Plain shovels/mattocks -> supply; all others (ego digging tools) -> weapon
        if sval in (_SV_SHOVEL, _SV_MATTOCK):
            return DROP_CAT_SUPPLY
        return DROP_CAT_WEAPON
    if tval in (_TV_MAIL, _TV_SOFT_ARMOR, _TV_SHIELD, _TV_CLOAK,
                _TV_BOOTS, _TV_GLOVES, _TV_HELM, _TV_CROWN):
        return DROP_CAT_ARMOR
    if tval in (_TV_RING, _TV_AMULET, _TV_HORN):
        return DROP_CAT_JEWELRY
    if tval == _TV_LIGHT:
        if sval in (_SV_LIGHT_FEANORIAN, _SV_LIGHT_SILMARIL):
            return DROP_CAT_JEWELRY
        if sval in (_SV_LIGHT_TORCH, _SV_LIGHT_LANTERN,
                    _SV_LIGHT_LESSER_JEWEL, _SV_LIGHT_MALLORN):
            return DROP_CAT_SUPPLY
        return None  # unknown light sval -- excluded
    if tval in (_TV_POTION, _TV_STAFF, _TV_GEM, _TV_FOOD, _TV_FLASK):
        return DROP_CAT_SUPPLY
    return None  # unknown / excluded


# ---------------------------------------------------------------------------
# Group building
# ---------------------------------------------------------------------------

def _rarity_at_depth(rarity_schedule, depth):
    """Return the rarity weight for a group at the given depth (0 = ineligible)."""
    return rarity_from_schedule_py(rarity_schedule, depth, 0)


def build_drop_groups(normals, specials, dual_egos, artefacts):
    """
    Convert generated item variants into a flat list of drop-group records.

    Each record:
      group_key       : hashable key uniquely identifying the group
      type            : 'normal' | 'ego' | 'dual_ego' | 'artefact'
      name            : display name
      category        : DROP_CAT_*  (None = supply/ignored)
      tval / sval     : representative item type
      rarity_schedule : [(depth, rarity), ...] -- same for all variants in group
      min_depth       : depth at which the group first appears
      diff_min/max    : difficulty range across all stat-variants in the group
      variant_count   : number of stat-variants (informational)
    """
    groups = {}   # group_key -> record

    # --- Normal items: grouped by (tval, sval) ---
    for item in normals:
        key = ('normal', item['tval'], item['sval'])
        if key not in groups:
            sched = item['rarity_schedule']
            groups[key] = {
                'group_key'       : key,
                'type'            : 'normal',
                'name'            : item['name'],
                'category'        : get_drop_category(item['tval'], item['sval']),
                'tval'            : item['tval'],
                'sval'            : item['sval'],
                'rarity_schedule' : sched,
                'min_depth'       : item.get('depth', 1),
                'max_depth'       : item.get('max_depth', 0),
                'diff_min'        : item['difficulty'],
                'diff_max'        : item['difficulty'],
                'variant_count'   : 0,
            }
        g = groups[key]
        g['variant_count'] += 1
        if item['difficulty'] < g['diff_min']:
            g['diff_min'] = item['difficulty']
        if item['difficulty'] > g['diff_max']:
            g['diff_max'] = item['difficulty']

    # --- Single ego items: grouped by (special_idx, tval, sval) ---
    for item in specials:
        key = ('ego', item['special_idx'], item['tval'], item['sval'])
        if key not in groups:
            is_pfx  = item.get('is_prefix', False)
            base_nm = item.get('base_name', clean_obj_name(item['name']))
            ego_nm  = item.get('special_name', '')
            name    = f"{ego_nm} {base_nm}" if is_pfx else f"{base_nm} {ego_nm}"
            sched   = item['rarity_schedule']
            cat = get_drop_category(item['tval'], item['sval'])
            # Mirror C add_drop_entry overrides:
            # Ego digging tools go to weapon (normals stay in supply)
            if item['tval'] == _TV_DIGGING:
                cat = DROP_CAT_WEAPON
            # Lesser Jewel of Grace goes to jewelry (EGO_GRACE = 75)
            if (item['tval'] == _TV_LIGHT and item['sval'] == _SV_LIGHT_LESSER_JEWEL
                    and item.get('special_idx') == 75):
                cat = DROP_CAT_JEWELRY
            groups[key] = {
                'group_key'       : key,
                'type'            : 'ego',
                'name'            : name,
                'special_idx'     : item['special_idx'],
                'special_name'    : ego_nm,
                'is_prefix'       : is_pfx,
                'category'        : cat,
                'tval'            : item['tval'],
                'sval'            : item['sval'],
                'rarity_schedule' : sched,
                'min_depth'       : item.get('depth', 1),
                'max_depth'       : item.get('max_depth', 0),
                'diff_min'        : item['difficulty'],
                'diff_max'        : item['difficulty'],
                'variant_count'   : 0,
            }
        g = groups[key]
        g['variant_count'] += 1
        if item['difficulty'] < g['diff_min']:
            g['diff_min'] = item['difficulty']
        if item['difficulty'] > g['diff_max']:
            g['diff_max'] = item['difficulty']

    # --- Dual ego items: grouped by (prefix_idx, suffix_idx, tval, sval) ---
    for item in dual_egos:
        key = ('dual_ego', item['prefix_idx'], item['suffix_idx'], item['tval'], item['sval'])
        if key not in groups:
            base_nm  = item.get('base_name', '')
            pfx_nm   = item.get('prefix_name', '')
            sfx_nm   = item.get('suffix_name', '')
            name     = f"{pfx_nm} {base_nm} {sfx_nm}".strip()
            sched    = item['rarity_schedule']
            cat = get_drop_category(item['tval'], item['sval'])
            # Mirror C add_drop_entry overrides:
            # Ego digging tools go to weapon (normals stay in supply)
            if item['tval'] == _TV_DIGGING:
                cat = DROP_CAT_WEAPON
            groups[key] = {
                'group_key'       : key,
                'type'            : 'dual_ego',
                'name'            : name,
                'prefix_idx'      : item['prefix_idx'],
                'suffix_idx'      : item['suffix_idx'],
                'category'        : cat,
                'tval'            : item['tval'],
                'sval'            : item['sval'],
                'rarity_schedule' : sched,
                'min_depth'       : item.get('depth', 1),
                'max_depth'       : item.get('max_depth', 0),
                'diff_min'        : item['difficulty'],
                'diff_max'        : item['difficulty'],
                'variant_count'   : 0,
            }
        g = groups[key]
        g['variant_count'] += 1
        if item['difficulty'] < g['diff_min']:
            g['diff_min'] = item['difficulty']
        if item['difficulty'] > g['diff_max']:
            g['diff_max'] = item['difficulty']

    # --- Artefacts: each is its own group ---
    for art in artefacts:
        key = ('artefact', art.get('idx', id(art)))
        sched = art.get('rarity_schedule', [(art.get('depth', 1), art.get('rarity', 0))])
        cap = schedule_max_depth_cap_py(sched)
        art_cat = get_drop_category(art['tval'], art['sval'])
        # Mirror C add_drop_entry overrides:
        # Artefact digging tools go to weapon
        if art['tval'] == _TV_DIGGING:
            art_cat = DROP_CAT_WEAPON
        # Artefact lesser jewels go to jewelry
        if art['tval'] == _TV_LIGHT and art['sval'] == _SV_LIGHT_LESSER_JEWEL:
            art_cat = DROP_CAT_JEWELRY
        groups[key] = {
            'group_key'       : key,
            'type'            : 'artefact',
            'name'            : art['name'],
            'idx'             : art.get('idx', 0),
            'category'        : art_cat,
            'tval'            : art['tval'],
            'sval'            : art['sval'],
            'rarity_schedule' : sched,
            'min_depth'       : art.get('depth', 1),
            'max_depth'       : max(cap, 0),
            'diff_min'        : art['difficulty'],
            'diff_max'        : art['difficulty'],
            'variant_count'   : 1,
        }

    return list(groups.values())


# ---------------------------------------------------------------------------
# Display
# ---------------------------------------------------------------------------

# ANSI color helpers
_USE_COLOR = True

def _c(code, text):
    if not _USE_COLOR:
        return text
    return f"\033[{code}m{text}\033[0m"

def _yellow(t): return _c('93', t)
def _cyan(t):   return _c('96', t)
def _green(t):  return _c('92', t)
def _dim(t):    return _c('90', t)
def _bold(t):   return _c('1',  t)


TYPE_COLORS = {
    'normal'    : lambda t: t,          # no color
    'ego'       : _green,
    'dual_ego'  : _cyan,
    'artefact'  : _yellow,
}

CAT_LABELS = {
    DROP_CAT_WEAPON  : "WEAPON",
    DROP_CAT_ARMOR   : "ARMOR",
    DROP_CAT_JEWELRY : "JEWELRY",
    DROP_CAT_SUPPLY  : "SUPPLY",
}


def _eligible_groups(all_groups, depth, cat_filter=None, min_weight=0, diff_lo=None, diff_hi=None):
    """Return annotated groups that are eligible at depth, grouped by category."""
    by_cat = {i: [] for i in range(4)}

    for g in all_groups:
        cat = g.get('category')
        if cat is None:
            continue
        if cat_filter is not None and cat != cat_filter:
            continue

        # Max-depth cap: exclude when depth strictly exceeds cap
        max_d = g.get('max_depth', 0)
        if max_d > 0 and depth > max_d:
            continue

        w = _rarity_at_depth(g['rarity_schedule'], depth)
        if w < max(1, min_weight):
            continue

        # Difficulty filter: include group if any of its variants are in range
        if diff_lo is not None and g['diff_max'] < diff_lo:
            continue
        if diff_hi is not None and g['diff_min'] > diff_hi:
            continue

        g2 = dict(g)
        g2['weight_at_depth'] = w
        by_cat[cat].append(g2)

    return by_cat


def show_groups_at_depth(all_groups, depth,
                         cat_filter=None, min_weight=0, diff_lo=None, diff_hi=None):
    """
    Print all eligible drop groups at the given depth, organised by category.
    """
    eligible_by_cat = _eligible_groups(all_groups, depth, cat_filter, min_weight,
                                       diff_lo, diff_hi)

    W = 118  # total line width

    grand_total_weight = sum(sum(g['weight_at_depth'] for g in glst)
                             for glst in eligible_by_cat.values())
    grand_total_groups = sum(len(glst) for glst in eligible_by_cat.values())

    print()
    print(_bold("=" * W))
    print(_bold(f"  DROP GROUPS AT DEPTH {depth}"
                f"   ({grand_total_groups} groups, total weight {grand_total_weight})"))
    print(_bold("=" * W))

    for cat_id in range(4):
        glst = eligible_by_cat[cat_id]
        if not glst:
            continue

        total_w = sum(g['weight_at_depth'] for g in glst)
        glst_sorted = sorted(glst, key=lambda g: (-g['weight_at_depth'], g['name']))

        cat_label = CAT_LABELS[cat_id]
        print()
        print(_bold(f"  -- {cat_label} --  ({len(glst)} groups, total weight {total_w})"))
        print()

        hdr = (f"  {'Weight':>6}  {'% cat':>5}  {'% all':>5}  "
               f"{'MinDp':>5}  {'MaxDp':>5}  {'Diff':>9}  {'Vars':>4}  "
               f"{'Type':<9}  Name")
        print(_dim(hdr))
        print(_dim("  " + "-" * (W - 2)))

        for g in glst_sorted:
            w        = g['weight_at_depth']
            pct_cat  = 100.0 * w / total_w          if total_w          > 0 else 0.0
            pct_all  = 100.0 * w / grand_total_weight if grand_total_weight > 0 else 0.0
            dmin, dmax = g['diff_min'], g['diff_max']
            diff_str = f"{dmin}" if dmin == dmax else f"{dmin}-{dmax}"
            max_d    = g.get('max_depth', 0)
            maxd_str = str(max_d) if max_d > 0 else "-"
            gtype    = g['type'].replace('_', '-')
            name     = g['name']
            max_name = W - 56
            if len(name) > max_name:
                name = name[:max_name - 1] + '...'

            color_fn = TYPE_COLORS.get(g['type'], lambda t: t)
            row = (f"  {w:>6}  {pct_cat:>4.1f}%  {pct_all:>4.1f}%  "
                   f"{g['min_depth']:>5}  {maxd_str:>5}  {diff_str:>9}  {g['variant_count']:>4}  "
                   f"{gtype:<9}  {name}")
            print(color_fn(row))

    print()
    print(_dim("=" * W))
    print(_dim("  Legend:  Weight = rarity weight  | % cat = within category  "
               "| % all = across all categories"))
    print(_dim("           MinDp/MaxDp = depth range  | Diff = smithing difficulty range"))
    print(_dim("  Colors:  normal | " + _green("ego") + " | " + _cyan("dual-ego") +
               " | " + _yellow("artefact")))
    print()


# Default category weights (match C DROP_DEFAULT_CAT_WEIGHT = 25)
_DEFAULT_CAT_WEIGHT = 25


def _c_merge_key(g):
    """Compute the C equivalence key for drop_entries_share_group().

    C ego rule:    same prefix + same suffix + same weight at depth.
    C normal rule: same sval + same weight at depth.
    C artefact rule: each artefact is its own group (unique idx).
    """
    w     = g['weight_at_depth']
    gtype = g['type']
    if gtype == 'artefact':
        return ('artefact', g.get('idx', id(g)))
    if gtype == 'normal':
        return ('normal', w)
    if gtype == 'ego':
        pfx = g['special_idx'] if g.get('is_prefix') else -1
        sfx = g['special_idx'] if not g.get('is_prefix') else -1
        return ('ego', pfx, sfx, w)
    # dual_ego
    return ('dual_ego', g['prefix_idx'], g['suffix_idx'], w)


def _merge_c_groups(glst):
    """Merge eligible groups the way C's drop_entries_share_group() would.

    Items that share the same C merge key form one group; within that group
    the game picks uniformly (rand_int(entry_count)).

    Returns a list of merged-group dicts, each adding:
      'member_count'  -- number of original groups merged (N in the 1/N column)
      'names'         -- list of all constituent group names
    """
    from collections import OrderedDict
    merged = OrderedDict()
    for g in glst:
        key = _c_merge_key(g)
        if key in merged:
            m = merged[key]
            m['member_count'] += 1
            m['names'].append(g['name'])
            m['variant_count'] += g['variant_count']
            m['eff_diff_min'] = min(m['eff_diff_min'], g['eff_diff_min'])
            m['eff_diff_max'] = max(m['eff_diff_max'], g['eff_diff_max'])
            m['min_depth']    = min(m['min_depth'],    g['min_depth'])
        else:
            mg = dict(g)
            mg['member_count'] = 1
            mg['names'] = [g['name']]
            merged[key] = mg
    return list(merged.values())


def show_drop_simulation(all_groups, depth, throw):
    """
    Simulate a single drop at 'depth' after a difficulty roll of 'throw'.

    Eligible items: effective difficulty in [throw-2, throw+2].
    Groups are merged the same way C does (same ego combo + same weight -> one group;
    item within group chosen uniformly). The 1/N column shows this split.
    """
    diff_lo = throw - 2
    diff_hi = throw + 2

    def _eff_diff_min(g):
        return g['diff_min'] + max(0, 2 * (g['min_depth'] - depth))

    def _eff_diff_max(g):
        return g['diff_max'] + max(0, 2 * (g['min_depth'] - depth))

    eligible_by_cat = _eligible_groups(all_groups, depth)

    # Difficulty filter + C-merge
    filtered_by_cat = {}
    for cat_id, glst in eligible_by_cat.items():
        kept = []
        for g in glst:
            eff_min = _eff_diff_min(g)
            eff_max = _eff_diff_max(g)
            if eff_max < diff_lo or eff_min > diff_hi:
                continue
            g2 = dict(g)
            g2['eff_diff_min'] = eff_min
            g2['eff_diff_max'] = eff_max
            kept.append(g2)
        filtered_by_cat[cat_id] = _merge_c_groups(kept)

    cat_weights      = {i: _DEFAULT_CAT_WEIGHT for i in range(4)}
    total_cat_weight = sum(cat_weights[i] for i in range(4) if filtered_by_cat.get(i))

    W = 118
    grand_total_groups = sum(len(v) for v in filtered_by_cat.values())
    diff_range_str = f"throw {throw}  [{diff_lo}, {diff_hi}]"

    print()
    print(_bold("=" * W))
    print(_bold(f"  DROP SIMULATION -- depth {depth}, {diff_range_str}"
                f"   ({grand_total_groups} C-groups)"))
    print(_bold("=" * W))

    for cat_id in range(4):
        glst    = filtered_by_cat.get(cat_id, [])
        cat_w   = cat_weights[cat_id] if glst else 0
        pct_cat = 100.0 * cat_w / total_cat_weight if total_cat_weight > 0 else 0.0

        cat_label    = CAT_LABELS[cat_id]
        total_group_w = sum(g['weight_at_depth'] for g in glst)

        print()
        print(_bold(f"  -- {cat_label} ({pct_cat:4.1f}% of drops) --  "
                    f"{len(glst)} C-groups, pool weight {total_group_w}"))

        if not glst:
            print(_dim("       (no eligible groups at this depth/difficulty)"))
            continue

        print()
        hdr = (f"  {'GrpWt':>6}  {'%cat':>5}  {'%drop':>5}  {'1/N':>4}  "
               f"{'MinDp':>5}  {'EffDiff':>10}  {'Vars':>4}  "
               f"{'Type':<9}  Name")
        print(_dim(hdr))
        print(_dim("  " + "-" * (W - 2)))

        glst_sorted = sorted(glst, key=lambda g: (-g['weight_at_depth'], g['names'][0]))
        for g in glst_sorted:
            w        = g['weight_at_depth']
            n        = g['member_count']
            pct_grp  = 100.0 * w / total_group_w if total_group_w > 0 else 0.0
            # P(this item) = P(cat) * P(group|cat) * P(item|group) = pct_cat * pct_grp/100 / n
            pct_drop = pct_cat * pct_grp / 100.0 / n

            ed_min_d = max(g['eff_diff_min'], diff_lo)
            ed_max_d = min(g['eff_diff_max'], diff_hi)
            ed_str   = f"{ed_min_d}" if ed_min_d == ed_max_d else f"{ed_min_d}-{ed_max_d}"

            in_ratio = f"1/{n}" if n > 1 else "    "
            gtype    = g['type'].replace('_', '-')
            name     = " / ".join(g['names'])
            max_name = W - 62
            if len(name) > max_name:
                name = name[:max_name - 1] + '...'

            color_fn = TYPE_COLORS.get(g['type'], lambda t: t)
            row = (f"  {w:>6}  {pct_grp:>4.1f}%  {pct_drop:>4.1f}%  {in_ratio:>4}  "
                   f"{g['min_depth']:>5}  {ed_str:>10}  {g['variant_count']:>4}  "
                   f"{gtype:<9}  {name}")
            print(color_fn(row))

    print()
    print(_dim("=" * W))
    print(_dim("  %cat = P(group selected within category)  "
               "| %drop = P(this specific item from any drop)  "
               "| 1/N = uniform selection within C-group"))
    print(_dim(f"  EffDiff = difficulty within throw band [{diff_lo}, {diff_hi}]"))
    print(_dim("  C-group merges: ego items with same ego+weight; "
               "normal items with same sval+weight"))
    print(_dim("  Colors:  normal | " + _green("ego") + " | " + _cyan("dual-ego") +
               " | " + _yellow("artefact")))
    print()


# ---------------------------------------------------------------------------
# Data loading (shared between main and interactive loop)
# ---------------------------------------------------------------------------

def _find_file(base_paths, filename):
    """Return the first existing path among candidates, or None."""
    for p in base_paths:
        full = os.path.join(p, filename)
        if os.path.exists(full):
            return full
    return None


def load_all_data(script_dir=None):
    """
    Parse all edit files and compute drop groups.
    Returns (all_groups, load_notes) where load_notes is a list of strings.
    """
    if script_dir is None:
        try:
            script_dir = os.path.dirname(os.path.abspath(__file__))
        except NameError:
            script_dir = os.getcwd()

    edit_search = [
        os.path.join(script_dir, '..', 'lib', 'edit'),
        os.path.join(script_dir, 'lib', 'edit'),
        r'c:\Users\efrem\Documents\GitHub\sil-qh\lib\edit',
    ]

    notes = []

    def load(fname):
        p = _find_file(edit_search, fname)
        if p is None:
            notes.append(f"WARNING: {fname} not found")
        return p

    ability_file  = load('ability.txt')
    artefact_file = load('artefact.txt')
    special_file  = load('special.txt')
    object_file   = load('object.txt')

    if not artefact_file:
        raise FileNotFoundError("artefact.txt not found")

    # Ability levels
    if ability_file:
        parse_ability_file(ability_file)
        notes.append(f"Loaded {len(_cad.ABILITY_LEVELS)} ability levels")

    # Objects
    objects = parse_object_file(object_file) if object_file else []
    if objects:
        populate_objects_dict(objects)
        notes.append(f"Loaded {len(_cad.OBJECTS_BY_TYPE)} base item types")

    # Artefacts
    artefacts_raw, _sval_order = parse_artefact_file(artefact_file)
    # Filter out ultimate templates and Morgoth Crown variants
    artefacts_raw = [a for a in artefacts_raw if not (
        (a['name'].startswith("'Ultimate") and a['idx'] >= 182) or
        a['idx'] in [175, 176, 177, 178]
    )]
    # Resolve full display names (e.g. "Great Spear" + " of Melkor")
    for art in artefacts_raw:
        art['name'] = resolve_artefact_name(art)
    notes.append(f"Loaded {len(artefacts_raw)} artefacts")

    # Specials (egos)
    specials_raw = parse_special_file(special_file) if special_file else []
    notes.append(f"Loaded {len(specials_raw)} ego types")

    # Generate variants
    normals = generate_normal_variants(objects)
    notes.append(f"Generated {len(normals)} normal variants")

    specials = []
    for spec in specials_raw:
        specials.extend(generate_special_variants(spec, objects))
    notes.append(f"Generated {len(specials)} single-ego variants")

    dual_egos = generate_dual_ego_variants(specials_raw, objects)
    notes.append(f"Generated {len(dual_egos)} dual-ego variants")

    # Calculate difficulties
    for art in artefacts_raw:
        art['type'] = 'artefact'
        art['rarity_schedule'] = [(art['depth'], art['rarity'])]
        art['difficulty'] = calculate_difficulty(art)

    for item in normals:
        item['difficulty'] = calculate_difficulty(item)

    for item in specials:
        item['difficulty'] = calculate_difficulty(item)

    for item in dual_egos:
        item['difficulty'] = calculate_difficulty(item)

    notes.append(f"Difficulties calculated")

    # Build drop groups
    all_groups = build_drop_groups(normals, specials, dual_egos, artefacts_raw)
    # Filter to groups that have a known category
    all_groups = [g for g in all_groups if g['category'] is not None]
    notes.append(f"Built {len(all_groups)} drop groups")

    return all_groups, notes


# ---------------------------------------------------------------------------
# Entry point
# ---------------------------------------------------------------------------

def main():
    parser = argparse.ArgumentParser(
        description=(
            'Show eligible drop groups at a given dungeon depth.\n\n'
            'Modes:\n'
            '  depth           -- list all eligible groups (browse mode)\n'
            '  depth throw     -- simulate a single drop (depth + difficulty upper bound)\n'
            '  depth lo hi     -- browse with explicit difficulty filter'
        ),
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    parser.add_argument('depth', nargs='?', type=int,
                        help='Dungeon depth to query (1-20+)')
    parser.add_argument('throw', nargs='?', type=int,
                        help='Difficulty upper bound for simulate mode')
    parser.add_argument('diff_hi_arg', nargs='?', type=int,
                        help='Difficulty upper bound when specifying explicit range (depth lo hi)')
    parser.add_argument('--cat', choices=['weapon', 'armor', 'jewelry', 'supply'],
                        help='Filter to a single drop category (browse mode only)')
    parser.add_argument('--min-weight', '-w', type=int, default=0, metavar='N',
                        help='Hide groups with weight < N')
    parser.add_argument('--no-color', action='store_true',
                        help='Disable ANSI colour output')
    args = parser.parse_args()

    global _USE_COLOR
    if args.no_color:
        _USE_COLOR = False

    cat_filter = None
    if args.cat:
        cat_filter = {'weapon': DROP_CAT_WEAPON, 'armor': DROP_CAT_ARMOR,
                      'jewelry': DROP_CAT_JEWELRY, 'supply': DROP_CAT_SUPPLY}[args.cat]

    print("Loading data files...")
    all_groups, notes = load_all_data()
    for note in notes:
        print(f"  {note}")
    print()

    # CLI mode: all positional args supplied
    if args.depth is not None:
        if args.throw is not None and args.diff_hi_arg is not None:
            # depth lo hi  -> browse with difficulty filter
            show_groups_at_depth(all_groups, args.depth,
                                 cat_filter=cat_filter,
                                 min_weight=args.min_weight,
                                 diff_lo=args.throw, diff_hi=args.diff_hi_arg)
        elif args.throw is not None:
            # depth throw -> simulate with +/-2 band
            show_drop_simulation(all_groups, args.depth, throw=args.throw)
        else:
            # depth only -> browse
            show_groups_at_depth(all_groups, args.depth,
                                 cat_filter=cat_filter,
                                 min_weight=args.min_weight)
        return

    # Interactive loop
    print("Data loaded. Commands at the prompt:")
    print("  <depth>                -- browse all groups at this depth")
    print("  <depth> <throw>        -- simulate a drop: groups within throw +/-2 difficulty")
    print("  <depth> <lo> <hi>      -- browse with explicit difficulty window")
    print("  q / exit               -- quit")
    print()

    while True:
        try:
            raw = input("Depth [throw]: ").strip()
        except (EOFError, KeyboardInterrupt):
            break
        if raw.lower() in ('q', 'quit', 'exit', ''):
            break

        parts = raw.split()
        try:
            depth_val = int(parts[0])
        except (ValueError, IndexError):
            print("  Enter a depth number, e.g. '10' or '10 25' for simulate")
            continue

        if sys.platform == 'win32':
            os.system('cls')
        else:
            os.system('clear')

        if len(parts) >= 3:
            # depth lo hi -> browse filtered
            try:
                lo, hi = int(parts[1]), int(parts[2])
                show_groups_at_depth(all_groups, depth_val,
                                     cat_filter=cat_filter,
                                     min_weight=args.min_weight,
                                     diff_lo=lo, diff_hi=hi)
            except ValueError:
                print("  Expected: depth lo hi")
        elif len(parts) == 2:
            # depth throw -> simulate with +/-2 band
            try:
                throw_val = int(parts[1])
                show_drop_simulation(all_groups, depth_val, throw=throw_val)
            except ValueError:
                print("  Expected: depth throw")
        else:
            # depth only -> browse
            show_groups_at_depth(all_groups, depth_val,
                                 cat_filter=cat_filter,
                                 min_weight=args.min_weight)

        sys.stdout.flush()


if __name__ == '__main__':
    main()
