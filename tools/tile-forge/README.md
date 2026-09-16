# Tile Forge

A local editor for composing **any existing PNG artwork** into floor, wall, and chasm autotiles. It reads the current Sil-More atlas and exports new files through the browser. Source images remain unchanged. The accompanying SDL changes connect arbitrary floor–floor, floor–wall, and wall–wall materials and add chasm borders to every known neighboring terrain tile, independently of its atlas coordinates.

## Open

Double-click **Launch Tile Forge.cmd**, or run from the repository root:

```powershell
node tools/tile-forge/server.cjs --open
```

Open **http://127.0.0.1:8787/**. Node.js 18+ is sufficient; no npm install or build is needed. If that port is occupied, use `--port=8788`. The server binds only to loopback and serves a fixed list of tool files plus the source atlas. Exports are also saved with unique filenames in `scripts/output/tile-forge/` (ignored by Git); the source atlas remains read-only. Stop it with Ctrl+C in its terminal.

Alternatively, open `index.html` directly and use **+ PNG** to load source artwork. The local server preloads the repository atlas and saves a local copy of each export. Image processing and project loading happen in the browser; there are no external services, fonts, or libraries. Without the server, exports use browser downloads only.

## Workflow

1. Choose a **starting point**: stone over earth, snow over stone, stone wall, or cavern ledge. Presets use coordinates verified against the current repository atlas.
2. Click a material card to pick its source cell. Coordinates are **column, row**, starting at zero. Use the numeric fields or arrow keys in the atlas. **+ PNG** imports additional source images; different materials may come from different images. **Derive face from upper tile** uses that tile's dark palette color (or shaded texture when palette locking is off); selecting a separate face tile overrides this choice.
3. Choose the transition type:
   - **Floor:** the upper material cuts into the lower one.
   - **Wall:** wall-top pixels sit above a vertical face, with ground behind it.
   - **Chasm:** the upper material is the walkable floor; the lower material is the void. A lit/dark rim and descending face establish the drop.
4. Tune the contour. **Cut** is appropriate for masonry; **organic** perturbs the boundary; **dither** selects source colors in a narrow band; **soft** interpolates colors only when palette locking is off. For 16px art, begin with an inset of 3–4px, roughness below 1px, and a 1px rim.
5. Paint the preview using Upper/Lower brushes. Right-drag also paints lower material. Undo reverses a stroke. The supplied cavern, islands, and rooms exercise holes, inner corners, narrow passages, and isolated tiles.
6. Inspect any of the **47 shapes**, or toggle neighbors around the enlarged tile. **Seam check** repeats the selected tile; use full fill (255) to inspect source repetition. Arbitrary edge tiles are not expected to tile against themselves.
7. For manual corrections, **Save this tile**, edit the PNG externally, then **Import drawn replacement**. The replacement must match the selected native tile size. It overrides that canonical mask in the current mode. **Restore generated tile** removes the override. Source/parameter changes retain overrides; changing tile size clears them. Replacements can create seams, so check them in context.
8. **Save project** embeds sources, parameters, the painted grid, and replacements in a JSON file. **Open project** restores it. Projects are not automatically saved; save before refreshing or closing the page.

The source grid supports 8, 16, 24, 32, 48, and 64px cells without resampling. Change tile size before selecting cells. Partial cells at the image's right/bottom edge are not selectable. Preview zoom uses integer scaling with image smoothing disabled.

## Mixed terrain palette

Select **Mixed terrain** to paint with up to 32 independently chosen materials. The initial palette includes original and imported floors/walls, plus chasm. **+ Material from PNG** chooses any cell from any loaded source. Select a brush, assign **Floor**, **Wall**, or **Chasm**, and paint; **Change brush tile** replaces its artwork everywhere in the scene. Right-click paints material zero. Reset rebuilds a junction test layout using the current palette; Undo also restores the previous layout.

Every different floor/wall pairing participates automatically. All solid materials beside chasm receive a rim, including diagonal corners. The compositor borrows actual neighbor pixels in a narrow 1–3px band, keeps tile centers intact, and handles multiple materials at a junction. Inset belongs to pair-atlas authoring; mixed contacts remain within three pixels even with larger width/depth settings. **Save mixed scene PNG** exports the composed map. Save project includes both scenes and the palette.

The mixed preview is an art workbench, not a pixel-identical reproduction of the SDL renderer. Runtime uses a restrained fixed-width blend; the editor offers adjustable contour, dither, and palette controls. Both use the same material-independent approach. The 47-shape gallery and atlas exports still describe the upper/lower pair selected in the sidebar; they do not encode every mixed neighborhood in one binary mask.

## Exports

**Export pack** downloads a ZIP containing:

