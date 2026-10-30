# AFTERIMAGE

*Design and Development of a Behaviour-Responsive 2D Survival Horror Game Using C++*

A top-down, 1990s-style pixel-art survival horror game written in **C++17** with **raylib 5.5**, with props rendered in **Blender**. You wake in Apartment 4C during a storm with no memory of arriving. The building remembers where everything was and where you walked, and so does the thing that hunts you.

The campaign now has four areas: Apartment 4C, the Repeating Corridor, the Archive, and the Residue Ward. Choose **Continue** or **New Game** from the title screen. A story and objective briefing pauses before the first area and every transition; play starts only after you confirm it. Continue resumes the latest area checkpoint, score, visit count and carried items. Progress and the separate adaptive AI profile are saved beside the game executable.

Raylib plays original generated footsteps, sprint clatter, strained breathing, and layered death-scare sounds. The full-screen creature face and sustained horror bed last five seconds. See [`docs/CHARACTER_SPRITE_BRIEF.md`](docs/CHARACTER_SPRITE_BRIEF.md) for character atlas requirements and the audio asset pipeline.

| Document | Contents |
|---|---|
| `README.md` (this file) | Build and run, controls, assets, testing, troubleshooting |
| [`docs/DESIGN.md`](docs/DESIGN.md) | Story, areas, puzzles, the afterimage system, enemy AI algorithm, architecture |
| [`docs/BLENDER_PIPELINE.md`](docs/BLENDER_PIPELINE.md) | Blender camera, lighting and render settings, pixel-art conversion, naming |
| [`docs/CHARACTER_SPRITE_BRIEF.md`](docs/CHARACTER_SPRITE_BRIEF.md) | Character animation atlas and sound-production requirements |
| `docs/screenshots/` | Screenshots captured by the automated self test |

---

## 1. Directory tree

```
afterimage/
├── CMakeLists.txt              build script (finds raylib, or downloads 5.5 automatically)
├── README.md
├── docs/
│   ├── DESIGN.md
│   ├── BLENDER_PIPELINE.md
│   └── screenshots/            self-test captures
├── src/
│   ├── main.cpp                window, game loop, --selftest / --export-sprites
│   ├── Config.h                resolution, palette, tuning constants
│   ├── World.h / World.cpp     Level, Object, tiles, collision, line of sight
│   ├── Levels.cpp              the 4 maps, object placement, all story text
│   ├── Player.h / Player.cpp   movement, collision, sprint/stamina, hiding
│   ├── Enemy.h / Enemy.cpp     "The Tenant" finite state machine and historical route playback
│   ├── Habits.h                bounded, versioned cross-session behaviour profile
│   ├── AtomicFile.h / .cpp     safe replacement of the AI profile file
│   ├── Pathfinding.h / .cpp    grid A*
│   ├── Afterimage.h / .cpp     object snapshots, heat map, repetition score
│   ├── Art.h / Art.cpp         PNG loading with procedural pixel-art fallback
│   ├── Game.h / Game.cpp       game states, interaction, puzzles, checkpoints, self test
│   ├── Render.cpp              world rendering and visual effects
│   └── UI.cpp                  HUD, menus, reading panel, game over, victory
├── assets/sprites/             game-ready sprites (8 rendered from Blender)
├── assets_src/renders/         raw Blender renders (64×64 / 64×96)
├── blender/render_props.py     Blender script that builds and renders the props
└── tools/pixelate_sprites.py   converts renders into palette-locked pixel-art sprites
```

---

## 2. Setting up raylib on Windows

The `CMakeLists.txt` handles raylib in this order:
1. If CMake can find an installed raylib (MSYS2, vcpkg), it uses it.
2. Otherwise it **downloads the raylib 5.5 source automatically** and builds it with the game. This needs internet once.
3. Offline: pass `-DRAYLIB_SOURCE_DIR=C:/path/to/raylib-5.5` to use a local copy of the source.

Pick **one** of these toolchains.

### Option A: Visual Studio 2022 (simplest)
1. Install **Visual Studio 2022 Community** with the **"Desktop development with C++"** workload. This includes CMake.
2. Open **"x64 Native Tools Command Prompt for VS 2022"**, `cd` to the project folder, and run:
```bat
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
build\Release\afterimage.exe
```
raylib is downloaded and built automatically on the first configure.

