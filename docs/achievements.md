# Achievements

Achievements work locally in every build. Open **Main menu > Achievements**
(keyboard shortcut **v** inside that menu). The list uses the existing book
layout, including wrapping, page navigation, scrolling, touch, and controller
controls. New unlocks appear in the message history and the popup notification.

On mobile, achievement text uses a density-scaled 24 logical pixel font
(36 with Big Font), with wrapping, pagination and scrolling. While an adventure
is active, its hero-specific achievements appear first under **For this
character**, once each. After the dungeon appears, a new adventure opens a
**Character achievements** panel showing that hero's deeds, icons, full
requirements and locked/unlocked status. The panel stays open until dismissed
with **Done**, the keyboard, or controller; long entries wrap, paginate and
scroll. Only non-hidden deeds and already earned hidden deeds appear in this
panel or its player message history reminders. Characters without visible
unique deeds skip the panel. Restoring a save or entering another level does not
repeat it. Debug, cheat and practice games omit it.

Steam currently omits ten achievements: **A Will
of Your Own**, **Release from Bondage**, **A Fool's Hope**, **The Hasty Stroke
Goes Oft Astray**, **Neither Voice Diminished**, **The Fords of Sirion**, **The
Oath He Should Have Sworn**, **Out of Thin Ore**, **The Ring-maker Overruled**
and **One Jewel Is Enough**. All ten remain earnable in every other build,
including Google Play, with their original provider IDs. The Steam book,
unlocks, profile import and synchronization ignore their stable bits; existing
shared profile records are preserved for other builds.
`src/achievement-steam-exclusions.h` controls both runtime and export exclusions.
The two new Blitz achievements bring the native catalogue to **112** and the
Steam candidates to **102**. Both Blitz entries are included for Steam; two
additional exclusions remain undecided. Until they are chosen, store exports
write Steam and Game Center candidates to explicitly named **draft** folders.
No existing Steam exclusions or published Google provider IDs were changed.

