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
have the supplied bat recordings explicitly assigned. Eagle of Manwe (306) and
Thorondor (307) have no suitable supplied recordings and remain silent. A shared
display glyph is not a sound-family classification.

Spider hatchling (32) has spider bite, damage, death, and idle recordings. Its
ranged object is empty because the monster has no ranged abilities. Playback
uses the paths in monster-sounds.json, not folders named after race numbers.

Only supplied OGG files are used, credited to Coucassi (see Terms Of Use.txt).
A family's single normal attack recording is explicitly assigned to its melee
blows; these independent assignments can be replaced as more recordings become
available. Unavailable ranged recordings remain empty. Unmatched monster types
remain empty. Footsteps and the extra Orc_Attack.ogg in Behemoth are unused.
Emerge vocalizations are used for idle sounds.

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

| Race ID | Monster | Supplied recording family |
| --- | --- | --- |
| 6 | Nienna, Lady of Pity |  |
| 11 | Wolf |  |
| 12 | Tanglethorn |  |
| 13 | Dejected human thrall |  |
| 14 | Dejected elven thrall |  |
| 15 | Orc thrallmaster | Orc |
| 16 | Alert human thrall |  |
| 17 | Alert elven thrall |  |
| 18 | Tulkas Unclad |  |
| 19 | Aulë the Smith |  |
| 20 | Mandos the Doomsman |  |
| 21 | Orc skirmisher | Orc |
| 22 | Grimhawk | Bat |
| 23 | Mewlip |  |
| 31 | Orc scout | Orc |
| 32 | Spider hatchling | Spider |
| 33 | Blue serpent |  |
| 41 | Orc soldier | Orc |
| 42 | Madthorn |  |
| 43 | Gorcrow | Bat |
| 44 | Brood spider | Spider |
| 51 | Orc archer | Orc |
| 52 | White wolf |  |
| 53 | Red serpent |  |
| 54 | Gorgol, the Butcher | Orc |
| 61 | Orc warrior | Orc |
| 62 | Sword spider | Spider |
| 63 | Crebain | Bat |
| 64 | Phantom | Ghost |
| 71 | Attercop | Spider |
| 72 | Nightthorn |  |
| 73 | Green serpent |  |
| 74 | Mountain troll |  |
| 75 | Tattered wight |  |
| 76 | Boldog, the Merciless | Orc |
| 81 | Orc champion | Orc |
| 82 | Easterling warrior |  |
| 83 | Hummerhorn | Hornet |
| 84 | Balcmeg, the Relentless | Orc |
| 85 | Lug, the Grotesque | Orc |
| 91 | Orc captain | Orc |
| 92 | Warg |  |
| 93 | Grave wight |  |
| 94 | Dark serpent |  |
| 95 | Orcobal, Champion of the Orcs | Orc |
| 101 | Whispering shadow | Ghost |
| 102 | Distended spider | Spider |
| 103 | Easterling archer |  |
| 104 | Twisted bat | Bat |
| 105 | Othrod, the Orc Lord | Orc |
| 111 | Snow troll |  |
| 112 | Barrow wight |  |
| 113 | Lurking horror |  |
| 114 | Giant |  |
| 115 | Uldor, the Accursed |  |
| 117 | Brodda, the Easterling Lord |  |
| 121 | Easterling spy |  |
| 122 | Shadow bat | Bat |
| 123 | Sulrauko |  |
| 124 | Fire-drake hatchling | Dragon |
| 125 | Ulfang the Black |  |
| 126 | Duruin, Least of the Balrogs |  |
| 131 | Werewolf |  |
| 132 | Shadow spider | Spider |
| 133 | Shadow | Ghost |
| 134 | Sapphire serpent |  |
| 135 | Gilim, the Giant of Eruman |  |
| 141 | Ruby serpent |  |
| 142 | Creeping horror |  |
| 143 | Ringrauko |  |
| 145 | Delthaur, Balrog of Terror |  |
| 146 | Nan, the Giant |  |
| 151 | Cave troll |  |
| 152 | Emerald serpent |  |
| 153 | Oathwraith | Ghost |
| 154 | Cat warrior |  |
| 161 | Amethyst serpent |  |
| 162 | Kemenrauko |  |
| 163 | Grotesque |  |
| 164 | Young cold-drake | Dragon |
| 165 | Umuiyan, the Doorkeeper |  |
| 166 | Belegwath, Balrog of Shadow |  |
| 167 | Spectre | Ghost |
| 171 | Spider of Gorgoroth | Spider |
| 172 | Greater werewolf |  |
| 173 | Adamant serpent |  |
| 174 | Lesser vampire |  |
| 175 | Cat assassin |  |
| 176 | Scatha the Worm | Dragon |
| 177 | Oikeroi, Guard of Tevildo |  |
| 181 | Young fire-drake | Dragon |
| 182 | Darting horror |  |
| 183 | Wraith | Ghost |
| 184 | Ururauko |  |
| 185 | Tevildo, Prince of Cats |  |
| 186 | Turkano, Balrog of the Hosts |  |
| 191 | Ancient sapphire serpent |  |
| 192 | Troll guard |  |
| 193 | Vampire |  |
| 194 | Nameless thing |  |
| 195 | Smaug the Golden | Dragon |
| 196 | Maeglin, Betrayer of Gondolin |  |
| 201 | Ancient ruby serpent |  |
| 202 | Great cold-drake | Dragon |
| 203 | Silent watcher |  |
| 204 | Gwathrauko |  |
| 205 | Draugluin, Sire of Werewolves |  |
| 206 | Vallach, Balrog of Sudden Flame |  |
| 211 | Ancient spider | Spider |
| 212 | Ancient emerald serpent |  |
| 213 | Vampire lord |  |
| 214 | Dagorhir, the Elfbane |  |
| 215 | Gostir, the Dread Glance | Dragon |
| 221 | Ancient amethyst serpent |  |
| 222 | Great fire-drake | Dragon |
| 223 | Hithrauko |  |
| 224 | Shelob, Spider of Darkness | Spider |
| 225 | Lungorthin, Lord of Balrogs |  |
| 231 | Ancient adamant serpent |  |
| 232 | Unrelenting horror |  |
| 233 | Ancalagon the Black | Dragon |
| 234 | Thuringwethil, the Vampire Messenger |  |
| 241 | Gothmog, High Captain of Balrogs |  |
| 242 | Ungoliant, the Gloomweaver | Spider |
| 243 | Glaurung, the Deceiver | Dragon |
| 244 | Gorthaur, Servant of Morgoth |  |
| 245 | Flying cold-drake | Dragon |
| 246 | Flying fire-drake | Dragon |
| 251 | Morgoth, Lord of Darkness |  |
| 253 | Carcharoth, the Jaws of Thirst |  |
| 260 | Nienna, Lady of Pity |  |
| 301 | Fëanor, High King of the Noldor |  |
| 302 | Lúthien Tinúviel |  |
| 303 | Thingol, the Hidden King |  |
| 304 | Beren, Son of Barahir |  |
| 305 | Huan, Hound of Valinor |  |
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
| 402 | Green Great Dragon | Dragon |
