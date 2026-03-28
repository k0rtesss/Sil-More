import { bytesToHex, encodeLegacyKeyPacket } from "./wire.js";

const keyMap = {
  ArrowLeft: "h",
  ArrowDown: "j",
  ArrowUp: "k",
  ArrowRight: "l",
  Enter: 13,
  Escape: 27,
};

export function attachInputHandlers({ onPacket }) {
  let sequence = 1;

  function handleKeyDown(event) {
    const mapped = keyMap[event.key] ?? (event.key.length === 1 ? event.key : null);
    if (mapped == null) {
      return;
    }

    event.preventDefault();
    const packet = encodeLegacyKeyPacket(mapped, sequence++);
    onPacket({
      key: typeof mapped === "number" ? event.key : mapped,
      packet,
      hex: bytesToHex(packet),
    });
  }

  window.addEventListener("keydown", handleKeyDown);
  return () => window.removeEventListener("keydown", handleKeyDown);
}
