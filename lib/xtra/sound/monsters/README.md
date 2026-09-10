# Monster sounds

Edit `monster-sounds.json` to assign sounds explicitly for each monster type.
Keys are the stable N: IDs in `lib/edit/monster.txt`; names are included for editing.
Each `melee` entry corresponds to one B: line, in order, and lists its method,
effect, and recordings. Even two blows with the same method have independent
assignments. Each `ranged` entry is a separate ability belonging to that monster.
`damage`, `death`, and `idle` have their own assignments too.

Every `sounds` array lists exact audio paths relative to `lib/xtra`.
An empty array means silence. Multiple entries are randomized within that one
attack only (up to 64 variants). There is no automatic family, shared-ability,
folder, or normal-attack fallback. Adding or changing B: lines or S: abilities
requires updating that monster's assignments here. Restart or reload sound
settings after changing the file.

Example: Attercop (71) explicitly assigns Spider_Attack.ogg to its BITE/ENTRANCE
blow, and Bonus_Spider_Netshot_Attack.ogg to its throw_web ability. Another
monster's web attack needs its own explicit entry. Crebain (63) assigns the
normal Bat_Attack.ogg to its peck and Bonus_Bat_Sonic_Attack.ogg to its shriek.
Grimhawk (22), Gorcrow (43), Crebain (63), Twisted bat (104), and Shadow bat (122)
have explicit bat recordings assigned. Eagle of Manwë (306) and Thorondor (307)
remain silent because no bird recordings were assigned to them. A shared display
glyph is not a sound-family classification.

Spider hatchling (32) has spider bite, damage, death, and idle recordings. Its
ranged object is empty because the monster has no ranged abilities. Playback
uses the paths in monster-sounds.json, not folders named after race numbers.

The original OGG recordings are credited to Coucassi (see Terms Of Use.txt).
The added Minifantasy and Leohpaz WAV recordings are converted to OGG for the
project's OGG-only release tree. A family's recordings are explicitly assigned
to each monster's individual blow slots and event types. Unavailable ranged
recordings remain empty, and unmatched monster types remain empty.
Footsteps and the extra Orc_Attack.ogg in Behemoth are unused. Emerge or idle
vocalizations are used only where the source pack provides a suitable named clip.

The new assignments use the supplied Minifantasy Wolf, Bat, Wargo, Orc, Human,
Large Humanoid, and Yeti sets; Leohpaz Hellish Creatures Dragon and Demon Lord
sets; AlesiaDavina's processed Vampire OGG set; Monster Voices - Werewolf; and
the explicitly named CC0 troll and breath clips. The Elemental, Imp, Diablo,
Chimera, Krampus, and other generic clips were inventoried but not assigned to
an unrelated Sil monster. The supplied AlesiaDavina and Werewolf folders had no
license text, so verify redistribution rights before a public release.

Melee sounds play per executed blow, including misses. Ranged sounds follow the
executed ability. Damage sounds require positive damage and survival; death uses
the shared death path. All are audible within 20 tiles. Awake monsters have a
2% idle chance per eligible turn within 10 tiles, including unseen monsters.
Audio does not reveal grids or affect gameplay noise/detection or combat RNG.
The global Attack, Damage, Death, and Idle sound switches control the corresponding
event types for both player and monster sound effects. The existing Monster sounds
toggle and volume also control these monster recordings. Type switches are saved
in sound.json and default to enabled for existing configurations.

Validation: after building the standard target, run
`powershell -ExecutionPolicy Bypass -File scripts/check_monster_sounds.ps1`.
This uses a dummy audio device and does not replace listening in game.

