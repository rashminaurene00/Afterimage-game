# AFTERIMAGE character sprite production brief

The game uses a 320×180 pixel-art world rendered on a 16 px tile map. Characters are drawn 16 px wide on screen, so larger source cells retain detail while the renderer point-filters them down.

## Production atlas contract

Create transparent PNGs in `assets/sprites/`:

| File | Cell | Grid | Canvas |
|---|---:|---:|---:|
| `player_atlas.png` | 32×48 | 8 columns × 4 rows | 256×192 |
| `tenant_atlas.png` | 40×48 | 8 columns × 4 rows | 320×192 |

Rows are down, up, left, right, matching `Player::dir` values 0–3. Columns: idle (0), four walk poses (1–4), two run poses (5–6), and turn/interaction-ready (7). Loop the walk poses 1→2→3→4. Keep a consistent centerline and feet baseline in every frame; the bottom pixels are the floor contact. Leave transparent padding around the silhouette.

The protagonist must keep consistent hair, coat, palette, and a recognizable prop/silhouette in all directions. The Tenant needs a readable hunched posture, unnaturally long arms, and its signature red eye slit. Its mimic uses the protagonist atlas with a red eye tell. Attack, damage, and jumpscare may be added as separate transparent frames or short sheets.

## Art direction and export

- 3/4 top-down view with feet and head visible; don't adapt a front-facing portrait pose.
- Muted violet, charcoal, faded bone, and restrained crimson. Avoid large pure-black areas that disappear under game lighting.
- Clustered pixel shading, readable anatomy, and clear silhouettes at 16 px displayed width. Preview every cell after downscaling.
- Redraw for each pose. Do not stretch a single illustration or translate a static image to fake a walk cycle.
- Export straight-alpha PNG with no background, grid, labels, drop shadow, or baked-in lighting.
- Use an illustration as art direction only; create gameplay-camera-compatible animation frames.

## Integration status

The loader accepts the two exact atlas names, checks the 8×4 grid, and uses point filtering. It draws at 16 px wide and anchors to the existing floor/collision position. Until actual atlases are delivered, the game continues using the small existing player and Tenant sprites as provisional fallback art. Those are not finished character art. The current procedural export is for props/fallbacks, not a replacement for the character animation work. No image-generation tool was available during this implementation.

## Audio production and runtime

Raylib initializes the audio device and plays generated `footstep.wav`, `sprint_clatter.wav`, and `breathing.wav` cues during play. On catch it layers `death_stinger.wav` over a five-second `jumpscare_bed.wav`, matching the full-screen face animation. `tools/generate_death_stinger.py` deterministically creates these original synthetic mono PCM files. For recorded or edited production sound, Audacity can trim, clean, equalize, add reverb, gain-stage and export WAV/OGG assets; it is an authoring tool, not the game's playback engine. These cues are not Audacity recordings, and ambient music remains absent.
