# Spirit of Fire: Feanor's Last Memory

## Scenario Design Document

---

## I. Lore Foundation

### The Death of Feanor (Silmarillion, Chapter 13)

After the Dagor-nuin-Giliath — the Battle-under-Stars, first battle of the Wars
of Beleriand — Feanor pressed far ahead of his host in his fury, and was
ambushed by Balrogs issuing from Angband, among them Gothmog, Lord of Balrogs.
Long he fought, and was wounded with many wounds. His sons came upon him, and
bore him back toward Eithel Sirion. But as they came within sight of the peaks
of Thangorodrim, Feanor knew with the clarity of death that no power of the
Noldor would ever overthrow them. Yet he cursed the name of Morgoth thrice, and
laid it upon his sons to hold to their Oath and avenge their father. Then he
died; and so fiery was his spirit that as it sped his body fell to ash, and was
borne away like smoke; and his like has never again appeared in Arda, neither has
his spirit left the halls of Mandos.

### Thematic Analysis

This scenario inverts the Glorfindel narrative. Where the base game tells a
story of *remembering forward* — a hero gradually recovering lost identity and
finding renewed purpose — the Feanor scenario tells a story of *remembering
backward*. A dying spirit, the greatest and most terrible of the Eldar, relives
the trajectory of his life in the moments before his fire is spent.

The tragedy is that Feanor sees everything clearly in death — the light of
Valinor, the love of his wife, the wisdom of his father, the faces of those he
wronged — and *still* commands his sons to uphold the Oath. This is the core of
Tolkien's conception of Feanor: not ignorance but willful pride. He understands.
He chooses doom anyway. The Oath is greater than wisdom, greater than love,
greater even than the clear sight of death.

In gameplay terms, this creates a **fast campaign** (fixed number of runs, not
silmaril-gated) where the player's score across all runs is the measure of
Feanor's "life well-burned." The heroes are drawn from those Feanor knew
personally — figures of Valinor's golden age, legends from before the Exile.

### Key Lore Touchstones Referenced

- **Miriel Serinde's death** — Feanor's mother poured so much of her spirit into
  his making that she was consumed. She passed in the gardens of Lorien, the
  first of the Eldar to know weariness unto death. (Silmarillion, "Of Feanor")
- **The Light of the Two Trees** — Laurelin and Telperion, whose mingled light
  Feanor alone captured in the Silmarils. (Silmarillion, "Of the Silmarils")
- **Finwe's love and death** — When Feanor was exiled from Tirion, Finwe chose
  exile with his son at Formenos. There Morgoth slew him, the only Elf-king to
  die by violence in Aman. (Silmarillion, "Of the Darkening of Valinor")
- **Nerdanel's estrangement** — She alone could temper Feanor's flame, yet his
  pride grew beyond her counsel. She refused to follow him into exile, and they
  parted in grief. (Silmarillion, "Of Feanor"; HoME XII)
- **Morgoth's whispers** — The Dark Vala came to Feanor in seeming friendship,
  whispering lies that fed his suspicion of the Valar and his half-brothers.
  Feanor alone saw through Morgoth's ultimate design, yet the poison had already
  taken root. (Silmarillion, "Of the Silmarils")
- **The Oath of Feanor** — Sworn by Feanor and his seven sons in the light of
  the dying Trees, calling Iluvatar himself to witness, invoking the Everlasting
  Darkness upon any who would keep a Silmaril from them. The most terrible oath
  ever spoken. (Silmarillion, "Of the Flight of the Noldor")
- **The Kinslaying at Alqualonde** — The Noldor under Feanor attacked the Teleri
  to seize their ships. Many were slain on both sides. The blood on the white
  quays has never been washed away. (Silmarillion, "Of the Flight of the Noldor")
- **The Burning at Losgar** — Feanor burned the stolen ships upon reaching
  Middle-earth, stranding Fingolfin's host to cross the Helcaraxe or turn back.
  The flames were visible from afar. (Silmarillion, "Of the Return of the Noldor")
- **The Doom of Mandos (Prophecy of the North)** — Spoken by Mandos himself
  at Araman: "Tears unnumbered ye shall shed; and the Valar will fence Valinor
  against you... the Dispossessed shall ye be for ever." (Silmarillion, "Of the
  Flight of the Noldor")

---

## II. Scenario Overview

**Name:** Spirit of Fire
**Subtitle:** Feanor's Last Memory
**Type:** Fast Campaign (run-count-based, not silmaril-gated)
**Runs:** 7 (for the seven sons of Feanor)
**End Condition:** After 7 runs, the campaign concludes regardless of outcome
**Scoring:** Total score across all 7 runs is compared
**Hero Pool:** 9 heroes, all personally known to Feanor in Valinor

### Narrative Frame

The player is Feanor, dying upon the ash-fields within sight of Thangorodrim.
His sons have borne him back from the Balrog-fire. In the moments before his
spirit consumes his body, he relives his entire life — each run is a memory
surging through his fading consciousness. The heroes he plays ARE his memories:
the people who shaped him, loved him, opposed him, suffered for him.

After all seven memories have played out (regardless of victory or defeat), the
final scene always fires: Feanor opens his eyes one last time, looks upon
Thangorodrim, curses Morgoth thrice, and commands his sons to hold the Oath.
Then his body burns to ash.

### Progression Structure

| Run | Memory Theme | Narrative Focus |
|-----|-------------|-----------------|
| 1 | Light | The Two Trees, first wonder of Valinor |
| 2 | Craft | The forge, creation, the Silmarils born |
| 3 | Love | Family — mother lost, wife found, father's devotion |
| 4 | Discord | Morgoth's lies, strife among kin, exile |
| 5 | Darkness | The Trees destroyed, Finwe slain, grief |
| 6 | Blood | The Oath, Alqualonde, ships burning |
| 7 | Fire | The battle, the Balrogs, the fall |

---

## III. New Heroes

