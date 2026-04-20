# Sil-More

Sil-More is a fork of Sil / Sil-Q focused on two major directions.
First, it builds a new storyline around Tolkien's First Age material.
Second, it uses a metarun structure that connects consecutive runs into a
broader campaign-like arc.

# Building

## Windows

### Prerequisites
- MSYS2 with MinGW64 (install from https://www.msys2.org/)
- CMake
- SDL3 libraries

### Building
1. Install MSYS2 and open the MINGW64 terminal.
2. Install required packages:
   ```bash
   pacman -S mingw-w64-x86_64-gcc mingw-w64-x86_64-cmake mingw-w64-x86_64-SDL3 mingw-w64-x86_64-SDL3_image mingw-w64-x86_64-SDL3_ttf make
   ```
3. Navigate to the Sil-More source directory.
4. Run the build script:
   ```bash
   ./build-cmake.bat
   ```
   Or manually configure and build:
   ```bash
   cmake -G "MinGW Makefiles" -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=C:/msys64/mingw64
   cmake --build build --parallel
   ```
5. The executable will be in `build/sil-more.exe` and deployed to `sil-more-windows-sdl3/`.
6. Run from the deployment directory: `cd sil-more-windows-sdl3 && ./sil-more.exe`

## Linux

### Prerequisites
- GCC or Clang compiler
- CMake
- SDL3 development libraries

### Building
1. Install dependencies:
   - **Debian/Ubuntu:**
     ```bash
     sudo apt install build-essential cmake libsdl3-dev libsdl3-image-dev libsdl3-ttf-dev
     ```
   - **Fedora:**
     ```bash
     sudo dnf install gcc cmake SDL3-devel SDL3-image-devel SDL3-ttf-devel
     ```
   - **Arch:**
     ```bash
     sudo pacman -S base-devel cmake sdl3
     paru -S sdl3_ttf sdl3_image # or use any other AUR helper
     ```

2. Navigate to the Sil-More source directory.
3. Configure and build:
   ```bash
   cmake -B build -DCMAKE_BUILD_TYPE=Release
   cmake --build build --parallel
   ```
4. The executable will be in `build/sil-more`.
5. Run the game:
   ```bash
   ./build/sil-more
   ```

# Road Map
## Done
- Implement main curses (done)
- Implement common RHF flags (done)
  * Cheap cost (done)
  * Morgoth Curse (done)
- Add more debugging functions (done)
- Add unique RHF flags (done)
  * Feanor (done)
  * Telchar (done)
  * Gamil (done)
  * Melian (done)
  * Thingol (done)
  * Tuor (done)
  * Hurin (done)
- Figure out last abilities and balance tweaks (done)
  * All starting abilities (done)
  * Multiple starting abilities (done)


## Release closed alpha 0.5 (done)

- Bug fixes (ongoing)
- UI fixes (done)
  * Start menu (done)
  * Character menu (done)
- Decriptions update (done)

## Release alpha 0.6 (UI updates) (done)

- Bug fixes (ongoing) 
- UI updates 
  * Score menu (done) 
  * Final menu (done) 
- Flavor ideas for final menus (done) 

## Release of alpha 0.7 (Storyline updates) (done)

- Balance tweaks
- Add unique RHF flags
  * Earendil
  * Turin (done)
  * Celeborn
  * Maedhros (done)
- New heroes
  * Eol (done) 
- Dynamic tile system (done)
-- Wall tiles (done)
-- Floor tiles (done)
-- Doors (done)
- Unique style for each depth (done)
- Level entrance message depending on the style (done)
- Difficulty levels for current run (done) 
- Automatic load if run is not finished (done)
## Release of beta 0.8 (Visual update) (done)

- Quest systems 
  * Tulkas -> kill unique -> get artifact (done) 
  * Quest vault implementation (done) 
  * Mandos -> kill specific -> get ability (done) 
  * Aule -> forge -> get ability (done) 
  * Niena -> spawn -> get ability (done) 
  * Orome (done)
- Update to oath system (done)
  * After completed quest you get oaths (done)
- UI
  * Better oath texts (done)
  * Better oath menu (done)
- New oaths
  * Smith (done)
  * Valor (done)
- Bug fixes (ongoing) 
  * save names (done)
  * new metarun saves (done)
  * rubble (done)
  * speacial monster (done)
## Release of beta 0.87 (Quests and Oaths update) (done) 

- Fullscreen mode for windows (done) 
- Combat rolls logs (done) 
- Help screen update (done) 
- Combat rolls logs (done) 
- Steamdeck keybinds, art, etc (done)
- Bug fixes (ongoing) 
  * OathBreaking (done)
## Release of beta 0.88 (Steam Deck update)

- SDL
## Release of beta 0.9 


### Ideas

- More frequent forges for dwarves
- Calculate the forge probability
- Pride
- Greed
- More Vaults
- More monsters
- Change Score to Character database
- Multiple runs support 
- New Quests
  * Manwe
  * Este
