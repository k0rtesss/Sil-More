export const WIRE_VERSION = 1;
export const PACKET_MAGIC = 0x31574953;
export const PACKET_KIND = {
  snapshot: 1,
  events: 2,
  session: 3,
  input: 4,
  intent: 5,
};

const COMMON_HEADER_SIZE = 24;
const SNAPSHOT_META_SIZE = 16;
const SNAPSHOT_BLOB_DESC_SIZE = 12;
const EVENT_META_SIZE = 8;
const EVENT_RECORD_SIZE = 36;
const INPUT_PACKET_SIZE = 24 + 52;
const textDecoder = new TextDecoder();

function dataView(bytes) {
  return new DataView(bytes.buffer, bytes.byteOffset, bytes.byteLength);
}

function readString(bytes, offset, maxLength) {
  const end = offset + maxLength;
  let cursor = offset;
  while (cursor < end && bytes[cursor] !== 0) {
    cursor += 1;
  }
  return textDecoder.decode(bytes.subarray(offset, cursor));
}

export function bytesToHex(bytes) {
  return Array.from(bytes, (value) => value.toString(16).padStart(2, "0")).join("");
}

export function decodePacketHeader(bytes) {
  const view = dataView(bytes);
  const magic = view.getUint32(0, true);
  const version = view.getUint16(4, true);
  const kind = view.getUint16(6, true);
  const packetSize = view.getUint32(8, true);
  const headerSize = view.getUint32(12, true);
  const itemCount = view.getUint32(16, true);

  if (magic !== PACKET_MAGIC) {
    throw new Error(`Unexpected packet magic: 0x${magic.toString(16)}`);
  }
  if (version !== WIRE_VERSION) {
    throw new Error(`Unsupported wire version: ${version}`);
  }
  if (packetSize > bytes.byteLength) {
    throw new Error(`Truncated packet: ${packetSize} > ${bytes.byteLength}`);
  }

  return { magic, version, kind, packetSize, headerSize, itemCount };
}

export function decodeSnapshotPacket(bytes) {
  const header = decodePacketHeader(bytes);
  if (header.kind !== PACKET_KIND.snapshot) {
    throw new Error(`Expected snapshot packet, got ${header.kind}`);
  }

  const view = dataView(bytes);
  const revision = Number(view.getBigUint64(COMMON_HEADER_SIZE + 0, true));
  const scene = view.getUint16(COMMON_HEADER_SIZE + 8, true);
  const flags = view.getUint16(COMMON_HEADER_SIZE + 10, true);
  const blobs = [];

  for (let index = 0; index < header.itemCount; index += 1) {
    const base = COMMON_HEADER_SIZE + SNAPSHOT_META_SIZE + (index * SNAPSHOT_BLOB_DESC_SIZE);
    const kind = view.getUint16(base + 0, true);
    const formatVersion = view.getUint16(base + 2, true);
    const offset = view.getUint32(base + 4, true);
    const size = view.getUint32(base + 8, true);
    blobs.push({
      kind,
      formatVersion,
      offset,
      size,
      bytes: bytes.subarray(offset, offset + size),
    });
  }

  return { ...header, revision, scene, flags, blobs };
}

export function decodeEventPacket(bytes) {
  const header = decodePacketHeader(bytes);
  if (header.kind !== PACKET_KIND.events) {
    throw new Error(`Expected events packet, got ${header.kind}`);
  }

  const view = dataView(bytes);
  const droppedCount = view.getUint32(COMMON_HEADER_SIZE + 0, true);
  const records = [];

  for (let index = 0; index < header.itemCount; index += 1) {
    const base = COMMON_HEADER_SIZE + EVENT_META_SIZE + (index * EVENT_RECORD_SIZE);
    records.push({
      kind: view.getUint16(base + 0, true),
      scope: view.getUint16(base + 2, true),
      flags: view.getUint16(base + 4, true),
      payloadSize: view.getUint16(base + 6, true),
      sequence: view.getUint32(base + 8, true),
      timestampUsec: Number(view.getBigUint64(base + 12, true)),
      subject: view.getInt32(base + 20, true),
      arg0: view.getInt32(base + 24, true),
      arg1: view.getInt32(base + 28, true),
      arg2: view.getInt32(base + 32, true),
    });
  }

  return { ...header, droppedCount, records };
}