Five new heroes to be added to `character.txt` for this scenario. All are
legendary figures from Valinor whom Feanor knew personally.

### Hero 1: Finwe

*Father of Feanor, First High King of the Noldor in Aman*

```
# First King of the Noldor. Leader, warrior, father.
# Strong presence, good fighter, high will. Died defending Formenos alone.
# Stats +7, Power 3

N:XX:Finwe
Q:[auto-generated]
A:, High King of the Noldor
B:You stand alone at the door-first among kings, whose love kindled the brightest and most terrible flame.
F:WIL_AFFINITY | MEL_AFFINITY | PER_AFFINITY | SNG_PENALTY
S:2:1:2:2
C:5:9:0:10  # Majesty, Zone of Control
P:3
D:First King of the Noldor, who led his people upon the Great Journey beneath
D:starlight immemorial, bravest among the Eldar before sorrow marred the world.
D:Father of flame and fury, whose love for Feanor burned deeper than any crown
D:or counsel; he chose exile beside his firstborn when wisdom urged him to
D:remain. Alone at Formenos he stood against the shadow of Morgoth, a king who
D:would not flee though no strength of Elf could withstand that darkness; his
D:blood upon the threshold kindled the wrath that sundered the Blessed Realm
D:and drove his sons to ruin beyond the Sea.
```

**Lore notes:** Finwe was the first Elf to speak with the Valar's embassy. He
led the Noldor across the world to Valinor. When Feanor was banished from
Tirion for threatening Fingolfin with a sword, Finwe — as king — could have
stayed. Instead, he relinquished the crown and went into exile with his son at
Formenos. He was slain by Morgoth in the dark, alone, defending the door to the
treasure-chamber where the Silmarils were kept. He is the only king of the Eldar
to die by violence in the Blessed Realm.

---

### Hero 2: Ingwe

*High King of the Vanyar, High King of all the Eldar*

```
# Purest of all Elves, nearest to the Valar. Grace-dominant spiritual leader.
# Very high grace, low physical stats. Supreme in song and will.
# Stats +6, Power 3

N:XX:Ingwe
Q:[auto-generated]
A:, High King of the Eldar
B:You dwell in radiance-purest of the Firstborn, whose voice echoes nearest to the Throne of Arda.
F:WIL_AFFINITY | SNG_AFFINITY | PER_AFFINITY | SMT_PENALTY | STL_PENALTY | MEL_PENALTY
S:0:0:1:5
C:7:7:7:16  # Song of the Trees, Song of Mastery
P:3
D:High King of all the Eldar, lord of the Vanyar who dwell upon holy
D:Taniquetil in the undimmed light of the Blessed Realm, nearest among the
D:Children of Iluvatar to the thought of Manwe. His spirit shines purest of
D:all who woke beneath the first stars, untouched by pride or shadow, yet
D:watching afar the doom of exiled kindred with grief that no song can heal.
D:No sword he bears in wrath, yet his voice commands the deeps of Arda; his
D:light foreshadows the host of the Vanyar who shall march at last to war in
D:the Great Battle, heralding Morgoth's overthrow in an age yet to come.
```

**Lore notes:** Ingwe is the acknowledged overlord of all three kindreds of the
Eldar. He was the first of the three ambassadors (with Finwe and Elwe) whom
Orome brought to Valinor to see its splendor. He dwells upon Taniquetil itself,
at the feet of Manwe and Varda. He is mentioned in the Silmarillion as "the most
noble of the Elves." Though he never returned to Middle-earth himself, the
Vanyar host that he sends to the War of Wrath is decisive in Morgoth's defeat.
Feanor would have known him well in Valinor — Ingwe was the supreme king, and
Feanor's own half-brothers were of Vanyar blood through Indis.

---

### Hero 3: Nerdanel the Wise

*Wife of Feanor, daughter of Mahtan, sculptor of Valinor*

```
# The only one who could temper Feanor. Wise, perceptive, crafty.
# Perception and grace focused, with smithing skill. Not a warrior.
# Stats +5, Power 2

N:XX:Nerdanel
Q:[auto-generated]
A: the Wise
B:You shape in patience-sculptor whose wisdom tempered flame, whose love could not hold it.
F:PER_AFFINITY | WIL_AFFINITY | SMT_AFFINITY | MEL_PENALTY
S:0:1:1:3
C:6:4:4:7  # Expertise, Listen
P:2
D:Daughter of Mahtan and wife of Feanor, called the Wise among the Noldor;
D:sculptor of stone and bronze whose patient hands shaped works of beauty that
D:endured beyond wrath and ruin. Alone among all the Eldar she could match
D:his fiery spirit, tempering flame with counsel; in their youth they
D:wandered far together, and she bore him seven sons, each touched by their
D:father's fire. Yet his pride grew beyond her reaching, and her counsel was
D:spurned for the Oath's sake; she remained in Aman when sons and husband
D:departed into exile, her grief deep as the Sundering Sea, foreseeing the
D:ruin she could not prevent, a heart broken by the flame it once loved.
```

**Lore notes:** Nerdanel is described in "The Peoples of Middle-earth" (HoME
XII) and referenced in the Silmarillion. She was brown-haired (unusual among
Noldor), strong-willed, and talented in sculpture — she made statues so
lifelike they were mistaken for real people. She travelled with Feanor in their
early years and was the only one who could restrain his temperament. Their
estrangement is one of the great quiet tragedies of the Silmarillion. She tried
to dissuade at least some of her sons from following Feanor, but only managed to
keep none — all seven departed. She remained in Valinor, grieving.

---

### Hero 4: Mahtan

*Great smith of the Noldor, pupil of Aule, father of Nerdanel*

