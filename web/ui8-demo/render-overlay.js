import { termColorForAttr } from "./render-dungeon.js";

export function renderOverlay(container, interaction) {
  container.replaceChildren();

  if (!interaction || interaction.kind === 0) {
    const empty = document.createElement("div");
    empty.className = "overlay-empty";
    empty.textContent = "No active overlay in this snapshot.";
    container.append(empty);
    return;
  }

  const card = document.createElement("div");
  card.className = "overlay-card";

  const prompt = document.createElement("div");
  prompt.className = "overlay-prompt";
  prompt.textContent = interaction.prompt;
  card.append(prompt);

  if (interaction.detail) {
    const detail = document.createElement("div");
    detail.className = "overlay-detail";
    detail.textContent = interaction.detail;
    card.append(detail);
  }

  if (interaction.value) {
    const value = document.createElement("div");
    value.className = "overlay-value";
    value.textContent = interaction.value;
    card.append(value);
  }

  const options = document.createElement("div");
  options.className = "overlay-options";

  for (const option of interaction.options) {
    const row = document.createElement("div");
    row.className = "overlay-option";
    row.style.color = termColorForAttr(option.attr);
    if (option.selected) {
      row.classList.add("is-selected");
    }
    if (!option.enabled) {
      row.classList.add("is-disabled");
    }

    const key = document.createElement("strong");
    key.textContent = option.key || option.tag;
    const label = document.createElement("span");
    label.textContent = option.label;
    const meta = document.createElement("small");
    meta.textContent = option.meta;

    row.append(key, label, meta);
    options.append(row);
  }

  card.append(options);
  container.append(card);
}
