#!/usr/bin/env python3
"""
Generate an HTML reference gallery for the MicroChasm 16×16 tileset.

This script parses the `graf-new.prf` file that accompanies the tileset to
extract sprite → name mappings, converts their attr/char codes to (row, col)
coordinates, an            parts.append("</div>")
        
        # Show tiles with multiple sprites
        multi_use_tiles = [(coord, sprites) for coord, sprites in unused_analysis.get('tile_details', {}).items() if len(sprites) > 1]
        if multi_use_tiles:
            multi_use_tiles.sort(key=lambda x: len(x[1]), reverse=True)  # Sort by usage count
            parts.extend([
                "<h2>Most Heavily Used Tiles</h2>",
                "<div style='font-family:monospace; font-size:12px; line-height:1.4;'>",
            ])
            
            for coord, sprites in multi_use_tiles[:10]:  # Show top 10
                sprite_list = ', '.join(sprites[:3])
                if len(sprites) > 3:
                    sprite_list += f' + {len(sprites) - 3} more'
                parts.append(f"Tile {coord}: {len(sprites)} sprites - {sprite_list}<br>")
            
            parts.append("</div>")
        
        # List unused tilesrites an `index.html` (or a file you specify) that shows
each tile next to its label.

Usage examples
--------------
$ python generate_tileset.py                 # uses defaults → index.html
$ python generate_tileset.py -o micro.html   # custom output name
$ python generate_tileset.py -i ./tiles.png  # custom PNG path

Place it in the same folder as **16x16_microchasm.png** and **graf-new.prf**
or adjust the `-i/--image` and `-p/--prf` options.
"""
import argparse
import html
import re
from pathlib import Path

# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------
SPRITE_SIZE = 16  # px
LOW6_MASK   = 0x3F

# More flexible regex patterns to handle various prf file formats
RE_MAPPING_S_HEX = re.compile(r"^S:0x([0-9A-Fa-f]+):0x([0-9A-Fa-f]+)/0x([0-9A-Fa-f]+)")  # S:0x00:0x8C/0x80
RE_MAPPING_ALL_DEC = re.compile(r"^([a-zA-Z]):([0-9]+):0x([0-9A-Fa-f]+)/0x([0-9A-Fa-f]+)")  # Any letter:num:0x80/0x81
RE_INCLUDE = re.compile(r"^%:(.+)\.prf")  # %:filename.prf


def parse_prf(path: Path, processed_files=None):
    """Parse graf-new.prf → list of dicts with keys 'name', 'x', 'y'."""
    if processed_files is None:
        processed_files = set()
    
    # Avoid circular includes
    if path.absolute() in processed_files:
        return []
    processed_files.add(path.absolute())
    
    items = []
    current_name: str | None = None
    line_num = 0

    # Try different encodings
    content = None
    for encoding in ['utf-8', 'latin-1', 'cp1252']:
        try:
            with path.open(encoding=encoding) as fh:
                content = fh.read()
            break
        except (UnicodeDecodeError, UnicodeError):
            continue
    
    if content is None:
        raise ValueError(f"Could not decode file {path} with any supported encoding")

    for raw in content.splitlines():
        line_num += 1
        line = raw.strip()
        if not line:
            continue
            
        # Handle include directives
        include_match = RE_INCLUDE.match(line)
        if include_match:
            include_filename = include_match.group(1) + ".prf"
            # Try to find the included file in the same directory first, then in pref directory
            include_paths = [
                path.parent / include_filename,
                path.parent.parent.parent / "lib" / "pref" / include_filename,  # Relative to project root
                Path("c:/Users/efrem/Documents/GitHub/sil-qh/lib/pref") / include_filename,  # Absolute fallback
            ]
            
            for include_path in include_paths:
                try:
                    if include_path.exists():
                        print(f"Including file: {include_path}")
                        included_items = parse_prf(include_path, processed_files)
                        items.extend(included_items)
                        break
                except (IndexError, OSError):
                    continue
            else:
                print(f"Warning: Could not find include file '{include_filename}' for line {line_num}")
                print(f"  Tried paths: {[str(p) for p in include_paths if p is not None]}")
            continue
            
        # Capture comment blocks as the current sprite name
        if line.startswith("#"):
            comment = line.lstrip("# ").strip()
            if comment:
                current_name = comment
            continue
            
        # Skip conditional lines (they're complex and would need game state to evaluate)
        if line.startswith("?:"):
            continue
            
        # Try different mapping patterns
        match = None
        for pattern in [RE_MAPPING_S_HEX, RE_MAPPING_ALL_DEC]:
            match = pattern.match(line)
            if match:
                break
                
        if not match:
            # Debug: print unmatched lines that look like they should be mappings
            if re.match(r"^[a-zA-Z]:[0-9]", line):
                print(f"Warning: Line {line_num} not parsed: {line}")
            continue
            
        try:
            groups = match.groups()
            
            if RE_MAPPING_S_HEX.match(line):
                # S format: S:0x00:0x8C/0x80 (all hex values)
                index_hex, attr_hex, char_hex = groups
                attr = int(attr_hex, 16)
                char = int(char_hex, 16)
            elif RE_MAPPING_ALL_DEC.match(line):
                # F/K format: F:1:0x80/0x81 (decimal index, hex attr/char)
                prefix, index_dec, attr_hex, char_hex = groups
                attr = int(attr_hex, 16)
                char = int(char_hex, 16)
            else:
                # Fallback - shouldn't reach here
                print(f"Warning: Matched but couldn't parse line {line_num}: {line}")
                continue
                
            row = attr & LOW6_MASK
            col = char & LOW6_MASK
            
            items.append({
                "name": current_name or f"(unnamed line {line_num})",
                "x": col * SPRITE_SIZE,
                "y": row * SPRITE_SIZE,
                "line": line_num,  # For debugging
                "attr": attr,  # Store original attr for debugging
                "char": char,  # Store original char for debugging
                "file": str(path),  # Track which file this came from
            })
        except (ValueError, IndexError) as e:
            print(f"Warning: Failed to parse line {line_num}: {line} - {e}")
            
    return items