```
# Master craftsman, taught Feanor. Copper-bearded, close to Aule.
# Smith-dominant with good grace. The source of Feanor's craft knowledge.
# Stats +6, Power 2

N:XX:Mahtan
Q:[auto-generated]
A:, Pupil of Aule
B:You forge in ancient wisdom-copper-bearded master, whose teaching lit fires both wondrous and terrible.
F:SMT_AFFINITY | PER_AFFINITY | WIL_AFFINITY | SNG_PENALTY | MEL_PENALTY
S:1:1:1:3
C:6:3:6:5:6:2  # Enchantment, Artifice, Jeweller
P:2
D:Greatest of the Noldorin smiths save Feanor alone, copper-bearded, beloved
D:pupil of Aule, dwelling near the Vala's own forge-halls in Valinor's peace.
D:His hands taught young Feanor the mastery of flame and metal, sharing
D:secrets of divine craft freely, not foreseeing that his teaching would
D:kindle both the supreme wonder and the deepest grief of Arda. Father of
D:Nerdanel the Wise, his heart perceived dimly the shadow that craft may
D:breed when wedded to unbridled pride; yet knowledge once given cannot be
D:recalled, and his legacy shines and darkens alike through all the works of
D:the Noldor unto the ending of the world.
```

**Lore notes:** Mahtan (also called Aulendur, "Servant of Aule") is described
in "The Peoples of Middle-earth." He had a copper-colored beard, which is
remarkable — beards are extremely rare among Elves in Tolkien's writing, and
this detail marks him as unusual, closer to the earth-craft of Aule than most
of his kin. He taught Feanor metalworking, though Feanor ultimately surpassed
him. Mahtan later regretted sharing so freely with one so proud. The teacher
whose gift outgrew his wisdom — a deeply Tolkienian theme.

---

### Hero 5: Miriel Serinde

*First wife of Finwe, mother of Feanor, the supreme needlewoman*

```
# Feanor's mother, who died giving him life. Fragile but immensely gifted.
# Very high grace, low physical. Craft and song focused.
# Stats +4, Power 2

N:XX:Miriel Serinde
Q:[auto-generated]
A:, the Broideress
B:You weave in silence-needlewoman whose art surpassed all thread and thought, whose life was the price of fire.
F:SMT_AFFINITY | PER_AFFINITY | SNG_AFFINITY | MEL_PENALTY | EVN_PENALTY
S:-1:2:-1:4
C:7:7:6:4  # Song of the Trees, Expertise
P:2
D:First wife of Finwe, surpassing all the Eldar in the arts of weaving and
D:needlework, whose tapestries captured the very light of the Two Trees in
D:thread of silver and gold. Yet in the bearing of Feanor she poured forth so
D:great a fire of spirit that her own was utterly consumed; she lay down in
D:the gardens of Lorien and her spirit departed, the first of the Eldar to
D:know weariness unto death. Her absence became the wound at the heart of
D:Feanor's making — the grief that fed his restless flame, the loss that no
D:craft could mend. She dwells now among Vaire's handmaidens in Mandos,
D:weaving into tapestry the fates she set in motion, her son's doom
D:threaded through every work of sorrow.
```

**Lore notes:** Miriel is central to understanding Feanor. She is described in
the Silmarillion and more fully in "Morgoth's Ring" (HoME X). Her death caused
the first crisis in Valinor — Finwe wished to remarry (Indis of the Vanyar),
which required unprecedented dispensation from Mandos. This second marriage
produced Fingolfin and Finarfin, and Feanor never forgave any of them — not his
father for remarrying, not the Valar for permitting it, and not his half-
brothers for existing. Miriel's sacrifice is the seed of the entire Noldorin
tragedy. Feanor never knew her, yet she shaped him more than anyone.

In the context of this scenario — a dying Elf remembering his life — seeing
Miriel for the first time through the veil of death is perhaps the most
Tolkienistic moment possible. The son returns at last to the mother who gave
everything for his fire.

---

## IV. Full Hero Pool (9 Heroes)

| # | Hero | Source | Relationship to Feanor |
|---|------|--------|----------------------|
| 1 | **Finwe** | NEW | Father. Died for him. |
| 2 | **Ingwe** | NEW | High King of all Elves. Spiritual overlord. |
| 3 | **Nerdanel** | NEW | Wife. The one who could not hold him. |
| 4 | **Mahtan** | NEW | Father-in-law and teacher. Source of his craft. |
| 5 | **Miriel Serinde** | NEW | Mother. Died to give him life. |
| 6 | **Fingolfin** | EXISTING (N:2) | Half-brother. Rival. The one he wronged most. |
| 7 | **Finarfin** | EXISTING (N:3) | Half-brother. The one who turned back. |
| 8 | **Galadriel** | EXISTING (N:8) | Niece. Opposed him openly. Pride matching pride. |
| 9 | **Finrod Felagund** | EXISTING (N:9) | Nephew. Noble heart Feanor could not corrupt. |

### Design Rationale

Every hero represents a facet of Feanor's life and character:

- **Finwe & Miriel** — the parents, one lost to love's sacrifice, one lost to
  Morgoth's malice. The bookends of Feanor's grief.
- **Nerdanel & Mahtan** — the family he chose, then abandoned. Craft and wisdom
  that could not hold him.
- **Ingwe** — the light Feanor rejected. The purity he could not match. The
  Valinor that was not enough.
- **Fingolfin & Finarfin** — the brothers of the divided house. One he betrayed
  at Losgar, one who saw through his madness and turned back.
- **Galadriel** — the niece whose pride rivalled his own, who desired Middle-
  earth for her own reasons, who refused to take a Silmaril when offered.
- **Finrod** — the noblest of Finarfin's children, whose gentleness and
  faithfulness Feanor could never understand. A mirror of what Noldor might
  have been without the Oath.

---

## V. Narrative Script

### Opening Scene (Plays Once, at Campaign Start)