| Stable key / Steam API name | Name | Unlock condition | Hidden until earned | Category |
| --- | --- | --- | --- | --- |
| `THE_ROAD_GOES_EVER_ON` | The Road Goes Ever On | Reach a depth of 150 feet. | No | First Steps |
| `SOMETHING_TOOKISH` | Something Tookish | Purchase your first ability with experience. | No | First Steps |
| `FLIES_AND_SPIDERS` | Flies and Spiders | Break free of a web. | No | First Steps |
| `A_WILL_OF_YOUR_OWN` | A Will of Your Own | Resist a hostile effect in an opposed Will check. | No | First Steps |
| `FORGED_BY_YOUR_HAND` | While Hammers Fell | Complete a smithing project. | No | First Steps |
| `INTO_THE_DEEP` | To Dungeons Deep | Reach a depth of 500 feet. | No | Journeys |
| `LIGHT_IN_DARKNESS` | The Fire of Fëanor | Cut a Silmaril from Morgoth's Iron Crown. | No | Journeys |
| `BACK_TO_STARLIGHT` | The Quest of the Silmaril | Escape Angband carrying at least one Silmaril. | No | Journeys |
| `THERE_AND_BACK_AGAIN` | There and Back for Nothing | Enter Morgoth's throne room and return to the Gates alive without carrying any Silmarils. | Yes | Journeys |
| `THREE_LIGHTS_IN_EXILE` | Three Lights in Exile | Escape carrying all three Silmarils. | No | Journeys |
| `THE_QUIET_ROAD` | Not to Strike Without Need | Escape with a Silmaril without killing a creature during the adventure. Your traps and poison count. | No | Journeys |
| `WORD_UNBROKEN` | When the Road Darkens | Escape with a Silmaril after choosing an oath and keeping it unbroken. | No | Journeys |
| `A_BURGLARS_BUSINESS` | A Burglar's Business | Cut a Silmaril without waking Morgoth during the attempt. | Yes | Journeys |
| `HEAVY_IS_THE_STOLEN_CROWN` | Heavy Is the Stolen Crown | Escape with Morgoth's Iron Crown and at least one Silmaril, loose or still set in the Crown. | Yes | Journeys |
| `NO_STEPS_REQUIRED` | No Steps Required | From the start of the adventure, reach Morgoth's level using only chasms and down shafts for descent. Upward stairs are allowed. | No | Journeys |
| `THERE_AND_ALMOST_BACK_AGAIN` | There and Almost Back Again | Die at the Gates of Angband, on the surface. | Yes | Journeys |
| `SWIFT_AS_THE_WIND` | Swift as the Wind | Win a Blitz run by escaping with a Silmaril or defeating Morgoth. | No | Journeys |
| `TENFOLD_DOOM` | Tenfold Doom | Win a Blitz run with at least 10 net curse stacks (curses minus blessings). | No | Journeys |
| `NOT_EVEN_CARCHAROTH` | Not Even Carcharoth | Be killed by the first ordinary wolf you encounter before making your first kill. | Yes | Exploration and Terrain |
| `CABED_EN_ARAS` | Cabed-en-Aras | Successfully leap over a chasm and land safely. | No | Exploration and Terrain |
| `THE_FORDS_OF_SIRION` | The Fords of Sirion | Cross a stream from one dry bank to the other. | No | Exploration and Terrain |
| `HELCARAXE` | Helcaraxë | Freeze water with a cold-branded attack, then cross your ice from one bank to the other. | Yes | Exploration and Terrain |
| `FIRE_AND_WATER` | Fire and Water | Melt the ice beneath an enemy with fire. | Yes | Exploration and Terrain |
| `THE_FIRES_OF_THANGORODRIM` | The Fires of Thangorodrim | Leap across lava and survive its heat. | No | Exploration and Terrain |
| `THROUGH_NAN_DUNGORTHEB` | Through Nan Dungortheb | Cross a poison pool and reach dry ground alive. | No | Exploration and Terrain |
| `A_JOURNEY_IN_THE_DARK` | A Journey in the Dark | Enter a labyrinth and leave through a different passage. | No | Exploration and Terrain |
| `A_MAP_OF_THE_IRON_HELLS` | A Map of the Iron Hells | Enter labyrinth, chasm, ice, fire and poison regions within one run. | No | Exploration and Terrain |
| `THE_HALLS_OF_UTUMNO` | The Halls of Utumno | Reach the frozen ruins of Utumno. | Yes | Exploration and Terrain |
| `WHERE_GROND_WAS_WROUGHT` | Where Grond Was Wrought | Discover Orodruth, the Great Forge of Grond in Utumno. | Yes | Exploration and Terrain |
| `A_DESPERATE_VICTORY` | A Desperate Victory | Slay a hostile unique while at 25% HP or less. | Yes | Combat and Survival |
| `BY_A_THREAD` | Aurë Entuluva! | Kill an enemy while you have exactly 1 HP. | Yes | Combat and Survival |
| `A_FEW_LEAVES` | A Few Leaves in a Forest | Kill an enemy with a Deadly Hail empowered shot. | No | Combat and Survival |
| `TWO_WEAPONS` | Two Blows, One Doom | Kill an enemy with a Power Throw in which both attacks hit. | No | Combat and Survival |
| `OFT_EVIL_WILL` | Oft Evil Will Shall Evil Mar | Kill an enemy with a trap you rewired. | No | Combat and Survival |
| `THE_HASTY_STROKE` | The Hasty Stroke Goes Oft Astray | Kill an enemy with a Riposte. | No | Combat and Survival |
| `FOR_EXTERNAL_USE_ONLY` | For External Use Only | Kill an enemy with a thrown potion's effect, including its delayed poison. | No | Combat and Survival |
| `LIGHT_OF_THE_ELDAR` | Get Out, You Old Wight! | Defeat an undead enemy with a light-based attack. | Yes | Combat and Survival |
| `FLY_YOU_FOOL` | Fly, You Fool! | Knock a non-flying enemy into a chasm. It need not die. | Yes | Combat and Survival |
| `KNOCK_WITH_YOUR_HEAD` | Knock on the Doors with Your Head | Jam a lock while picking it, then bash that same door open. | Yes | Combat and Survival |
| `DOWN_TO_GOBLIN_TOWN` | Down, Down to Goblin-town | Survive a fall through a false floor or down a chasm. | Yes | Combat and Survival |
| `ATHELAS_OUT_OF_STOCK` | A Bitter Cure | Cure a disease with an Herb of Sickness when it is the correct cure. | Yes | Combat and Survival |
| `ANGBAND_MAINTENANCE` | Angband's Maintenance Department | Witness an enemy dismantle a trap you rewired. | Yes | Combat and Survival |
| `A_FOOLS_HOPE` | A Fool's Hope | Succeed at a skill check with a success chance of 10% or less. | Yes | Songs and Skills |
| `NO_LONGER_BINDING` | No Longer Binding | Break an equipment curse with Curse Breaking. | No | Songs and Skills |
| `A_ELBERETH_GILTHONIEL` | A Elbereth Gilthoniel! | Make an enemy flee with the Song of Elbereth. | No | Songs and Skills |
| `RELEASE_FROM_BONDAGE` | Release from Bondage | Clear rubble or disarm a trap with the Song of Freedom. | No | Songs and Skills |
| `TWO_THEMES_ONE_VOICE` | Two Themes, One Voice | Have both woven themes affect enemies in the same turn. | No | Songs and Skills |
| `YOU_CANNOT_PASS` | You Cannot Pass | Hold a door against a pursuer with the Song of Thresholds. | No | Songs and Skills |
| `FELAGUNDS_ANSWER` | Felagund's Answer | Permanently weaken an opponent with the Song of Contest. | No | Songs and Skills |
| `SONG_BENEATH_THE_MOUNTAIN` | The Lay of Leithian | Escape with a Silmaril and at least 20 trained Song. | No | Songs and Skills |
| `A_NAME_WORTH_REMEMBERING` | And Light They Caught | Create a custom artefact. | No | Craft and Treasures |
| `THEIR_WORK_HAD_NO_RIVAL` | Their Work Had No Rival | Create a custom artefact of smithing difficulty 65 or higher. | No | Craft and Treasures |
| `FIND_ANGRIST` | Angrist, the Iron-Cleaver | Angrist, Telchar's knife that Beren took into Angband: 'iron it would cleave as if it were green wood'. | Yes | Craft and Treasures |
| `FIND_GURTHANG` | The Black Sword of Túrin | Gurthang, the black sword that slew Glaurung and answered Túrin: 'I will drink thy blood gladly'. | Yes | Craft and Treasures |
| `FIND_GOTHMOG_AXE` | The Black Axe of Gothmog | Gothmog's black axe, which struck down Fingon in the Battle of Unnumbered Tears. | Yes | Craft and Treasures |
| `FIND_MELKOR` | The Spear of Melkor | The Great Spear of Melkor, the black weapon that pierced the Two Trees before Ungoliant drank their light. | Yes | Craft and Treasures |
| `FIND_TURGON_STAFF` | The Staff of the Hidden King | The Quarterstaff of Turgon, the 'staff of doom' before which Eöl faced judgment in Gondolin. | Yes | Craft and Treasures |
| `FIND_BELTHRONDING` | The Black Bow of Beleg | Belthronding, Beleg's bow of black yew, laid beside him after his death at Túrin's hand. | Yes | Craft and Treasures |
| `FIND_IDRIL_ROBE` | The Raiment of Silverfoot | The Robe of Idril Celebrindal, a remembrance of Silverfoot and the secret way that saved Gondolin's survivors. | Yes | Craft and Treasures |
| `FIND_FINGOLFIN_SHIELD` | The Shield of the High King | Fingolfin's blue, crystal-set shield, borne when he rode alone to challenge Morgoth. | Yes | Craft and Treasures |
| `FIND_ULMO_CLOAK` | The Mantle of the Lord of Waters | The cloak Ulmo gave Tuor at Vinyamar, concealing the bearer of his warning on the road to Gondolin. | Yes | Craft and Treasures |
| `FIND_CELEBRIMBOR_GLOVES` | The Hands of the Jewel-Smith | Celebrimbor's gloves, recalling Fëanor's grandson, whose greatest works would be the Three Rings of the Elves. | Yes | Craft and Treasures |
| `FIND_MELIAN_RING` | The Foresight of Melian | The Ring of Melian, a remembrance of the Maia whose Girdle sheltered Doriath from Morgoth. | Yes | Craft and Treasures |
| `FIND_NAUGLAMIR` | The Necklace of the Dwarves | Nauglamír, the Dwarven masterpiece made for Finrod, resting on its wearer 'as a strand of flax'. | Yes | Craft and Treasures |
| `NOT_THE_DOOM_OF_THE_SILMARILS` | Not the Doom of the Silmarils | Have Angrist shatter while trying to cut the second Silmaril. | Yes | Craft and Treasures |
| `EVER_FOUND_AGAIN` | Ever Found Again | Kill with Dailir, recover the arrow, and kill another enemy with it. | Yes | Craft and Treasures |
| `BITER_AND_BEATER` | Biter and Beater | Make an orc flee while wielding Orcrist and Glamdring together. | Yes | Craft and Treasures |
| `NO_PHIAL_REQUIRED` | When All Other Lights Go Out | Slay Shelob with Sting while wielding a Silmaril as your light. | Yes | Craft and Treasures |
| `THE_HIGH_CAPTAIN_FALLS` | The High Captain Falls | Slay Gothmog, High Captain of Balrogs. | Yes | Unique Victories |
| `DAGNIR_GLAURUNGA` | Dagnir Glaurunga | Slay Glaurung, the Deceiver. | Yes | Unique Victories |
| `THE_ABHORRED` | The Abhorred | Slay Gorthaur, Servant of Morgoth. | Yes | Unique Victories |
| `UNLIGHT_UNWOVEN` | Unlight Unwoven | Slay Ungoliant, the Gloomweaver. | Yes | Unique Victories |
| `THE_DRAGON_HOST_BROKEN` | The Mightiest of the Dragon-host | Slay Ancalagon the Black. | Yes | Unique Victories |
| `THE_RED_MAW` | The Red Maw | Slay Carcharoth, the Jaws of Thirst. | Yes | Unique Victories |
| `THE_CAT_AND_THE_NECROMANCER` | The Cat and the Necromancer | Defeat Tevildo and Gorthaur in the same adventure. | Yes | Unique Victories |
| `AT_MOST_SEVEN` | At Most Seven | Slay all seven named Balrogs within one adventure. | No | Unique Victories |
| `THE_SHADOW_FALLS` | Dagor Dagorath | Defeat Morgoth's final form. | Yes | Unique Victories |
| `NO_RINGS_ATTACHED` | No Rings Attached | Give a thrall the item they requested. | No | Quests and Tales |
| `TULKAS_LAST_LAUGH` | Tulkas Has the Last Laugh | Slay the unique enemy named by Tulkas and collect his artefact reward. | Yes | Quests and Tales |
| `AULES_APPRENTICE` | Apprentice of the Maker | Forge an item of difficulty 21 or higher for Aulë and receive his blessing. | Yes | Quests and Tales |
| `THE_DOOMSMANS_DUE` | The Doomsman's Due | Release Brodda's bound spirit for Mandos and receive his blessing. | Yes | Quests and Tales |
| `PITY_FOR_THE_FORSAKEN` | Pity for the Forsaken | Reach a down stair or shaft without a creature dying during Nienna's trial and receive her blessing. | Yes | Quests and Tales |
| `THE_GREAT_HUNT` | The Great Hunt | Complete Oromë's hunt and receive his blessing. | Yes | Quests and Tales |
| `A_STAR_AGAINST_THE_BALROG` | A Star Against the Balrog | Slay Duruin in his Bastion for Varda and choose a radiant gift. | Yes | Quests and Tales |
| `THE_LONG_TALE` | Grew in the Telling | Win a Tale with at least two different characters having completed adventures in it. | No | Quests and Tales |
| `ONE_JEWEL_IS_ENOUGH` | One Jewel Is Enough | As Beren, escape with exactly one Silmaril while Morgoth lives. | No | Heroes and Lore |
| `STRONGBOW` | Strongbow | As Beleg, slay a hostile unique with an arrow. | No | Heroes and Lore |
| `THREE_UNTOUCHED` | Three Untouched | As Celebrimbor, forge three separate custom artefact rings during one adventure, none evil or cursed. | No | Heroes and Lore |
| `NEITHER_VOICE_DIMINISHED` | Neither Voice Diminished | As Daeron, have both full-strength woven themes affect enemies in the same turn. | No | Heroes and Lore |
| `CLAD_IN_THE_NIGHT` | Clad in the Night | As Eöl, forge and wear your own galvorn armour. | No | Heroes and Lore |
| `LORD_OF_THE_FOUNTAIN` | Lord of the Fountain | As Ecthelion, slay Gothmog. | No | Heroes and Lore |
| `BEFORE_THE_RISING_OF_THE_SUN` | Before the Rising of the Sun | As Eärendil, slay Ancalagon the Black. | No | Heroes and Lore |
| `A_SINGLE_SPARK` | A Single Spark | As Fëanor, create an artefact using a forge's last charge. “His spirit burned as a flame.” | No | Heroes and Lore |
| `THESE_THINGS_ARE_MINE` | These Things Are Mine | As Fëanor, escape with all three Silmarils. | No | Heroes and Lore |
| `THE_OATH_HE_SHOULD_HAVE_SWORN` | The Oath He Should Have Sworn | As Fëanor, escape with a Silmaril and an unbroken Oath of Mercy. | No | Heroes and Lore |
| `HIS_FATHERS_LIGHT` | His Father's Light | As Fëanor, use Finwë's lamp to lift the curse on Fëanor's crown. | No | Heroes and Lore |
| `THE_KING_WHO_KEPT_HIS_OATH` | The King Who Kept His Oath | As Finrod Felagund, kill a werewolf with an unarmed attack. | No | Heroes and Lore |
| `SEVEN_TIMES_HE_CRIED` | Seven Times He Cried | Land at least seven damaging blows on Morgoth with Ringil, then escape with a Silmaril while he lives. | No | Heroes and Lore |
| `THE_GOLDEN_FLOWER_STANDS` | The Golden Flower Stands | As Glorfindel, slay a Balrog in close quarters beside a chasm, then leave the level alive. | No | Heroes and Lore |
| `OUT_OF_THIN_ORE` | Out of Thin Ore | As Gamil Zirak, forge mithril equipment without consuming mithril. | No | Heroes and Lore |
| `HURINS_DEFIANCE` | Húrin's Defiance | Resist a hostile mental attack at 25% HP or less when Indomitable makes the difference. | Yes | Heroes and Lore |
| `A_DREAM_UPON_THE_DARK_LORD` | A Dream Upon the Dark Lord | As Lúthien, put Morgoth to sleep with the Song of Lórien. | No | Heroes and Lore |
| `THURINGWETHIL_I_AM` | Thuringwethil I Am | As Lúthien, enter Morgoth's throne room wearing Thuringwethil's Bat-Fell. | No | Heroes and Lore |
| `ON_THE_OTHER_HAND` | More Deadly Than His Right | As Maedhros, kill with a Subtlety critical using a hand-and-a-half weapon. | No | Heroes and Lore |
| `IRON_CLEAVER` | Iron-Cleaver | As Telchar, forge a blade that ignores all armour and cut a Silmaril with that same blade. | No | Heroes and Lore |
| `FOR_IDRIL_AND_EARENDIL` | For Idril and Eärendil | As Tuor, slay Maeglin, Betrayer of Gondolin. | No | Heroes and Lore |
| `MASTER_OF_DOOM` | Master of Doom | As Túrin Turambar, slay Glaurung. | No | Heroes and Lore |
| `DOOM_CAN_WAIT` | Doom Can Wait | As Túrin, resist entrancement through your unique power and kill the attacker before the resulting rage ends. | No | Heroes and Lore |
| `HIS_DEATH_AND_FINAL_END` | His Death and Final End | As Túrin, slay Morgoth's final form with Gurthang. | No | Heroes and Lore |
| `BELEGOST_REMEMBERS` | Belegost Remembers | As a Dwarf, slay Glaurung with Azaghâl's dagger. | No | Heroes and Lore |
| `HEIRLOOMS_OF_THE_HIDDEN_KING` | Heirlooms of the Hidden King | As a Man, escape with a Silmaril, Narsil and the Ring of Barahir. | No | Heroes and Lore |
| `THE_RING_MAKER_OVERRULED` | The Ring-maker Overruled | Permanently weaken Gorthaur with the Song of Contest while wearing the Ring of Mairon. | Yes | Heroes and Lore |

