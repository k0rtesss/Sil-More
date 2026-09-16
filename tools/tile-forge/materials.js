(function (root, factory) {
  if (typeof module === 'object' && module.exports) {
    module.exports = factory();
  } else {
    root.ForgeMaterials = factory();
  }
}(typeof globalThis !== 'undefined' ? globalThis : this, function () {
  'use strict';

  /*
   * This renderer deliberately does not depend on a canvas or on TileForge.
   * TileForge is useful for making one binary tile, while this module has to
   * compose an arbitrary material at every cell of a map.  Keeping the two
   * paths separate also means imported source pixels remain available in
   * browser workers and in the command-line tests.
   */

  var DIRECTIONS = [
    { dx: 0, dy: -1, diagonal: false },
    { dx: 1, dy: 0, diagonal: false },
    { dx: 0, dy: 1, diagonal: false },
    { dx: -1, dy: 0, diagonal: false },
    { dx: 1, dy: -1, diagonal: true },
    { dx: 1, dy: 1, diagonal: true },
    { dx: -1, dy: 1, diagonal: true },
    { dx: -1, dy: -1, diagonal: true }
  ];

  var KINDS = { floor: true, wall: true, chasm: true };

  function clamp(value, low, high) {
    return value < low ? low : value > high ? high : value;
  }

  function finiteNumber(value, fallback) {
    var number = Number(value);
    return Number.isFinite(number) ? number : fallback;
  }

  function integer(value, fallback) {
    return Math.round(finiteNumber(value, fallback));
  }

  function positiveInteger(value, fallback) {
    return Math.max(1, integer(value, fallback));
  }

  function rowAt(grid, y) {
    if (!grid || y < 0 || y >= grid.length) return undefined;
    return grid[y];
  }

  function cellAt(grid, x, y) {
    var row = rowAt(grid, y);
    return row == null || x < 0 || x >= row.length ? undefined : row[x];
  }

  function hash32(value, seed) {
    var h = (value ^ seed) >>> 0;
    h = Math.imul(h ^ (h >>> 16), 0x7feb352d) >>> 0;
    h = Math.imul(h ^ (h >>> 15), 0x846ca68b) >>> 0;
    return (h ^ (h >>> 16)) >>> 0;
  }

  function hashMany(seed) {
    var h = (integer(seed, 0) | 0) >>> 0;
    for (var i = 1; i < arguments.length; i += 1) {
      var value = integer(arguments[i], 0) | 0;
      h = hash32((h ^ value) >>> 0, (0x9e3779b9 + i * 0x85ebca6b) | 0);
    }
    return h >>> 0;
  }

  function hashUnit(value) {
    return (value >>> 0) / 4294967295;
  }

  function smooth(value) {
    return value * value * (3 - 2 * value);
  }

  function modulo(value, period) {
    return ((value % period) + period) % period;
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

  function pixelFromData(source, x, y) {
    var index = (y * source.width + x) * 4;
    var data = source.data;
    return [
      data[index] == null ? 0 : data[index],
      data[index + 1] == null ? 0 : data[index + 1],
      data[index + 2] == null ? 0 : data[index + 2],
      data[index + 3] == null ? 255 : data[index + 3]
    ];
  }

  function sourcePixel(source, x, y, size) {
    /* Nearest-neighbour scaling lets a map choose one tile size while still
     * accepting imported PNGs with a different (but valid) pixel size. */
    var sx = source.width === size
      ? x
      : Math.min(source.width - 1, Math.floor(x * source.width / size));
    var sy = source.height === size
      ? y
      : Math.min(source.height - 1, Math.floor(y * source.height / size));
    return pixelFromData(source, sx, sy);
  }

  function copyPixel(pixel) {
    return [pixel[0], pixel[1], pixel[2], pixel[3]];
  }

  function validateSource(source, index) {
    if (!source || typeof source !== 'object') {
      throw new TypeError('materials[' + index + '].pixels is required');
    }
    var width = positiveInteger(source.width, 0);
    var height = positiveInteger(source.height, 0);
    if (!source.width || !source.height || width !== Number(source.width) ||
        height !== Number(source.height)) {
      throw new RangeError('materials[' + index + '].pixels needs positive dimensions');
    }
    if (!source.data || source.data.length < width * height * 4) {
      throw new RangeError('materials[' + index + '].pixels needs RGBA data');
    }
    return { width: width, height: height, data: source.data };
  }

  function normalizeMaterials(materials) {
    if (!materials || typeof materials.length !== 'number') {
      throw new TypeError('materials must be an array');
    }
    return Array.prototype.map.call(materials, function (material, index) {
      if (!material || typeof material !== 'object') {
        throw new TypeError('materials[' + index + '] must be an object');
      }
      var kind = material.kind == null ? 'floor' : String(material.kind).toLowerCase();
      if (!KINDS[kind]) {
        throw new RangeError('materials[' + index + '].kind must be floor, wall, or chasm');
      }
      return { kind: kind, pixels: validateSource(material.pixels, index) };
    });
  }

  function normalizeGrid(grid, materialCount) {
    if (!grid || typeof grid.length !== 'number') {
      throw new TypeError('grid must be an array of rows');
    }
    var height = grid.length;
    var width = 0;
    for (var y = 0; y < height; y += 1) {
      var row = grid[y];
      if (!row || typeof row.length !== 'number') {
        throw new TypeError('grid rows must be arrays');
      }
      width = Math.max(width, row.length);
      for (var x = 0; x < row.length; x += 1) {
        var value = Number(row[x]);
        if (!Number.isInteger(value) || value < 0 || value >= materialCount) {
          throw new RangeError('grid[' + y + '][' + x + '] is not a material index');
        }
      }
    }
    return { grid: grid, width: width, height: height };
  }

  function normalizeOptions(input, firstSource) {
    var suppliedSize = input.size == null ? firstSource.width : input.size;
    var size = positiveInteger(suppliedSize, 16);
    var style = ['organic', 'cut', 'dither', 'soft'].indexOf(input.style) >= 0
      ? input.style : 'organic';
    /* `inset` controls the silhouette of one TileForge binary tile.  A mixed
     * map has pairwise boundaries instead, so its intentionally bounded
     * three-pixel neighbourhood is controlled by blend/rim/depth. */
    var blend = clamp(integer(input.blend, 2), 1, 3);
    var roughness = clamp(finiteNumber(input.roughness, 1), 0, 3);
    var rim = clamp(integer(input.rim, 1), 1, 3);
    var depth = clamp(integer(input.depth, 2), 0, 3);
    return {
      size: size,
      style: style,
      roughness: roughness,
      blend: blend,
      rim: rim,
      depth: depth,
      seed: integer(input.seed, 0) | 0,
      paletteLock: input.paletteLock !== false
    };
  }

  function directionDistance(direction, x, y, size) {
    var horizontal = direction.dx < 0 ? x + 0.5 :
      direction.dx > 0 ? size - x - 0.5 : 1000000;
    var vertical = direction.dy < 0 ? y + 0.5 :
      direction.dy > 0 ? size - y - 0.5 : 1000000;
    if (direction.diagonal) return Math.max(horizontal, vertical);
    return Math.min(horizontal, vertical);
  }

  function neighbourPixel(source, direction, x, y, size) {
    var nx = direction.dx === 0 ? x : size - 1 - x;
    var ny = direction.dy === 0 ? y : size - 1 - y;
    return sourcePixel(source, nx, ny, size);
  }

  function pairKey(a, b) {
    return a < b ? [a, b] : [b, a];
  }

  function boundaryNoise(options, centreId, neighbourId, direction,
                         cellX, cellY, x, y, size) {
    var pair = pairKey(centreId, neighbourId);
    var gx = cellX * size + x;
    var gy = cellY * size + y;
    var orientation;
    var seam;
    var tangent;
    if (!direction.diagonal && direction.dx !== 0) {
      orientation = 0;
      seam = direction.dx > 0 ? (cellX + 1) * size : cellX * size;
      tangent = gy;
    } else if (!direction.diagonal && direction.dy !== 0) {
      orientation = 1;
      seam = direction.dy > 0 ? (cellY + 1) * size : cellY * size;
      tangent = gx;
    } else {
      orientation = 2;
      seam = (direction.dx > 0 ? cellX + 1 : cellX) * size;
      tangent = (direction.dy > 0 ? cellY + 1 : cellY) * size;
    }

    var key = hashMany(options.seed, pair[0], pair[1], orientation, seam, tangent);
    if (direction.diagonal) return hashUnit(key);

    /* Interpolate neighbouring deterministic samples.  This makes organic
     * profiles irregular without giving the two sides of a seam unrelated
     * random thresholds. */
    var p = Math.floor(tangent);
    var fraction = tangent - p;
    var a = hashUnit(hashMany(options.seed, pair[0], pair[1], orientation, seam, p));
    var b = hashUnit(hashMany(options.seed, pair[0], pair[1], orientation, seam, p + 1));
    return a + (b - a) * smooth(fraction);
  }

  function blendAmount(distance, direction, options, centreId, neighbourId,
                       cellX, cellY, x, y) {
    var width = options.blend;
    if (options.style === 'organic') {
      var noise = boundaryNoise(options, centreId, neighbourId, direction,
        cellX, cellY, x, y, options.size) * 2 - 1;
      width = clamp(width + noise * options.roughness * 0.55, 1, 3);
    } else if (options.style === 'soft') {
      var softNoise = boundaryNoise(options, centreId, neighbourId, direction,
        cellX, cellY, x, y, options.size) * 2 - 1;
      width = clamp(width + softNoise * options.roughness * 0.2, 1, 3);
    }
    if (options.style === 'cut') return distance <= width + 0.001 ? 1 : 0;

    var amount = clamp((width + 0.5 - distance) / width, 0, 1);
    if (options.style === 'soft') amount = smooth(amount);
    if (options.style === 'dither') {
      var gx = cellX * options.size + x;
      var gy = cellY * options.size + y;
      return amount >= bayer4(gx, gy) ? 1 : 0;
    }
    /* A diagonal's square influence only exists at a corner.  It should be
     * gentler than either cardinal edge, otherwise a diagonal chasm can cut a
     * conspicuous one-pixel spike into the tile. */
    return direction.diagonal ? amount * 0.72 : amount;
  }

  function mixContributors(contributors, paletteLock) {
    if (contributors.length === 1) return copyPixel(contributors[0].pixel);

    if (paletteLock) {
      /* Palette locking is intentionally deterministic.  The strongest
       * contributor wins, and a tie prefers the lower material ID.  Thus both
       * pixels at a shared seam select the same canonical source texel at the
       * contact edge while interior pixels remain their own material. */
      var best = contributors[0];
      for (var i = 1; i < contributors.length; i += 1) {
        if (contributors[i].weight > best.weight ||
            (contributors[i].weight === best.weight &&
             contributors[i].materialId < best.materialId)) {
          best = contributors[i];
        }
      }
      return copyPixel(best.pixel);
    }

    var totalWeight = 0;
    var alphaWeight = 0;
    var red = 0;
    var green = 0;
    var blue = 0;
    for (var j = 0; j < contributors.length; j += 1) {
      var item = contributors[j];
      var alpha = clamp(item.pixel[3] / 255, 0, 1);
      totalWeight += item.weight;
      alphaWeight += alpha * item.weight;
      red += item.pixel[0] * alpha * item.weight;
      green += item.pixel[1] * alpha * item.weight;
      blue += item.pixel[2] * alpha * item.weight;
    }
    if (totalWeight <= 0 || alphaWeight <= 0) return [0, 0, 0, 0];
    return [
      red / alphaWeight,
      green / alphaWeight,
      blue / alphaWeight,
      alphaWeight / totalWeight * 255
    ];
  }

  function shade(pixel, factor, paletteLock, palette) {
    var target = [pixel[0] * factor, pixel[1] * factor, pixel[2] * factor, pixel[3]];
    if (!paletteLock || !palette || !palette.length) return target;

    /* Keep source-palette mode honest even for wall faces and chasm rims.
     * Choose the closest imported RGBA texel to the shaded target.  Alpha is
     * weighted heavily so a transparent imported edge stays transparent when
     * a matching alpha exists in the source set. */
    var best = palette[0];
    var bestDistance = Infinity;
    for (var i = 0; i < palette.length; i += 1) {
      var candidate = palette[i];
      var distance =
        (candidate[0] - target[0]) * (candidate[0] - target[0]) +
        (candidate[1] - target[1]) * (candidate[1] - target[1]) +
        (candidate[2] - target[2]) * (candidate[2] - target[2]) +
        (candidate[3] - target[3]) * (candidate[3] - target[3]) * 4;
      if (distance < bestDistance) {
        best = candidate;
        bestDistance = distance;
      }
    }
    return copyPixel(best);
  }

  function sourcePalette(materials) {
    var palette = [];
    var seen = Object.create(null);
    for (var materialIndex = 0; materialIndex < materials.length; materialIndex += 1) {
      var source = materials[materialIndex].pixels;
      for (var y = 0; y < source.height; y += 1) {
        for (var x = 0; x < source.width; x += 1) {
          var pixel = pixelFromData(source, x, y);
          var key = pixel.join(',');
          if (!seen[key]) {
            seen[key] = true;
            palette.push(pixel);
          }
        }
      }
    }
    return palette;
  }

  function writePixel(data, width, x, y, pixel) {
    var index = (y * width + x) * 4;
    data[index] = clamp(Math.round(pixel[0]), 0, 255);
    data[index + 1] = clamp(Math.round(pixel[1]), 0, 255);
    data[index + 2] = clamp(Math.round(pixel[2]), 0, 255);
    data[index + 3] = clamp(Math.round(pixel[3]), 0, 255);
  }

  function renderMap(input) {
    input = input || {};
    var materials = normalizeMaterials(input.materials);
    if (!materials.length) throw new RangeError('materials must not be empty');
    var firstSource = materials[0].pixels;
    var options = normalizeOptions(input, firstSource);
    var map = normalizeGrid(input.grid, materials.length);
    var outputWidth = map.width * options.size;
    var outputHeight = map.height * options.size;
    var output = new Uint8ClampedArray(outputWidth * outputHeight * 4);
    var palette = sourcePalette(materials);

    for (var cellY = 0; cellY < map.height; cellY += 1) {
      for (var cellX = 0; cellX < map.width; cellX += 1) {
        var centreIdValue = cellAt(map.grid, cellX, cellY);
        /* A ragged row has no material beyond its end.  It remains transparent
         * rather than borrowing a neighbouring cell. */
        if (centreIdValue === undefined) continue;
        var centreId = Number(centreIdValue);
        var centre = materials[centreId];

        for (var y = 0; y < options.size; y += 1) {
          for (var x = 0; x < options.size; x += 1) {
            var base = sourcePixel(centre.pixels, x, y, options.size);
            var contributors = [{ pixel: base, weight: 1, materialId: centreId }];
            var shadeFactor = 1;

            for (var directionIndex = 0; directionIndex < DIRECTIONS.length; directionIndex += 1) {
              var direction = DIRECTIONS[directionIndex];
              var neighbourIdValue = cellAt(map.grid,
                cellX + direction.dx, cellY + direction.dy);
              if (neighbourIdValue === undefined) continue;
              var neighbourId = Number(neighbourIdValue);
              if (neighbourId === centreId) continue;
              var neighbour = materials[neighbourId];
              var distance = directionDistance(direction, x, y, options.size);
              var amount = blendAmount(distance, direction, options, centreId,
                neighbourId, cellX, cellY, x, y);
              if (amount <= 0) continue;

              contributors.push({
                pixel: neighbourPixel(neighbour.pixels, direction, x, y, options.size),
                weight: amount,
                materialId: neighbourId
              });

              var centreIsChasm = centre.kind === 'chasm';
              var neighbourIsChasm = neighbour.kind === 'chasm';
              if (!centreIsChasm && neighbourIsChasm) {
                /* Solids bordering chasm always have an inward cut.  The
                 * factor is applied after source composition so it is visible
                 * for floor, wall, and transparent imported textures alike. */
                var rimWidth = options.rim;
                var rimAmount = clamp((rimWidth + 0.5 - distance) / rimWidth, 0, 1);
                var rimFactor = centre.kind === 'wall' ? 0.46 : 0.64;
                shadeFactor = Math.min(shadeFactor,
                  1 - rimAmount * (1 - rimFactor));
              } else if (centre.kind === 'wall' && neighbour.kind === 'floor') {
                /* Wall-floor contact gets a compact darker face.  A floor on
                 * the opposite side gets a lighter contact shade below. */
                var depthWidth = Math.max(1, options.depth);
                var faceAmount = clamp((depthWidth + 0.5 - distance) / depthWidth, 0, 1);
                shadeFactor = Math.min(shadeFactor, 1 - faceAmount * 0.43);
              } else if (centre.kind === 'floor' && neighbour.kind === 'wall') {
                var contactAmount = clamp(1.5 - distance, 0, 1);
                shadeFactor = Math.min(shadeFactor, 1 - contactAmount * 0.16);
              }
            }

            var composed = mixContributors(contributors, options.paletteLock);
            if (shadeFactor < 1) {
              composed = shade(composed, shadeFactor, options.paletteLock, palette);
            }
            writePixel(output, outputWidth, cellX * options.size + x,
              cellY * options.size + y, composed);
          }
        }
      }
    }

    return { width: outputWidth, height: outputHeight, data: output };
  }

  return {
    renderMap: renderMap
  };
}));
