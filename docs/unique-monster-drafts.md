# Unique monsters for tiles 21,13–21,24

Design draft, 2026-09-08. These are proposed encounters and starting balance values, not implemented monsters or tested balance. The existing working-tree monster roster and combat rules were checked when preparing this document.

## Agreed direction

- Tiles `(21,13)` and `(21,14)` are both ice raukar, following the artist's identification. Give them different combat roles.
- Tile `(21,18)` is the brown winged dragon. Propose Lhamthanc, with the source qualification below.
- Tile `(21,19)` is a sickle-bearing Boldog: an Orc-shaped Maia, classified as both Orc and Rauko.
- Tile `(21,20)` becomes Langon, interpreted as an armed wind spirit and messenger. This replaces the proposed swordsman Celegon for this tile.
- Tile `(21,21)` is an agile shadow spirit; `(21,23)` is the massive stone spirit.
- Tile `(21,22)` is Fankil, an emissary and corrupter of Men.
- Tile `(21,24)` is Nambatur, a pale smith with a hammer. Hammer is the working interpretation of the ambiguous weapon.
- Keep `(21,15)` for the existing Green Great Dragon, and `(21,16–17)` for the existing flying cold-drake and flying fire-drake. They are outside these nine new unique drafts.
- The game permits abandoned passages connecting Angband to remnants of Utumno. Use this as the setting for the oldest spirits.

## Lore and names

Langon is Tolkien's emissary of Melko in *The Book of Lost Tales Part One*, "The Chaining of Melko." His race and powers are unspecified. Making him a wind spirit is our adaptation, not an identification made by Tolkien. His sword suits an armed messenger; wind need not mean an unarmed elemental. [Langon and primary-text references](https://tolkiengateway.net/wiki/Langon)