The original 110 achievements preserve all previous 96 stable keys and native indexes;
the reviewed additions occupy indexes 96–109; the unassigned provisional sword
slot at index 99 now recognizes Gurthang instead of Celeg Aithorn. Sixty are visible,
including introductory actions, ordinary mechanic goals and character-specific
deed plans. Fifty are hidden, including all twelve rare finds, general unique
victories, Helcaraxë, Fire and Water, Fly, You Fool! and A Desperate Victory.
Three Lights in Exile, At Most Seven and Seven Times He Cried are visible from
the start. Earned achievements are always shown.

There and Back for Nothing replaces the previous return-trip condition. It
requires entering Morgoth's throne room and reaching the Gates alive with zero
Silmarils carried, including jewels still in a carried Crown. Ordinary escapes
with jewels no longer unlock this goal. It does not change the game's normal
ascent rules: the check concerns the load carried on returning to the Gates.
The existing native key and provider identity are preserved.

Heavy Is the Stolen Crown accepts all four Iron Crown variants. The end-of-run
count already sums loose Silmarils and the one, two or three jewels still set
in the Crown, across equipment, the legacy inventory and expandable carried
entries. A filled Crown qualifies without a loose jewel; an empty Crown needs
at least one loose jewel. Two embedded jewels plus one loose jewel count as
three for Three Lights in Exile and the normal ending/scoring logic.

Display order follows `src/achievement-order.h`, independently of native indexes.
The nine categories are First Steps, Journeys, Exploration and Terrain, Combat
and Survival, Songs and Skills, Craft and Treasures, Unique Victories, Quests and
Tales, and Heroes and Lore. The table above follows that order. Existing save/provider identities
are never renumbered to reorder the book. Steam CSV rows and Google's display-
order values use this presentation order; Java/native mask arrays retain the
original stable index order.

Open **Show hidden requirements [R]** in the viewer to inspect every condition
and icon temporarily. This changes only the current book view; it neither earns
an achievement nor sends a provider reveal request. The clickable action works
with the shared book controls. **Hide hidden requirements [R]** restores the
normal presentation.

Locked visible entries show current progress where useful: five region visits,
seven named Balrogs, three artefact rings and Ringil blows within the current
run; completed characters within the Tale; and the route, pacifist and oath
conditions. Region visits survive level travel, teleportation and save/resume,
then reset on a new run, including another character in the same Tale. Old
Tale-wide region history never supplies visits to the new run counter. The five
bits use unused space in the existing saved discovery flag word; the save's
byte layout is unchanged. Saves without adventure tracking show the applicable
progress as unavailable.

Google's published initial states cannot be changed. The original 14 retain
their published revealed states; the following 40 retain their original Console
states (36 hidden, four revealed), even where the local viewer now reveals a
goal for planning. Existing provider IDs and unlocks are preserved. The newest
56 entries have no assigned provider IDs yet. New-project exports follow the
current 60-visible/50-hidden catalogue policy. Existing-project imports contain
only those 56 unmapped entries.

The in-game list displays existing game sprites beside each revealed entry,
including in ASCII map mode. `src/achievement-icons.h` is the shared mapping
for the book and exported artwork. The book renders the complete two- or
three-sprite compositions, using a compact triangle for narrow three-sprite
icons so their count remains visible. Entries use one paragraph each so the whole catalogue fits
the book's 128-paragraph capacity. Wrapping, pagination and scrolling retain the
chosen font size on narrow screens.

Icon choices consider the Tolkien title as well as the unlock condition.
The 43 approved compositions are defined in
`src/achievement-icon-compositions.json` and generated by
`scripts/sync_achievement_icons.cjs`. They reuse existing game sprites in
64px atlas squares starting at row 64, below the runtime quartz overlays.
All original gameplay pixels remain unchanged. The book and both store
exports use those same finished compositions, preserving overlays, triangles,
and relative sprite sizes rather than rebuilding them as separate tiny tiles.
The book retains its existing reading gutter and font size.

The Fire of Fëanor shows two Silmarils still in the Iron Crown and one freed
beside it; only the achievement's copy of the crown is altered. Flies and
Spiders uses Attercop, Not Even Carcharoth uses an ordinary wolf alone, and
water, ice and lava crossings share the traveller-on-terrain design. No Steps
Required shows a tumbling traveller entering a dark shaft, without a direction
glyph. A Journey in the Dark uses the Brass Lantern and a map. A Desperate
Victory uses a sword and a quarter-full native HP meter. Get Out, You Old Wight!
pairs the sunlight floor patch (light +3) with a wight. Maintenance uses an
ordinary Orc soldier with a trap. Melkor's spear is paired with Morgoth,
Ulmo's cloak is placed on water, and Varda's quest pairs Varda with Duruin.
Three Lights in Exile and Three Untouched use compact triangles; Neither Voice
Diminished shows two music symbols, and Seven Times He Cried includes Ringil,
Morgoth and the number seven.

Unchanged entries retain their existing atlas sprites and compositions.
Water, ice and lava tiles in row 32 and the native violet fleeing marker are
maintained by `scripts/sync_achievement_terrain_icons.cjs`. Google-only accents
remain only for unchanged legacy entries that need distinct exported images.
The exporter verifies that all 112 Google images are distinct. Regenerate the
approved compositions with `node scripts/sync_achievement_icons.cjs`, or append
`--check` to verify the shipped image and mappings without changing files.

