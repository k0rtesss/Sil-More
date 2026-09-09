"""Check explicit sound assignments against every monster's real attacks."""
import json
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
DATA = ROOT / "lib/xtra/sound/monsters/monster-sounds.json"
RANGED = dict(zip(
    "ARROW1 ARROW2 BOULDER BRTH_FIRE BRTH_COLD BRTH_POIS BRTH_DARK EARTHQUAKE "
    "SHRIEK SCREECH DARKNESS FORGET SCARE CONF HOLD SLOW HATCH_SPIDER DIM "
    "SNG_BINDING SNG_PIERCING SNG_OATHS THROW_WEB RALLY".split(),
    "arrow1 arrow2 boulder breath_fire breath_cold breath_poison breath_dark earthquake "
    "shriek screech darkness forget scare confuse hold slow hatch_spider dim "
    "song_binding song_piercing song_oaths throw_web rally".split()))


def main():
    races = json.loads(DATA.read_text(encoding="utf-8"))["monsters"]
    text = (ROOT / "lib/edit/monster.txt").read_text(encoding="utf-8")
    seen = set()
    attack_count = 0
    for block in re.split(r"(?m)^N:", text)[1:]:
        race_id, name = block.splitlines()[0].split(":", 1)
        if int(race_id) < 6:
            continue
        seen.add(race_id)
        race = races[race_id]
        assert race["name"] == name, race_id
        blows = re.findall(r"(?m)^B:([^:]+):([^:]+):", block)
        assert [(b["method"], b["effect"]) for b in race["melee"]] == blows, name
        spells = set(re.findall(r"\b[A-Z][A-Z_0-9]+\b",
                               " ".join(re.findall(r"(?m)^S:.*", block))))
        expected = {key for flag, key in RANGED.items() if flag in spells}
        assert set(race["ranged"]) == expected, name
        attack_count += len(blows) + len(expected)
        events = [*race["melee"], *race["ranged"].values(),
                  race["damage"], race["death"], race["idle"]]
        for event in events:
            sounds = event["sounds"]
            assert isinstance(sounds, list) and len(sounds) <= 64, name
            for filename in sounds:
                path = (ROOT / "lib/xtra" / filename).resolve()
                assert path.is_relative_to((ROOT / "lib/xtra").resolve()), filename
                assert path.suffix == ".ogg" and path.is_file(), filename
    assert set(races) == seen
    print(f"PASS: {len(seen)} monster types, {attack_count} explicit attack assignments; "
          "all methods, effects, abilities, and OGG paths match.")


if __name__ == "__main__":
    main()
