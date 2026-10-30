# AFTERIMAGE — Design Document

**Formal title:** Design and Development of a Behaviour-Responsive 2D Survival Horror Game Using C++
**Framework:** raylib 5.5 · **Language:** C++17 · **Art pipeline:** Blender props and validated character PNG atlases → raylib · **Platform:** Windows PC · **Audio:** raylib plays generated movement, breathing, and jumpscare cues

---

## 1. Story

### Premise
You wake in a bed in **Apartment 4C, Halvorsen Court**, during a storm. You don't remember coming here. A note on the floor, in your own handwriting, says this is **VISIT 47**.

### What is really happening (revealed gradually)
- You keep returning to this building looking for **Ilse**, the person who lived in 4C with you. She moved out six years ago. The archive holds 112 letters from her, all addressed to you, all unopened.
- Every visit that ended in failure left a **residue**: a copy of your habits, routes and posture. The building files these copies away. They accumulated, and eventually they began to walk.
- **The Tenant**, the tall figure hunting you, is that residue. It isn't hunting you so much as trying to *become* you. This is why it learns the routes you repeat: they were always its routes too.

### Structure
| Act | Area | Beats |
|---|---|---|
| Beginning | Apartment 4C | Wake up, find the "VISIT 47" note, notice that a room has changed, learn the afterimage, restore power, glimpse a tall figure in the hall that stands exactly like you. |
| Middle | The Repeating Corridor | A corridor too long for the building. The east end loops you back to the west. The Tenant patrols. You must find what it moved to get the ARCHIVE key. |
| Climax | The Archive | Records reveal the visit count, Ilse's letters and the nature of the residue. The terminal explains everything. The final ward opens, and the Tenant unfolds between the shelves. |
| Ending | The Residue Ward and Outside | Find Ilse's last letter in the room where the earliest copy was stored. Use it to release the stair door. You step into the rain; the building keeps the version of you that stayed. |

### Opening sequence
The title menu offers **Continue** and **New Game**. A new run opens a paused briefing with the premise, immediate objective, movement/interact controls and survival advice. Each area transition has a fresh story and objective briefing. Gameplay advances only after the player confirms the briefing, so the story and next task are clear before danger resumes.

---

## 2. Areas, puzzles and progression

### Area 1 — Apartment 4C (the Tenant wakes after 40 seconds; teaches the controls)
1. **Movement and interaction:** bed, note, table and chair can all be examined with **E**.
2. **Afterimage introduction (essential puzzle 1):** entering the living room triggers an event. The bulb stutters and a cabinet slides across the floor in the dark. The afterimage unlocks, and the game explains **Q**. Pressing Q shows ghost outlines of where things were, with moved objects outlined in red. Searching the **moved cabinet** yields the **Fuse**. The other cabinets are empty.
3. **Power puzzle (essential puzzle 2):** the fuse goes into the fuse box in the kitchen (south-east). Power returns and the front door's electronic lock releases.
4. **First hunt:** the Tenant starts in the living room and wakes after 40 seconds. It patrols the room and can chase through the apartment; a saved movement trace can make it mimic a route from an earlier visit.
5. **Apparition:** after power is restored, a tall afterimage appears briefly in the west hall.
6. Exit through the **front door** (west side of the hall).

### Area 2 — The Repeating Corridor (enemy active)
- A long corridor with seven rooms per side. Walking off the **east end loops you back to the west** (the "repeating" corridor). The message escalates on each loop.
- **Changed-object puzzle (essential puzzle 3):** eight crates sit in side rooms. The Tenant moved one while you were elsewhere. Q reveals which crate differs from the building's snapshot, and that crate holds the **Brass Key**.
- **Risk/reward:** searching a wrong crate is **loud**. It creates a noise event and the Tenant comes to investigate.
- **Hiding and evasion:** wardrobes and a dark alcove, doors to break line of sight, and a sprint that is fast but noisy.
- Unlock the **ARCHIVE door** (north wall, east end) with the key.

### Area 3 — The Archive (enemy active; story reveal; finale)
- Shelving aisles break line of sight. The space has a wardrobe and an alcove for hiding, plus three record notes that tell the story.
- **Lever sequence (essential puzzle 4):** four relay levers must be thrown in the order the building remembers. That order is **only visible in the afterimage**: pressing Q near the levers shows ghost numbers left by your own hands on earlier visits. The order is **c → a → d → b**. A wrong lever snaps all four back and makes a loud noise.
- **Terminal reveal:** once powered, the terminal shows the multi-page reveal.

### Area 4 — The Residue Ward (last record; final exit)
- The Tenant's oldest copy is stored here. The briefing sets the immediate goal: find Ilse's letter in the archive drawer and use it to release the outside stair door.
- The Tenant patrols the ward. Its 8-second wake-up period gives the player a moment to get oriented after entering; it can still catch a player who stays in danger.
- The stair door leads outside and completes the ending.

### Complete gameplay loop
Title → story/objective briefing → explore → find items → solve puzzles → encounter the enemy → hide or escape → use the afterimage → open the Residue Ward → find Ilse's letter → escape.

---