### Option B: MSYS2 + MinGW-w64 (UCRT64)
1. Install MSYS2 from <https://www.msys2.org>, then open the **"MSYS2 UCRT64"** shell.
2. Install the tools:
```bash
pacman -S --needed mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-cmake mingw-w64-ucrt-x86_64-ninja
```
3. Build. `AFTERIMAGE_FETCH_RAYLIB=ON` builds raylib from source and produces a **fully static .exe** that runs on any Windows PC:
```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DAFTERIMAGE_FETCH_RAYLIB=ON
cmake --build build
./build/afterimage.exe
```
If you'd rather use MSYS2's raylib package (`pacman -S mingw-w64-ucrt-x86_64-raylib`), leave out the option. The .exe will then need the MSYS2 DLLs, so run it from the UCRT64 shell or copy the DLLs listed by `ldd build/afterimage.exe`.

### Linux (also works)
```bash
sudo apt install build-essential cmake git libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev libgl1-mesa-dev
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build -j
./build/afterimage
```

### Distributing the game
Copy `afterimage.exe` **together with the `assets` folder next to it**. For a MinGW build linked to shared raylib, include the runtime DLLs used by that executable beside it. Blender, CMake and the compiler aren't needed to play. If `assets/` is missing, the game still runs using procedural art.

`--selftest` runs the automated gameplay checks. `--fresh-ai` removes only `afterimage_ai_profile.dat` beside the executable for a baseline AI run; it leaves progress, settings, high scores, and `afterimage_movement_history.dat` intact. Normal **New Game**, retry, and restarting the app retain up to 132 seconds of movement history per area and adaptive learning. The profile stores bounded route, hiding-place, sprint, and escape summaries; the separate movement-history file stores sampled positions and posture.

---

## 3. Controls

| Key | Action |
|---|---|
| **W A S D** / **Arrow keys** | Move |
| **Shift** | Run (fast, but **loud**, and uses stamina) |
| **E** | Interact: open, search, read, take, use, hide, leave a hiding place |
| **Q** | **AFTERIMAGE**: see old object positions and puzzle clues (4 s, then a 9 s cooldown) |
| **J** | Open the case file for current objective, room notes and threat clues |
| **Esc** / **P** | Pause menu (Resume / Rules / Settings / Retry / Quit to title) |
| **F1** | Developer diagnostics: enemy state, target, historical sample, learned use counts, and decision reason |
| Menus | W/S or arrows to move; A/D or left/right to change settings; Enter / E / Space to select |
| Game over | **R** retry area, **N** start a new run, **M** title |
| Title | **Continue** resumes a saved area; **New Game** starts the story over |

### How to play
- **Objective** (top-left) always tells you what to do next. **Carrying** (top-right) shows your items. Press **J** to review the current lead and room notes.
- Press **Q** when an object or puzzle is unclear: a pale ghost marks its old position, a red outline marks the changed object, and green numbers show the archive relay order. Search the red-outlined cabinet/crate or use the numbered levers in order. The active Q overlay displays a matching instruction.
- The **Tenant** can't be fought. Watch for its **red eyes** in the dark, and for the red screen tint when it has detected you.
- Break line of sight, close doors behind you, and hide in **wardrobes** or **dark alcoves**. Don't let it watch you climb in.
- **Running**, searching the wrong crate and pulling the wrong lever all make **noise** it will investigate.
- The **PATTERN** bar fills as you repeat the same routes. The more predictable you are, the faster and sharper it becomes, and it starts going to the places you always go.
- Explore, collect items, read notes and solve puzzles to earn score. The best score is saved beside the game.
- Getting caught triggers a five-second full-screen Tenant jumpscare with a sharp death stinger and sustained horror bed, then costs score. Retry the area, start a new run, or return to the title; the ending also offers Play Again.
- The default pace and threat are calmer. Open **Settings** from the title or pause menu to adjust movement, threat, message duration and screen effects.
- The Tenant appears in all four areas and can replay your route from two minutes earlier once a full trace for that area is available. Recent movement history survives closing and reopening the game, so it can begin a new visit by retracing your previous route. When it takes your appearance, a red eye-slit identifies the imitation; changing rooms and breaking sight helps you avoid it.
- The Tenant stays dormant for 40 seconds in the apartment, 24 seconds in the corridor and Archive, and 8 seconds in the Residue Ward. Use the opening time to read the objective and get oriented.

### Walkthrough (spoilers)
<details><summary>Show</summary>