> **Title: The Ash-Field Before Thangorodrim**
>
> Fire. The memory of fire. Not Angband's furnaces, not the burning ships at
> Losgar, not even the death of the Trees — but the fire that is *you*.
>
> You lie upon the grey ash of a land never lit by sun or moon, beneath
> skies pierced only by the cold malice of stars. Your sons' faces swim
> above you — Maedhros with his copper hair matted dark, Maglor's lips
> moving in song you can no longer hear, Celegorm's hound whimpering at the
> edge of sight. Seven faces. Seven flames you kindled, now bent over the
> forge that made them.
>
> The Balrog-wounds are mortal. You know this. You have always known what
> mortality means — your mother taught you before you drew your first breath.
>
> But the fire within you is not yet spent. It surges, and with it comes
> memory — brilliant, terrible, inescapable. Valinor's light floods back,
> and you remember *everything*.

---

### Memory Chapters (Run Progression — Type 0: Light)

These play at the START of each run, setting the memory's theme.

---

#### Run 1 — Memory of Light

> **Title: Laurelin and Telperion**
>
> Golden and silver, waxing and waning, the Two Trees breathe light across
> the Blessed Realm. You remember them now as you remembered nothing else —
> the mingling hour when gold and silver met, when the air itself was
> holy, and all of Valinor was bathed in a radiance that can never return.
>
> You stood beneath them as a child, small and burning, your eyes drinking
> the light that would one day drive you to madness and mastery alike. Even
> then you knew: this beauty was not eternal. Even then the thought formed,
> unbidden: *what if it could be preserved?*
>
> The light streams through your dying mind, and for a moment the ash-field
> dissolves, and you are home.

---

#### Run 2 — Memory of Craft

> **Title: The Hammer and the Jewel**
>
> Your hands remember before your mind does. The weight of the hammer, the
> ring of mithril beneath the stroke, the patient turning of silver wire
> into shapes no other hand could conceive. Mahtan taught you flame and
> form, but the *vision* was yours alone — born in you as language was born
> in Rumil, as song was born in the Ainur.
>
> You remember the day the first Silmaril was finished. You held it up and
> the light of the Two Trees blazed within it, captured, perfected,
> *imprisoned*. The Valar looked upon your work in wonder and unease. And
> you understood, in the terrible clarity of the maker, that you had wrought
> something greater than yourself — and that it would destroy you.
>
> Yet you forged the second, and the third. For what smith can leave a work
> half-done?

---

#### Run 3 — Memory of Love

> **Title: The Unfinished Sculpture**
>
> Nerdanel's face forms in the flame — not the grief-stricken face that
> watched you depart into exile, but the young face, laughing, bronze dust
> on her cheeks, her sculptor's hands still wet with clay. You remember
> wandering with her beyond the borders of Valinor, into the starlit
> wilderness where no Elda had walked, and speaking of craft and wonder and
> the shapes of things not yet made.
>
> Behind her stands Finwe, your father, whose eyes you have — proud, keen,
> burning with love so fierce it chose exile over wisdom. And behind him, a
> shadow you have never seen but always felt: Miriel, who gave her life that
> yours might burn.
>
> Seven sons. A wife who could match your fire. A father who stood alone
> against Morgoth for your sake. A mother whose face you carry without
> knowing it.
>
> What was the Oath, weighed against this?

---

#### Run 4 — Memory of Discord

> **Title: The Poison and the Sword**
>
> Morgoth came to you in seeming friendship, his voice smooth as poured
> silver. "The Valar would keep the Silmarils," he whispered. "Thy half-
> brothers covet what is thine. Fingolfin speaks against thee in secret."
> And you listened. Not because you believed him — you alone among the
> Noldor saw through the Vala's mask to the hunger beneath — but because
> the lies were shaped from truth's own clay. The Valar *did* desire the
> Silmarils. Fingolfin *did* resent your precedence.
>
> You drew your sword upon your brother in the Ring of Doom. Fingolfin
> stepped back, saying nothing, his eyes holding neither fear nor anger —
> only pity. And that pity burned worse than any flame.
>
> The Valar exiled you from Tirion. Your father came with you. The poison
> had done its work; not in making you believe Morgoth's lies, but in
> teaching you to trust nothing but your own fire.

---

#### Run 5 — Memory of Darkness

> **Title: The Door at Formenos**
>
> Darkness. Not the gentle dark of night beneath Varda's stars, but the
> Unlight of Ungoliant — a void that devours sight and memory and hope.
> The Trees are dead. Their light exists now only in your Silmarils, and
> the Valar come to you *begging*.
>
> "Break open the Jewels," they say. "Release the light. Restore what
> was lost." And you refuse. Not from greed alone — though greed is there,
> burning like a coal — but because you know what Yavanna will not speak
> aloud: that even the light of the Silmarils cannot revive what is truly
> dead.
>
> Then comes the messenger. Formenos broken open. The Silmarils stolen.
> And your father — your father who chose you over kingship, over wisdom,
> over everything — lies dead upon the threshold, his blood the last
> offering of the old world.
>
> In this memory, you scream. Even dying, even here on the ash-field, the
> scream tears through you.

---

#### Run 6 — Memory of Blood

> **Title: The White Shore, the Red Shore**
>
> The Oath blazes through you like a forge-fire stoked beyond endurance.
> You remember every word: *Be he foe or friend, be he foul or clean...
> neither law, nor love, nor league of swords, nor the might of the Valar,
> nor the dread of Morgoth shall defend him from the pursuing wrath of the
> Sons of Feanor.* Your seven sons echoed it, their voices bright and
> terrible under the dying light.
>
> Alqualonde. The white ships rocking in the harbor. Olwe's people, who
> would not lend what they had built with love and labor. You remember the
> first sword drawn — yours. You remember the blood on the white quay-
> stones, and how the sea did not wash it away. You remember Finarfin's
> face as he turned back, weeping, leading a remnant home to beg the
> Valar's pardon.
>
> Then Losgar. The ships burning. Maedhros standing apart, his face stricken,
> because he understood what you had done: you had made the sundering
> permanent. Fingolfin would cross the Helcaraxe, and the ice would grind
> away whatever brotherhood remained.
>
> You lit the ships yourself. You watched them burn, and felt nothing but
> the cold satisfaction of a door sealed shut.