def analyze_tile_usage(items: list[dict], tileset_width: int = 32, tileset_height: int = 16):
    """Analyze which tiles are used and unused in the tileset."""
    used_tiles = set()
    tile_details = {}  # Track what sprites use each tile
    
    # Collect all used tile coordinates and track what uses them
    for item in items:
        col = item['x'] // SPRITE_SIZE
        row = item['y'] // SPRITE_SIZE
        coord = (row, col)
        used_tiles.add(coord)
        
        if coord not in tile_details:
            tile_details[coord] = []
        tile_details[coord].append(item['name'])
    
    # Generate all possible tile coordinates
    all_tiles = set()
    for row in range(tileset_height):
        for col in range(tileset_width):
            all_tiles.add((row, col))
    
    unused_tiles = all_tiles - used_tiles
    
    return {
        'used_tiles': sorted(used_tiles),
        'unused_tiles': sorted(unused_tiles),
        'used_count': len(used_tiles),
        'unused_count': len(unused_tiles),
        'total_tiles': tileset_width * tileset_height,
        'usage_percentage': (len(used_tiles) / len(all_tiles)) * 100,
        'tileset_width': tileset_width,
        'tileset_height': tileset_height,
        'tile_details': tile_details  # Include details of what uses each tile
    }


def render_html(items: list[dict], image_path: str, unused_analysis=None):
    """Return a full HTML document as a string."""
    css = f"""
    body {{ background:#202020; color:#e0e0e0; font-family: sans-serif; padding:16px; }}
    h1   {{ margin-top:0; }}
    h2   {{ color:#00ff00; margin-top:24px; }}
    .list {{ display:flex; flex-wrap:wrap; gap:6px; }}
    .item {{ display:flex; align-items:center; background:#2d2d2d; border-radius:4px; padding:4px 6px; font-size:13px; }}
    .sprite {{ width:{SPRITE_SIZE}px; height:{SPRITE_SIZE}px; background-image:url('{html.escape(image_path)}'); background-repeat:no-repeat; image-rendering:pixelated; margin-right:6px; }}
    .unused-grid {{ display:grid; grid-template-columns:repeat(32, {SPRITE_SIZE}px); gap:1px; margin:16px 0; }}
    .unused-tile {{ width:{SPRITE_SIZE}px; height:{SPRITE_SIZE}px; background-image:url('{html.escape(image_path)}'); background-repeat:no-repeat; image-rendering:pixelated; border:2px solid #ff4444; }}
    .used-tile {{ width:{SPRITE_SIZE}px; height:{SPRITE_SIZE}px; background-image:url('{html.escape(image_path)}'); background-repeat:no-repeat; image-rendering:pixelated; border:1px solid #00ff00; opacity:0.7; }}
    .overlap-tile {{ width:{SPRITE_SIZE}px; height:{SPRITE_SIZE}px; background-image:url('{html.escape(image_path)}'); background-repeat:no-repeat; image-rendering:pixelated; border:2px solid #ffff00; opacity:0.9; }}
    .stats {{ background:#333; padding:12px; border-radius:4px; margin:16px 0; }}
    """

    parts = [
        "<!DOCTYPE html>",
        "<html><head><meta charset='utf-8'><title>MicroChasm Tileset</title>",
        f"<style>{css}</style></head><body>",
        "<h1>MicroChasm 16×16 Tileset Reference</h1>",
    ]

    # Add usage statistics if available
    if unused_analysis:
        parts.extend([
            "<div class='stats'>",
            f"<strong>Tileset Usage Statistics:</strong><br>",
            f"Used tiles: {unused_analysis['used_count']} / {unused_analysis['total_tiles']} ({unused_analysis['usage_percentage']:.1f}%)<br>",
            f"Unused tiles: {unused_analysis['unused_count']} ({100 - unused_analysis['usage_percentage']:.1f}%)",
            "</div>",
        ])

    parts.extend([
        "<h2>Used Tiles</h2>",
        "<div class='list'>",
    ])

    for itm in items:
        pos = f"background-position:-{itm['x']}px -{itm['y']}px;"
        label = html.escape(itm["name"])
        parts.append(f"<div class='item'><span class='sprite' style='{pos}'></span>{label}</div>")

    parts.append("</div>")

    # Add unused tiles visualization if available
    if unused_analysis and unused_analysis['unused_tiles']:
        parts.extend([
            "<h2>Tileset Overview (Red = Unused, Green = Used, Yellow = Multiple Sprites)</h2>",
            "<div class='unused-grid'>",
        ])
        
        # Create a grid showing all tiles
        for row in range(unused_analysis.get('tileset_height', 16)):  # Use actual height
            for col in range(unused_analysis.get('tileset_width', 32)):  # Use actual width
                x = col * SPRITE_SIZE
                y = row * SPRITE_SIZE
                pos = f"background-position:-{x}px -{y}px;"
                
                if (row, col) in unused_analysis['unused_tiles']:
                    parts.append(f"<div class='unused-tile' style='{pos}' title='Unused tile ({row}, {col})'></div>")
                else:
                    # Show what sprites use this tile
                    tile_users = unused_analysis.get('tile_details', {}).get((row, col), ['Used'])
                    user_list = ', '.join(tile_users[:3])  # Show first 3 users
                    if len(tile_users) > 3:
                        user_list += f' + {len(tile_users) - 3} more'
                    
                    # Use different style for tiles with multiple sprites
                    if len(tile_users) > 1:
                        parts.append(f"<div class='overlap-tile' style='{pos}' title='Multi-use tile ({row}, {col}): {user_list}'></div>")
                    else:
                        parts.append(f"<div class='used-tile' style='{pos}' title='Used tile ({row}, {col}): {user_list}'></div>")
        
        parts.append("</div>")
        
        # List unused tiles
        if unused_analysis['unused_tiles']:
            parts.extend([
                "<h2>Unused Tiles (Row, Col)</h2>",
                "<div style='font-family:monospace; font-size:12px; line-height:1.4;'>",
            ])
            
            # Group unused tiles by rows for better readability
            unused_by_row = {}
            for row, col in unused_analysis['unused_tiles']:
                if row not in unused_by_row:
                    unused_by_row[row] = []
                unused_by_row[row].append(col)
            
            for row in sorted(unused_by_row.keys()):
                cols = sorted(unused_by_row[row])
                col_ranges = []
                start = cols[0]
                end = cols[0]
                
                for i in range(1, len(cols)):
                    if cols[i] == end + 1:
                        end = cols[i]
                    else:
                        if start == end:
                            col_ranges.append(f"{start}")
                        else:
                            col_ranges.append(f"{start}-{end}")
                        start = end = cols[i]
                
                if start == end:
                    col_ranges.append(f"{start}")
                else:
                    col_ranges.append(f"{start}-{end}")
                
                parts.append(f"Row {row:2d}: cols {', '.join(col_ranges)}<br>")
            
            parts.append("</div>")

    parts.extend(["</body></html>"])
    return "\n".join(parts)