| Race ID | Monster | Assigned recording families |
| --- | --- | --- |
| 6 | Nienna, Lady of Pity |  |
| 11 | Wolf | Minifantasy/Wolf |
| 12 | Tanglethorn |  |
| 13 | Dejected human thrall | Minifantasy/Human |
| 14 | Dejected elven thrall | Minifantasy/Human |
| 15 | Orc thrallmaster | Minifantasy/Orc, Orc |
| 16 | Alert human thrall | Minifantasy/Human |
| 17 | Alert elven thrall | Minifantasy/Human |
| 18 | Tulkas Unclad |  |
| 19 | Aulë the Smith |  |
| 20 | Mandos the Doomsman |  |
| 21 | Orc skirmisher | Minifantasy/Orc, Orc |
| 22 | Grimhawk | Bat, Minifantasy/Bat |
| 23 | Mewlip |  |
| 31 | Orc scout | Minifantasy/Orc, Orc |
| 32 | Spider hatchling | Spider |
| 33 | Blue serpent | CC0 |
| 41 | Orc soldier | Minifantasy/Orc, Orc |
| 42 | Madthorn |  |
| 43 | Gorcrow | Bat, Minifantasy/Bat |
| 44 | Brood spider | Spider |
| 51 | Orc archer | Minifantasy/Orc, Orc |
| 52 | White wolf | Minifantasy/Wolf |
| 53 | Red serpent | CC0 |
| 54 | Gorgol, the Butcher | Minifantasy/Orc, Orc |
| 61 | Orc warrior | Minifantasy/Orc, Orc |
| 62 | Sword spider | Spider |
| 63 | Crebain | Bat, Minifantasy/Bat |
| 64 | Phantom | Ghost |
| 71 | Attercop | Spider |
| 72 | Nightthorn |  |
| 73 | Green serpent | CC0 |
| 74 | Mountain troll | CC0, Minifantasy/Large_Humanoid |
| 75 | Tattered wight |  |
| 76 | Baugon, the Merciless | Minifantasy/Orc, Orc |
| 81 | Orc champion | Minifantasy/Orc, Orc |
| 82 | Easterling warrior | Minifantasy/Human |
| 83 | Hummerhorn | Hornet |
| 84 | Balcmeg, the Relentless | Minifantasy/Orc, Orc |
| 85 | Lug, the Grotesque | Minifantasy/Orc, Orc |
| 91 | Orc captain | Minifantasy/Orc, Orc |
| 92 | Warg | Minifantasy/Wargo, Minifantasy/Wolf |
| 93 | Grave wight |  |
| 94 | Dark serpent | CC0 |
| 95 | Orcobal, Champion of the Orcs | Minifantasy/Orc, Orc |
| 101 | Whispering shadow | Ghost |
| 102 | Distended spider | Spider |
| 103 | Easterling archer | Minifantasy/Human |
| 104 | Twisted bat | Bat, Minifantasy/Bat |
| 105 | Othrod, the Orc Lord | Minifantasy/Orc, Orc |
| 111 | Snow troll | CC0, Minifantasy/Yeti |
| 112 | Barrow wight |  |
| 113 | Lurking horror |  |
| 114 | Giant | Minifantasy/Large_Humanoid |
| 115 | Uldor, the Accursed |  |
| 117 | Brodda, the Easterling Lord | Minifantasy/Human |
| 121 | Easterling spy | Minifantasy/Human |
| 122 | Shadow bat | Bat, Minifantasy/Bat |
| 123 | Sulrauko |  |
| 124 | Fire-drake hatchling | CC0, Dragon, LEOHPAZ/Dragon |
| 125 | Ulfang the Black | Minifantasy/Human |
| 126 | Duruin, Least of the Balrogs | LEOHPAZ/Demon_Lord |
| 131 | Werewolf | Werewolf |
| 132 | Shadow spider | Spider |
| 133 | Shadow | Ghost |
| 134 | Sapphire serpent | CC0 |
| 135 | Gilim, the Giant of Eruman | Minifantasy/Large_Humanoid |
| 141 | Ruby serpent | CC0 |
| 142 | Creeping horror |  |
| 143 | Ringrauko | LEOHPAZ/Demon_Lord |
| 145 | Delthaur, Balrog of Terror | LEOHPAZ/Demon_Lord |
| 146 | Nan, the Giant | Minifantasy/Large_Humanoid |
| 151 | Cave troll | CC0, Minifantasy/Large_Humanoid |
| 152 | Emerald serpent | CC0 |
| 153 | Oathwraith | Ghost |
| 154 | Cat warrior |  |
| 161 | Amethyst serpent | CC0 |
| 162 | Kemenrauko | LEOHPAZ/Demon_Lord |
| 163 | Grotesque |  |
| 164 | Young cold-drake | Dragon, LEOHPAZ/Dragon |
| 165 | Umuiyan, the Doorkeeper |  |
| 166 | Belegwath, Balrog of Shadow | LEOHPAZ/Demon_Lord |
| 167 | Spectre | Ghost |
| 171 | Spider of Gorgoroth | Spider |
| 172 | Greater werewolf | Werewolf |
| 173 | Adamant serpent | CC0 |
| 174 | Lesser vampire | AlesiaDavina/Vampire |
| 175 | Cat assassin |  |
| 176 | Scatha the Worm | CC0, Dragon, LEOHPAZ/Dragon |
| 177 | Oikeroi, Guard of Tevildo |  |
| 181 | Young fire-drake | CC0, Dragon, LEOHPAZ/Dragon |
| 182 | Darting horror |  |
| 183 | Wraith | Ghost |
| 184 | Ururauko | LEOHPAZ/Demon_Lord |
| 185 | Tevildo, Prince of Cats |  |
| 186 | Turkano, Balrog of the Hosts | LEOHPAZ/Demon_Lord |
| 191 | Ancient sapphire serpent | CC0 |
| 192 | Troll guard | CC0, Minifantasy/Large_Humanoid |
| 193 | Vampire | AlesiaDavina/Vampire |
| 194 | Nameless thing |  |
| 195 | Smaug the Golden | CC0, Dragon, LEOHPAZ/Dragon |
| 196 | Maeglin, Betrayer of Gondolin | Minifantasy/Human |
| 201 | Ancient ruby serpent | CC0 |
| 202 | Great cold-drake | Dragon, LEOHPAZ/Dragon |
| 203 | Silent watcher |  |
| 204 | Gwathrauko | LEOHPAZ/Demon_Lord |
| 205 | Draugluin, Sire of Werewolves | Werewolf |
| 206 | Vallach, Balrog of Sudden Flame | LEOHPAZ/Demon_Lord |
| 211 | Ancient spider | Spider |
| 212 | Ancient emerald serpent | CC0 |
| 213 | Vampire lord | AlesiaDavina/Vampire |
| 214 | Dagorhir, the Elfbane |  |
| 215 | Gostir, the Dread Glance | Dragon, LEOHPAZ/Dragon |
| 221 | Ancient amethyst serpent | CC0 |
| 222 | Great fire-drake | CC0, Dragon, LEOHPAZ/Dragon |
| 223 | Hithrauko | LEOHPAZ/Demon_Lord |
| 224 | Shelob, Spider of Darkness | Spider |
| 225 | Lungorthin, Lord of Balrogs | CC0, LEOHPAZ/Demon_Lord |
| 231 | Ancient adamant serpent | CC0 |
| 232 | Unrelenting horror |  |
| 233 | Ancalagon the Black | CC0, Dragon, LEOHPAZ/Dragon |
| 234 | Thuringwethil, the Vampire Messenger | AlesiaDavina/Vampire |
| 241 | Gothmog, High Captain of Balrogs | LEOHPAZ/Demon_Lord |
| 242 | Ungoliant, the Gloomweaver | CC0, Spider |
| 243 | Glaurung, the Deceiver | CC0, Dragon, LEOHPAZ/Dragon |
| 244 | Gorthaur, Servant of Morgoth | LEOHPAZ/Demon_Lord |
| 245 | Flying cold-drake | CC0, Dragon, LEOHPAZ/Dragon |
| 246 | Flying fire-drake | CC0, Dragon, LEOHPAZ/Dragon |
| 251 | Morgoth, Lord of Darkness |  |
| 253 | Carcharoth, the Jaws of Thirst | Minifantasy/Wolf |
| 260 | Nienna, Lady of Pity |  |
| 301 | Fëanor, High King of the Noldor | Minifantasy/Human |
| 302 | Lúthien Tinúviel | Minifantasy/Human |
| 303 | Thingol, the Hidden King | Minifantasy/Human |
| 304 | Beren, Son of Barahir | Minifantasy/Human |
| 305 | Huan, Hound of Valinor | Minifantasy/Wolf |
| 306 | Eagle of Manwë |  |
| 307 | Thorondor, King of Eagles |  |
| 308 | Ent |  |
| 309 | Ent-wife |  |
| 320 | Manwë, Lord of the Breath of Arda |  |
| 321 | Varda, Lady of the Stars |  |
| 322 | Ulmo, Lord of Waters |  |
| 323 | Aulë, the Smith |  |
| 324 | Yavanna, the Giver of Fruits |  |
| 325 | Mandos, Doomsman of the Valar |  |
| 326 | Vairë, the Weaver |  |
| 327 | Lórien, Master of Dreams |  |
| 328 | Este, the Healer |  |
| 329 | Nienna, Lady of Mourning |  |
| 330 | Tulkas, the Valiant |  |
| 331 | Nessa, the Dancer |  |
| 332 | Oromë, Lord of Forests |  |
| 333 | Vana, the Ever Young |  |
| 401 | Melkor, Rightful Lord of Arda |  |
| 402 | Green Great Dragon | Dragon, LEOHPAZ/Dragon |
| 403 | Ringwion, the Pale Blade | LEOHPAZ/Demon_Lord |
| 404 | Helcamo, the Hoarfrost | LEOHPAZ/Demon_Lord |
| 405 | Lhamthanc, the Forked Tongue | LEOHPAZ/Dragon |
| 406 | Angacirca, Reaper of Thralls | Minifantasy/Orc |
| 407 | Langon, the Rushing Herald | LEOHPAZ/Demon_Lord |
| 408 | Dúron, Keeper of the Unlit Ways | LEOHPAZ/Demon_Lord |
| 409 | Fankil, the Sower of Strife | Minifantasy/Human |
| 410 | Ondotur, the Buried Lord | Minifantasy/Large_Humanoid |
| 411 | Nambatur, Custodian of Grond's Forge | Minifantasy/Large_Humanoid |
