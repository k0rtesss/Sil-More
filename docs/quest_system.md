# Sil-Morë Quest System — Complete Reference

This document describes every quest in Sil-Morë, including objectives, rewards, oath unlocks, challenge unlocks, eligibility requirements, chain progression, and the underlying mechanical systems.

---

## Table of Contents

1. [System Overview](#system-overview)
2. [The Six Valar and Their Quest Chains](#the-six-valar-and-their-quest-chains)
3. [Complete Quest Reference](#complete-quest-reference)
   - [Tulkas the Strong](#tulkas-the-strong)
   - [Aulë the Smith](#aulë-the-smith)
   - [Mandos the Doomsman](#mandos-the-doomsman)
   - [Nienna, Lady of Pity](#nienna-lady-of-pity)
   - [Oromë, the Great Hunter](#oromë-the-great-hunter)
   - [Varda, Lady of the Stars](#varda-lady-of-the-stars)
4. [The Six Oaths](#the-six-oaths)
5. [Challenge Unlocks](#challenge-unlocks)
6. [Special Abilities (Quest-Granted)](#special-abilities-quest-granted)
7. [Mechanical Systems](#mechanical-systems)

---

## System Overview

Sil-Morë features a quest system centered around the **Valar** — the divine powers of Tolkien's legendarium. Six Valar can each offer the player up to **three sequential quests**, forming a quest chain. Completing quests grants stat bonuses, special abilities, and unlocks **oaths** — powerful voluntary restrictions that persist across metaruns — as well as **challenge modes** that alter future gameplay.

### Key Concepts

| Concept | Description |
|---------|-------------|
| **Vala** | One of six divine quest-givers (Tulkas, Aulë, Mandos, Nienna, Oromë, Varda) |
| **Quest Chain** | Each Vala offers up to 3 sequential quests (Stage 1 → 2 → 3) |
| **Quest Type** | Either **Vault** (spawns in a special vault room) or **Roulette** (random chance to appear per level) |
| **Oath** | A voluntary restriction unlocked by quest completion; provides bonuses but forbids certain actions |
| **Challenge** | A metarun-wide gameplay modifier unlocked by completing certain stage 2 quests |
| **Completion Cap** | Maximum times a quest can be completed within a single metarun |
| **GLOBAL Flag** | Quest objective spans multiple lives/runs within the metarun |
| **Metarun** | A multi-generation saga tracking progress across sequential characters |

### Quest States

Every quest passes through these states:

| State | Value | Meaning |
|-------|-------|---------|
| `NOT_STARTED` | 0 | Quest not yet encountered |
| `GIVER_PRESENT` | 1 | Quest giver has spawned; quest can be accepted |
| `ACTIVE` | 2 | Quest accepted; objective is in progress |
| `SUCCESS` | 3 | Objective completed; awaiting reward |
| `REWARDED` | 4 | Reward granted; quest fully complete |

Some quests also have a `FAILED` state (e.g., Nienna's pacifism quest fails if you kill).

### Quest Types

- **Vault-based (Y:0)**: The quest giver appears in a specific vault room (e.g., Aulë's Forge, Mandos's Tomb). The player must find the vault during level generation.
- **Roulette-based (Y:1)**: The quest giver has a probability of appearing on each level, governed by a parametric formula. The player may encounter them on any eligible level.

---

## The Six Valar and Their Quest Chains

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                          THE SIX VALAR                                     │
├──────────────┬──────────┬──────────┬───────────┬──────────┬────────────────┤
│   Tulkas     │  Aulë    │  Mandos  │  Nienna   │  Oromë   │    Varda      │
│  (Strength)  │ (Craft)  │  (Doom)  │  (Mercy)  │ (Hunt)   │  (Starlight)  │
├──────────────┼──────────┼──────────┼───────────┼──────────┼────────────────┤
│ Stage 1:     │ Stage 1: │ Stage 1: │ Stage 1:  │ Stage 1: │ Stage 1:      │
│ Slay Shadow  │ Forge    │ Free     │ Stairs    │ Hunt     │ Slay Duruin   │
│ Creature     │ Artifact │ Brodda   │ No Kill   │ 100/80/  │ in Bastion    │
│              │          │          │           │ 60/30    │               │
│ Oath:Valor   │Oath:Smith│Oath:Iron │Oath:Mercy │Oath:     │ Oath:Light    │
│              │          │          │           │Silence   │               │
├──────────────┼──────────┼──────────┼───────────┼──────────┼────────────────┤
│ Stage 2:     │   ---    │ Stage 2: │ Stage 2:  │ Stage 2: │ Stage 2:      │
│ Slay 6 Orc   │          │ Slay     │ Silmaril  │ Slay 10  │ Slay          │
│ Captains     │          │ Ulfang & │ w/o Hit   │ Great    │ Belegwath     │
│              │          │ Uldor    │ Morgoth   │ Dragons  │               │
│ Unlocks:     │          │ Unlocks: │ Unlocks:  │ Unlocks: │ Unlocks:      │
│ BLUNT        │          │ DISCON   │ FIXED_50K │ SINGLE   │ TORCHLIGHT    │
├──────────────┼──────────┼──────────┼───────────┼──────────┼────────────────┤
│ Stage 3:     │   ---    │ Stage 3: │ Stage 3:  │ Stage 3: │ Stage 3:      │
│ Half         │          │ End      │ Escape    │ Hunt 6   │ Slay          │
│ Morgoth's HP │          │ Maeglin  │ Pacifist  │ Named    │ Ungoliant     │
│ [GLOBAL]     │          │          │ [GLOBAL]  │ [GLOBAL] │ [GLOBAL]      │
└──────────────┴──────────┴──────────┴───────────┴──────────┴────────────────┘
```

---

## Complete Quest Reference

---

### Tulkas the Strong

> *"Ho, champion! I have marked your passage through these lightless depths. There is fire in your heart and strength in your arm! Such valour pleases me."*

Tulkas, the mightiest of the Valar in physical prowess, tests the player's combat capability with increasingly daunting martial challenges.

#### Quest 1: Tulkas the Strong (ID: 1)

| Field | Value |
|-------|-------|
| **Vala** | Tulkas (Stage 1) |
| **Type** | Roulette |
| **Oath Unlocked** | Oath of the Valorous Heart (ID: 5) |
| **Completion Cap** | 7 per metarun |
| **Eligibility** | Depth 6–19 |
| **Formula** | `LINEAR_DECAY: 1/(27 - depth)` — becomes more likely as you descend |
| **Challenge Unlock** | None |

**Objective**: Hunt down a named creature of shadow designated by Tulkas and slay it.

**Rewards**:
- **+1 Strength**
- No skill bonus
- No special ability from quest data (reward is typically a unique artifact weapon)

**Narrative**: Tulkas appears with a booming laugh, challenging you to prove your mettle. He names a specific shadow creature haunting the depths. Upon the creature's defeat, Tulkas returns, shaking dust from the ceiling with his laughter, and presents a weapon of ancient renown.

> *"A glorious fight! I felt the earth shake with the fall of your foe! You have proven your might and shown courage worthy of the songs of old. Take this prize—wrought ere the Sun and Moon first rose—and wield it well!"*

---

#### Quest 13: Tulkas, Orc-Bane (ID: 13)

| Field | Value |
|-------|-------|
| **Vala** | Tulkas (Stage 2) |
| **Type** | Roulette |
| **Oath Unlocked** | Oath of the Valorous Heart (ID: 5) |
| **Completion Cap** | 1 per metarun |
| **Eligibility** | Depth 5–6 |
| **Formula** | `LINEAR_DECAY: 1/(27 - depth)` |
| **Challenge Unlock** | **Blunt Arms Challenge** (TULKAS_BLUNT) |

**Objective**: Storm an orc stronghold and slay all six marked captains: **Gorgol, Boldog, Balcmeg, Lug, Orcobal, and Othrod**.

**Rewards**:
- No stat bonuses
- **Unique Bane** ability (SPC_UNIQUE_BANE) — +3 attack and evasion vs unique monsters

**Narrative**: Tulkas seals the orc captains in their lair and bars their kind from the tunnels. The player must hunt down every last captain.

> *"You have crushed their chiefs and broken their muster! Not one captain remains to rally their wretched host. The orcs shall long remember the day their stronghold was emptied."*

**Challenge Unlocked**: The **Blunt Arms Challenge** — a future delve where only blunt weapons may serve.

---

#### Quest 14: Tulkas, Black Foe's Scourge (ID: 14) — GLOBAL

| Field | Value |
|-------|-------|
| **Vala** | Tulkas (Stage 3) |
| **Type** | Vault |
| **Oath Unlocked** | Oath of the Valorous Heart (ID: 5) |
| **Completion Cap** | 1 per metarun |
| **Eligibility** | No specific requirements (assigned at quest start) |
| **Flags** | GLOBAL — spans multiple lives |
| **Challenge Unlock** | None |

**Objective**: Drive Morgoth to half his total strength with your own blows, proving you can shake the Enemy upon his throne.

**Rewards**:
- No stat bonuses
- **Wrath of Tulkas** ability (SPC_TULKAS_WRATH) — Smite counts strength twice when landing

**Narrative**: Tulkas, who wrestled Morgoth before the gates of Utumno, sets the ultimate combat charge.

> *"Well struck! The Black Foe reels, and the pillars of his hall tremble! Not since I cast him upon his face before Utumno has he known such indignity! Let my wrath flow through your smiting hand forevermore."*

---

### Aulë the Smith

> *"I have seen your works, mortal. I have marked the stone you have hewn and the metal you have shaped. There is a spark of the fire of creation within you."*

Aulë, the master craftsman who shaped the substance of the world, challenges the player to prove their skill at the forge. Aulë's chain has only a single quest — his challenge is singular and definitive.

#### Quest 2: Aulë the Smith (ID: 2)

| Field | Value |
|-------|-------|
| **Vala** | Aulë (Stage 1) |
| **Type** | Vault |
| **Oath Unlocked** | Oath of the Smith (ID: 4) |
| **Completion Cap** | 7 per metarun |
| **Eligibility** | Minimum Smithing skill ≥ 10 |
| **Challenge Unlock** | None |

**Objective**: Find Aulë's forge vault and craft an artifact of true worth and mastery at his anvil.

**Rewards**:
- **+2 Smithing** skill bonus
- **Aulë's Forge** ability (SPC_AULE) — counts as Masterpiece with +2 extra difficulty allowance

**Narrative**: The player enters a vault containing Aulë's divine forge, heated by the fires of the world's heart. The challenge is to create a qualifying artifact using the player's own smithing skill.

> *"It is well-wrought. You have not merely forced the shape, but listened to the substance and persuaded it to your will. This is true craft. You are worthy of my blessing."*

---

### Mandos the Doomsman

> *"Child of Middle-earth, here lingers a spirit that should long ago have departed the circles of the world."*

Mandos, the Judge of the Dead, sends the player to execute divine judgments upon spirits and traitors bound by Morgoth's malice, restoring the natural order of death.

#### Quest 3: Mandos the Doomsman (ID: 3)

| Field | Value |
|-------|-------|
| **Vala** | Mandos (Stage 1) |
| **Type** | Vault |
| **Oath Unlocked** | Oath of Iron (ID: 3) |
| **Completion Cap** | 2 per metarun |
| **Eligibility** | No specific requirements |
| **Challenge Unlock** | None |

**Objective**: Enter the Tomb of the King and free Brodda's spirit by unmaking his false body, releasing him to Mandos's judgment.

**Context**: Brodda the Easterling was slain by Túrin, but Morgoth's malice kept his spirit bound, lending it a mockery of life.

**Rewards**:
- **+1 Constitution**
- **Mandos' Doom** ability (SPC_MANDOS) — Immune to fear, hallucination, trance, rage, stun, confusion

**Narrative**: Mandos appears as a veiled, solemn figure upon a throne of black stone. His voice is grave, like the turning of ages.

> *"The mockery is unmade. The spirit of Brodda is unbound and set upon the path to its final judgment. Accept now this gift: a spirit shielded from the great fears that unman the hearts of mortals. Go forth, and walk unafraid."*

---

#### Quest 7: Mandos, Doom of the Easterlings (ID: 7)

| Field | Value |
|-------|-------|
| **Vala** | Mandos (Stage 2) |
| **Type** | Vault |
| **Oath Unlocked** | Oath of Iron (ID: 3) |
| **Completion Cap** | 2 per metarun |
| **Eligibility** | Depth 10–13 |
| **Challenge Unlock** | **Disconnected Stairs Challenge** (DISCONNECTED) |

**Objective**: Storm the Easterling Fortress and slay **Ulfang the Black** and **Uldor the Accursed**, the treacherous Easterling kings whose betrayal at the Nirnaeth Arnoediad brought ruin upon the Eldar.

**Rewards**:
- **+1 Constitution**
- **Mandos' Doom** ability (SPC_MANDOS)

**Narrative**: The chill of Mandos settles once more, heavy as iron. He declares the doom of two mortal king traitors, demanding the player judge them in their own hall.

> *"The traitors of the East are cast down. The doom I spoke upon them is fulfilled. You have been the instrument of a judgment long deferred."*

**Challenge Unlocked**: The **Disconnected Stairs Challenge** — staircases are disconnected, making navigation more treacherous.

---

#### Quest 8: Mandos, Doom of the Betrayer (ID: 8)

| Field | Value |
|-------|-------|
| **Vala** | Mandos (Stage 3) |
| **Type** | Vault |
| **Oath Unlocked** | Oath of Iron (ID: 3) |
| **Completion Cap** | 1 per metarun |
| **Eligibility** | Depth 17–19 |
| **Challenge Unlock** | None |

**Objective**: Seek the Den of Maeglin the Traitor — the Elf who betrayed Gondolin to Morgoth — and deliver final judgment between the 17th and 19th delvings.

**Rewards**:
- **+1 Constitution**
- No special ability (unique reward: a single return from death's gate)

**Narrative**: Mandos's voice returns, colder and more terrible than before. Maeglin, who sold Gondolin for a promise of dominion and dark love, lurks as a shade in the deep places, evading Mandos's summons.

> *"The betrayer of Gondolin meets his doom at last. His treachery brought the Hidden City to ruin and delivered its people to fire and sword. Now that account is settled. Carry this reprieve I grant: once only, death shall loosen its hold at your call."*

---

### Nienna, Lady of Pity

> *"Child, this place is filled with pain. The creatures that dwell here are not all born of evil, but are twisted and marred by the darkness they have long endured."*

Nienna, who weeps for all the sorrows of the world, challenges the player to show compassion and mercy — even in the depths of Angband.

#### Quest 4: Nienna, Lady of Pity (ID: 4)

| Field | Value |
|-------|-------|
| **Vala** | Nienna (Stage 1) |
| **Type** | Roulette |
| **Oath Unlocked** | Oath of Mercy (ID: 1) |
| **Completion Cap** | 7 per metarun |
| **Eligibility** | Depth 14–25 |
| **Formula** | `SCALED_RANGE: 0.125 × max(0, min(1, (depth-14)/5))` — scales up from depth 14 |
| **Challenge Unlock** | None |

**Objective**: Find the downward stair **without taking a single life** on the current level. Any kill fails the quest.

**Rewards**:
- **+1 Grace**
- **Nienna's Gift of Mercy** (SPC_NIENA_MERCY) — Up to +10 stealth based on the ratio of seen-to-killed monsters

**Narrative**: A gentle, sorrowful light gathers. Nienna offers temporary sight of all staircases and asks the player to find the path downward without violence.

> *"You have chosen pity over violence. You have walked through the valley of fear and left behind not death, but a glimmer of hope. For the mercy you have shown, I grant you this blessing."*

---

#### Quest 11: Nienna's Mercy in Angband (ID: 11)

| Field | Value |
|-------|-------|
| **Vala** | Nienna (Stage 2) |
| **Type** | Vault |
| **Oath Unlocked** | Oath of Mercy (ID: 1) |
| **Completion Cap** | 1 per metarun |
| **Eligibility** | Depth 20 (Morgoth's throne room) |
| **Challenge Unlock** | **Fixed 50K XP Challenge** (FIXED_50K) |

**Objective**: Steal a Silmaril from Morgoth's crown **without striking him at all** — not with blade, arrow, song, nor blast of wrath. Even area attacks must spare him. Only damage from Morgoth's own servants is not counted against you.

**Rewards**:
- No stat bonuses
- Nienna's Gift of Mercy is available for 5,000 XP in future runs

**Narrative**: In the shadow of the Iron Crown, Nienna lends her Gift of Mercy for this supreme test of restraint.

> *"You have stolen light from the Iron Crown without striking the Black Foe. Mercy has endured even in Angband. Few deeds in all the ages of the world shall rival this."*

**Challenge Unlocked**: The **Fixed 50,000 XP Challenge** — a fixed experience cap for all future runs.

---

#### Quest 12: Nienna's Path of Peace (ID: 12) — GLOBAL

| Field | Value |
|-------|-------|
| **Vala** | Nienna (Stage 3) |
| **Type** | Vault |
| **Oath Unlocked** | Oath of Mercy (ID: 1) |
| **Completion Cap** | 1 per metarun |
| **Flags** | GLOBAL — spans multiple lives |
| **Challenge Unlock** | None |

**Objective**: Escape Angband **without killing anyone**. Not a single life may be taken — the first blood spilled breaks the vow forever for that character.

**Rewards**:
- No stat bonuses
- Nienna will sweep away every curse that clings to your tale (one-time doom cleanse)

**Narrative**: The ultimate test of mercy. Every creature, however foul, bears a spark kindled by Ilúvatar.

> *"Mercy has endured even in the pits of Angband, where all other virtues fail. You have walked through the domain of the Enemy and left not a single corpse behind."*

---

### Oromë, the Great Hunter

> *"Hail, hunter! I have heard the echo of your deeds in these sunless caverns, but the shadows here run too deep."*

Oromë, the Huntsman of the Valar who rides the forests on his great steed Nahar, challenges the player to hunt increasingly dangerous quarry.

#### Quest 5: Oromë, the Great Hunter (ID: 5)

| Field | Value |
|-------|-------|
| **Vala** | Oromë (Stage 1) |
| **Type** | Roulette |
| **Oath Unlocked** | Oath of Silence (ID: 2) |
| **Completion Cap** | 7 per metarun |
| **Eligibility** | Depth 2–10 |
| **Formula** | `LINEAR_INTERPOLATE: 0.05 → 0.125` over depth range — increasing chance as you go deeper |
| **Challenge Unlock** | None |

**Objective**: Hunt one of the four monstrous broods throughout your journey. The target count varies by creature type:
- **100 wolves**
- **80 spiders**
- **60 serpents**
- **30 vampires**

**Rewards**:
- **+1 Dexterity**
- **Unique Bane** (SPC_UNIQUE_BANE) — +3 attack and evasion vs unique monsters

**Narrative**: The Valaróma sounds, shaking the stones and sending fear through all evil things. Oromë calls upon the player to cleanse the darkness.

> *"A mighty hunt! The foul things of the dark have learned to fear your name. For this service, I grant you the Hunter's Insight."*

---

#### Quest 9: Oromë, Warden of the Drakes (ID: 9)

| Field | Value |
|-------|-------|
| **Vala** | Oromë (Stage 2) |
| **Type** | Roulette |
| **Oath Unlocked** | Oath of Silence (ID: 2) |
| **Completion Cap** | 7 per metarun |
| **Eligibility** | Depth 14–25 |
| **Formula** | `LINEAR_INTERPOLATE: 0.05 → 0.10` over depth range |
| **Challenge Unlock** | **Single Stair Challenge** (SINGLE_STAIR) |

**Objective**: Slay **ten great dragons of renown** — not hatchlings or whelps, but the great drakes and their ancient kin.

**Rewards**:
- No stat bonuses
- **Wraith of Oromë** (SPC_OROME_WRAITH) — Enhanced rage: all stats +1 instead of normal STR/CON gain

**Narrative**: Oromë sets you against the mightiest servants of Morgoth after his Balrogs.

> *"Well struck! The lords of flame and frost have learned to flee before your horn. No hunter since the riding of the Valar has broken so many of the great worms."*

**Challenge Unlocked**: The **Single Stair Challenge** — lone stairs, one way up and one way down on each delving.

---

#### Quest 10: Oromë, Hunt of the Great (ID: 10) — GLOBAL

| Field | Value |
|-------|-------|
| **Vala** | Oromë (Stage 3) |
| **Type** | Vault |
| **Oath Unlocked** | Oath of Silence (ID: 2) |
| **Completion Cap** | 1 per metarun |
| **Flags** | GLOBAL — spans multiple lives |
| **Challenge Unlock** | None |

**Objective**: Slay Oromë's six marked foes across all your delvings:
1. **Scatha** the Worm
2. **Smaug** the Golden
3. **Draugluin**, lord of werewolves
4. **Gostir** the dreadful
5. **Shelob**, spawn of Ungoliant
6. **Thuringwethil** the vampire

**Rewards**:
- No stat bonuses
- **Huntsman's Rhythm** (SPC_HUNTSMAN_RHYTHM) — Two successful arrows drive your next spear with doubled damage
- Future heirs can purchase this ability for 5,000 XP

**Narrative**: The greatest hunt since the world was young — a doom and a glory spanning an entire lineage.

> *"Your line has answered every horn-blast, and the great hunt is ended at last. Take this gift: the rhythm of bow and spear."*

---

### Varda, Lady of the Stars

> *"Child of the darkened Halls, long have I watched your wandering beneath the weight of shadow. Know that even here, in the deepest pits of the Enemy, my light is not wholly quenched."*

Varda, the Queen of Stars who kindled the light of the heavens, sends the player against bastions of shadow and darkness. Varda's rewards are **unique** — the player chooses from a selection of **radiant artefacts** (light-bearing items).

#### Quest 6: Varda, Lady of the Stars (ID: 6)

| Field | Value |
|-------|-------|
| **Vala** | Varda (Stage 1) |
| **Type** | Roulette |
| **Oath Unlocked** | Oath of Light (ID: 6) |
| **Completion Cap** | 1 per metarun |
| **Eligibility** | Depth 1–3 (very early game) |
| **Formula** | `LINEAR_INTERPOLATE: 0.50 → 0.15` over depth range — most likely on level 1, decreasing |
| **Challenge Unlock** | None |

**Objective**: Find and slay **Duruin**, the least of the Balrogs, within his Bastion of Shadow.

**Rewards**:
- No stat bonuses
- **Radiant Artefact Choice** — The player selects one light-bearing artefact from a curated list of items with the LIGHT or RADIANCE flags (no cursed/dark items eligible)

**Narrative**: Varda pierces the gloom with starlight. Duruin has stolen a shard of her light and raised a bastion of shadow.

> *"It is done. The stolen light is free, and the Bastion is emptied of its whispers. Choose now one lamp of my blessing, and bear it through the night that lies ahead."*

**Special Mechanic — Radiant Gift Choice**: Unlike other Valar rewards, Varda presents a scrollable menu of eligible radiant artefacts. Items must bear the LIGHT or RADIANCE flag, must not have DARKNESS/UNLIGHT/LIGHT_CURSE flags, and are depth-capped at approximately player depth + 6 (minimum 15).

---

#### Quest 15: Varda, Shadow's Bastion (ID: 15)

| Field | Value |
|-------|-------|
| **Vala** | Varda (Stage 2) |
| **Type** | Roulette |
| **Oath Unlocked** | Oath of Light (ID: 6) |
| **Completion Cap** | 1 per metarun |
| **Eligibility** | Depth 1–3 |
| **Formula** | `LINEAR_INTERPOLATE: 0.15 → 0.05` over depth range |
| **Challenge Unlock** | **Torchlight Challenge** (TORCHLIGHT — torches only) |

**Objective**: Face **Belegwath**, a Balrog mightier than Duruin, in his Shadow Bastion beyond the 15th delving.

**Rewards**:
- No stat bonuses
- **Radiant Artefact Choice** (renewed selection)

**Narrative**: Belegwath's darkness is a living thing that devours the light and chokes the spirit.

> *"You have broken another stronghold of the Enemy's shadow. The deep places breathe freely once more. Choose your radiant gift anew."*

**Challenge Unlocked**: The **Torchlight Challenge** — only torches may be used for illumination.

---

#### Quest 16: Varda, Gloomweaver's Doom (ID: 16) — GLOBAL

| Field | Value |
|-------|-------|
| **Vala** | Varda (Stage 3) |
| **Type** | Vault |
| **Oath Unlocked** | Oath of Light (ID: 6) |
| **Completion Cap** | 1 per metarun |
| **Flags** | GLOBAL — spans multiple lives |
| **Challenge Unlock** | None |

**Objective**: Seek out and slay **Ungoliant, the Gloomweaver** — she who drank the light of the Two Trees and vomited forth the Unlight that even Varda's stars could not pierce.

**Activation Requirements**: Requires the Oath of Light active and at least one prior Varda quest completed (Quest 6 or 15).

**Rewards**:
- No stat bonuses
- Every new venture of your line begins with a **relic chosen under starlight** (starting item for future heirs)

**Narrative**: Varda's gaze fixes upon the deepest darkness. Ungoliant's hunger is boundless, and her malice older than Morgoth's reign.

> *"Her hunger is ended at last. The Gloomweaver shall devour no more light. Henceforth, every new venture of your line shall begin with a relic chosen under starlight."*

---

## The Six Oaths

Oaths are powerful voluntary restrictions unlocked by completing Stage 1 quests. Each oath provides permanent bonuses but **forbids specific actions**. Breaking an oath has severe narrative and mechanical consequences, and the oath is marked as **permanently sundered** for that metarun generation.

### Oath Summary Table

| # | Oath | Unlocked By | Stat Bonus | Skill Bonus | Forbidden Action | Behavioral Restriction |
|---|------|-------------|------------|-------------|------------------|----------------------|
| 1 | **Oath of Mercy** | Nienna (Q:4) | +1 Grace | — | Attack Men or Elves | Cannot attack RF3_MAN/RF3_ELF creatures |
| 2 | **Oath of Silence** | Oromë (Q:5) | +1 Dexterity | — | Sing or speak vocally | Cannot use any singing abilities/songs |
| 3 | **Oath of Iron** | Mandos (Q:3) | +1 Constitution | — | Use upstairs without Silmaril | Cannot ascend until possessing a Silmaril |
| 4 | **Oath of the Smith** | Aulë (Q:2) | — | +5 Smithing | Equip non-self-crafted gear | Cannot equip weapons/armor not self-crafted |
| 5 | **Oath of Valor** | Tulkas (Q:1) | +1 Strength | — | Attack fleeing foes | Cannot attack creatures with M_FEAR (terror) |
| 6 | **Oath of Light** | Varda (Q:6) | +1 Light radius | — | Equip shadowed gear | Cannot equip items with DARKNESS/reduced light |

### Oath Details

#### Oath of Mercy (ID: 1)
- **Pledge**: *"I swear to leave Angband without shedding blood of Man or Elf, showing mercy to all Children of Ilúvatar I encounter."*
- **Breaking Confirmation**: *"Your hand stays its course. The life before you is one of the Children of Ilúvatar. To strike them down is to stain your very soul..."*
- **Consequence**: *"The light of pity leaves your eyes, replaced by the shadow of wrath."*
- **Permanent**: *"The echo of your mercy is now a mockery. Your Oath of Mercy is forever broken."*
- **Birth Screen**: `OATH OF MERCY SUNDERED — Thy hands are stained with the blood of kindred.`

#### Oath of Silence (ID: 2)
- **Pledge**: *"I swear to leave Angband as I came, grim and silent, speaking no word and raising no song in these dark halls."*
- **Breaking Confirmation**: *"A word forms on your lips. To give it voice is to shatter the solemn vow..."*
- **Consequence**: *"The quiet strength you held is lost in a cacophony of your own making."*
- **Permanent**: *"The profound stillness you swore to keep is drowned by a single sound."*
- **Birth Screen**: `OATH OF SILENCE SUNDERED — Thy vow is drowned in empty sound.`

#### Oath of Iron (ID: 3)
- **Pledge**: *"I swear that none will daunt me from facing Morgoth forthwith, and I shall not flee upward until a Silmaril is won."*
- **Breaking Confirmation**: *"The path of escape lies before you, a refuge for the weary and the weak..."*
- **Consequence**: *"As you turn from your purpose, the iron will within you turns to brittle rust."*
- **Permanent**: *"Your great purpose is abandoned. Your Oath of Iron is forever shattered."*
- **Birth Screen**: `OATH OF IRON SHATTERED — Thy will is broken, thy purpose abandoned in the dark.`

#### Oath of the Smith (ID: 4)
- **Pledge**: *"I swear to craft all blades and armor by my own hand, taking up no weapon or protection wrought by others."*
- **Breaking Confirmation**: *"Your hand reaches for a tool not of your own making..."*
- **Consequence**: *"As you grasp the foreign craft, the fire of your own forge grows dim within you."*
- **Permanent**: *"You have cast aside self-reliance for the fleeting ease of another's work."*
- **Birth Screen**: `OATH OF THE SMITH FORSAKEN — Thy forge is cold, thy hammer idle.`

#### Oath of the Valorous Heart (ID: 5)
- **Pledge**: *"I swear to face my enemy while it has the heart to fight, and to let the craven flee into the darkness."*
- **Breaking Confirmation**: *"Your foe's will has shattered; it turns and flees for its life..."*
- **Consequence**: *"As your weapon strikes the fleeing foe, the glory of the battle turns to shame."*
- **Permanent**: *"You have chosen a brute's work over a warrior's honor."*
- **Birth Screen**: `OATH OF THE VALOROUS HEART SUNDERED — Thy blade is stained by a coward's stroke.`

#### Oath of Light (ID: 6)
- **Pledge**: *"I swear to wear no shadowed gear that dims my light, and to keep the clear radiance of the stars upon my path."*
- **Breaking Confirmation**: *"Your hand closes on a shadowed thing..."*
- **Consequence**: *"The light within you gutters and fades as you embrace the shadowed gear."*
- **Permanent**: *"The flame you vowed to guard is smothered by your own hand."*
- **Birth Screen**: `OATH OF LIGHT EXTINGUISHED — Thy hand has chosen shadow over starlight.`

---

## Challenge Unlocks

Completing certain Stage 2 quests unlocks persistent challenge modes for future runs within the metarun.

| Challenge | Unlock Quest | Description |
|-----------|-------------|-------------|
| **Disconnected Stairs** | Mandos Q:7 (Ulfang & Uldor) | Staircases are disconnected — up stairs and down stairs don't correspond |
| **Single Stair** | Oromë Q:9 (10 Dragons) | Only one up stair and one down stair per level |
| **Fixed 50K XP** | Nienna Q:11 (Silmaril w/o hitting Morgoth) | Experience is capped at 50,000 |
| **Blunt Arms** | Tulkas Q:13 (6 Orc Captains) | Only blunt weapons may be used |
| **Torchlight** | Varda Q:15 (Belegwath) | Only torches may be used for illumination |

---

## Special Abilities (Quest-Granted)

All quest-granted abilities use the Special skill type (`S_SPC`, ability type 8).

| ID | Ability Name | Source Quest | Effect |
|----|-------------|-------------|--------|
| 0 | **Mandos' Doom** | Mandos Q:3, Q:7 | Immune to fear, hallucination, trance, rage, stun, and confusion |
| 1 | **Aulë's Forge** | Aulë Q:2 | Counts as Masterpiece with +2 extra difficulty allowance in smithing |
| 2 | **Oath of Mercy** (passive) | Oath of Mercy | +1 Grace (oath bonus) |
| 3 | **Oath of Silence** (passive) | Oath of Silence | +1 Dexterity (oath bonus) |
| 4 | **Oath of Iron** (passive) | Oath of Iron | +1 Constitution (oath bonus) |
| 5 | **Nienna's Gift of Mercy** | Nienna Q:4 | Up to +10 stealth based on (seen - killed) monster ratio |
| 6 | **Oath of the Smith** (passive) | Oath of the Smith | +5 Smithing (oath bonus) |
| 7 | **Oath of Valorous Heart** (passive) | Oath of Valor | +1 Strength (oath bonus) |
| 8 | **Unique Bane** | Oromë Q:5, Tulkas Q:13 | +3 attack and +3 evasion vs unique monsters |
| 9 | **Oath of Light** (passive) | Oath of Light | +1 Light radius (oath bonus) |
| 10 | **Wraith of Oromë** | Oromë Q:9 (Dragons) | Enhanced rage: all stats +1 instead of normal STR/CON gain |
| 11 | **Huntsman's Rhythm** | Oromë Q:10 (Great Hunt) | Two successful arrows drive your next spear with doubled damage |
| 12 | **Wrath of Tulkas** | Tulkas Q:14 (Morgoth) | Smite counts strength twice when landing |
| 13 | **Queen of the Stars** | Varda Q:6+ | Extended light reach into darkness |

---

## Mechanical Systems

### Quest Encounter Probability (Formula System)

Roulette-type quests use parametric formulas to determine the probability of the quest giver appearing on each level:

| Formula | Description | Example |
|---------|-------------|---------|
| `LINEAR_DECAY` | `1 / (base - depth)` — increases as depth approaches base | Tulkas Q:1: `1/(27-depth)` |
| `SCALED_RANGE` | `max_prob × clamp((depth-start)/range, 0, 1)` — ramps from 0 to max | Nienna Q:4: `0.125 × (depth-14)/5` |
| `LINEAR_INTERPOLATE` | Linear interpolation from min_prob to max_prob over depth range | Oromë Q:5: `0.05 → 0.125` |
| `FIXED_PERCENT` | Constant probability regardless of depth | — |
| `EXPONENTIAL` | Exponential growth/decay | — |

### Eligibility Requirements

Quests use the `E:` field to gate access:

| Type | Format | Example |
|------|--------|---------|
| `DEPTH_RANGE` | `E:DEPTH_RANGE:min:max` | Tulkas Q:1: Depth 6–19 |
| `SKILL_MIN` | `E:SKILL_MIN:skill:value` | Aulë Q:2: Smithing ≥ 10 |
| `SKILL_RANGE` | `E:SKILL_RANGE:skill:value:depth_min:depth_max` | (not currently used) |

### Metarun Integration

- Each quest has a **metarun flag** (bitmask) tracking completion across the metarun
- **Completion counts** are tracked per quest (capped by L: field)
- Oaths unlocked in one generation are available in subsequent generations
- Challenges unlocked persist for the entire metarun
- An **oath override** allows re-encountering a quest beyond the normal completion cap if the player has that quest's associated oath active

### GLOBAL Quests

Quests marked with `F:GLOBAL` span multiple lives within a metarun. Their objectives persist across character deaths — progress made by one heir carries over to the next. The four GLOBAL quests are:

1. **Tulkas Q:14** — Halve Morgoth's HP with your blows
2. **Nienna Q:12** — Escape Angband without killing anyone
3. **Oromë Q:10** — Slay six named great foes
4. **Varda Q:16** — Slay Ungoliant

### Quest Chain Progression

Stage 2 and 3 quests generally require:
1. Completion of the previous stage in the metarun (tracked via metarun flags)
2. The associated oath to be active (for some chains)
3. Depth eligibility requirements

The `grant_followup_quest_rewards()` function handles the completion flow:
1. Mark quest as completed in the metarun bitmask
2. Unlock the associated oath (if not already unlocked)
3. Apply stat/skill/ability rewards
4. Unlock any challenge tied to the quest

---

*Document generated from analysis of `lib/edit/quest.txt`, `lib/edit/oath.txt`, `src/quest/`, `src/defines.h`, `src/types.h`, and related source files.*
