const TERM_COLORS = {
  1: "#f4ecd6",
  2: "#99a39b",
  3: "#d89b4f",
  4: "#d96858",
  6: "#6f9bcc",
  8: "#4c5853",
  11: "#e7d36a",
  12: "#f56d5d",
  13: "#7fca8f",
  14: "#7ec8d9",
};

export function termColorForAttr(attr) {
  return TERM_COLORS[attr] ?? "#d7d1bf";
}

export function renderDungeon(container, map, layout) {
  container.replaceChildren();
  container.style.gridTemplateColumns = `repeat(${map.width}, var(--map-cell))`;

  for (const cell of map.cells) {
    const element = document.createElement("div");
    element.className = "map-cell";
    element.textContent = cell.ch;
    element.style.color = termColorForAttr(cell.attr);
    element.style.opacity = (cell.flags & layout.constants.mapFlags.masked) ? "0.32" : "1";

    if (cell.flags & layout.constants.mapFlags.target) {
      element.classList.add("is-target");
    }
    if (cell.flags & layout.constants.mapFlags.cursor) {
      element.classList.add("is-cursor");
    }

    container.append(element);
  }
}