1. **Apartment 4C:** read the note by the bed. Walk into the living room; a cabinet moves. Press **Q**, search the cabinet outlined in red, and take the **Fuse**. Put it in the fuse box in the kitchen (south-east), then leave by the front door (west side of the hall).
2. **Repeating Corridor:** the east end loops back to the west. Press **Q** to find the crate that differs from the past (red outline), search it, and take the **Brass Key**. Avoid searching other crates, because they're loud. Open the ARCHIVE door at the far east, on the north wall.
3. **Archive:** press **Q** near the levers to see the echo numbers. The order is bottom-left → top-left → bottom-right → top-right. Use the terminal, then **run** to the exit in the north-west corner.
</details>

---

## 4. Assets

| Asset | Source |
|---|---|
| `bed`, `cabinet`, `chair`, `crate`, `fusebox`, `table`, `wardrobe`, `tenant` | **Rendered in Blender** (`blender/render_props.py`), converted by `tools/pixelate_sprites.py`, loaded from `assets/sprites/` |
| `shelf` | Rendered in Blender (`assets_src/renders/shelf.png`), but the procedural version is used because it reads better at 16 px |
| `player`, `door`, `door_open`, `exit_locked`, `exit_open`, `fusebox_on`, `alcove`, `note`, `switch_off`, `switch_on`, `terminal`, `terminal_on` | Procedural pixel art generated in code (`src/Art.cpp`) |
| Floor tiles, walls, rain, darkness, light, ghosts, scanlines, UI | Drawn procedurally with raylib each frame |
| Font | raylib's built-in pixel font |
| Audio | Original generated footstep, sprint-clatter, breathing, death-stinger, and five-second jumpscare-bed WAV cues play through raylib. |

Every sprite has a procedural fallback, so deleting any PNG never breaks the game. See `docs/BLENDER_PIPELINE.md` for the camera, lighting, resolution and palette settings and how to re-render.

---

## 5. Testing

### 5.1 Automated self test
```bat
afterimage.exe --selftest
```
This plays through scripted checks at a fixed timestep, saves screenshots next to the executable, prints `PASS` / `FAIL` lines, and exits. The checks are:

| # | Check |
|---|---|
| 1–4 | Every object and waypoint in all four levels is reachable |
| 5 | Settings save and reload correctly |
| 6–18 | Profile save/reload, corruption handling, bounded counts, baseline-versus-learned patrol choice, sprint-noise learning, audio cue loading, mimic route playback/edge cases, committed attack wind-up and one-shot catch |
| 19–33 | Afterimage unlock, objects and items, enemy detection/noise/search/hiding, archive puzzle, level progression, and final exit |

During normal play, press **F1** to inspect developer diagnostics for the enemy and its current adaptation decision.

### 5.2 Manual testing checklist

| Area | Test | Expected |
|---|---|---|
| Movement | Move in all 8 directions; run with Shift | Smooth movement; running is faster and drains the stamina bar |
| Collision | Walk into walls, furniture and closed doors | Player stops and slides along walls; never passes through |
| Interaction | Stand next to an object | `[E] …` prompt appears; pressing E gives a message or opens a panel |
| Interaction | Use a locked door without the item | Clear "locked" message, nothing breaks |
| Inventory | Take the Fuse and the Brass Key | Items appear under CARRYING; the fuse disappears when used |
| Afterimage | Press Q before the living-room event | Not available yet |
| Afterimage | Press Q after the event | Ghosts of past positions, red outline on the moved cabinet, cooldown bar |
| Puzzle 1–2 | Search the moved cabinet, use the fuse box | Fuse found; power on; front door unlocks |
| Puzzle 3 | Q in the corridor, search the red-outlined crate | Brass Key found; the ARCHIVE door opens with it |
| Puzzle 4 | Pull a wrong lever | All levers reset, noise event, enemy investigates |
| Puzzle 4 | Pull levers in echo order | Terminal powers on, story pages appear |
| Enemy: PATROL | Watch from a distance | Walks its waypoints |
| Enemy: INVESTIGATE | Run, or search a wrong crate, within range | It heads to that spot |
| Enemy: CHASE | Step into its view cone | Red tint, it pursues around walls |
| Enemy: SEARCH | Break line of sight and hide | It searches nearby, including your worn tiles |
| Enemy: RETURN | Stay hidden about 10 s | It returns to patrol |
| Behaviour response | Walk the same corridor loop repeatedly | PATTERN rises; it gets faster and visits your favourite tile |
| Hiding | Hide while unseen | You stay safe |
| Hiding | Hide in front of it during a chase (within 3 tiles) | It drags you out (game over) |
| Death | Get caught | Game-over screen; R restarts the current area with its state reset |
| Score | Explore, collect, read and solve | Score increases; high score persists after closing the game |
| Settings | Change each option, leave and reopen | Selected settings remain saved |
| Briefing | Start a new run or enter the next area | Story and first objective appear; gameplay waits for Enter |
| Continue | Start a run, then return to title and choose Continue | Latest area checkpoint resumes |
| Replay | Choose retry/new run after death or Play Again at victory | The requested run starts cleanly |
| Pause | Esc during play | Pause menu; Retry area and Quit to title work |
| Victory | Use the terminal and reach the exit | Final chase, then the ending screen, then the title |
| Window | Resize or maximise | Image scales with sharp pixels and letterboxing |
| Assets | Delete `assets/` and run | Game still runs with procedural art |

