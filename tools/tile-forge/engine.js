(function (root, factory) {
  if (typeof module === 'object' && module.exports) {
    module.exports = factory();
  } else {
    root.TileForge = factory();
  }
}(typeof globalThis !== 'undefined' ? globalThis : this, function () {
  'use strict';

  /*
   * The bit order is clockwise, starting at north.  A diagonal is only
   * meaningful when both of its cardinal neighbours are present; this is the
   * usual 47-shape autotile reduction and keeps a diagonal corner from making
   * a one-pixel spike.
   */
  var DIRECTIONS = Object.freeze([
    { bit: 1, dx: 0, dy: -1, name: 'N' },
    { bit: 2, dx: 1, dy: -1, name: 'NE' },
    { bit: 4, dx: 1, dy: 0, name: 'E' },
    { bit: 8, dx: 1, dy: 1, name: 'SE' },
    { bit: 16, dx: 0, dy: 1, name: 'S' },
    { bit: 32, dx: -1, dy: 1, name: 'SW' },
    { bit: 64, dx: -1, dy: 0, name: 'W' },
    { bit: 128, dx: -1, dy: -1, name: 'NW' }
  ]);

  var DIAGONAL_REQUIREMENTS = [
    [2, 1 | 4],
    [8, 4 | 16],
    [32, 16 | 64],
    [128, 64 | 1]
  ];

  function normalizeMask(value) {
    var mask = Number.isFinite(Number(value)) ? (Number(value) & 255) : 0;
    for (var i = 0; i < DIAGONAL_REQUIREMENTS.length; i += 1) {
      var diagonal = DIAGONAL_REQUIREMENTS[i][0];
      var sides = DIAGONAL_REQUIREMENTS[i][1];
      if ((mask & sides) !== sides) mask &= ~diagonal;
    }
    return mask;
  }

  var MASKS = Object.freeze(Array.from({ length: 256 }, function (_, value) {
    return normalizeMask(value);
  }).filter(function (value, index, values) {
    return values.indexOf(value) === index;
  }).sort(function (a, b) { return a - b; }));

  function rowAt(grid, y) {
    if (!grid || y < 0 || y >= grid.length) return undefined;
    return grid[y];
  }

  function valueAt(grid, x, y) {
    var row = rowAt(grid, y);
    return row == null || x < 0 || x >= row.length ? undefined : row[x];
  }

  /* Return the raw eight-neighbour mask.  Call normalizeMask() when a
   * canonical 47-shape is desired; renderTile normalizes its own input. */
  function maskAt(grid, x, y, predicate) {
    var center = valueAt(grid, x, y);
    if (center === undefined) return 0;
    var same = typeof predicate === 'function' ? predicate : function (a, b) {
      return a === b;
    };
    var mask = 0;
    for (var i = 0; i < DIRECTIONS.length; i += 1) {
      var direction = DIRECTIONS[i];
      var nx = x + direction.dx;
      var ny = y + direction.dy;
      /* At the edge of a map, extending the cell value keeps a solid map
       * border solid.  It also makes adjacent tile seams deterministic. */
      var neighbour = valueAt(grid, nx, ny);
      if (neighbour === undefined) neighbour = center;
      if (same(neighbour, center, nx, ny, x, y)) mask |= direction.bit;
    }
    return mask;
  }

  function clamp(value, low, high) {
    return value < low ? low : value > high ? high : value;
  }

  function intOr(value, fallback) {
    var number = Number(value);
    return Number.isFinite(number) ? number : fallback;
  }

  function hash32(value, seed) {
    var h = (value ^ seed) >>> 0;
    h = Math.imul(h ^ (h >>> 16), 0x7feb352d) >>> 0;
    h = Math.imul(h ^ (h >>> 15), 0x846ca68b) >>> 0;
    return (h ^ (h >>> 16)) >>> 0;
  }

  function smooth(value) {
    return value * value * (3 - 2 * value);
  }

  function modulo(value, period) {
    return ((value % period) + period) % period;
  }

  /* A one-dimensional periodic profile is used for both sides of an axis.
   * Consequently a south edge and the north edge of the tile beside it read
   * the same row profile, even when their masks differ.  No mask bits enter
   * this function. */
  function periodicProfile(position, size, seed, axis) {
    /* Pixel centres at the two ends of an edge are half a tile apart.  Using
     * size - 1 as the torus period makes those endpoint samples identical,
     * so a concave corner and a straight edge meet at a tile seam. */
    var period = Math.max(1, (size | 0) - 1);
    var p = Math.floor(position);
    var fraction = position - p;
    var a = hash32(modulo(p, period) + axis * 977, (seed | 0) ^ 0x51ed270b);
    var b = hash32(modulo(p + 1, period) + axis * 977, (seed | 0) ^ 0x51ed270b);
    var value = (a + (b - a) * smooth(fraction)) / 4294967295;
    return value * 2 - 1;
  }

  function bayer4(x, y) {
    var rows = [
      [0, 8, 2, 10],
      [12, 4, 14, 6],
      [3, 11, 1, 9],
      [15, 7, 13, 5]
    ];
    return (rows[modulo(y, 4)][modulo(x, 4)] + 0.5) / 16;
  }

  function sourcePixel(source, x, y) {
    var index = (y * source.width + x) * 4;
    return [
      source.data[index] || 0,
      source.data[index + 1] || 0,
      source.data[index + 2] || 0,
      source.data[index + 3] == null ? 255 : source.data[index + 3]
    ];
  }

  function solidSource(size, color) {
    var data = new Uint8ClampedArray(size * size * 4);
    for (var i = 0; i < size * size; i += 1) {
      data[i * 4] = color[0];
      data[i * 4 + 1] = color[1];
      data[i * 4 + 2] = color[2];
      data[i * 4 + 3] = color[3];
    }
    return { width: size, height: size, data: data };
  }

  function checkSource(source, size, name, fallback) {
    if (source == null) return solidSource(size, fallback);
    if (source.width !== size || source.height !== size ||
        !source.data || source.data.length < size * size * 4) {
      throw new RangeError(name + ' must be a size-square RGBA pixel source');
    }
    return source;
  }

  function writePixel(data, x, y, pixel) {
    var index = (y * Math.sqrt(data.length / 4) + x) * 4;
    data[index] = clamp(Math.round(pixel[0]), 0, 255);
    data[index + 1] = clamp(Math.round(pixel[1]), 0, 255);
    data[index + 2] = clamp(Math.round(pixel[2]), 0, 255);
    data[index + 3] = clamp(Math.round(pixel[3]), 0, 255);
  }

  function mixPixel(foreground, background, amount) {
    var foregroundAlpha = clamp(foreground[3] / 255, 0, 1) * amount;
    var backgroundAlpha = clamp(background[3] / 255, 0, 1) * (1 - amount);
    var alpha = foregroundAlpha + backgroundAlpha;
    if (alpha <= 0) return [0, 0, 0, 0];
    return [
      (foreground[0] * foregroundAlpha + background[0] * backgroundAlpha) / alpha,
      (foreground[1] * foregroundAlpha + background[1] * backgroundAlpha) / alpha,
      (foreground[2] * foregroundAlpha + background[2] * backgroundAlpha) / alpha,
      alpha * 255
    ];
  }

  function overPixel(bottom, top) {
    var alpha = clamp(top[3] / 255, 0, 1);
    var bottomAlpha = clamp(bottom[3] / 255, 0, 1);
    if (alpha <= 0) return bottom;
    if (alpha >= 1) return top;
    var inverse = 1 - alpha;
    var outAlpha = alpha + bottomAlpha * inverse;
    if (outAlpha <= 0) return [0, 0, 0, 0];
    return [
      (top[0] * alpha + bottom[0] * bottomAlpha * inverse) / outAlpha,
      (top[1] * alpha + bottom[1] * bottomAlpha * inverse) / outAlpha,
      (top[2] * alpha + bottom[2] * bottomAlpha * inverse) / outAlpha,
      outAlpha * 255
    ];
  }

  function shade(pixel, factor) {
    return [pixel[0] * factor, pixel[1] * factor, pixel[2] * factor, pixel[3]];
  }

  function luminance(pixel) {
    return pixel[0] * 0.2126 + pixel[1] * 0.7152 + pixel[2] * 0.0722;
  }

  function makePalette(source) {
    var colours = [];
    var seen = Object.create(null);
    var hasOpaque = false;
    for (var y = 0; y < source.height; y += 1) {
      for (var x = 0; x < source.width; x += 1) {
        var pixel = sourcePixel(source, x, y);
        if (pixel[3] > 0) hasOpaque = true;
        var key = pixel.join(',');
        if (!seen[key]) {
          seen[key] = true;
          colours.push(pixel);
        }
      }
    }
    if (hasOpaque) colours = colours.filter(function (pixel) { return pixel[3] > 0; });
    if (!colours.length) colours = [[0, 0, 0, 0]];
    var darkest = colours[0];
    var brightest = colours[0];
    for (var i = 1; i < colours.length; i += 1) {
      if (luminance(colours[i]) < luminance(darkest)) darkest = colours[i];
      if (luminance(colours[i]) > luminance(brightest)) brightest = colours[i];
    }
    return { colours: colours, darkest: darkest, brightest: brightest };
  }

  function paletteColour(palette, light) {
    if (!palette) return [0, 0, 0, 0];
    return (light ? palette.brightest : palette.darkest).slice();
  }

  function optionsFor(input) {
    input = input || {};
    var size = Math.max(1, Math.round(intOr(input.size, 16)));
    var inset = clamp(intOr(input.inset, 3), 0, Math.max(0, size / 2 - 0.5));
    var style = ['organic', 'cut', 'dither', 'soft'].indexOf(input.style) >= 0
      ? input.style : 'organic';
    var foreground = checkSource(input.foreground, size, 'foreground', [170, 170, 170, 255]);
    var background = checkSource(input.background, size, 'background', [35, 35, 35, 255]);
    var face = input.face == null ? null : checkSource(input.face, size, 'face', [70, 70, 70, 255]);
    return {
      size: size,
      mask: normalizeMask(input.mask),
      mode: input.mode === 'wall' || input.mode === 'chasm' ? input.mode : 'floor',
      style: style,
      inset: inset,
      roughness: Math.max(0, intOr(input.roughness, 1)),
      blend: Math.max(0, intOr(input.blend, 2)),
      rim: Math.max(0, intOr(input.rim, 1)),
      depth: Math.max(0, intOr(input.depth, 4)),
      seed: intOr(input.seed, 0) | 0,
      paletteLock: input.paletteLock !== false,
      foreground: foreground,
      background: background,
      face: face,
      foregroundPalette: makePalette(foreground),
      facePalette: face ? makePalette(face) : null
    };
  }

  function hasSide(mask, side) {
    return (mask & side) !== 0;
  }

  function edgeOffset(side, coordinate, options) {
    var jitter = 0;
    if (options.style !== 'cut') {
      var axis = (side === 1 || side === 16) ? 0 : 1;
      var strength = options.style === 'soft' ? 0.45 : 0.8;
      jitter = periodicProfile(coordinate + 0.5, options.size, options.seed, axis) *
        options.roughness * strength;
    }
    return options.inset + jitter;
  }

  function edgeThreshold(side, coordinate, options) {
    return hasSide(options.mask, side) ? -1000000 : edgeOffset(side, coordinate, options);
  }

  function shapeMetrics(x, y, options) {
    var size = options.size;
    var px = x + 0.5;
    var py = y + 0.5;
    var left = px < size / 2;
    var top = py < size / 2;
    var horizontal = left ? 64 : 4;
    var vertical = top ? 1 : 16;
    var diagonal = top
      ? (left ? 128 : 2)
      : (left ? 32 : 8);
    var u = left ? px : size - px;
    var v = top ? py : size - py;
    var h = hasSide(options.mask, horizontal);
    var vv = hasSide(options.mask, vertical);
    var corner = hasSide(options.mask, diagonal);
    var hThreshold = edgeThreshold(horizontal, py, options);
    var vThreshold = edgeThreshold(vertical, px, options);
    var score;
    var diagonalGap = 1000000;
    if (h && vv) {
      /* A concave corner has two seam endpoints.  Keep each endpoint on the
       * same straight-edge profile as its neighbour, then add a diagonal
       * interior cut.  The max() terms are essential: a lone u + v - inset
       * line misses both organic jitter and the half-pixel at a tile edge. */
      var horizontalEdge = edgeOffset(horizontal, py, options);
      var verticalEdge = edgeOffset(vertical, px, options);
      diagonalGap = corner ? 1000000 : Math.max(
        u - horizontalEdge,
        v - verticalEdge,
        u + v - (Math.max(horizontalEdge, verticalEdge) + 0.5)
      );
      score = diagonalGap;
    } else if (vv) {
      score = u - hThreshold;
    } else if (h) {
      score = v - vThreshold;
    } else {
      score = Math.min(u - hThreshold, v - vThreshold);
    }
    var west = hasSide(options.mask, 64) ? 1000000 : px - edgeThreshold(64, py, options);
    var east = hasSide(options.mask, 4) ? 1000000 : size - px - edgeThreshold(4, py, options);
    var north = hasSide(options.mask, 1) ? 1000000 : py - edgeThreshold(1, px, options);
    var south = hasSide(options.mask, 16) ? 1000000 : size - py - edgeThreshold(16, px, options);
    return {
      score: score,
      inside: score >= 0,
      west: west,
      east: east,
      north: north,
      south: south,
      diagonal: diagonalGap
    };
  }

  function coverage(metrics, x, y, options) {
    if (options.style !== 'soft' && options.style !== 'dither') {
      return metrics.inside ? 1 : 0;
    }
    if (options.blend <= 0) return metrics.inside ? 1 : 0;
    var radius = Math.max(0.5, options.blend);
    var value = clamp(0.5 + metrics.score / (2 * radius), 0, 1);
    var torus = Math.max(1, options.size - 1);
    var threshold = bayer4(modulo(x, torus), modulo(y, torus));
    if (options.style === 'dither') {
      return value > threshold ? 1 : 0;
    }
    if (options.paletteLock) {
      /* Ordered coverage keeps soft edges crisp while still giving the
       * palette-locked result a visibly softer contour. */
      return value > threshold ? 1 : 0;
    }
    return value;
  }

  function rimSide(metrics, x, y, options, width) {
    var candidates = [];
    if (metrics.north >= 0 && metrics.north < width) candidates.push({ side: 'N', distance: metrics.north });
    if (metrics.west >= 0 && metrics.west < width) candidates.push({ side: 'W', distance: metrics.west });
    if (metrics.east >= 0 && metrics.east < width) candidates.push({ side: 'E', distance: metrics.east });
    if (metrics.south >= 0 && metrics.south < width) candidates.push({ side: 'S', distance: metrics.south });
    if (metrics.diagonal >= 0 && metrics.diagonal < width) {
      var top = y < options.size / 2;
      var left = x < options.size / 2;
      candidates.push({
        side: top ? 'N' : (left ? 'W' : 'S'),
        distance: metrics.diagonal
      });
    }
    candidates.sort(function (a, b) { return a.distance - b.distance; });
    return candidates.length ? candidates[0].side : null;
  }

  function rimPixel(side, x, y, options) {
    var light = side === 'N' || side === 'W';
    if (options.paletteLock) return paletteColour(options.foregroundPalette, light);
    var pixel = sourcePixel(options.foreground, x, y);
    return shade(pixel, light ? 1.12 : 0.58);
  }

  function facePixel(x, y, options) {
    if (options.face) {
      if (options.paletteLock) {
        /* Keep the face texture in its own supplied palette.  Transparent
         * texels fall back to the darkest opaque face colour. */
        var sampled = sourcePixel(options.face, x, y);
        if (sampled[3] > 0) return sampled;
        return paletteColour(options.facePalette, false);
      }
      return sourcePixel(options.face, x, y);
    }
    if (options.paletteLock) return paletteColour(options.foregroundPalette, false);
    return shade(sourcePixel(options.foreground, x, y), options.mode === 'chasm' ? 0.38 : 0.52);
  }

  function sourceOrShade(source, x, y, factor, fallback) {
    return source ? sourcePixel(source, x, y) : shade(fallback, factor);
  }

  function renderTile(input) {
    var options = optionsFor(input);
    var size = options.size;
    var out = new Uint8ClampedArray(size * size * 4);
    var normalized = options.mask;

    /* Returning a copy is intentional: callers are free to mutate their
     * result, while a full material tile remains byte-identical to its source. */
    if (normalized === 255 && (options.mode === 'floor' || options.mode === 'chasm')) {
      var sourceData = options.foreground.data;
      out.set(sourceData.subarray
        ? sourceData.subarray(0, size * size * 4)
        : Array.prototype.slice.call(sourceData, 0, size * size * 4));
      return { width: size, height: size, data: out };
    }

    var geometry = new Array(size * size);
    for (var y = 0; y < size; y += 1) {
      for (var x = 0; x < size; x += 1) {
        var metrics = shapeMetrics(x, y, options);
        geometry[y * size + x] = metrics;
        var amount = coverage(metrics, x, y, options);
        var fg = sourcePixel(options.foreground, x, y);
        var bg = sourcePixel(options.background, x, y);
        var pixel;
        if (amount >= 1) pixel = fg;
        else if (amount <= 0) pixel = bg;
        else pixel = mixPixel(fg, bg, amount);
        writePixel(out, x, y, pixel);
      }
    }

    var rimWidth = Math.max(0, options.rim);
    var faceDepth = Math.max(0, options.depth);
    var paintRim = options.mode === 'chasm' || options.mode === 'wall';
    for (var yy = 0; yy < size; yy += 1) {
      for (var xx = 0; xx < size; xx += 1) {
        var item = geometry[yy * size + xx];
        if (!paintRim || rimWidth <= 0 || !item.inside) continue;
        var side = rimSide(item, xx, yy, options, rimWidth);
        if (side) {
          var base = sourcePixel({ width: size, height: size, data: out }, xx, yy);
          writePixel(out, xx, yy, overPixel(base, rimPixel(side, xx, yy, options)));
        }
      }
    }

    if ((options.mode === 'chasm' || options.mode === 'wall') &&
        faceDepth > 0) {
      for (var sy = 0; sy < size; sy += 1) {
        for (var sx = 0; sx < size; sx += 1) {
          var southMetrics = geometry[sy * size + sx];
          var southCut = !hasSide(normalized, 16)
            ? southMetrics.south
            : (sy >= size / 2 && southMetrics.diagonal < 1000000
              ? southMetrics.diagonal : 1000000);
          if (southCut >= 0 || southCut < -faceDepth ||
              southMetrics.west < -0.5 || southMetrics.east < -0.5) continue;
          var under = sourcePixel({ width: size, height: size, data: out }, sx, sy);
          writePixel(out, sx, sy, overPixel(under, facePixel(sx, sy, options)));
        }
      }
    }

    return { width: size, height: size, data: out };
  }

  return {
    DIRECTIONS: DIRECTIONS,
    MASKS: MASKS,
    normalizeMask: normalizeMask,
    maskAt: maskAt,
    renderTile: renderTile
  };
}));