---

#### Run 7 — Memory of Fire

> **Title: Spirit of Fire**
>
> The last memory is now.
>
> Stars above Beleriand — not the gentle stars of Valinor but hard, cold,
> brilliant with Varda's distant judgment. Your host victorious, the Orcs
> scattered, and you pressing forward because you have *always* pressed
> forward, because to hesitate is to admit that the fire has limits, and
> the fire has no limits.
>
> Then the Balrogs came from Angband.
>
> Gothmog. Fire meeting fire. You fought as no Elf has ever fought, your
> sword blazing with the fury of your own spirit, but they were flame born
> of the World's making and you were flame born of one Elf-woman's death.
> They surrounded you. They broke you.
>
> Your sons carry you now. Through the ash. Toward the peaks.
>
> You can see Thangorodrim.
>
> You can see everything.

---

### Memory Chapters (Run Progression — Type 1: Dark/Defeat)

These play when a run ENDS IN DEFEAT (hero dies in Angband). They represent
Feanor's memory collapsing under the weight of his wounds.

---

#### Run 1 Defeat — The Light That Burns

> The golden light of Laurelin flickers and dims. You reach for it, but
> your hands pass through, and the radiance turns to embers. Even in
> memory, the Trees are dying. Even in memory, you cannot save them.
> Your fire only hastened their end.

---

#### Run 2 Defeat — The Shattered Jewel

> The hammer falls wrong. The crystal splits. In this memory the Silmaril
> cracks before completion, its inner light bleeding away like water through
> broken stone. You scream for it to hold — but the work unravels. Not all
> craft can redeem what flame consumes. Not even yours.

---

#### Run 3 Defeat — The Empty House

> You return to the halls of your home and find them hollow. Nerdanel's
> sculptures stare with blank eyes. Finwe's chair sits cold. The cradle
> where your sons slept gathers dust of ages. You chose the Oath over this.
> The house remembers. The house does not forgive.

---

#### Run 4 Defeat — The Mirror of Morgoth

> Morgoth laughs. Not with malice, but with recognition. "We are alike,
> Feanaro," he whispers. "Maker and Breaker. You called me thief, yet you
> stole ships. You called me murderer, yet you slew kin. The fire that
> made you is the fire that made me. There is no difference." And the worst
> of it — the very worst — is that you cannot answer.

---

#### Run 5 Defeat — The Threshold

> You stand before the door of Formenos, but now it is *your* body on the
> threshold, broken and still. Finwe kneels over you, weeping as you never
> saw him weep — not the proud king, but a father holding his dying child.
> "You should have stayed," he whispers. "You should have let the Jewels go."
> But you cannot. Even in death, even in this inverted memory, you cannot.

---

#### Run 6 Defeat — Ashes on Water

> The sea at Alqualonde is red. The ships at Losgar are embers. Between
> them stretches a road of ash across dark water, and upon it walks every
> Elf slain at your command — Teleri mariners, ice-broken Noldor of
> Fingolfin's host, unnamed, uncounted, their faces turned to you in silent
> accusation. You burn. They do not. That is the difference Morgoth did
> not mention.

---

#### Run 7 Defeat — The Fire Goes Out

> The flames of the Balrog-wounds spread inward, and with them comes a cold
> you have never felt — not winter's cold, nor Helcaraxe's grinding frost,
> but the cold of *ending*. Your fire, which burned since before you drew
> breath, which consumed your mother and father and wife and sons and an
> entire kindred — gutters. Dims. The ash-field darkens. Thangorodrim
> vanishes. For the first time since the making of Arda, the Spirit of
> Fire knows what it is to be cold.

---

## VI. Hero Victory and Defeat Messages

Each message is written from Feanor's dying perspective — the memory of that
person filtered through his fading consciousness. Victory messages are luminous;
defeat messages are bitter or regretful.

Victory messages should acknowledge silmaril count in their rendering. The text
references "the light you reclaimed" — the game engine should display the actual
number contextually.

---

### 1. Finwe

**Victory:**

> *Father.* You stood alone, and the door did not break while you lived.
> In this memory you see him clear: not the king who led the Noldor across
> the world, but the father who chose exile over a crown, who died rather
> than let the Enemy pass unchallenged. The Silmaril-light he guarded
> blazes through the memory, and for a moment Feanor sees his own face in
> his father's — the same fire, the same refusal to yield. The same doom.
> "I understand now," he whispers to no one. "I understand why you stayed."

**Defeat:**

> *Father.* You fell again, as you fell at Formenos, and this time there
> is no door to stand before, no treasure left to guard. The memory darkens
> and Finwe's face becomes a mask of stone, cold as Mandos, cold as the
> world you made when you lit those ships. "You were not worth it," the
> mask says. Feanor cannot argue. He was the most gifted of the Eldar, and
> he was not worth the life of one good king.

---

### 2. Ingwe

**Victory:**

> *High King.* Lord of Taniquetil, dweller in the Undying Light. Feanor
> remembers him standing in radiance that no Silmaril could match — not
> because the light was greater, but because Ingwe *was* the light, needing
> no vessel, no craft, no captured beauty. Only purity. Only peace. In this
> memory the reclaimed Silmaril-light seems dim beside Ingwe's grace, and
> Feanor understands at last what the Valar tried to tell him: that the
> greatest light is the one that gives freely, not the one that is kept.
> He understands. He does not repent.

**Defeat:**

> *High King.* Ingwe turns away, ascending Taniquetil in sorrow, and
> Feanor watches the purest light in Arda rise beyond his reach forever.
> He was offered that light. He chose instead to *make* light, to *own*
> it, to *hoard* it. The memory of Ingwe's back — tall, luminous,
> unhurried — is worse than any Balrog-wound. It is the image of everything
> he refused.