| File | Content |
| --- | --- |
| `atlas-47.png` | Eight columns, six rows; 47 normalized masks in numeric order. Last cell is transparent padding. |
| `atlas-256.png` | 16 × 16 raw-mask slots. Equivalent neighborhoods intentionally duplicate the corresponding canonical tile. |
| `tiles/mask-NNN.png` | Each of the 47 tiles as an individual PNG. |
| `tiles.json` | Source coordinates, settings, bit order, mask-to-index lookup, pixel rectangles, and override flags. |
| `tileset.tsx` | Tiled atlas descriptor with mask properties. Terrain brush/Wang configuration is not supplied. |
| `preview.png` | Native-resolution painted pair scene, without editor grid lines. |
| `mixed-preview.png` / `mixed-scene.json` | Mixed scene pixels plus material references, grid, and settings. Source images are embedded only in Save project. |
| `README.txt` | Layout and import conventions. |

Exported images contain only native pixels. Preview zoom and editor grid lines do not affect them. Save the project separately if you need the embedded sources and editable setup.

### Sil-More integration

The current main atlas is **512 × 592px**, or **32 × 37 cells** at 16px. The verified preset cells (row:column in game templates) are:

| Material | Row | Column |
| --- | --- | --- |
| Flagstone | 33 | 0 |
| Masonry face | 33 | 2 (dark: 3) |
| Chasm fill | 34 | 0 |
| Snow | 35 | 0 |
| Dirt | 36 | 8 |
| Stone | 36 | 16 |

Additional fill variants exist in the game atlas at even columns, with darker versions immediately to their right. This first editor version chooses **one source tile per material**, with no random rotation or procedural redrawing. It does not automatically generate dark variants or animation frames.

At 16px the expanded sheet is 256 × 256px, matching the existing transition-page layout in `src/sdl/render/sdl-idle-animation.c`:

```text
N = 1, NE = 2, E = 4, SE = 8, S = 16, SW = 32, W = 64, NW = 128
source x = (raw_mask % 16) * 16
source y = floor(raw_mask / 16) * 16
```

A diagonal is relevant only when both neighboring cardinal bits are present. `tiles.json` includes both raw-to-canonical and raw-to-sheet-index lookup arrays.

**Chasm exports are floor-centered:** the center and set neighbor bits mean solid floor. They are not drop-in replacements for the game's chasm-centered static fill. Custom exported atlases still need explicit material mappings; adjusting the editor does not change runtime settings.

### Automatic runtime material connections

`src/sdl/render/sdl-material-edge.c` blends a narrow strip of the actual visible neighbor terrain texture into each material boundary. It covers arbitrary floor–floor, floor–wall, and wall–wall pairs, including diagonal and multi-material corners. Material families group style variants so random-looking fill choices within one floor style do not create internal outlines. Fallback atlas coordinates distinguish unstyled artwork. There is no Verdant whitelist or per-pair asset requirement.

The pass runs below objects/actors and before chasm rims, with no gameplay RNG, per-cell texture creation, or new asset loading. Known terrain alone can supply donor pixels. Hallucination skips material blending to avoid invoking random display selection repeatedly. Existing liquid/hazard/bridge treatments retain their dedicated passes. Terrain-source invalidation propagates artwork and visibility changes to adjacent cached cells, including removal of a boundary.

### Automatic runtime chasm borders

`src/sdl/render/sdl-chasm-edge.c` generates a small cached transparent overlay atlas at runtime. The shared map renderer composites it over the currently selected terrain, below objects and actors. It applies to **all known non-chasm cells bordering a known chasm**, including original floors, custom floors, walls, liquids, and bridges. There is no Verdant-only coordinate or style check. A bridge is solid for this adjacency test.

This runtime overlay uses **set bits for known chasm neighbors**, whereas the authoring atlas uses set bits for matching upper-material neighbors. Do not interchange the sheets directly. The runtime treatment is a restrained jagged lip/contact shade; the editor can author deeper, material-specific faces. The overlay preserves center pixels and follows existing remembered-terrain and rage/labyrinth visibility rules. Unknown chasms cannot affect visible edges. Neighbor invalidation and map-pan repainting prevent stale contours when discovery, terrain, or viewport position changes.

No terrain IDs, GUIDs, save formats, or game PNGs were changed. The renderer builds its overlay in memory and releases it on SDL teardown.

## Research and reference analysis

The local reference is `C:\Assets\verdant-forge-tileset-generator-generator-html`. Its strongest workflow ideas are complete neighborhood enumeration, adjustable edge profiles, custom PNG materials, preview maps, and metadata shipped alongside the art. Its custom-image route is based on 16px inputs and uses the first material variant to build transitions. Its interior walls distinguish a top and a vertical face.