export function decodeSessionPacket(bytes) {
  const header = decodePacketHeader(bytes);
  if (header.kind !== PACKET_KIND.session) {
    throw new Error(`Expected session packet, got ${header.kind}`);
  }

  const view = dataView(bytes);
  return {
    ...header,
    apiVersion: view.getUint32(COMMON_HEADER_SIZE + 0, true),
    flags: view.getUint32(COMMON_HEADER_SIZE + 4, true),
    state: view.getUint16(COMMON_HEADER_SIZE + 8, true),
    waitReason: view.getUint16(COMMON_HEADER_SIZE + 10, true),
    waitFlags: view.getUint16(COMMON_HEADER_SIZE + 12, true),
    snapshotScene: view.getUint16(COMMON_HEADER_SIZE + 14, true),
    waitDetail0: view.getInt32(COMMON_HEADER_SIZE + 16, true),
    waitDetail1: view.getInt32(COMMON_HEADER_SIZE + 20, true),
    snapshotFlags: view.getUint16(COMMON_HEADER_SIZE + 24, true),
    snapshotBlobCount: view.getUint32(COMMON_HEADER_SIZE + 28, true),
    pendingInputCount: view.getUint32(COMMON_HEADER_SIZE + 32, true),
    pendingIntentCount: view.getUint32(COMMON_HEADER_SIZE + 36, true),
    pendingEventCount: view.getUint32(COMMON_HEADER_SIZE + 40, true),
    pendingEventDroppedCount: view.getUint32(COMMON_HEADER_SIZE + 44, true),
    snapshotRevision: Number(view.getBigUint64(COMMON_HEADER_SIZE + 52, true)),
  };
}

export function findBlob(snapshotPacket, blobKind) {
  return snapshotPacket.blobs.find((blob) => blob.kind === blobKind) ?? null;
}

export function decodeMapBlob(bytes, layout) {
  const view = dataView(bytes);
  const width = view.getUint16(layout.map.width, true);
  const height = view.getUint16(layout.map.height, true);
  const cells = [];

  for (let index = 0; index < width * height; index += 1) {
    const base = layout.map.cellsOffset + (index * layout.mapCell.size);
    cells.push({
      mapY: view.getInt16(base + layout.mapCell.mapY, true),
      mapX: view.getInt16(base + layout.mapCell.mapX, true),
      flags: view.getUint16(base + layout.mapCell.flags, true),
      attr: bytes[base + layout.mapCell.attr],
      ch: String.fromCharCode(bytes[base + layout.mapCell.ch] || 32),
    });
  }

  return {
    width,
    height,
    cells,
    cursorVisible: bytes[layout.map.cursor.visible] !== 0,
    cursorRelative: bytes[layout.map.cursor.relative] !== 0,
    cursorMapY: view.getInt16(layout.map.cursor.mapY, true),
    cursorMapX: view.getInt16(layout.map.cursor.mapX, true),
    targetActive: bytes[layout.map.target.active] !== 0,
    targetMapY: view.getInt16(layout.map.target.mapY, true),
    targetMapX: view.getInt16(layout.map.target.mapX, true),
  };
}

function decodeInteraction(bytes, baseOffset, layout) {
  const view = dataView(bytes);
  const optionCount = view.getUint16(baseOffset + layout.interaction.optionCount, true);
  const options = [];

  for (let index = 0; index < optionCount; index += 1) {
    const base = baseOffset + layout.interaction.options + (index * layout.interactionOption.size);
    options.push({
      attr: bytes[base + layout.interactionOption.attr],
      tag: String.fromCharCode(bytes[base + layout.interactionOption.tag] || 0),
      enabled: bytes[base + layout.interactionOption.enabled] !== 0,
      selected: bytes[base + layout.interactionOption.selected] !== 0,
      flags: bytes[base + layout.interactionOption.flags],
      key: readString(bytes, base + layout.interactionOption.key,
        layout.interactionOption.label - layout.interactionOption.key),
      label: readString(bytes, base + layout.interactionOption.label,
        layout.interactionOption.meta - layout.interactionOption.label),
      meta: readString(bytes, base + layout.interactionOption.meta,
        layout.interactionOption.size - layout.interactionOption.meta),
    });
  }

  return {
    kind: view.getUint16(baseOffset + layout.interaction.kind, true),
    reason: view.getUint16(baseOffset + layout.interaction.reason, true),
    flags: view.getUint16(baseOffset + layout.interaction.flags, true),
    selectedIndex: view.getInt16(baseOffset + layout.interaction.selectedIndex, true),
    cursorIndex: view.getInt16(baseOffset + layout.interaction.cursorIndex, true),
    optionCount,
    prompt: readString(bytes, baseOffset + layout.interaction.prompt,
      layout.interaction.detail - layout.interaction.prompt),
    detail: readString(bytes, baseOffset + layout.interaction.detail,
      layout.interaction.value - layout.interaction.detail),
    value: readString(bytes, baseOffset + layout.interaction.value,
      layout.interaction.options - layout.interaction.value),
    options,
  };
}