---

### 3. Nerdanel

**Victory:**

> *Beloved.* She is young again in this memory — laughing, bronze-flecked,
> fierce in argument, fiercer in love. They walked together beyond
> Valinor's borders, naming stones and stars, and she alone could match
> his fire without being consumed. The Silmaril-light that returns is warm
> in a way the Jewels never were in life, because it carries *her* warmth —
> the memory of hands that shaped beauty without possessing it. "You were
> right," he says. In death, he can say what pride forbade in life. "You
> were always right. I should have listened."

**Defeat:**

> *Beloved.* The sculpture falls from the shelf and shatters — her last
> work, the one she never finished, the one that bore his face as she
> remembered it before the Oath. In this failing memory Nerdanel does not
> weep. She is beyond weeping. She stands in Valinor's twilight, hands
> empty, watching seven fires pass beyond the Sea and knowing every one
> of them will go out. Her silence is louder than the Oath.

---

### 4. Mahtan

**Victory:**

> *Teacher.* The copper-bearded smith stands at his forge in Aule's light,
> and in this memory his face holds neither pride nor regret — only the
> quiet joy of craft rightly done. Feanor remembers learning the first
> secret of metal from those steady hands, the way fire could be persuaded
> rather than commanded, the patience that makes the hammer true. The
> Silmaril-light that blazes now is Mahtan's teaching perfected — and
> ruined — and perfected again. "You gave me everything," Feanor whispers.
> "I made of it what I am." The old smith nods. He does not say whether
> that is praise or condemnation.

**Defeat:**

> *Teacher.* The forge is cold. The copper beard is grey with ash. Mahtan
> lifts a flawed work from the anvil and turns it in his hands, and Feanor
> sees that it is shaped like a Silmaril — beautiful, luminous, cracked
> through the heart. "I should not have taught you," the old smith says,
> not in anger but in the weariness of one who has watched his greatest
> work become his greatest grief. The forge-light dies. The teaching was
> perfect. The student was the flaw.

---

### 5. Miriel Serinde

**Victory:**

> *Mother.* The face he has never seen. The voice he has never heard. Yet
> in this memory, bright with reclaimed Silmaril-light, she is there — not
> as she was in life, not as she is in Mandos, but as she exists in him:
> woven into the very thread of his making, her art and her sacrifice
> inseparable from his fire. He sees her hands upon the loom, silver and
> gold thread capturing the light of the Two Trees, and he understands at
> last that the Silmarils were not *his* invention — they were *her* legacy,
> the imprisoned light she died to kindle. "I am your son," he says to the
> face he has never kissed. "I am your unfinished work."

**Defeat:**

> *Mother.* The loom stands empty, threads trailing like abandoned veins,
> the pattern half-woven and incomprehensible. She is not here. She was
> never here. She poured everything into the fire that became him, and
> there was nothing left — no face, no voice, no comfort, only the ache
> of something missing that no craft could fill. He was born of sacrifice,
> and all he made of it was ruin. The loom unravels. The threads dissolve.
> Mother and son, separated by death before the first breath, reunited now
> only in the undoing of all things.

---

### 6. Fingolfin

**Victory:**

> *Brother.* You drew sword upon him in the Ring of Doom, and he did not
> draw back. You burned the ships at Losgar, and he crossed the Grinding
> Ice. You crowned yourself High King, and he bowed his head and waited.
> In this memory, bright with the Silmarils' recovered fire, Feanor sees
> what he could never see in life: that Fingolfin's patience was not
> weakness but a valor greater than his own — the courage to endure wrong
> without becoming wrongful. "You were the better king," Feanor admits to
> the memory. "But I was the greater flame. And flame does not kneel, not
> even to what it knows is right."

**Defeat:**

> *Brother.* The ships burn at Losgar, and across the dark water Feanor
> sees Fingolfin standing on the far shore, haloed by the fire's reflection,
> his face betraying nothing. No rage. No grief. Only the long, terrible
> patience of one who has been abandoned by his own blood and must now
> lead his people across the deadliest passage in the world. The ice of
> the Helcaraxe cracks and groans in Feanor's fading memory. How many
> died upon it? He never asked. He never wanted to know.

---

### 7. Finarfin

**Victory:**

> *Brother.* Youngest of Finwe's sons, golden-haired child of Indis, who
> walked with the host as far as the dark shore of Araman — and there
> heard the Doom of Mandos, and turned back. In this dying memory Feanor
> sees the moment clearly: Finarfin weeping, leading a remnant home to
> beg the Valar's pardon, choosing humility over pride. And the reclaimed
> Silmaril-light shows what Feanor refused to see then: that turning back
> was not cowardice but the bravest thing any of them did that night. The
> only one of Finwe's sons who truly defied the Oath — by refusing it.

**Defeat:**

> *Brother.* Finarfin walks away into the west, growing smaller with each
> step, taking the gentle light of Valinor with him. The darkness closes
> in. Feanor watches the last of his father's blood return to the peace
> he himself can never reclaim, and the bitterness is not toward Finarfin
> but toward himself — because he knows, in the honesty that death compels,
> that the youngest brother made the only wise choice of that terrible
> night, and that wisdom was always the one craft Feanor could not master.

---

### 8. Galadriel

**Victory:**

> *Niece.* Daughter of Finarfin, golden-haired, proud-eyed, who defied
> him to his face in Tirion when the rest kept silence. She crossed the
> Helcaraxe rather than accept passage on his stolen ships. In this
> memory, luminous with Silmaril-light, Feanor sees what he denied in
> life: that her pride was his own pride, unbroken, redirected, refined —
> the fire of the Noldor burning in her without the Oath's corruption.
> She will endure. She will outlast his sons and his Oath and his very
> name, bearing the fire of the Noldor without the corruption of the
> Jewels. "You are what I should have been," he whispers. "All my fire,
> and none of my ruin."

**Defeat:**