Fankil is Tolkien's early servant who corrupts Men and estranges them from Elves. His appearance, Maia classification, and later work among the Easterling chieftains are adaptations for this game. Keep his political work distinct from Langon's delivery of commands. [Fankil and primary-text references](https://tolkiengateway.net/wiki/Fankil)

Lhamthanc, "Forked Tongue," is an authentic Noldorin serpent-name in *The Etymologies*, STAK. Tolkien does not establish wings, colour, breath, or even an unambiguous dragon identity. The brown winged dragon is our use of that name. Its biography should not claim participation in battles before winged dragons were revealed. [Lhamthanc](https://eldamo.org/content/words/word-3320454473.html)

The Boldog concept adopts Tolkien's exploratory Orc-shaped Maiar conception in *Morgoth's Ring*, "Myths Transformed," Text X. Angacirca is an invented individual within that conception, not a mortal Orc who became a Maia through promotion. [Discussion and quotation of Tolkien's proposal](https://tolkien.ro/of-the-origins-of-the-orcs/)

All other names are provisional Elvish coinages. They can be names given or recorded by Elves, rather than the spirits' original names before the awakening of the Elves:

| Name | Intended construction |
| --- | --- |
| Ringwion | A new name drawing on early Qenya *ringwë*, "rime, frost," with *-ion*. Intend "Frost-son" poetically. This deliberately uses early vocabulary: later *ringwë* has a different meaning. [Vocabulary history](https://eldamo.org/content/words/word-1231740975.html) |
| Helcamo | Neo-Quenya, "icy one," using *helca* and the personal suffix *-mo*. [Helca](https://eldamo.org/content/words/word-785386149.html), [-mo](https://tolkiengateway.net/wiki/-mo) |
| Angacirca | Neo-Quenya, "Iron-sickle," an enemy-name derived from his weapon. [Anga](https://eldamo.org/content/words/word-3314352465.html), [circa](https://eldamo.org/content/words/word-576900033.html) |
| Dúron | Provisional Neo-Sindarin personal formation based on *dûr*, "dark," intended as "Dark One." [Dûr](https://eldamo.org/content/words/word-3265511325.html) |
| Ondotur | Neo-Quenya, "Stone-lord," from *ondo* and *-tur*. [Ondo](https://eldamo.org/content/words/word-3686939787.html), [-tur](https://eldamo.org/content/words/word-2427731641.html) |
| Nambatur | Neo-Quenya using older *namba*, "hammer," and *-tur*: "Hammer-lord." [Namba in Tolkien's earlier naming material](https://www.eldamo.org/content/words/word-2891577631.html), [-tur](https://eldamo.org/content/words/word-2427731641.html) |

The language labels describe our intended formations, not attestation of these complete names. First Age Orc-only names would be Orkish, not Sauron's later Black Speech. [Black Speech](https://tolkiengateway.net/wiki/Black_Speech)

## Depth and balance conventions

The playable descent ends at `MORGOTH_DEPTH = 20`, or **1,000 feet**. Monster levels 21–24 are stronger generation tiers; they are not floors at 1,050–1,200 feet. The table distinguishes a proposed monster tier from the physical depth at which an encounter should be placed.

Preferred depths are design targets for authored placement. A `W:` level alone does not enforce that band: random generation, vault bonuses, Danger, and the escape phase alter selection. Restricting a unique to a particular home vault requires placement work. Do not add `FORCE_DEPTH` to a tier-21+ monster expecting it to appear at 950 feet; the current predicate compares its tier with the player's physical level.

Starting values assume no curses, blessings, or other difficulty modifiers. Unique HP is the average of its HP dice in the current engine. Damage dice are raw attack dice before hit results, criticals, protection, and elemental handling. They are not guaranteed damage to the player.

Speed 1 is 50% of normal action energy, speed 2 is normal, and speed 3 is 150%. Fast movement also increases opportunities to attack or cast; it is not merely cosmetic movement speed.

Danger assessments assume a reasonably prepared character at the preferred depth. They describe the intended encounter, not a measured survival probability.

| Tile | Unique | Tier | Preferred physical depth | Base HP | Speed | Evasion / protection | Will | Principal danger |
| --- | --- | ---: | --- | ---: | --- | --- | ---: | --- |
| 21,13 | Ringwion, the Pale Blade | 16 | 800–900 ft | 60 (`24d4`) | Normal | +20 / 2d4 | 16 | Missed melee attacks provoke counters |
| 21,14 | Helcamo, the Hoarfrost | 19 | 900–950 ft | 100 (`40d4`) | Normal | +16 / 4d4 | 20 | Slowing makes retreat and prolonged melee dangerous |
| 21,18 | Lhamthanc, the Forked Tongue | 22 | 950 ft, hoard vault | 150 (`60d4`) | Normal | +17 / 3d4 | 23 | Confusion or fear followed by heavy melee |
| 21,19 | Angacirca, Reaper of Thralls | 21 | 900–950 ft, prison vault | 115 (`46d4`) | Normal | +19 / 4d4 | 21 | Disarm and attacks on movement around him |
| 21,20 | Langon, the Rushing Herald | 17 | 850–950 ft | 55 (`22d4`) | Fast | +22 / 1d4 | 18 | Alerts existing enemies and is hard to catch |
| 21,21 | Dúron, Keeper of the Unlit Ways | 20 | 900–950 ft | 70 (`28d4`) | Normal | +24 / 2d4 | 20 | Darkness and flanking undermine safe positioning |
| 21,22 | Fankil, the Sower of Strife | 20 | 900–950 ft, embassy vault | 90 (`36d4`) | Normal | +19 / 2d4 | 23 | Fear plus rallying Orcs, Men, and Raukar |
| 21,23 | Ondotur, the Buried Lord | 23 | 950 ft, Utumno remnant | 160 (`64d4`) | Slow | +12 / 6d4 | 24 | Walls cease to provide reliable separation |
| 21,24 | Nambatur, the Pale Smith | 22 | 900–950 ft, forge vault | 120 (`48d4`) | Normal | +17 / 5d4 | 22 | Stunning hammer blows and knockback |

Reference points in the checked roster: Ringrauko is tier 14, about 25 HP; Belegwath is tier 16, 90 HP; Turkano is tier 18, 110 HP; Vallach is tier 20, 110 HP and fast; Lungorthin is tier 22, 130 HP. New support uniques intentionally have lower direct damage than comparable Balrogs. HP and defence are starting estimates, not the result of encounter simulation.

## 21,13 — Ringwion, the Pale Blade

**Identity.** A ringrauko swordsman whose frost is concentrated in his blade. He served among the watchers of the northern ways, and remains proud of his skill long after their fortifications fell.

**Draft description:** Frost follows the edge of his narrow blade. He watches your hands with an ancient patience, and gives no ground until you strike. Then the pale steel moves.

**Abilities and numbers**

- One sword blow per ordinary melee action: `HIT:COLD`, attack **+20**, **2d9** damage dice.
- **Riposte**: a sufficiently bad melee miss against him can provoke a counterattack, using the existing Riposte rules.
- Cold resistance, fire vulnerability; `RAUKO`, `SMART`, weapon-bearing, door-bashing.
- No slowing spell, entrancement, damaging aura, or extra permanent speed in the first draft.

**Tactics.** A direct duellist who closes to melee. His high evasion punishes inaccurate repeated attacks. The baseline can use normal melee AI; it does not require a special stance or a rule that he literally waits for the player's first attack.

**Synergy.** Helcamo's slowing gives Ringwion more time to engage. Orc archers make standing still to fight him costly. These are situational combinations; he should not always arrive with another unique.

**Danger.** High for characters relying on many low-accuracy attacks; moderate for accurate or ranged specialists. A miss can cost both an action and a counterattack.

**Counterplay.** Accuracy, ranged attacks, cold resistance, and fire. His modest HP and protection reward landing decisive blows. With no Opportunist, stepping directly away does not gain an extra attack from that ability.

**Placement and reward.** First encounter alone in a passage with room to withdraw. One good or great weapon-quality drop is sufficient; no required new artefact.

## 21,14 — Helcamo, the Hoarfrost

**Identity.** An older and heavier cold spirit. Helcamo preserves a sealed watchpost and everything within it, including the dead. His stillness contrasts with Ringwion's precision.

**Draft description:** Rime lies thick upon his shoulders. Behind him the broken doors are sealed with ice, and beneath that ice something still wears its chains. He lifts his weapon, and your breath hangs motionless between you.

**Abilities and numbers**

- One cold blow: `HIT:COLD`, **+22**, **3d8**.
- **Slow**, with `SPELL_PCT_20`. This is the existing Will-based slowing effect, currently adding `2d4` to the slow counter on success.
- Cold resistance, fire vulnerability; `RAUKO`, `SMART`, weapon-bearing, door-bashing.
- Use the existing territorial behaviour for a guardian that stops pursuing around corners. This is not an exact leash to a specific doorway.
- No Hold, cloud damage, regeneration, or movement-triggered extra attacks in this draft.

**Tactics.** Guards a compact chamber and approaches enemies that remain in view. A slowed opponent cannot safely treat his normal speed as an easy retreat. His lower evasion makes him easier to hit than Ringwion, but his protection and HP make prolonged combat more expensive.

**Synergy.** Slowing becomes particularly dangerous with an adjacent duellist, a pursuing Troll, or ranged enemies. A paired frost vault should contain no additional slowing or entrancement caster.

**Danger.** High alone; very high if the player is slowed while another enemy blocks the exit. Cold resistance reduces the elemental threat; it does **not** resist the slowing spell. Will and Free Action address that separate check.

**Counterplay.** Break line of sight before becoming surrounded, use fire, and protect against both cold and slowing. Clear other enemies before committing to melee.

**Placement and reward.** Optional frozen chamber at 900–950 ft with an accessible corner on the approach. A chest or great-quality item should justify opening it. Do not make this a compulsory stair guardian.

## 21,18 — Lhamthanc, the Forked Tongue

**Identity.** A brown winged dragon, using Tolkien's serpent-name as an adaptation. A covetous and deceitful keeper of plunder, concealed in Angband's breeding caverns; no invented claim that it flew over earlier historical battles.

**Draft description:** His brown wings fold close about a mound of stolen things. A divided tongue tastes the air before he speaks, promising passage and naming a price. His eyes have not left your sword-hand.

**Abilities and numbers**

- Claw: `CLAW:HURT`, **+22**, **2d9**.
- Bite: `BITE:WOUND`, **+26**, **3d13**. Both blows belong to the normal melee sequence; this is a dangerous two-blow attacker.
- **Confusion and Fear**, sharing a `SPELL_PCT_25` casting budget. That is not 25% separately for each spell.
- Flying, territorial, intelligent, door-bashing; `DRAGON`.
- No breath weapon or Hold in the initial design. Colour alone does not establish an element. Do not grant resistance to every element merely because it is a dragon.

**Tactics.** A hoard guardian that uses the existing ranged/caster behaviour, then bites and claws when approached. Flight lets it cross chasms within its chamber. Bargaining is descriptive flavour; there is no proposed trading or dialogue subsystem in this first pass.

**Synergy.** Fear and confusion become much worse if the player has brought other enemies into the lair. Place it alone by default; it does not need an escort or another controller to be threatening.

**Danger.** Very high close to the hoard. Two melee blows, wounding, and disrupted actions make entering at low HP especially dangerous. The risk comes from mind-affecting abilities plus physical violence, rather than another coloured breath attack.

**Counterplay.** Will, fear/confusion resistance, a cleared retreat, and ranged attacks around corners. Maintain a walking route out of the chamber; flight should provide shortcuts for the dragon, not require the player to cross a chasm.

**Placement and reward.** A clearly optional 950-ft hoard vault with at least one ordinary walking exit and room to break sight. Several great-quality objects and a chest; a new fixed artefact is not a prerequisite.

## 21,19 — Angacirca, Reaper of Thralls

**Identity.** A Boldog who has taken an Orc body and an overseer's office. His sickle catches weapons and fugitives. He is a captor and executioner, not a supernatural collector of souls.

**Draft description:** An Orc's face looks out beneath the dark helm, but the malice behind it is older than the Orcs. His hooked blade is polished along its inward edge. The guards give way when he passes.

**Abilities and numbers**

- Sickle cut: `HIT:WOUND`, **+24**, **3d8**.
- Weapon hook: `HIT:DISARM`, **+22**, no additional HP damage. This is a second blow/check in the melee sequence, not a literal pull of the player.
- **Zone of Control**: moving between squares that remain adjacent can provoke his ordinary attack sequence. That can include the disarm attempt.
- Both **ORC and RAUKO**, intelligent, armed and armoured, opens/unlocks/bashes doors. Normal speed.
- No Opportunist, player-pulling effect, slowing spell, innate spellcasting, or automatic escape prevention.

**Tactics.** Occupies a doorway or the front of a prison guard formation. Trying to circle him to recover a dropped weapon is dangerous; withdrawing directly out of adjacency remains a meaningful option. Exact doorway selection is a placement/AI objective, not a consequence of the Zone of Control flag alone.

**Synergy.** Orc archers can exploit time spent recovering a weapon. Fankil can sustain the morale of both Angacirca and his guards. Helcamo's slowing would make this encounter substantially more dangerous and should not be a default combination.

**Danger.** Very high for lightly equipped, low-Strength melee characters. The trap is choosing a bad recovery move after disarm, not an unavoidable stun lock.

**Counterplay.** Strength and a two-handed weapon help against the existing disarm check. Carry a usable fallback, clear the escorts, and retreat straight out rather than circle through his threatened squares. Both Orc-slaying and Rauko-slaying categories can recognize him; test their combined weapon interaction before promising additive bonuses.

**Placement and reward.** Prison vault with two to four authored ordinary guards, not a random large escort. Good weapon/armour rewards and access to the prison's treasury. Mixed or tightly limited groups require explicit placement.

## 21,20 — Langon, the Rushing Herald

**Identity.** Tolkien's messenger, interpreted here as a wind spirit clothed in an armed, running form. His task is to carry warning through Angband. A retreating messenger can be more dangerous than a warrior who stands and dies.

**Draft description:** His cloak streams behind him though the air is still. Beneath it you glimpse a drawn sword and a form that will not remain at rest. He turns towards the deeper halls, already drawing breath to cry aloud.

**Abilities and numbers**

- Sword: `HIT:HURT`, **+20**, **2d7**.
- Fast speed (**150% normal action energy**), Flying, Pass Door, Exchange Places; `RAUKO`, `SMART`, poison resistance.
- **Shriek**, with `SPELL_PCT_15`: uses the current alarm mechanic to alert existing monsters. It does not create or summon a fresh army.
- Keep him visible in adequate light. No permanent invisibility, wall passage, teleportation, or damaging wind blast in the initial version.
- No stun-producing Screech: the alarm, mobility, and sword already give him a clear role.

**Tactics.** Keep away, sound the alarm, and use passages or an exchange to avoid being pinned. The existing `SMART + SHRIEK` spy behaviour already prefers substantial distance; this is an evasive messenger, not a charger. He fights when cornered. A guaranteed "warn once, then run to a particular guard post" sequence would require new AI and is not assumed here.

**Synergy.** Strong with almost any nearby hostile population. He is most dangerous near sleeping guardrooms, unseen Orc archers, and a route leading past an unopened vault. He does not provide Fankil's morale rally.

**Danger.** Moderate in an empty area; very high when his warning brings several enemies into the same encounter. Chasing him into unexplored rooms is the intended player mistake.

**Counterplay.** Ranged attacks exploit his low protection and HP. Clear or scout the route, constrain movement with actual walls, and use stealth or existing noise-reducing tools. A closed door alone will not contain a spirit with Pass Door. If an exchange is possible, assess where it would leave the player.

**Placement and reward.** Passage networks at 850–950 ft, initially without another unique. His defeat should give a useful reward despite low HP because preventing an alarm has value. A special carried message or quest is optional later work, not required for the monster.

## 21,21 — Dúron, Keeper of the Unlit Ways

**Identity.** An embodied shadow spirit with an agile outline. Dúron watches the boundary between the inhabited fortress and its forgotten foundations. He is not another creature made of stone.

**Draft description:** The figure stands apart from the wall, yet its shadow joins every shadow about it. No flame is reflected in its face. When it moves, the doorway behind it seems to change its place.

**Abilities and numbers**

- Dark blow: `HIT:DARK`, **+23**, **2d8**.
- Innate darkness strength **-3**, matching the base Gwathrauko's light entry; high evasion **+24**.
- Flanking and Pass Door; `RAUKO`, `SMART`, light vulnerability.
- Normal speed, no additional Invisible flag, no teleport, no wall passage, and no map-forgetting spell.

**Tactics.** Moves around opponents within a room and attacks through the existing Flanking behaviour. Darkness makes his approach and the surrounding fight harder to read. The visual description does not imply actual moving doorways or an illusion-map mechanic.

**Synergy.** Darkness makes archers, traps, and other melee threats harder to assess. Ondotur can remove walls separating their territories, but they should not start together in an unavoidable fight.

**Danger.** High when the player depends on a small light source. The combination of visibility pressure and high evasion is more significant than the modest raw damage dice.

**Counterplay.** Stronger light and light-based attacks, accurate attacks, and fighting in an already explored area with limited flanking space. Light vulnerability does not mean that equipping any lamp automatically damages him.

**Placement and reward.** A side passage or outer chamber of an Utumno remnant at 900–950 ft. Use actual alternate routes, not invisible one-way geometry. One great-quality reward behind his watchpoint.

## 21,22 — Fankil, the Sower of Strife

**Identity.** A dignified emissary who speaks for Morgoth among eastern Men. His violence is exercised through promises, fear, and the obedience of others. His appearance and game classification as a Rauko are adaptations of the early character.

**Draft description:** Red and gold still adorn the envoy's dark raiment. He speaks of gifts, of lands beyond the mountains, and of the weakness of those you trusted. At his quiet command the guards turn towards you.

**Abilities and numbers**

- Defensive sword: `HIT:HURT`, **+20**, **2d8**.
- **Rally and Fear**, sharing `SPELL_PCT_30`; `POW_12` for Rally, base Will **23** for the fear contest.
- Normal speed, intelligent, armed, able to open/unlock doors; `RAUKO`.
- No direct damage spell, Hold, confusion, mind-control faction switching, or summons.

**Tactics.** Stays behind a finite guard group and lets them engage. Existing ranged preferences provide a starting point, but reliably selecting a safe position behind a specific guard formation remains an AI/placement objective. He is killable without a long duel once isolated.

**Synergy.** Rally currently raises temporary morale for other Orcs, Men, and Raukar, with distance falloff. It is not healing, haste, or a direct damage bonus. It can affect eligible monsters beyond the immediate room because the current loop has no line-of-sight or fixed-radius requirement. Angacirca is a particularly strong beneficiary; Dragons and Trolls are not directly rallied by this implementation.

**Danger.** Moderate alone; very high with a guard group. Fear can prevent the player from using melee at the moment the guards cease retreating. Killing a few guards is less reliable as a way to collapse the encounter while Fankil remains alive.

**Counterplay.** Will and fear resistance, separating the group with corners, and finding a shot at the envoy. Breaking sight does not itself stop his Rally from helping eligible allies elsewhere. No claim that he controls every Easterling or that all eastern Men serve Morgoth.

**Placement and reward.** Embassy chamber at 900–950 ft. Suggested initial group: two Easterling warriors, one Easterling archer, and two ordinary Orc guards, placed explicitly. Do not use generic `ESCORTS` for this mixture. A chest and great-quality equipment fit gifts intended for chieftains.

## 21,23 — Ondotur, the Buried Lord

**Identity.** A massive stone spirit from the first excavations of Utumno. It escaped discovery by withdrawing into the mountain. It remembers the ancient siege as pressure and broken rock; an intruder's footsteps can wake that memory.

**Draft description:** What seemed a fallen pillar rises from the floor. Dust pours from hollows where a face might be, and beneath the dust the stone is unbroken. The roots of the mountain groan as it turns.

**Abilities and numbers**

- Stony fist: `HIT:SHATTER`, **+24**, **5d8**.
- Slow speed (**50% normal action energy**), Tunnel Wall, stone body, critical resistance; `RAUKO`, `SMART`.
- Fire, cold, and poison resistance, matching the basic Kemenrauko's elemental identity. Fear immunity fits an unyielding guardian.
- Use the existing Shatter earthquake behaviour. Do not add the separate Earthquake spell or a boulder attack in the first draft.
- No permanent regeneration or player slowing: being able to retreat is the deliberate compensation for its durability.

**Tactics.** Advances slowly through ordinary walls rather than being reliably funnelled through doorways. A suitable near-miss with Shatter can produce an earthquake under current combat rules. It does not intelligently plan a cave-in or guarantee an earthquake on every hit.

**Synergy.** Tunnelling and earthquakes can join spaces previously separated from other threats, including Dúron's watchpoint. Other enemies may benefit from new routes; this is a terrain interaction, not an ally buff.

**Danger.** Extreme if fought in a cramped place or with enemies behind the player; manageable to avoid in open space because it is slow. The greatest mistake is assuming a wall guarantees safety while spending many actions on another threat.

**Counterplay.** Scout escape routes, withdraw early, choose open ground, and use attacks that can overcome high protection. Low evasion rewards accurate, powerful attacks. Cold and fire are poor default plans against it. Do not rely on the same bottleneck surviving the fight.

**Placement and reward.** An optional deep remnant at 950 ft with more than one route out. Place treasure in recoverable locations and check earthquake behaviour around exits before release. No scripted collapse or buried-player state is required.

## 21,24 — Nambatur, the Pale Smith

**Identity.** A craft spirit whose work has become wholly cruel: fetters, prison doors, and instruments of torment. Its pallor is the appearance of an ancient underground body, not a third ice identity.

**Draft description:** Pale hands close about the haft of a great hammer. Behind the smith hang chains of many sizes, each link finished with patient care. There is no pride left in the face, only the will to make another thing that cannot be broken.

**Abilities and numbers**

- Hammer: `HIT:BATTER`, **+25**, **4d8**.
- **Knock Back**, substantial worn armour, fire resistance; `RAUKO`, `SMART`, weapon-bearing, door-opening/bashing.
- Normal speed; no stone-body tag, earthquake, cold attack, or permanent equipment disenchantment.
- Battering can cause stun under the existing critical/stun rules. It does not automatically stun on every hit and does not literally reduce the player's armour.

**Tactics.** A direct armoured fighter whose hammer changes the player's position. A forge chamber gives those pushes a consequence: the player may lose access to a doorway or be displaced towards guards. He does not need to tunnel through the room, which preserves Ondotur's distinct role.

**Synergy.** Knockback can expose the player to an Orc archer or break a favourable formation. Stun can make subsequent attacks and defensive decisions less effective. Avoid adding a slowing caster to the initial forge encounter.

**Danger.** Very high in a crowded forge or beside unsafe terrain; high in an open, cleared room. Armour and HP allow him to survive long enough for repeated hammer blows to matter.

**Counterplay.** Position away from chasm edges and enemy firing lanes, prepare for knockback, and use stun resistance or existing recovery tools. Clear guards first and bring sufficient damage to penetrate protection. Fire resistance belongs to Nambatur; no extra cold vulnerability is assumed.

**Placement and reward.** Optional forge vault at 900–950 ft with two ordinary armed guards. Armour, a good hammer, or access to an existing usable forge make appropriate rewards. Extra forge uses or special crafting powers would be separate features, not promised rewards in this draft.

## Group encounters and limits

These are encounter designs, not new automatic spawn rules. Normally give each unique its own introduction. The presence of nine new uniques should not mean every late level contains several of them.

| Encounter | Proposed composition | Interaction | Design limit |
| --- | --- | --- | --- |
| The Frozen Watch | Ringwion and Helcamo, 950 ft optional vault | Slowing gives the duellist more time; misses risk Riposte | Two ordinary walking exits; no archers or further controllers in the first version. Neither unique is automatically spawned with the other elsewhere. |
| The Herald's Road | Langon and nearby existing ordinary patrols | Alarm turns exploration into pursuit | Do not generate new monsters when he cries. His route should not require opening another boss vault to catch him. |
| The Black Embassy | Fankil and his five authored guards | Fear disrupts the player while Rally sustains guards | Initial version has no other unique. Fankil plus Angacirca is a later, explicitly harder variant, with fewer ordinary guards. |
| The Prison Gate | Angacirca and two to four guards | Recovering a weapon or circling exposes the player | Avoid a forced fight immediately on stair arrival. Maintain a direct withdrawal square. |
| The Old Foundations | Dúron in an outer annex; Ondotur deeper within | Darkness and changing terrain can make encounters converge | Separate their starting areas and provide retreat corners. Do not begin with the player surrounded by both. |
| The Pale Forge | Nambatur and two guards | Knockback alters the room's geometry for the player | No unavoidable chasm push at the entrance and no slowing caster. |
| The Divided Tongue | Lhamthanc alone | Fear/confusion plus two strong melee blows | No second controller or default escort; offer a walking retreat from the hoard. |

Avoid adding every thematically plausible power. In particular, the drafts contain no Hold user, no blanket immunity to all status effects, no repeated summoning, and no new permanent equipment damage. Their difficulty should arise from the specific interactions described above.

## Implementation boundary and source checks

The monster definitions have not been changed. There are no allocated numeric IDs or GUIDs in this document. Append new definitions and generate stable GUIDs only during implementation; preserve existing IDs. Baugon is a proposed rename of the existing tier-7 Boldog, not a tenth new spawn.

**Existing Boldog:** propose **Baugon, the Merciless**, a Neo-Sindarin name using older *baug*, "cruel, tyrannous, oppressive." Keep his existing ID, GUID, tile, tier and combat profile when renaming; rewrite his biography as an ordinary Orc captain. Do not transfer Tolkien's Boldog expedition to seize Lúthien to this unrelated invented captain. The new Angacirca supplies the deep-level Orc-shaped Maia encounter. [Baug and its source history](https://www.elfdict.com/w/baug)

- `lib/edit/monster.txt`: reference monsters, format, tiers, flags and attacks. The header's `I:` synopsis mentions mana, but the actual parser reads **speed, HP dice, light**. Do not draft an extra mana field from that synopsis.
- `src/defines.h:240`, `src/monster/monster-select.c`, `src/externs.h`: 1,000-ft final depth, higher generation tiers, depth variation, and escape generation. Home-vault depth restrictions must be enforced by actual placement, not just prose or `W:`.
- `src/monster/monster-spawn.c`: average unique HP; generic escorts select by ASCII monster letter and have 4–7 or 8–16 targets. They do not express arbitrary mixed groups. `UNIQUE_FRIEND` also scans broadly by letter; do not use it to make the frost pair inseparable.
- `src/tables.c:47`: speed/action-energy ratios. Ranged frequency is an input to the existing chooser and mana economy, not a promise of a fixed spell every N player actions.
- `src/melee/melee-process.c:62`: `SMART + SHRIEK` activates the spy's large preferred separation distance. This supports the evasive Langon draft; it would conflict with a design that expected him to charge continually.
- `src/melee/melee-attack-ranged.c:701`: Rally affects other Orcs, Men and Raukar with distance falloff, without a line-of-sight/radius gate. Keep its power modest and inspect wider-level interactions.
- `src/player/effects.c:13`, `src/player/effects.c:581`: Will-based saves and Free Action interaction for Slow. Elemental cold resistance does not replace that check.
- `src/monster/monster-move.c:600`: Opportunist attacks movement away; Zone of Control attacks movement that remains adjacent. The Angacirca draft uses only the latter to preserve direct withdrawal as a counter.
- `src/melee/melee-attack.c:1650`: disarm targets the wielded melee weapon and checks Strength, with a benefit for a two-handed weapon. It is not a player-pull or shield-theft ability.
- `src/melee/melee-attack.c:1968`: Shatter's near-miss earthquake has existing visibility and hit-margin conditions. The stony-fist versus hammer message also depends on flags; use suitable flavour when implementing Ondotur.
- `src/melee/melee-attack-ranged.c:768`: the current cloud type is derived from breath flags. No frost aura is included here: adding `CLOUD_SURROUND` alone would not establish the desired effect.
- Existing generic messages for Slow, Shriek, and the spirit's sword may need species-appropriate wording. That is later code/text work. No custom hook/pull, wind blast, temporary armour reduction, alarm destination, or intelligent collapse mechanic is assumed.

Before integration, check that Orc/Rauko dual classification is reflected in recall, slaying, morale and placement; that each status and movement ability has the promised counterplay; and that authored exits remain usable under the chosen terrain effects. The numerical starting points need actual combat and encounter playtesting before they can be called balanced.