function decodeUiScene(bytes, baseOffset, layout) {
  const view = dataView(bytes);
  const panelCount = view.getUint16(baseOffset + layout.uiScene.panelCount, true);
  const panels = [];

  for (let index = 0; index < panelCount; index += 1) {
    const panelBase = baseOffset + layout.uiScene.panels + (index * layout.uiPanel.size);
    const bodyLineCount = view.getUint16(panelBase + layout.uiPanel.bodyLineCount, true);
    const bodyLines = [];

    for (let lineIndex = 0; lineIndex < bodyLineCount; lineIndex += 1) {
      const lineBase = panelBase + layout.uiPanel.bodyLines + (lineIndex * layout.uiTextLine.size);
      bodyLines.push({
        attr: bytes[lineBase + layout.uiTextLine.attr],
        text: readString(bytes, lineBase + layout.uiTextLine.text,
          layout.uiTextLine.size - layout.uiTextLine.text),
      });
    }

    panels.push({
      style: view.getUint16(panelBase + layout.uiPanel.style, true),
      flags: view.getUint16(panelBase + layout.uiPanel.flags, true),
      bodyLines,
    });
  }

  return { panelCount, panels };
}

function decodeChromeMessages(scene) {
  const allLines = [];

  for (const panel of scene.panels) {
    for (const line of panel.bodyLines) {
      if (line.text) {
        allLines.push(line);
      }
    }
  }

  return {
    topLineActive: allLines.length > 0,
    topLineColor: allLines.length > 0 ? allLines[0].attr : 0,
    topLine: allLines.length > 0 ? allLines[0].text : "",
    lines: allLines.slice(1).map((line) => ({
      color: line.attr,
      text: line.text,
    })),
  };
}

export function decodeOverlayBlob(bytes, layout) {
  const view = dataView(bytes);
  const interaction = decodeInteraction(bytes, layout.overlay.interactionOffset, layout);
  const chromeScene = decodeUiScene(bytes, layout.overlay.chromeSceneOffset, layout);

  return {
    flags: view.getUint16(layout.overlay.flags, true),
    interaction,
    chromeScene,
    messages: decodeChromeMessages(chromeScene),
  };
}

export function encodeLegacyKeyPacket(keyValue, sequence = 1) {
  const bytes = new Uint8Array(INPUT_PACKET_SIZE);
  const view = dataView(bytes);
  const charCode = typeof keyValue === "number" ? keyValue : keyValue.charCodeAt(0);

  view.setUint32(0, PACKET_MAGIC, true);
  view.setUint16(4, WIRE_VERSION, true);
  view.setUint16(6, PACKET_KIND.input, true);
  view.setUint32(8, INPUT_PACKET_SIZE, true);
  view.setUint32(12, INPUT_PACKET_SIZE, true);
  view.setUint32(16, 1, true);
  view.setUint16(24 + 0, 0, true);
  view.setUint16(24 + 2, 1, true);
  view.setUint16(24 + 4, 1, true);
  view.setUint16(24 + 8, 1, true);
  view.setBigUint64(24 + 16, BigInt(sequence), true);
  view.setBigUint64(24 + 24, BigInt(Date.now() * 1000), true);
  view.setUint32(24 + 32, charCode, true);
  view.setUint32(24 + 36, charCode, true);
  view.setUint16(24 + 40, 1, true);

  return bytes;
}