> *Niece.* She stands before him in Tirion, her eyes holding neither fear
> nor reverence, and speaks the words no one else dared: "You lead the
> Noldor to their doom, uncle, and not even your Silmarils are worth the
> blood-price." In the failing memory her voice sharpens to judgment, and
> Feanor cannot silence her, because the truth was always the one thing
> his fire could not burn. She crossed the Ice. She survived. She will
> outlast his sons, his Oath, and his very name. And she will *never*
> forgive him for Alqualonde.

---

### 9. Finrod Felagund

**Victory:**

> *Nephew.* Son of the brother who turned back, and yet Finrod went
> forward — not for the Oath, not for the Silmarils, but for the sheer
> love of Middle-earth's wild beauty, for friendship with mortals yet
> unmet, for a nobility that needed no jewels to prove itself. In this
> bright memory, warm with reclaimed light, Feanor sees the builder of
> Nargothrond — gentle, brave, singing in the dark — and knows that Finrod
> is the Noldor as they might have been, had one proud smith set down his
> hammer and listened to his wife. "You are your mother's line," Feanor
> says. "Not mine. Thank the Valar for that."

**Defeat:**

> *Nephew.* Finrod lies broken in a dark dungeon, his song spent, his body
> torn, having given everything — life, kingdom, hope — to keep a promise
> made to a mortal he barely knew. The Oath of Feanor drives his sons to
> murder for jewels; Finrod's oath drives him to die for a friend. The
> memory is a mirror Feanor cannot bear to face. All oaths are not equal.
> All fires are not the same. Some burn to possess. Some burn to protect.
> Finrod's light goes out in the darkness, and the darkness is ashamed.

---

## VII. Final Scene — The Oath Renewed

**This scene ALWAYS plays after the 7th run, regardless of outcomes.**

It consists of three parts: the Sight, the Curses, and the Command.

---

### Part 1: The Sight

> The memories fade. The light of Valinor, the ring of the hammer, the
> faces of the beloved and the wronged — all dissolve into the ash-grey
> present. Feanor opens his eyes.
>
> Thangorodrim.
>
> Three peaks like black teeth against the starless north, capped with
> perpetual fume, beneath which Morgoth sits enthroned in iron halls a
> thousand fathoms deep. The fortress is vast beyond comprehension, hewn
> from the bones of the world by a Vala's will and the labor of ten
> thousand slaves. Its shadow falls across all of Beleriand, and will
> fall across all the ages of the world until Powers greater than any
> Elf overthrow it.
>
> Feanor sees this. He sees it with perfect clarity — the clarity that
> comes only to the dying and the damned. No army of the Noldor will
> breach those gates. No siege will starve that darkness. No craft, no
> courage, no oath, no fire will ever be enough.
>
> He sees this. And his eyes burn brighter.

---

### Part 2: The Three Curses

> **"Moringotto!"**
>
> The name tears from him like a blade drawn from its own wound — Morgoth,
> Black Enemy of the World, who slew his father, who stole his Jewels,
> who poisoned everything he loved.
>
> **"Moringotto!"**
>
> Again, louder, a forge-cry, the Spirit of Fire spending the last of its
> fuel. The name echoes across the ash-field and breaks against the
> mountains of iron like a wave upon a cliff. Thangorodrim does not tremble.
> The darkness does not retreat. But the stars brighten — or seem to — as
> if Varda herself leans closer to hear.
>
> **"Moringotto!"**
>
> Three times, as is the custom of the great among the Eldar: once for
> injury, once for defiance, once for prophecy. For the name is not merely
> a curse but a *naming* — the true name Feanor gave the Enemy before all
> other names, the word that strips away "Melkor, He Who Arises in Might"
> and replaces it with "Dark Enemy of the World," and by that naming
> diminishes him forever.

---

### Part 3: The Command

*The following text adjusts slightly based on the campaign's total score, but
the Oath is ALWAYS upheld. The command is NEVER withdrawn. This is the
unbreakable core of the scenario.*

---

#### If total score is HIGH (many silmarils recovered across all runs):

> His sons kneel around him, seven flames in the darkness, and Feanor's
> dying eyes move from face to face — Maedhros the tall, Maglor the singer,
> Celegorm the hunter, Caranthir the dark, Curufin who is most like him,
> and the twins Amrod and Amras, still young, still bright, not yet broken.
>
> "I have seen," he whispers, and his voice is ash and iron. "I have seen
> the light of the Trees, and the love of your mother, and the face of my
> father upon the threshold. I have seen the High King in his radiance, and
> the ships burning, and the blood on the white quays. I have seen what I
> have made of this world."
>
> A pause. The fire within him dims and surges.
>
> "And I tell you: *the Oath holds.*"
>
> "Not because we shall prevail. Look upon that fortress — no power of
> ours shall overthrow it. Not because we are righteous. The blood of
> Alqualonde will never wash clean. But because we *swore*, by Iluvatar
> who is above all thrones, by the Everlasting Darkness that awaits the
> forsworn, and an oath so sworn cannot be broken and cannot be kept — it
> can only be *fulfilled*, in fire or in ruin, and we are children of fire."
>
> "Hold to the Oath. Avenge your father. Reclaim what was stolen, though
> it cost you everything, as it has cost me everything. Let the world call
> us proud, and mad, and doomed. We are the Noldor. We do not yield."
>
> "Not to Morgoth. Not to the Valar. Not to death."
>
> His body ignites. Not from without — from *within*. The Spirit of Fire,
> too great for any vessel of flesh, consumes itself at last. His sons
> stumble back from the heat, shielding their eyes, and when the flames
> die there is nothing upon the grey earth but ash, fine and pale as
> starlight, already scattering on a wind that blows from no direction.
>
> Feanor, mightiest of the Noldor, is gone. His like shall never appear
> again. But the Oath endures, and will endure, until the Silmarils are
> reclaimed or the world is broken.
>
> *The fire is spent. The oath is not.*