---

## 6. Troubleshooting

| Problem | Fix |
|---|---|
| `Could not find raylib` / download fails during configure | Check your internet connection, or download the raylib 5.5 source zip manually and configure with `-DRAYLIB_SOURCE_DIR=C:/path/to/raylib-5.5`. |
| `'cmake' is not recognized` | Use the *x64 Native Tools Command Prompt for VS 2022*, or the MSYS2 UCRT64 shell, where CMake is on the PATH. |
| CMake picks the wrong compiler | Delete the `build` folder and configure again with an explicit `-G` (see section 2). |
| `undefined reference to WinMain` / `__imp_timeBeginPeriod` | You linked without CMake. Use the provided CMakeLists (it links `opengl32 gdi32 winmm`). |
| MinGW: "libgcc_s_seh-1.dll / libstdc++-6.dll / libraylib.dll not found" | Configure with `-DAFTERIMAGE_FETCH_RAYLIB=ON` for a static .exe, or create a package with `cmake --install build --prefix release` so the required MinGW and raylib DLLs are copied beside the game. |
| Game starts but the props look procedural | `assets/` isn't next to the .exe. Rebuild (CMake copies it every build) or copy the folder manually. The console line `ART: … loaded from assets/sprites` shows what loaded. |
| Black window or OpenGL error on start | Update the GPU driver. raylib needs OpenGL 3.3. On very old or virtual machines, install current graphics drivers. |
| Text or pixels look blurry | Use a window size that's a multiple of 640×360 (for example 1280×720 or 1920×1080). Fullscreen scaling is integer where possible. |
| Game too hard | Avoid running near the Tenant, vary your routes so PATTERN stays low, and hide before it sees you. Getting caught only restarts the current area. |
| Blender script errors | Use Blender 3.6 LTS or 4.x, run with `--background`, and put script arguments after `--`. |
| `pixelate_sprites.py`: `No module named PIL` | `pip install pillow` |

---

## 7. What has actually been tested

The project has an earlier Linux build/self-test record (Ubuntu 24.04, GCC 13.3, CMake 4, raylib 5.5, Xvfb with Mesa software OpenGL). The current refinement was built and exercised on Windows with MSYS2 MinGW-w64 GCC 15.2, CMake 4.2.3, raylib 5.5 and Intel UHD 770/OpenGL 3.3:

- **Clean configure and build** from an empty build folder: success. The game code compiles with `-Wall -Wextra` and **zero warnings** (the only warnings come from raylib's own C sources).
- **`--selftest`**: all 33 checks in section 5.1 **passed**, including sound asset loading, five-second jumpscare gating, adaptive profile, mimic route, malformed profile, committed attack wind-up, and one-shot catch checks.
- **Asset loading**: confirmed `ART: 21 sprites (8 loaded from assets/sprites, rest procedural)`.
- **Blender renders**: the PNGs in `assets_src/renders/` were rendered by `blender/render_props.py` in Blender Cycles during development (they carry Blender's render metadata). Blender wasn't available for a second re-render in the final environment.

The current Windows package and scripted self-test were verified after the gameplay changes. Human playtesting and the full 30-minute campaign duration have not been validated; four areas are present, but playtesting will determine how much additional content is needed to reliably reach that duration. Dual-player mode is deferred until the solo experience is complete. The current character sprites are provisional; use `docs/CHARACTER_SPRITE_BRIEF.md` to produce or source actual animation artwork. These synthetic effects are generated by the included Python script; they are not Audacity recordings or ambient music.
