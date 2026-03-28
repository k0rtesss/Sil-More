import { termColorForAttr } from "./render-dungeon.js";

export function renderMessages(container, messages) {
  container.replaceChildren();

  if (messages.topLineActive) {
    const top = document.createElement("div");
    top.className = "message-top";
    top.style.color = termColorForAttr(messages.topLineColor);
    top.textContent = messages.topLine;
    container.append(top);
  }

  for (const line of messages.lines) {
    const entry = document.createElement("div");
    entry.className = "message-line";
    entry.style.color = termColorForAttr(line.color);
    entry.textContent = line.text;
    container.append(entry);
  }
}