Ordinary scored adventures qualify, including the contextual gameplay
tutorial. Wizard/debug runs, active cheats, recorded cheating, and the separate
practice tutorial (`game_type != 0`) cannot earn achievements. Previously
earned achievements remain visible while playing these modes.

The burglary and Beleg/Beren deeds use completed gameplay outcomes. **A Burglar's Business**
requires living Morgoth asleep before and after a successful Silmaril cut,
then still asleep after the next player-noise perception pass before further
input. Any intervening wake cancels that attempt, even if he is sung back to
sleep; earlier attempts do not disqualify a later success. Pending confirmation
clears on a new run or level travel. **Strongbow** requires Beleg's fired arrow
to deliver the killing blow to a hostile unique; thrown items, melee and delayed
poison do not qualify. Beleg and Beren are matched by their stable character GUIDs.
**One Jewel Is Enough**
requires Beren to escape carrying exactly one Silmaril while Morgoth remains
alive. These three entries need no additional persistent run state or save-format
change; the key-based achievement profile remains version 1. The burglary deed
remains hidden; Beleg's and Beren's goals are visible for planning.
Their store icons combine the existing dagger and Silmaril, longbow and arrow,
and single Silmaril and human traveller sprites, respectively.

## Reviewed conditions and new treasures

**Aurë Entuluva!** requires a player-attributed kill of a hostile enemy while
currently at exactly 1 HP. It has no earlier-hit, healing-history or same-level
requirement. Peaceful victims and other killers do not qualify. **Fire and Water**
accepts player, monster or environmental fire melting ice beneath a hostile
enemy. Monster cold still does not grant player-created ice for Helcaraxë.
**Iron-Cleaver** describes Telchar's effect as ignoring all armour, rather than
using the internal Sharpness II name. **The Lay of Leithian**
requires at least 20 trained Song and escape with a Silmaril; it has no singing-
turn quota.

**Helcaraxë** uses the existing cold-brand interaction: a successful melee,
throwing or arrow hit can freeze water on its target's square. Ringil is one
actual source. Cross that player-created ice between banks; naturally frozen
or monster-created ice alone does not qualify. **The Golden Flower Stands**
retains its close-quarters requirement: Glorfindel must be adjacent to the
Balrog and beside a chasm when the Balrog dies, then leave that level alive.

**The Great Hunt** continues to accept any one of Oromë's four totals: 100 wolves,
80 spiders excluding hatchlings, 60 serpents or 30 vampires. Completion of one
kindred is sufficient. Quest dialogue, supplemental messages and reward text
all describe that existing rule; no species is assigned or made mandatory.

The selected artefact discoveries cover twelve item types: Angrist, Gurthang,
the Great Axe of Gothmog, the Great Spear of Melkor, the Quarterstaff of Turgon,
Belthronding, the Robe of Idril Celebrindal, the Kite Shield of Fingolfin, the
Shadow Cloak of Ulmo, the Gloves of Celebrimbor, the Ring of Melian and Nauglamír.
All twelve discovery achievements are hidden and use the same identification
hook. Gurthang replaces the unassigned provisional Celeg Aithorn choice, keeping
one discovery per selected item type. The eight other newly selected finds are
appended at indexes 102–109; all assigned provider identities remain unchanged.

The descriptions explain the treasures through Tolkien's histories and their
legendary owners, rather than through numerical bonuses or abilities. Angrist
recalls Beren's quest; Gurthang, Túrin and Glaurung; Gothmog's axe, the death of
Fingon; Melkor's spear, the destruction of the Two Trees; Belthronding, Beleg's
burial; Idril, Gondolin's escape-way; Fingolfin, his lone challenge to Morgoth;
Ulmo, Tuor's concealed journey; Celebrimbor, the future Three Rings; Melian, the
Girdle of Doriath; Nauglamír, Finrod and Dwarven craft; and Turgon, the judgment
of Eöl. Four short quotations are taken from The Silmarillion, with book
references and corroboration recorded below and in the review PDF.

The robe, gloves and ring are game-specific relics whose descriptions invoke
canonical character deeds. The source passages attest the owners and deeds,
not those three named objects. The quarterstaff adapts Turgon's staff of doom.
Finding a treasure requires identification, not reenacting the historical deed.