## 3. Signature mechanic: the Afterimage (`src/Afterimage.h/.cpp`)

The building "remembers". The system stores two kinds of history with plain `std::vector` / `std::deque` data. It uses no machine learning and no image processing.

### 3.1 Object snapshot (puzzle-solving)
- When the player enters an area, `recordPast()` copies every object's `{id, tile x, tile y, open, on, present}` into a `std::vector<ObjSnap>`.
- Scripted events (the cabinet sliding in Area 1, the moved crate in Area 2) then change real object data.
- `moved(o)` / `stateChanged(o)` compare an object with its snapshot entry.
- **Pressing Q** (when unlocked and off cooldown) activates the afterimage for **4 seconds**, followed by a **9-second cooldown**. While active, the renderer:
  - draws ghosts at each snapshot position,
  - outlines changed objects in red,
  - shows lever "echo" numbers in the archive,
  - shows the player's recent trail and worn tiles.
- The HUD explains the marks while Q is active: ghost = old position; red = changed object; green numbers = relay order. The context hint names the object to search or the puzzle action to perform.
- The HUD bar labelled **[Q] AFTERIMAGE** shows whether it is ready, active or cooling down.

### 3.2 Behaviour history (enemy adaptation)
- `notePlayerTile()` increments a **per-tile visit counter** (heat map) each time the player enters a tile, and keeps a short trail of recent tiles.
- Every visit to a tile beyond the second adds 1 to the **repetition score**. `patternLevel() = min(1, score / 120)`.
- The HUD shows this as **PATTERN**, so players can see they are becoming predictable. The visit heat map is run-local; a compact profile stores repeated route transitions, hiding places, sprint events and successful escapes across sessions. A separate `afterimage_movement_history.dat` atomically stores up to 132 seconds of sampled positions, direction, movement, sprinting and hiding for each area. It is loaded on the next launch, allowing a matching-area route from before closing to be replayed.

---

## 4. Enemy AI: "The Tenant" (`src/Enemy.h/.cpp`, `src/Pathfinding.cpp`)

### 4.1 Finite state machine
```
           sees player (suspicion = 1)
 PATROL ─────────────────────────────► CHASE ◄──────────┐
   ▲  │ noise / habit check              │ lost sight     │ sees player
   │  ▼                                  ▼ > 1.2 s        │
   │ INVESTIGATE ── arrives ──────────► SEARCH ───────────┘
   │                                     │ timer (10 s) ends
   └────────────── RETURN ◄──────────────┘
```

| State | Behaviour | Exit condition |
|---|---|---|
| **PATROL** | Walks the level's waypoints (`'0'`–`'9'` in the map) in order. Every 14 s it considers worn tiles and sufficiently repeated, persisted habits. | Suspicion reaches 1 → CHASE. Noise or habit check → INVESTIGATE. |
| **INVESTIGATE** | Paths to the noise location or to the hottest tile. | Sees player → CHASE. Arrives → SEARCH, or RETURN if nothing is there. |
| **CHASE** | Re-paths to the player (or their last known position) frequently, at 64 px/s × behaviour multiplier. | Loses sight for more than 1.2 s → SEARCH. Touches player → caught. |
| **SEARCH** | Visits nearby search points for 10 s, **favouring tiles the player has walked often**. | Sees player → CHASE. Timer ends → RETURN. |
| **RETURN** | Paths back to the nearest waypoint. | Arrives → PATROL. |
| **MIMIC** | In all four areas, follows a bounded chronological segment of the route recorded about two minutes earlier, through ordinary A* navigation. The rolling 132-second trace is saved between launches, so a prior visit can supply the first replay after restarting. It does not use the player's current position as its destination. | Route ends, becomes unavailable, or current evidence overrides it. |
| **ATTACK** | Committed short wind-up followed by one catch event; close-range suspicion cannot reset the timer. | Catch event plays the death stinger and opens the jumpscare/game-over sequence. |

State transitions are logged (`AI: PATROL -> CHASE`) to the console for debugging. **F1** shows the AI's current path and sight radius in-game.

### 4.2 Detection
- **Range:** `(6 + 2 × patternLevel) × TILE` pixels.
- **View cone** of about 110° in the facing direction.
- **Line of sight:** a tile-stepped ray. Walls, closed doors and shelves block it.
- **Suspicion meter:** seeing the player fills it (faster when close) and it decays at 0.3/s when the player is out of sight. Chase starts only at 1.0, which gives a beginner a short moment to break line of sight.
- Hidden players cannot be seen.

### 4.3 Movement
- **Grid A\*** (4-directional, Manhattan heuristic, `std::priority_queue`) over the tile map. The same tile data drives collision, so walls are consistent for the player and the enemy.
- The Tenant opens unlocked doors on its way.

### 4.4 Hiding rules
- Hiding in a wardrobe or alcove makes the player invisible.
- **Exception:** if the Tenant is chasing, can see the player, and is within 3 tiles when they climb in, it saw them hide and drags them out. The game warns about this in the notes, so the rule is fair.