# ---------------------------------------------------------------------------
# CLI
# ---------------------------------------------------------------------------

def main():
    parser = argparse.ArgumentParser(description="Generate MicroChasm tileset HTML index.")
    parser.add_argument("-p", "--prf",  metavar="FILE", default="graf-new.prf", help="Path to graf-new.prf (default: %(default)s)")
    parser.add_argument("-i", "--image", metavar="PNG",  default="16x16_microchasm.png", help="Path to sprite PNG (default: %(default)s)")
    parser.add_argument("-o", "--out",   metavar="HTML", default="index.html", help="Output HTML file (default: %(default)s)")
    parser.add_argument("-d", "--debug", action="store_true", help="Enable debug output to show parsing details")
    parser.add_argument("-u", "--show-unused", action="store_true", help="Show unused tiles analysis and visualization")
    parser.add_argument("--unused-only", action="store_true", help="Only show unused tiles statistics, don't generate HTML")
    parser.add_argument("--check-coord", type=str, help="Check what sprites use a specific coordinate (format: 'row,col')")
    parser.add_argument("--show-overlaps", action="store_true", help="Show tiles that are used by multiple sprites")
    parser.add_argument("--tileset-width", type=int, default=32, help="Tileset width in tiles (default: 32)")
    parser.add_argument("--tileset-height", type=int, default=16, help="Tileset height in tiles (default: 16)")
    args = parser.parse_args()

    items = parse_prf(Path(args.prf))
    if not items:
        raise SystemExit("[error] No entries parsed – check that the .prf file is correct.")

    # Check specific coordinate if requested
    if args.check_coord:
        try:
            row, col = map(int, args.check_coord.split(','))
            coord_items = [item for item in items if (item['y'] // SPRITE_SIZE, item['x'] // SPRITE_SIZE) == (row, col)]
            print(f"Coordinate ({row}, {col}) is used by {len(coord_items)} sprites:")
            for item in coord_items:
                print(f"  - {item['name']} (attr=0x{item['attr']:02x}, char=0x{item['char']:02x}) from {item['file']}")
            return
        except ValueError:
            print("Error: --check-coord format should be 'row,col' (e.g., '10,18')")
            return

    # Show overlapping tiles if requested
    if args.show_overlaps:
        from collections import defaultdict
        coord_groups = defaultdict(list)
        for item in items:
            coord = (item['y'] // SPRITE_SIZE, item['x'] // SPRITE_SIZE)
            coord_groups[coord].append(item['name'])
        
        overlaps = [(coord, names) for coord, names in coord_groups.items() if len(names) > 1]
        overlaps.sort(key=lambda x: len(x[1]), reverse=True)  # Sort by number of overlaps
        
        print(f"Found {len(overlaps)} coordinates with multiple sprites:")
        for coord, names in overlaps[:20]:  # Show top 20
            print(f"  {coord}: {len(names)} sprites - {', '.join(names[:3])}{'...' if len(names) > 3 else ''}")
        return

    # Analyze tile usage
    unused_analysis = None
    if args.show_unused or args.unused_only:
        unused_analysis = analyze_tile_usage(items, args.tileset_width, args.tileset_height)
        print(f"Tile Usage Analysis:")
        print(f"  Used: {unused_analysis['used_count']} / {unused_analysis['total_tiles']} ({unused_analysis['usage_percentage']:.1f}%)")
        print(f"  Unused: {unused_analysis['unused_count']} ({100 - unused_analysis['usage_percentage']:.1f}%)")
        
        if args.debug and unused_analysis['unused_tiles']:
            print(f"  First 10 unused tiles (row, col): {unused_analysis['unused_tiles'][:10]}")
        
        if args.unused_only:
            # Print detailed unused tiles list
            print(f"\nDetailed unused tiles by row:")
            unused_by_row = {}
            for row, col in unused_analysis['unused_tiles']:
                if row not in unused_by_row:
                    unused_by_row[row] = []
                unused_by_row[row].append(col)
            
            for row in sorted(unused_by_row.keys()):
                cols = sorted(unused_by_row[row])
                col_ranges = []
                start = cols[0]
                end = cols[0]
                
                for i in range(1, len(cols)):
                    if cols[i] == end + 1:
                        end = cols[i]
                    else:
                        if start == end:
                            col_ranges.append(f"{start}")
                        else:
                            col_ranges.append(f"{start}-{end}")
                        start = end = cols[i]
                
                if start == end:
                    col_ranges.append(f"{start}")
                else:
                    col_ranges.append(f"{start}-{end}")
                
                print(f"  Row {row:2d}: cols {', '.join(col_ranges)}")
            return  # Exit early, don't generate HTML

    if args.debug:
        print(f"Debug: Parsed {len(items)} items")
        for i, item in enumerate(items[:10]):  # Show first 10 items
            print(f"  {i+1}: {item['name']} -> ({item['x']//16}, {item['y']//16}) [attr=0x{item['attr']:02x}, char=0x{item['char']:02x}]")
        if len(items) > 10:
            print(f"  ... and {len(items) - 10} more items")

    html_doc = render_html(items, args.image, unused_analysis if args.show_unused else None)
    Path(args.out).write_text(html_doc, encoding="utf-8")
    print(f"Generated {args.out} with {len(items)} sprite entries.")


if __name__ == "__main__":
    main()