Tile Forge is an independent implementation of general autotiling and raster-compositing techniques. No generator source, branding, or assets are bundled here. The reference's license permits personal source inspection/modification but disallows redistribution of its source. Source artwork, including the existing licensed tiles in this repository, retains its own license; exporting it does not grant asset-pack redistribution rights.

### Techniques informing this tool

1. **Blob autotiling:** Enumerating all 256 neighborhoods and suppressing irrelevant diagonal bits yields 47 shapes. This covers inner corners, outer corners, edges, corridors, and islands with a small stable lookup. Tiled documents the reduced Blob set and mixed edge/corner terrain matching: [Using Terrains](https://doc.mapeditor.org/en/stable/manual/terrain/).
2. **Wang tiles and shared-edge rules:** Adjacent tiles should agree about features on their shared boundary. This is the basis of Wang assembly and useful when expanding the tool to multiple compatible variants. The primary paper discusses texture mixing and non-periodic assembly: [Cohen et al., Wang Tiles for Image and Texture Generation](https://graphics.uni-konstanz.de/publikationen/Cohen2003WangTilesImage/index.html). The current editor uses 47-mask blobs, not a separate Wang variant solver.
3. **Narrow-band dithering:** Select between original source pixels around the boundary instead of blending their colors. This preserves pixel-art palettes. The current implementation uses a small ordered Bayer threshold; blue-noise masks are a possible extension for reducing visible patterns. NVIDIA discusses the spatial characteristics of blue noise: [Rendering in Real Time with Spatiotemporal Blue Noise Textures, Part 2](https://developer.nvidia.com/blog/rendering-in-real-time-with-spatiotemporal-blue-noise-textures-part-2/).
4. **Height and material layering:** A chasm needs value separation, a rim, and a descending face. Blending two colors alone does not describe a drop. Height-aware landscape layering is a useful larger-scale analogy; it is not the exact implementation here: [Unreal Engine Landscape Quick Start](https://dev.epicgames.com/documentation/unreal-engine/landscape-quick-start-guide-in-unreal-engine).
5. **Preserve pixel scale:** Integer zoom and nearest-neighbor filtering preserve authored pixel boundaries: [MDN imageSmoothingEnabled](https://developer.mozilla.org/en-US/docs/Web/API/CanvasRenderingContext2D/imageSmoothingEnabled), [Godot CanvasItem texture filtering](https://docs.godotengine.org/en/stable/classes/class_canvasitem.html).
6. **Keep mask metadata:** Engine terrain systems rely on peering information alongside atlas cells. A PNG alone is insufficient to reconstruct intended connections reliably: [Godot Using TileSets](https://docs.godotengine.org/en/stable/tutorials/2d/using_tilesets.html).

### Practical limits and next improvements

- A supplied base tile must already repeat well. Geometry continuity cannot remove a seam painted into the source texture.
- A single 47-mask atlas has periodic contour variation. Multiple seam-compatible variants and world-coordinate selection would reduce repeated contours and floor motifs.
- A binary 47-mask atlas authors one pair. Mixed mode and the runtime renderer compose several materials directly; a binary mask alone cannot describe every multi-material junction.
- Face depth is clipped to the cell. Tall cliffs need multi-cell wall pieces or a separate face layer. Do not increase depth and assume it creates extra space outside the tile.
- Manually painted corner replacements are often better than extreme roughness for detailed hand-drawn art.
- Automatic chasm borders are now integrated for all materials. A next improvement is optional artist-authored faces selected by floor style. Validate those at game zoom before replacing shipped art.

## Validation and source map

```powershell
node --test tools/tile-forge/*.test.cjs
.\build-cmake.bat standard
python scripts/check_chasm_edges.py
python scripts/check_material_edges.py
```

- `engine.js`: pure RGBA raster engine and mask normalization; usable from Node or a browser.
- `materials.js`: pure multi-material neighborhood compositor, independent of canvas.
- `app.js`: source picker, live preview, projects, replacements, and export metadata.
- `export.js`: PNG downloads and dependency-free ZIP packaging.
- `server.cjs`: optional loopback server; fixed read-only routes.
- `index.html` / `style.css`: interface.

The JavaScript checks cover mask completeness, pixel preservation, deterministic geometry, palette/alpha behavior, shared-edge continuity, and ZIP integrity. The production SDL harness checks all eight chasm directions, visibility, original/imported floor and wall samples, center preservation, opaque sprites, and incremental discovery/removal/pan redraw against full repaint. It also runs the existing idle-animation regressions and writes an eight-material comparison to `scripts/output/chasm-edges-check/materials.png`. Browser verification covered presets, numeric source picking, embedded project save/open with a drawn replacement, and an actual generated ZIP with all 256 slots checked against the compact sheet.