### 4.5 Behaviour response (deterministic, not machine learning)
| Input | Effect |
|---|---|
| Repetition score (`patternLevel` 0–1) | Speed × (1 + 0.30 × level); sight range + up to 2 tiles |
| Repeated route/hiding use (thresholded profile) | Every 14 s, influences patrol target selection; repeated hiding locations bias bounded search candidates |
| Visit counts near the last known position | Search points prefer worn tiles |
| Sprinting, wrong crate, wrong lever | Noise event → INVESTIGATE at that position |
| Two-minute movement memory in Archive / Ward | Once a historical segment exists, the Tenant follows reachable A* route samples in order and uses recorded pace cues. It does not copy puzzle state or use current player location for playback. The trace is temporary and level-local. |

### 4.6 Fairness
- Chase speed (64 px/s) is faster than walking (58) but slower than sprinting (92). Sprint costs stamina and makes noise.
- The Tenant's **red eyes stay visible through the darkness**, and the HUD shows a red danger tint when it has detected you.
- Being caught restarts the **current area** from its entry checkpoint, not the whole game.

The route/hiding summary is bounded and stored separately in `afterimage_ai_profile.dat` next to the executable. It is versioned and validated on load, and written at shutdown and selected checkpoints. `--fresh-ai` resets only this profile. The raw two-minute trace remains temporary. Persistent adaptation changes target priorities; it does not grant exact current-position knowledge or make mandatory objectives unreachable.

---

## 5. Technical architecture

```
main.cpp ──► Game (state machine: Title / Controls / Playing / Reading / Paused / GameOver / Victory)
              ├── World (Level: tiles, objects, collision, markers, waypoints)  ◄── Levels.cpp (map data + story text)
              ├── Player        (movement, collision, stamina, hiding)
              ├── Enemy         (FSM)  ──► Pathfinding (grid A*)
              ├── Afterimage    (object snapshots, heat map, trail, activation timer)
              ├── Art           (PNG loader with procedural pixel-art fallback)
              ├── Render.cpp    (world at 320×180: tiles, sprites, darkness, ghosts, screen FX)
              └── UI.cpp        (HUD and menus at 640×360: objective, inventory, prompts, menus, ending)
```

| File | Responsibility |
|---|---|
| `src/main.cpp` | Window, fixed 60 FPS loop, `--selftest` and `--export-sprites` flags |
| `src/Config.h` | Resolutions, palette, gameplay tuning constants |
| `src/World.h/.cpp` | `Level`, `Object`, tile queries, collision, line of sight, door/object blocking |
| `src/Levels.cpp` | The three ASCII maps, object placement and all in-world story text |
| `src/Player.h/.cpp` | Input, axis-separated collision, sprint/stamina, facing and animation |
| `src/Enemy.h/.cpp` | The Tenant's FSM, perception, search planning, behaviour response |
| `src/Pathfinding.h/.cpp` | Grid A* |
| `src/Afterimage.h/.cpp` | Snapshots, change detection, heat map, repetition score |
| `src/Art.h/.cpp` | 21 sprite keys plus validated player/Tenant atlases: loads assets or generates fallback pixels |
| `src/Game.h/.cpp` | Game states, interaction, inventory, puzzles, objectives, checkpoints, triggers, self test |
| `src/Render.cpp` | World rendering and visual effects |
| `src/UI.cpp` | HUD, title, controls, pause, reading panel, game over, victory |

### Character art status
The current player and Tenant images are prototype placeholders. The atlas loader supports a transparent 8×4 frame grid, and [`CHARACTER_SPRITE_BRIEF.md`](CHARACTER_SPRITE_BRIEF.md) specifies frame dimensions, directions, animation columns, pose alignment, palette, and export requirements. The runtime uses the existing small sprites until actual atlas PNGs are supplied. No image-generation tool was available during this implementation, so the fallback is not described as finished illustration-quality work.

### Audio status
The game initializes raylib audio when a device is available, loads the generated footstep, sprint-clatter, breathing, death-stinger, and five-second `jumpscare_bed.wav` cues, and unloads them on shutdown. Walking and sprinting trigger separate footfalls; low stamina and active pursuit trigger breathing. Catching the player starts a sharp stinger and a sustained five-second horror bed aligned with the full-screen creature face. `tools/generate_death_stinger.py` deterministically generates these original mono PCM effects. They are synthetic game cues, not Audacity recordings or ambient music.

### Rendering approach
- The world renders to a **320×180 render texture** with point filtering. The UI renders to a 640×360 texture. Both are scaled to the window with letterboxing.
- **Darkness:** the player's light radius is the only bright area, and it flickers slightly.
- **Effects:** scanlines, light noise, occasional flicker, red danger tint during chase, ghost outlines during the afterimage, and an afterimage trail behind the Tenant itself.

### Map representation
Each level is an array of strings. Legend: `#` wall, `.` planks, `,` carpet, `~` tile, `P` player start, `E` enemy spawn, `0`–`9` patrol waypoints, `D` door, `W` wardrobe, `H` alcove, `x` shelf. Other letters are handled per level (cabinets, crates, levers, notes, exits, script markers). Because collision, line of sight and A\* all read the same tile and object data, they always agree.