| Discovery | Literary source | Cross-reference |
| --- | --- | --- |
| Angrist | The Silmarillion, ch. 19, Of Beren and Lúthien | [Angrist](https://tolkiengateway.net/wiki/Angrist) |
| Gurthang | The Silmarillion, ch. 21, Of Túrin Turambar | [Gurthang](https://tolkiengateway.net/wiki/Gurthang) |
| Great Axe of Gothmog | The Silmarillion, ch. 20, Of the Fifth Battle: Nirnaeth Arnoediad | [Gothmog](https://www.encyclopedia-of-arda.com/g/gothmog.php) |
| Great Spear of Melkor | The Silmarillion, ch. 8, Of the Darkening of Valinor | [Melkor](https://tolkiengateway.net/wiki/Melkor) |
| Quarterstaff of Turgon | The Silmarillion, ch. 16, Of Maeglin | [Turgon](https://tolkiengateway.net/wiki/Turgon) |
| Belthronding | The Silmarillion, ch. 21, Of Túrin Turambar | [Belthronding](https://tolkiengateway.net/wiki/Belthronding) |
| Robe of Idril Celebrindal | The Silmarillion, ch. 23, Of Tuor and the Fall of Gondolin | [Idril](https://tolkiengateway.net/wiki/Idril) |
| Kite Shield of Fingolfin | The Silmarillion, ch. 18, Of the Ruin of Beleriand and the Fall of Fingolfin | [Fingolfin](https://tolkiengateway.net/wiki/Fingolfin) |
| Shadow Cloak of Ulmo | The Silmarillion, ch. 23, Of Tuor and the Fall of Gondolin; Unfinished Tales, Of Tuor and his Coming to Gondolin | [Ulmo](https://tolkiengateway.net/wiki/Ulmo) |
| Gloves of Celebrimbor | The Silmarillion, Of the Rings of Power and the Third Age | [Celebrimbor](https://tolkiengateway.net/wiki/Celebrimbor) |
| Ring of Melian | The Silmarillion, ch. 4, Of Thingol and Melian; ch. 10, Of the Sindar | [Melian](https://tolkiengateway.net/wiki/Melian) |
| Nauglamír | The Silmarillion, ch. 13, Of the Return of the Noldor | [Nauglamír](https://tolkiengateway.net/wiki/Nauglam%C3%ADr), [quoted passage](https://thainsbook.minastirith.cz/objects.html#Nauglamir) |

These are fixed named treasures with random-find routes. Calris and the other
fixed boss trophies remain excluded. Their template rarity fields are selection
weights: lower means rarer among eligible candidates, not an absolute chance.
The eight appended discoveries have 10 Play points each, leaving room for the
remaining item types within the provider's total-point allowance.

The full review PDF is generated from the same catalogue, display order and
shared sprite map by `scripts/export_achievement_pdf.py`. It reveals all names,
icons and descriptions, labels every entry Hidden or Visible, and numbers the
complete catalogue in the in-game display order. Its default output is
`output/pdf/sil-more-achievements.pdf`.

Smithing, item descriptions and drop selection now share the intrinsic evaluator
in `src/object/object-difficulty.c`. Celeg Aithorn is 50, the spear 55 and
Nauglamír 46 for a neutral smith. Negative attributes, negative skills, drawbacks,
curses, intrinsic base powers and ENCHANTABLE all enter the same calculation.
Hero discounts and paid alloy benefits remain explicit crafting adjustments;
resource fees are collected separately. The earlier forge values of 72, 116 and
67 came from a divergent implementation which skipped artefact drawbacks and
only recognized base-kind ENCHANTABLE. Those discrepancies were bugs.

The Python calculator mirrors the shared evaluator, including C's integer
division. It now retains artefact-only base kinds and uses the first matching
kind, as the engine does. The numeric comments in `lib/edit/artefact.txt` are
generated annotations, not inputs to gameplay; stale annotations had survived
changes to items and difficulty rules. Use
`python scripts/calc_artefact_difficulty.py --check-comments` to detect drift and
`--update-comments` to refresh only the existing annotations. The selected discovery set above uses these corrected comparisons.

After a standard build, `python scripts/check_smithing_difficulty.py` compares
the actual fixed artefacts and drop catalogue with Python, rejects an old cache,
checks preserved resource fees, and completes four negative-attribute projects
through the production Accept, Resume and work commands. The smaller
`python scripts/check_calc_artefact_difficulty.py` checks parser and arithmetic
edge cases without compiling an engine harness.

A find counts when the player actually identifies it in inventory or on a visible
floor. Merely generating it out of sight does not count, and an identically
named custom artefact does not substitute for the fixed treasure. Existing
identified carried finds can qualify when observed in the new build; wizard,
practice and postmortem discoveries cannot.

**Their Work Had No Rival** requires a newly created custom artefact of actual
smithing difficulty **65 or higher**, after costs and delivery succeed. A normal
item, an unfinished project or a reforge does not qualify. Delivery to the floor
counts if it succeeds. Measuring difficulty preserves the project's agreed
smithing costs. **A Desperate Victory** replaces First Blood: slay a hostile
unique while alive at 25% HP or less at the killing blow. Ordinary enemies,
nonlethal hits, ambient or monster kills and peaceful victims do not count.
The 25% boundary uses exact integer comparison; Morgoth's phase transition is
excluded while his final defeat qualifies. This hidden goal is in Combat and
Survival. The three revised entries retain their unpublished native positions;
the original 96 keys and all assigned provider identities remain unchanged.

Save version **0.9.8.39** stores the seven-Balrog adventure mask. Older saves
start it empty, preserving their other lore state. Their pending comeback
windows are cleared because old versions allowed healing inside the window.
Existing earned achievement unlocks are retained.

## Titles from Tolkien's text

Quotation-based titles and explicitly retained names keep their wording.
The audit records book provenance without replacing a quotation with a
mechanical paraphrase:

| Titles | Book passage |
| --- | --- |
| To Dungeons Deep; While Hammers Fell; And Light They Caught | *The Hobbit*, An Unexpected Party; the Dwarves' song. [Text and references](https://tolkiengateway.net/wiki/Far_over_the_misty_mountains_cold) |
| The Fire of Fëanor; Thuringwethil I Am | *The Lays of Beleriand*, The Lay of Leithian, Canto XIII. [Thuringwethil passage](https://www.silmarillionwritersguild.org/reference/references/pdf/thuringwethil.pdf) |
| When the Road Darkens | *The Fellowship of the Ring*, The Ring Goes South; Gimli's discussion of loyalty and oaths. |
| Get Out, You Old Wight! | *The Fellowship of the Ring*, Fog on the Barrow-downs. [Reference](https://tolkiengateway.net/wiki/Get_Out%21_You_old_Wight%21_Vanish_in_the_Sunlight%21) |
| Knock on the Doors with Your Head | *The Fellowship of the Ring*, A Journey in the Dark. |
| You Cannot Pass | *The Fellowship of the Ring*, The Bridge of Khazad-dûm. |
| A Few Leaves in a Forest | *The Two Towers*, Helm's Deep; Legolas's archery passage. |
| Oft Evil Will Shall Evil Mar | *The Two Towers*, The Palantír. |
| The Hasty Stroke Goes Oft Astray | *The Return of the King*, The Passing of the Grey Company. |
| A Fool's Hope | *The Return of the King*, The Siege of Gondor. |
| Grew in the Telling | *The Lord of the Rings*, Foreword to the Second Edition. |
| A Dream Upon the Dark Lord | *The Silmarillion*, Of Beren and Lúthien. |
| Not to Strike Without Need | *The Fellowship of the Ring*, The Shadow of the Past; Gandalf's explanation of pity and mercy. |
| More Deadly Than His Right | *The Silmarillion*, Of the Return of the Noldor; Maedhros learns to wield his sword with his left hand. |
| When All Other Lights Go Out | *The Fellowship of the Ring*, Farewell to Lórien; Galadriel's gift of the Phial. |
| The Mightiest of the Dragon-host | *The Silmarillion*, Of the Voyage of Eärendil and the War of Wrath; Ancalagon's fall. |
| A Single Spark (description) | *The Silmarillion*, Of Eldamar and the Princes of the Eldalië; Fëanor's spirit burns as a flame. |
| His Death and Final End | The Second Prophecy of Mandos in *The Lost Road*, Quenta Silmarillion conclusion. |
| At Most Seven | *Morgoth's Ring*, Annals of Aman, section 50 marginal note. |
| Their Work Had No Rival | *The Silmarillion*, Of the Sindar; exceptional Dwarven mail-making. [Passage](https://martijn.coppoolse.com/books/part/1217) |
| The Necklace of the Dwarves | Nauglamír's established name in *The Silmarillion*. [References](https://tolkiengateway.net/wiki/Nauglam%C3%ADr) |

There and Back Again, Flies and Spiders, Fire and Water, and A Journey in the
Dark are Tolkien book or chapter titles. Fly, You Fool! deliberately adapts the
book's plural line to one victim. Seven Times He Cried and Ever Found Again
retain their existing wording and their Ringil/Dailir connections. No Rings
Attached and Not Even Carcharoth retain their humour; Heirlooms of the Hidden
King retains **As a Man** in its condition. Their names are not rewritten merely
to restate the achievement condition.

## Unique kills, historical deeds and quests

The six new unique victories focus on the principal First Age enemies; Morgoth
already has **Dagor Dagorath**. Confirmed player kills include normal combat,
projectiles, player-attributed poison, traps and lethal knockback. Ambient damage
and kills by other monsters do not count. A hero can earn the general unique
victory and the corresponding historical deed from the same kill. Heroes match
the selected character template's stable GUID; borrowing a hero's equipment
or abilities does not replace selecting that hero. Ecthelion's template spelling is corrected
without changing its index or GUID.

The four historical pairings follow the published *Silmarillion*: Ecthelion and
Gothmog, and Tuor and Maeglin, in chapter 23, “Of Tuor and the Fall of Gondolin”;
Túrin and Glaurung in chapter 21, “Of Túrin Turambar”; Eärendil and Ancalagon in
chapter 24, “Of the Voyage of Eärendil and the War of Wrath”. Ecthelion's deed
requires the kill, without requiring his death; Tuor's requires the right victim,
without requiring a chasm. No particular weapon or time of day is required.
Online reference summaries and their book citations:
[Ecthelion](https://tolkiengateway.net/wiki/Ecthelion),
[Glaurung's death](https://tolkiengateway.net/wiki/The_Death_of_Glaurung),
[the fall of Gondolin](https://tolkiengateway.net/wiki/Of_Tuor_and_the_Fall_of_Gondolin),
[Ancalagon](https://tolkiengateway.net/wiki/Ancalagon_the_Black).

Further researched possibilities, not additional unlock conditions in this set:

| Coincidence | Source and qualification |
| --- | --- |
| Túrin slays Brodda | *The Children of Húrin*, “The Return of Túrin to Dor-lómin”; [reference](https://tolkiengateway.net/wiki/Brodda). The current Mandos quest makes this an especially apt repeated encounter. |
| Azaghâl faces Glaurung | *The Silmarillion*, ch. 20; [reference](https://tolkiengateway.net/wiki/Glaurung). He wounds and repels the dragon but dies; slaying it would change the story. |
| Fingolfin wounds Morgoth with Ringil | *The Silmarillion*, ch. 18; [reference](https://tolkiengateway.net/wiki/Fall_of_Fingolfin). A victory would reverse his historical fate. |

Attribution matters: Huan kills Carcharoth and Draugluin; Glorfindel's Balrog and
Finrod's last werewolf are unnamed. Lúthien and Huan defeat Gorthaur/Sauron but
release him after he yields. The generic Gorthaur kill is a game victory, without
claiming that Sauron was historically or permanently slain.

Each of the six Valar achievements fires in the successful reward-delivery path,
after the quest's actual completion checks. Acceptance, unfinished objectives,
failed quests, loaded pending rewards and cancellation of Varda's gift choice do
not unlock one. Claiming a reward again never announces a second unlock. The
existing **No Rings Attached** continues to cover fulfilling a thrall's request.
The key-based achievement profile remains version 1.

The 19 accepted lore achievements are appended after the original 74. They use
shipped game sprites; the reviewed visibility flags appear in the table above.
Rejected proposals are not in the catalogue;
no Narya, Nenya, Vilya or One Ring item is introduced.

Save version **0.9.8.38** persists Ringil's seven damaging blows, the Tevildo and
Gorthaur kills, Dailir's kill/recovery sequence, Glorfindel's pending survival
condition and Celebrimbor's three distinct forged artefact IDs. Older saves start
these counters empty. A reforge of the same ring does not count again, and ordinary,
evil or cursed rings do not count for **Three Untouched**. It requires three
separately created artefact rings within one adventure as Celebrimbor.

Weapon-specific deeds use the actual synchronous weapon damage source. Merely
wearing Gurthang, Sting or Azaghal's dagger while poison, a spell, a trap or another
weapon kills the victim does not qualify. Ringil requires seven damaging blows;
absorbed blows do not count. **Ever Found Again** requires a confirmed floor pickup
between Dailir's kills. **The Golden Flower Stands** is awarded on committed travel
away from the cliff-duel floor while Glorfindel is alive. **At Most Seven** counts
each of the seven named Balrogs once within one adventure. Its seven-bit mask
is saved with the character and resets at the next adventure; Tale history never
seeds the new mask.

The four final replacement titles are **Not the Doom of the Silmarils**, **The King
Who Kept His Oath**, **Thuringwethil I Am**, and **His Death and Final End**. The last
references Turin's black sword in the Second Prophecy of Mandos; it describes the
future Last Battle, not a First Age victory in the published Silmarillion.


Of the four journey/discovery entries, **No Steps Required** is visible for
planning; the other three retain their hidden flags. **No Steps Required** is awarded
on a living arrival at Morgoth's level (1000 ft), with every downward transition
since birth made through a chasm or down shaft. Ordinary down stairs, false
floors, horn-created falls and downward level teleportation disqualify the run.
Upward travel is allowed, and cancelling a stair prompt does not spoil the run.
A down shaft immediately above Morgoth may travel only one level and still counts.
Save version **0.9.8.37** stores positive route eligibility in the existing
achievement flags word; older saves start ineligible because their route is
unknown. The serialized block size stays unchanged. Invalid routes remain
ineligible after save/resume.

**There and Almost Back Again** requires an actual lethal hit at depth 0, the
Gates/surface. Last-chance equipment is resolved first. Escape, victory, quitting
and the suicide command do not count as this death.

**The Halls of Utumno** unlocks on a living arrival at the optional Utumno level,
including a legitimate saved character already there. **Where Grond Was Wrought**
requires seeing Orodruth's unique forge tile on the Forge of Grond level. Entering
the forge level's landing, remembering an unseen tile or seeing an ordinary forge
does not meet that condition. These discoveries require no combat or smithing.

The catalogue's naming pattern mixes specific book references and epithets with
original humour tied to the deed. Names such as **Dagnir Glaurunga**, **Lord of
the Fountain** and **The Red Maw** retain the specific-reference side. Quest
names such as **The Great Hunt** and **Apprentice of the Maker** are more general;
their descriptions state the deed or quest and its reward. **No Steps Required** uses original mechanical humour;
**There and Almost Back Again** riffs on the existing journey title. **The Halls
of Utumno** names the discovered place, and **Where Grond Was Wrought** refers to
the authored Forge of Grond setpiece. These new names are not presented as quotes.


## Local progress and synchronization

Progress lives in `ANGBAND_DIR_APEX/achievements.json`, outside character and
Tale files. This is `<user-root>/meta/achievements.json` in standard builds and
`lib/apex/achievements.json` in portable builds. Death, a new character, and a new
Tale do not clear achievements. Character saves now store run-wide kill,
singing and comeback history in save format
**0.9.8.31**. The expansion adds persistent door-jam provenance, pending-fall source depth
and thrown-potion poison attribution in save format **0.9.8.34**. Save format
**0.9.8.35** adds discovery crossing and labyrinth progress, player-created ice,
first-wolf tracking, and distinct rage episode provenance. Earlier saves still
load with safe defaults; they do not invent an unknown first-wolf encounter or
past discovery history.
The separate achievement file keeps schema version 1 and preserves all
previously unlocked achievement keys.

Unlocks are written immediately through a temporary file, flushed, closed, and
renamed over the previous file. Failed writes retain the previous file and the
in-memory unlock, and retry periodically. An unreadable, corrupt, oversized,
or unsupported-version file is preserved without overwriting it; the viewer
reports that progress could not be saved. Back up and repair/move that file if
this occurs. Unknown fields and unknown achievement keys in version 1 files
survive writes.

The local file doubles as the durable platform retry queue: every session
reconciles it with available services. Platform failures never revoke local
achievements or interrupt an adventure. Remote unlocks are merged into local
progress without replaying the local unlock notification. Dates show when this
device first recorded an unlock; imported platform unlocks use the import date.

This is one local profile per OS/app data directory. Its progress is merged
with the platform account currently using that profile. There is no new game
account, account selection UI, or direct cloud service between stores. Moving
the same local file between installations carries these milestones with it.

Existing normal character saves can establish **To Dungeons Deep** from their
recorded maximum depth on entering the dungeon. They can earn all event-based
milestones going forward, but **Not to Strike Without Need** requires a new character whose
entire kill history was tracked. Old saves never assume that an unknown kill
count means zero. Singing starts accumulating from the upgrade onward.

Player-origin poison damage retains its source through save/resume, so a delayed
poison kill disqualifies a pacifist run. Monster and environmental poison kills
do not. Lethal player knockbacks into chasms also count as player kills; monsters
that survive their fall do not. Poison's total damage and decay are unchanged.
The legacy comeback save fields remain compatible but no longer gate the 1-HP
kill achievement. Free menu actions and song turns whose full Voice cost cannot be
paid do not advance singing progress. A sustained song's normal zero-cost ticks
still count. Combat deeds from Morgoth's final killing blow are recorded even
when the victory sequence has already closed the adventure.

**Grew in the Telling** records completed ordinary adventures by race/template in the
current Tale's local progress. Repeated finalization or Eru resurrection of the
same template cannot count twice. Living autosave snapshots and Blitz games do
not count. Only an actual Tale victory awards it; bookkeeping for finished Tales
is removed. Historical completed adventures from before this feature are not
inferred from death totals or old score snapshots. Defeated Tales also discard
their bookkeeping. Profiles written above the original 64 KiB limit can now
load up to 4 MiB; writes use compact JSON and never exceed that read limit.
Optional Tale history has a 48 KiB budget. If older abandoned records fill it,
the oldest records outside the current Tale are discarded; this can lose partial
Long Tale progress for an older unfinished Tale, but never removes earned
unlocks or invents completed adventures. The current Tale's character set is
retained. Unknown extension fields remain preserved; if they alone prevent a
profile from fitting, saving reports failure and retains the previous file.

## Blitz completion achievements

**Swift as the Wind** and **Tenfold Doom** are appended stable IDs 110 and 111.
They are one-shot profile achievements. Winning means a finalized Silmaril
escape or Morgoth victory; death, an empty-handed escape, abandonment and a run
still in progress do not count. Debug, practice, cheat and previously tainted
runs remain ineligible. Saves without the persisted tracked-run provenance do
not invent a previously completed win.

The harder achievement sums all 64 signed runtime curse slots: positive curse
stacks minus negative blessing stacks, including repeated stacks of the same
curse. It checks **net total >= 10 at completion**, rather than setup sliders,
the number of distinct curse types or a Tale's history. The existing Blitz
save/restore data already contains these stacks; no save-format change is needed.

## Apple Game Center setup

The iOS build links GameKit and signs with `ios/Sil-More.entitlements`.
`src/achievements-game-center.m` initializes the local player after SDL and the
local achievement profile are ready. UIKit/GameKit work runs on the main queue;
the engine reads a locked snapshot. Sign-in cancellation leaves local play
available. The achievement book offers **Game Center achievements** when signed
in and opens Apple's achievement dashboard.

IDs are permanent: `com.silqh.silmore.achievement.<STABLE_KEY>`. The exporter
reads the same prefix from `src/achievements-game-center.h`; renaming an app or
achievement must not rename its ID. On authentication, the adapter downloads
the registered descriptions and earned progress. Only known registered IDs
can upload, so an unpublished or excluded candidate is skipped. Completed
remote entries merge silently into local progress. Uploads are confirmed only
by successful callbacks. Failures and missing callbacks retry after 60 seconds;
registered/remote state refreshes every five minutes. Account changes clear
provider state and invalidate old asynchronous callbacks. As on other stores,
the shared device profile supplies the durable offline queue.

The Apple catalogue currently mirrors the existing Steam exclusions for review:
**102 candidates**, **850 points**, with **two additional exclusions undecided**.
This is a draft, not a submitted selection. `--apple-exclusions path.json` can
provide a separate complete JSON array of excluded stable keys. This controls
the export; runtime discovery follows whichever IDs are actually registered.
Apple point tiers are independent of Google's published points: Google
5/10/15 map to Apple 5, 25 to 10, 50 to 20, and 100 to 40.

Run `python scripts/export_achievement_stores.py` with the existing Node/sharp
options. It writes `store-readiness.json`, Apple metadata in
`apple-game-center-draft/achievements.json`, and 1024px RGB PNGs at 72 ppi.
The JSON is a review/registration manifest, **not** an Apple bulk-import format.
Steam CSVs and icons go into `steam-draft/` while its selection exceeds 100.
`--require-ready` rejects unfinished selections before exporting. Once the
catalogues meet their limits, output uses `steam/` and `apple-game-center/`.
No credentials or store uploads are part of this exporter.

On a Mac with Xcode and access to the app's developer account:

1. Enable Game Center for App ID `com.silqh.silmore` in Certificates, Identifiers
   & Profiles. Refresh the signing/provisioning profile and confirm the signed
   app contains `com.apple.developer.game-center`.
2. Finalize at most 100 entries and at most 1,000 Apple points. In App Store
   Connect > the app > Game Center, register the exact exported IDs, points,
   hidden flags, English names, pre/post-earned descriptions and images. These
   are non-repeatable achievements. IDs and live point values must stay fixed.
3. Generate the existing iOS CMake project with the Xcode generator, sign it
   with the team's provisioning profile, and enable Game Center for the app
   version in App Store Connect.
4. Test on iPhone/iPad and through TestFlight: authentication/cancellation,
   dashboard dismissal, offline unlock/restart/reconnect, remote restore,
   account switching, hidden entries, and both Blitz victories and thresholds.
5. Submit the first Game Center components together with the new app version.
   Later component additions may be reviewed separately.

The portable C state and build wiring are checked on Windows. Apple SDK
compilation, signing, device presentation and live synchronization require a
Mac/device pass; a Windows build does not establish those results.

Apple references: [Game Center setup](https://developer.apple.com/documentation/gamekit/initializing-and-configuring-game-center),
[achievement limits](https://developer.apple.com/help/app-store-connect/configure-game-center/manage-achievements/),
[metadata and image requirements](https://developer.apple.com/help/app-store-connect/reference/game-center/achievements/),
[first submission](https://developer.apple.com/help/app-store-connect/manage-submissions-to-app-review/submit-game-center-components).

## Steam setup

Build with the optional Steamworks integration described in
[steam-build.md](steam-build.md). In the Steamworks dashboard for the configured
App ID, finalize the two pending exclusions, then create and **publish** at most
100 Steam achievements using the exact API names
in the table above. Supply their names, descriptions, and unlocked/locked icons.
These are ordinary one-shot achievements; no Steam stat definitions are needed
for progress tracking. Keep the API names stable after release.

The adapter reads the Steam client's preloaded achievement cache, calls
`SetAchievement`, and submits batches with `StoreStats`. A successful
`UserStatsStored_t` callback confirms an upload. Failures and missing callbacks
retry with a one-minute delay; unacknowledged cached `SetAchievement` changes
are kept pending. Calls are pumped during idle input waits. The in-game viewer
also offers the Steam achievements overlay when Steam is active.

The standalone executable has no Steam SDK requirement. A Steam build whose
Steam initialization fails continues with local achievements.

API references: [Steam user stats](https://partner.steamgames.com/doc/api/ISteamUserStats),
[Steam overlay](https://partner.steamgames.com/doc/api/ISteamFriends#ActivateGameOverlay).

## Google Play setup

The Android `play` flavor includes Play Games Services v2 (`22.1.0`); `sideload`
has no Play Games dependency and keeps the same local achievement experience.
Unconfigured Play builds also work locally and do not initialize Play Games.

1. Configure the game's Play Games Services project in Play Console. Link the
   Android application ID and the signing certificate SHA-1 of each build you
   will test/distribute, including the **Play App Signing** certificate for Play
   delivery. Register test accounts while the service is unpublished.
2. Create 112 **standard**, one-shot achievements with the conditions in the
   table above. Supply the Console metadata, points, and icons, and publish the
   Play Games configuration when releasing it.
3. The public project and achievement IDs for Sil-More are supplied by
   `android/play-games.properties`. For another project, use an alternate file
   through `SIL_MORE_PLAY_GAMES_CONFIG`, Gradle project properties, or environment
   variables. The project ID is the numeric **Games
   services project ID**, distinct from the Android application ID. Achievement
   IDs are the opaque values generated by Play Console, distinct from Steam
   API names. The numeric project ID is required when any achievement IDs are
   supplied. Every nonempty achievement ID must be distinct; blank IDs remain
   unmapped. Gradle derives the
   catalogue from the native enum to keep resource names and indexes aligned.

```powershell
$env:SIL_MORE_PLAY_GAMES_PROJECT_ID = '<Games services project ID>'
$env:SIL_MORE_PLAY_ACH_INTO_THE_DEEP = '<Console achievement ID>'
$env:SIL_MORE_PLAY_ACH_LIGHT_IN_DARKNESS = '<Console achievement ID>'
$env:SIL_MORE_PLAY_ACH_BACK_TO_STARLIGHT = '<Console achievement ID>'
$env:SIL_MORE_PLAY_ACH_THREE_LIGHTS_IN_EXILE = '<Console achievement ID>'
$env:SIL_MORE_PLAY_ACH_THE_SHADOW_FALLS = '<Console achievement ID>'
$env:SIL_MORE_PLAY_ACH_THE_QUIET_ROAD = '<Console achievement ID>'
$env:SIL_MORE_PLAY_ACH_WORD_UNBROKEN = '<Console achievement ID>'
$env:SIL_MORE_PLAY_ACH_FORGED_BY_YOUR_HAND = '<Console achievement ID>'
$env:SIL_MORE_PLAY_ACH_A_NAME_WORTH_REMEMBERING = '<Console achievement ID>'
$env:SIL_MORE_PLAY_ACH_SONG_BENEATH_THE_MOUNTAIN = '<Console achievement ID>'
$env:SIL_MORE_PLAY_ACH_THERE_AND_BACK_AGAIN = '<Console achievement ID>'
$env:SIL_MORE_PLAY_ACH_BY_A_THREAD = '<Console achievement ID>'
$env:SIL_MORE_PLAY_ACH_LIGHT_OF_THE_ELDAR = '<Console achievement ID>'
$env:SIL_MORE_PLAY_ACH_THE_LONG_TALE = '<Console achievement ID>'
$env:SIL_MORE_PLAY_ACH_NO_RINGS_ATTACHED = '<Console achievement ID>'
$env:SIL_MORE_PLAY_ACH_CABED_EN_ARAS = '<Console achievement ID>'
$env:SIL_MORE_PLAY_ACH_OFT_EVIL_WILL = '<Console achievement ID>'
$env:SIL_MORE_PLAY_ACH_THE_HASTY_STROKE = '<Console achievement ID>'
$env:SIL_MORE_PLAY_ACH_A_FEW_LEAVES = '<Console achievement ID>'
$env:SIL_MORE_PLAY_ACH_TWO_WEAPONS = '<Console achievement ID>'
$env:SIL_MORE_PLAY_ACH_FOR_EXTERNAL_USE_ONLY = '<Console achievement ID>'
$env:SIL_MORE_PLAY_ACH_FLY_YOU_FOOL = '<Console achievement ID>'
$env:SIL_MORE_PLAY_ACH_KNOCK_WITH_YOUR_HEAD = '<Console achievement ID>'
$env:SIL_MORE_PLAY_ACH_DOWN_TO_GOBLIN_TOWN = '<Console achievement ID>'
$env:SIL_MORE_PLAY_ACH_ATHELAS_OUT_OF_STOCK = '<Console achievement ID>'
$env:SIL_MORE_PLAY_ACH_ANGBAND_MAINTENANCE = '<Console achievement ID>'
$env:SIL_MORE_PLAY_ACH_THE_ROAD_GOES_EVER_ON = '<Console achievement ID>'
$env:SIL_MORE_PLAY_ACH_SOMETHING_TOOKISH = '<Console achievement ID>'
$env:SIL_MORE_PLAY_ACH_FLIES_AND_SPIDERS = '<Console achievement ID>'
$env:SIL_MORE_PLAY_ACH_NOT_EVEN_CARCHAROTH = '<Console achievement ID>'
$env:SIL_MORE_PLAY_ACH_THE_FORDS_OF_SIRION = '<Console achievement ID>'
$env:SIL_MORE_PLAY_ACH_HELCARAXE = '<Console achievement ID>'
$env:SIL_MORE_PLAY_ACH_FIRE_AND_WATER = '<Console achievement ID>'
$env:SIL_MORE_PLAY_ACH_THE_FIRES_OF_THANGORODRIM = '<Console achievement ID>'
$env:SIL_MORE_PLAY_ACH_THROUGH_NAN_DUNGORTHEB = '<Console achievement ID>'
$env:SIL_MORE_PLAY_ACH_A_JOURNEY_IN_THE_DARK = '<Console achievement ID>'
$env:SIL_MORE_PLAY_ACH_A_MAP_OF_THE_IRON_HELLS = '<Console achievement ID>'
$env:SIL_MORE_PLAY_ACH_A_ELBERETH_GILTHONIEL = '<Console achievement ID>'
$env:SIL_MORE_PLAY_ACH_RELEASE_FROM_BONDAGE = '<Console achievement ID>'
$env:SIL_MORE_PLAY_ACH_TWO_THEMES_ONE_VOICE = '<Console achievement ID>'
$env:SIL_MORE_PLAY_ACH_YOU_CANNOT_PASS = '<Console achievement ID>'
$env:SIL_MORE_PLAY_ACH_FELAGUNDS_ANSWER = '<Console achievement ID>'
$env:SIL_MORE_PLAY_ACH_A_WILL_OF_YOUR_OWN = '<Console achievement ID>'
$env:SIL_MORE_PLAY_ACH_HURINS_DEFIANCE = '<Console achievement ID>'
$env:SIL_MORE_PLAY_ACH_NO_LONGER_BINDING = '<Console achievement ID>'
$env:SIL_MORE_PLAY_ACH_A_FOOLS_HOPE = '<Console achievement ID>'
$env:SIL_MORE_PLAY_ACH_A_SINGLE_SPARK = '<Console achievement ID>'
$env:SIL_MORE_PLAY_ACH_ON_THE_OTHER_HAND = '<Console achievement ID>'
$env:SIL_MORE_PLAY_ACH_IRON_CLEAVER = '<Console achievement ID>'
$env:SIL_MORE_PLAY_ACH_OUT_OF_THIN_ORE = '<Console achievement ID>'
$env:SIL_MORE_PLAY_ACH_CLAD_IN_THE_NIGHT = '<Console achievement ID>'
$env:SIL_MORE_PLAY_ACH_A_DREAM_UPON_THE_DARK_LORD = '<Console achievement ID>'
$env:SIL_MORE_PLAY_ACH_NEITHER_VOICE_DIMINISHED = '<Console achievement ID>'
$env:SIL_MORE_PLAY_ACH_DOOM_CAN_WAIT = '<Console achievement ID>'
# Run the existing Play bundle/release build with these values in its environment.
```

The IDs are build configuration, not secrets. No OAuth secret or service
account key belongs in the app. Malformed IDs, duplicate nonempty IDs, or achievement IDs without a project
ID fail Gradle configuration. The checked-in configuration preserves all 110
existing Console-assigned IDs, including the original 14 provider identities.
Existing mapped achievements continue synchronizing. Unmapped achievements
remain durable in local progress and are uploaded after their real IDs are
configured in a later build. They never send an empty ID to Google. An entirely
unmapped build keeps achievements local. The two new Blitz IDs remain blank
until assigned by the Console. Assign and verify them before shipping their
Play integration; no provider IDs are fabricated.

Use `scripts/export_achievement_stores.py` to export the two Steam CSVs, Steam
256px icons, the Google 512px icon/import package and Apple 1024px assets. It reads the native
catalogue and extracts existing sprites from `lib/xtra/graf/16x16.png`; all
scaling uses nearest-neighbor sampling. `icon-sources.json` records the template
entry, atlas coordinates, and source hash. The three-Silmaril achievement uses
three copies of the same game sprite. The original 14 achievements retain their
assigned IDs and 680 Play points.
The next 12 existing achievements have 15 points each, and the following 28
have 5 points each. That previous catalogue totals 1000 points.
The next 20 entries add 460 points and the following 19 lore entries add 190,
bringing the previous 93-entry catalogue to 1650 points. The next three
entries add 10 points each, for 1680 points. The five reviewed additions add
140 points: 25 for A Desperate Victory, 50 for Their Work Had No Rival, 15 for
Nauglamír and 25 each for Gurthang and the Spear of Melkor. The Great Axe of Gothmog adds 25 points and the eight appended discoveries add
10 each, bringing the previous total to 1925. Swift as the Wind adds 10 and
Tenfold Doom adds 25, for **1960 Google points**. All previously published point
values remain unchanged.
The exporter enforces the current
[2000-point allowance](https://developer.android.com/games/pgs/achievements#points_and_experience_points).
Google import titles omit commas and descriptions replace
remaining commas with semicolons to meet its import format; native and Steam
wording retains punctuation. The preview height follows the catalogue size.
Generated files belong under
`scripts/output/achievement-stores/` and are not source assets to commit.

In Steam's bulk editor, wait for **Changes were saved**, then reload the
catalogue and verify **100** Steam records before publishing. Successful file import
only prepares pending changes. Publish the Steamworks configuration with images
included. Google edits must keep the existing achievement IDs. The full
`google-play-achievements.zip` is for a new project. For the existing project,
import only `google-play-new-achievements.zip` for the two Blitz entries without
configured provider IDs, then update existing names and icons in place. Published initial
states cannot be changed. Reimporting existing entries would create duplicates
and lose their provider identity. The prior 110 assigned IDs remain in
`android/play-games.properties`; the two new IDs remain blank until assigned.

Google Play cloud backups are a separate explicit main-menu action; see
[cloud saves](cloud-saves.md).

The Play-only Application initializes the SDK when configuration is complete.
Automatic profile creation prompts are suppressed. Authentication checks and
provider tasks run asynchronously on Android's UI thread; the native game
thread polls a snapshot. Native and Steam masks use arrays of `uint64_t`;
Android uses arrays of Java `long` and JNI `jlong`, with `(I)J` and `([J)V`
method signatures. Index `i`
always uses word `i / 64` and bit `i % 64`, including the sign bit in Java.
Remote Java masks are immutable snapshots published to the game thread. Uploads use
`unlockImmediate`, and failures retry
with bounded backoff (5 seconds to 5 minutes). Remote achievement loads restore
unlocks on a new installation. Lifecycle/account generations reject stale
responses. The **Google Play achievements** action may explicitly request
sign-in and open Google's achievements screen; gameplay never forces that UI.

Keep the JNI methods in `proguard-rules.pro`: release builds look them up by
name. Play-only manifest entries and dependencies must stay out of sideload.

API references: [Android authentication](https://developer.android.com/games/pgs/android/android-signin),
[Android achievements](https://developer.android.com/games/pgs/android/achievements),
[AchievementsClient](https://developers.google.com/android/reference/com/google/android/gms/games/AchievementsClient).

## Validation

```powershell
.\build-incremental.ps1 -Target standard
.\build-incremental.ps1 -Target steam
python scripts/check_achievement_playtest.py --all
python scripts/check_achievements.py
python scripts/check_achievement_steam.py
python scripts/check_achievement_viewer.py
python scripts/check_achievement_events.py
python scripts/check_achievement_expansion.py
python scripts/check_achievement_discoveries.py
python scripts/check_achievement_characters.py
python scripts/check_achievement_legends.py
python scripts/check_achievement_journeys.py
python scripts/check_achievement_lore.py
python scripts/check_achievement_review.py
python scripts/check_achievement_blitz.py
python scripts/check_achievement_game_center.py
python scripts/check_achievement_store_exports.py --sharp <sharp module path>
python scripts/check_achievement_play_games.py
python scripts/check_achievement_save.py
python scripts/check_android_achievement_catalogue.py --gradle
python scripts/check_steamworks.py
$env:JAVA_HOME = 'C:/Program Files/BellSoft/LibericaJDK-17'
cd android
.\gradlew.bat :app:compileSideloadDebugJavaWithJavac :app:compilePlayDebugJavaWithJavac --console=plain
.\gradlew.bat :app:externalNativeBuildPlayDebug -PSIL_MORE_ANDROID_ABIS=arm64-v8a --console=plain
```

The combined playtest command runs every achievement suite and records saved
production unlock evidence for all 112 entries in
`scripts/output/achievement-playtest/catalogue-coverage.json`. It also checks
the actual singing payment and final-Morgoth combat completion paths.

The focused checks use temporary profiles and SDK-boundary doubles. They do
not authenticate to platform services or modify real achievements. Before release,
test real published IDs on authorized accounts: each gameplay trigger; an
offline unlock followed by reconnection/restart; an existing remote unlock
with a fresh local profile; sign-in cancellation; Android pause/resume; and
the local viewer with keyboard, controller, and portrait/landscape touch.

## Adding another achievement

Append a stable enum entry and catalogue row in `src/achievements.{h,c}`; never
reorder existing indexes. Add the corresponding Play ID resource/configuration
and Java ID array. Add its existing-atlas sprite to
`src/achievement-icons.h`, used by the in-game viewer and store exporter, and
its category and position to `src/achievement-order.h`. Choose its Play point
allocation within the catalogue's 2000-point allowance.
Gradle and the platform bridge masks derive from the enum;
the catalogue check verifies Java's order against it.
Hook a successful production gameplay event after its outcome is known. Keep
eligibility, local persistence, notifications, and platform synchronization in
the shared core. Publish the matching entries in both store dashboards before
shipping a configured release.

The full catalogue above is implemented. The No Armour proposal is excluded
at the user's request. Native book pagination supports up to 128 pages; each achievement occupies one
semantic paragraph, so all 112 remain present with large text and narrow screens.
