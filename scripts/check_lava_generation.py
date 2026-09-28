#!/usr/bin/env python3
"""Exercise production lava generation in isolated maps and vault dry routes.

Compiles the actual generation/access sources with in-memory map primitives;
does not launch the game or read player saves/configuration.
"""
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def components(rows):
    dry = {(y, x) for y, row in enumerate(rows) for x, char in enumerate(row)
           if char not in " #:%7`"}
    result = []
    while dry:
        todo = [dry.pop()]
        group = set(todo)
        while todo:
            y, x = todo.pop()
            for dy in (-1, 0, 1):
                for dx in (-1, 0, 1):
                    nxt = (y + dy, x + dx)
                    if nxt in dry:
                        dry.remove(nxt)
                        group.add(nxt)
                        todo.append(nxt)
        result.append(group)
    return result


def vault_tests():
    text = (ROOT / "lib/edit/vault.txt").read_text(encoding="utf-8")
    for serial, name in ((402, "Gothmog's hall"), (420, "Flame Pits")):
        block = text.split(f"N:{serial}:", 1)[1].split("\nN:", 1)[0]
        rows = [line[2:] for line in block.splitlines() if line.startswith("D:")]
        assert len({len(row) for row in rows}) == 1
        assert sum(row.count("`") for row in rows) >= 4
        original = [row.replace("`", "7" if serial == 420 else ".") for row in rows]
        after = components(rows)
        for before in components(original):
            survivors = before & set().union(*after)
            assert any(survivors <= group for group in after), name
        print(f"{name} ({serial}): lava present, original dry routes preserved: PASS")


def main():
    # All procedural materials now share one planner. Keep authored lava-vault
    # route coverage here, alongside the shared production-code suite.
    import check_terrain_generation
    check_terrain_generation.main()
    vault_tests()


if __name__ == "__main__":
    main()