---

#### If total score is LOW (few or no silmarils recovered):

> His sons kneel around him, seven flames guttering in the dark, and
> Feanor's dying eyes — still proud, still burning — see in their faces
> the grief he put there. Maedhros, who will lose a hand. Maglor, who
> will wander weeping by the sea forever. Celegorm, who will die in
> treachery. Caranthir, consumed by wrath. Curufin, twisted by cunning.
> The twins, who will perish in the last desperate madness.
>
> He sees all of it. Death grants this much.
>
> "I have seen," he whispers, and his voice is a dying forge. "I have seen
> the beauty of Valinor, and the ruin I have made. I have seen the faces
> of those I loved and those I failed, and they are the same faces. I
> have seen the fortress of my Enemy, and I know — *I know* — that we
> shall never conquer it."
>
> His hand closes on empty air where a Silmaril should burn.
>
> "And still I say: *the Oath holds.*"
>
> "We are damned already. The Doom of Mandos lies upon us. We crossed the
> Ice and the Blood and the Fire to come to this grey country beneath
> sunless stars, and there is no road back. What remains? Only the Oath.
> Only the fire. Only the refusal to be less than what we swore to be,
> even if what we swore was our own destruction."
>
> "Hold to it, my sons. Let me be ash and let the wind take me, but do
> not let the flame die. Do not yield. Do not break. Arda will call you
> cursed, and Arda will be right, but we are the sons of fire and fire
> does not repent."
>
> His body ignites. The Spirit of Fire, too fierce for flesh, consumes
> itself in a blaze without fuel, a light without warmth. His sons shield
> their eyes. The heat is terrible and brief. When it passes, there is
> only ash upon the grey field, drifting northward on a bitter wind,
> toward Thangorodrim, toward the Enemy, toward the darkness that will
> swallow everything he loved and everything he made.
>
> Feanor is gone. The Oath remains.
>
> *The fire is spent. The oath is not.*

---

## VIII. Implementation Notes (For Future Coding)

### Campaign Structure
- **Run count:** 7 (hardcoded, representing the seven sons)
- **End trigger:** After 7th run completion, fire Final Scene
- **Score tracking:** Sum of all individual run scores
- **Hero pool:** Restricted to 9 specific characters (5 new + 4 existing)
- **Hero reuse:** Allow same hero in multiple runs (Feanor revisits memories)

### Story System Integration
- Memory Chapters (Type 0) trigger at run start, indexed by run number (1-7)
- Defeat Chapters (Type 1) trigger on hero death, indexed by run number
- Hero Victory/Defeat messages trigger on run completion, indexed by hero ID
- Final Scene triggers after Run 7 completion, with score-based text selection
- Opening Scene triggers once at campaign creation

### New Character Entries
- 5 new entries needed in `character.txt` (Finwe, Ingwe, Nerdanel, Mahtan, Miriel)
- New race/group entry in `race.txt` for "Feanor's Memory" scenario pool
- New story entries in `story.txt` for all narrative chapters
- New scenario flag/type to distinguish from base Glorfindel campaign

### New Flags Potentially Needed
- `SMT_MAHTAN` — unique smith flag for Mahtan (teacher of Feanor; perhaps
  reduced difficulty for teaching/knowledge-themed crafts)
- Scenario identifier flag to mark this as "Spirit of Fire" campaign
- Score-comparison display for end-of-campaign screen

### Character Stats Summary (New Heroes)

| Hero | Str | Dex | Con | Gra | Total | Power | Key Affinities |
|------|-----|-----|-----|-----|-------|-------|---------------|
| Finwe | 2 | 1 | 2 | 2 | +7 | 3 | WIL, MEL, PER |
| Ingwe | 0 | 0 | 1 | 5 | +6 | 3 | WIL, SNG, PER |
| Nerdanel | 0 | 1 | 1 | 3 | +5 | 2 | PER, WIL, SMT |
| Mahtan | 1 | 1 | 1 | 3 | +6 | 2 | SMT, PER, WIL |
| Miriel | -1 | 2 | -1 | 4 | +4 | 2 | SMT, PER, SNG |

---

## IX. Lore Consistency Notes

### Strict Adherences
- Feanor's death scene follows the Silmarillion exactly: sons bearing him back,
  sight of Thangorodrim, triple curse of Morgoth, command to uphold the Oath,
  body consumed by inner fire
- Finwe's death at Formenos is accurate: alone, defending the door
- Miriel's fate is accurate: exhaustion from bearing Feanor, passing in Lorien
- The Oath's wording echoes Tolkien's text from "Of the Flight of the Noldor"
- The kinslaying at Alqualonde and burning at Losgar are depicted accurately
- Ingwe's status as High King of all Eldar on Taniquetil is per Silmarillion
- Nerdanel's characterization draws from HoME XII ("The Peoples of Middle-earth")
- Mahtan's copper beard and connection to Aule are from HoME XII
- The Doom of Mandos is referenced accurately
- Feanor's naming of "Moringotto" (Morgoth) is the Quenya original per HoME

### Creative Liberties (Tolkien-Compatible)
- Miriel appearing in Feanor's deathbed vision — she died before he could know
  her, but Elvish spiritual bonds transcend physical meeting. In death, the
  barrier thins. This is consistent with Tolkien's metaphysics.
- The run-by-run memory progression is original but maps accurately onto the
  chronology of Feanor's life as described in the Silmarillion
- The "memory as gameplay" conceit is thematically consonant with Tolkien's
  emphasis on *recollection* as a central Elvish experience — Elves remember
  everything, and their greatest art is the preservation of memory
- The final command having two score-based variants maintains the *essential*
  element (the Oath is always upheld) while allowing the *tone* to shift — a
  Feanor who "succeeded" more speaks from defiant pride, while one who "failed"
  speaks from desperate resolve. Both are authentic to his character.

---

*The fire is spent. The oath is not.*
