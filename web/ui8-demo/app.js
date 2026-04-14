import { createBrowserHost } from "./host-bridge.js";
import { attachInputHandlers } from "./input.js";
import { renderDungeon } from "./render-dungeon.js";
import { renderMessages } from "./render-messages.js";
import { renderOverlay } from "./render-overlay.js";
import {
  decodeEventPacket,
  decodeMapBlob,
  decodeOverlayBlob,
  decodeSessionPacket,
  decodeSnapshotPacket,
  findBlob,
} from "./wire.js";

const samples = {
  open: {
    snapshot: "choice-open.snapshot.bin",
    events: "choice-open.events.bin",
    session: "choice-open.session.bin",
  },
  picked: {
    snapshot: "choice-picked.snapshot.bin",
    events: "choice-picked.events.bin",
    session: "choice-picked.session.bin",
  },
};

const sessionSummary = document.querySelector("#session-summary");
const dungeonGrid = document.querySelector("#dungeon-grid");
const messagesPanel = document.querySelector("#messages");
const overlayPanel = document.querySelector("#overlay");
const eventsPanel = document.querySelector("#events");
const hostLog = document.querySelector("#host-log");
const inputLog = document.querySelector("#input-log");
const resetButton = document.querySelector("#reset-demo");

const host = createBrowserHost(hostLog);
let layout;
let currentState = "open";
let inputHistory = host.loadPersistedInputs();

function requireBlob(snapshotPacket, blobKind, label) {
  const blob = findBlob(snapshotPacket, blobKind);
  if (!blob) {
    throw new Error(`Snapshot packet is missing the ${label} blob (${blobKind}).`);
  }
  return blob;
}

function renderSessionSummary(session) {
  sessionSummary.replaceChildren();
  const pairs = [
    ["Scene", String(session.snapshotScene)],
    ["State", String(session.state)],
    ["Wait", String(session.waitReason)],
    ["Revision", String(session.snapshotRevision)],
    ["Events", String(session.pendingEventCount)],
  ];

  for (const [label, value] of pairs) {
    const line = document.createElement("div");
    line.className = "summary-line";
    line.innerHTML = `<span>${label}</span><strong>${value}</strong>`;
    sessionSummary.append(line);
  }
}

function renderEvents(packet) {
  eventsPanel.replaceChildren();
  for (const record of packet.records) {
    const line = document.createElement("div");
    line.className = "event-line";
    line.textContent = `#${record.sequence} kind=${record.kind} subject=${record.subject} arg0=${record.arg0} arg1=${record.arg1}`;
    eventsPanel.append(line);
  }
}

function renderInputHistory() {
  inputLog.replaceChildren();
  for (const entry of inputHistory) {
    const line = document.createElement("div");
    line.className = "input-line";
    line.textContent = `${entry.key}: ${entry.hex}`;
    inputLog.append(line);
  }
}

async function loadSampleState(name) {
  const sample = samples[name];
  host.log("info", "demo", `loading state ${name}`);

  const [snapshotBytes, eventBytes, sessionBytes] = await Promise.all([
    host.loadBinaryResource(sample.snapshot),
    host.loadBinaryResource(sample.events),
    host.loadBinaryResource(sample.session),
  ]);

  const snapshotPacket = decodeSnapshotPacket(snapshotBytes);
  const eventPacket = decodeEventPacket(eventBytes);
  const sessionPacket = decodeSessionPacket(sessionBytes);
  const mapPacketBlob = requireBlob(snapshotPacket, layout.constants.blobKinds.map, "map");
  requireBlob(snapshotPacket, layout.constants.blobKinds.panes, "panes");
  const overlayPacketBlob = requireBlob(
    snapshotPacket,
    layout.constants.blobKinds.overlay,
    "overlay"
  );
  const mapBlob = decodeMapBlob(
    mapPacketBlob.bytes,
    layout
  );
  const overlayBlob = decodeOverlayBlob(
    overlayPacketBlob.bytes,
    layout
  );

  renderSessionSummary(sessionPacket);
  renderDungeon(dungeonGrid, mapBlob, layout);
  renderMessages(messagesPanel, overlayBlob.messages);
  renderOverlay(overlayPanel, overlayBlob.interaction);
  renderEvents(eventPacket);
  currentState = name;
}

function handlePacket(packetInfo) {
  inputHistory = [packetInfo, ...inputHistory].slice(0, 8);
  host.storePersistedInputs(inputHistory);
  renderInputHistory();
  host.log("info", "input", `encoded ${packetInfo.key}`);

  if (currentState === "open" && (packetInfo.key === "b" || packetInfo.key === "Enter")) {
    loadSampleState("picked").catch((error) => {
      host.log("error", "demo", error.message);
    });
  }
}

async function main() {
  layout = await host.loadJsonResource("layout.json");
  renderInputHistory();
  await loadSampleState(currentState);

  attachInputHandlers({ onPacket: handlePacket });
  resetButton.addEventListener("click", () => {
    loadSampleState("open").catch((error) => {
      host.log("error", "demo", error.message);
    });
  });

  host.log("info", "demo", `bridge ready at ${host.monotonicUsec()} usec`);
}

main().catch((error) => {
  host.log("error", "demo", error.message);
});
