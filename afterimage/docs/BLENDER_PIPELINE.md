# Blender → raylib Asset Pipeline

Blender is used **only as an authoring tool**. The game never needs Blender at runtime: it loads PNG files from `assets/sprites/`. Any sprite that is missing is generated procedurally as pixel art, so the game always runs.

```
blender/render_props.py           tools/pixelate_sprites.py          src/Art.cpp
 (builds + renders 3D props)  ──►  (downscale, palette, outline)  ──►  LoadTexture()
 assets_src/renders/*.png          assets/sprites/*.png                (or procedural fallback)
   64×64 / 64×96 RGBA                16×16 / 16×24 RGBA
```

## 1. What is made in Blender

`blender/render_props.py` builds nine original low-poly models from primitives (cubes, cylinders, spheres). One Blender unit equals one game tile.

| Key | Model | Render size | Game sprite | Shipped in `assets/sprites/`? |
|---|---|---|---|---|
| `wardrobe` | Two-door wardrobe (hiding spot) | 64×96 | 16×24 | Yes |
| `cabinet` | Low cabinet with drawers | 64×64 | 16×16 | Yes |
| `crate` | Plank crate with lid | 64×64 | 16×16 | Yes |
| `chair` | Wooden chair | 64×64 | 16×16 | Yes |
| `table` | Table with two cups | 64×64 | 16×16 | Yes |
| `bed` | Bed with pillow and blanket | 64×64 | 16×16 | Yes |
| `fusebox` | Wall fuse box | 64×64 | 16×16 | Yes |
| `tenant` | The enemy: tall, thin, long arms, head tilted 18° "as if listening" | 64×96 | 16×24 | Yes |
| `shelf` | Archive shelf | 64×64 | 16×16 | No. At 16 px the files became muddy, so the procedural shelf is used instead. |

The renders in `assets_src/renders/` were produced by this script in Blender (each PNG carries Blender's Cycles render stamp in its metadata).

## 2. Render settings and why

| Setting | Value | Reason |
|---|---|---|
| Camera type | **Orthographic** | No perspective distortion, and every prop stays the same scale (1 unit = 1 tile) |
| Camera angle | Tilted **30° from straight down**, looking north | Matches the game's 3/4 top-down view, so the fronts of furniture are visible |
| Ortho scale | 1.12 for 1-tile props, 1.62 for the wardrobe, 2.0 for the Tenant | Fills the frame so the downscale keeps maximum detail |
| Engine | **Cycles, CPU, 24 samples, no denoising** | Works headless on any PC without a GPU. Low sample noise disappears after downscaling. |
| Pixel filter | **Box, 0.01 px** | Crisp edges. Soft anti-aliased edges would leave dirty fringes in pixel art. |
| Lighting | One **Sun** (energy 3, rotated 35° / –35°) for upper-left light, plus a **violet world background** (0.10, 0.06, 0.16) as ambient fill | Shadows fall into the game's purple range instead of neutral grey |
| Materials | Flat Principled materials in palette colours | Colours stay close to the palette before quantising |
| Background | **Film → Transparent**, PNG **RGBA** | Sprites drop straight onto the tile map |
| Colour management | View transform **Standard** | Filmic/AgX would wash out the palette colours |
| Resolution | **64×64** (tall props: **64×96**) | Four times the final size, so area-averaged downscaling gives clean pixel shapes |

## 3. Pixel-art conversion (`tools/pixelate_sprites.py`)

For each render the script:
1. crops the image to the visible pixels and anchors the object bottom-centre,
2. downscales it with area averaging to **16 px wide** (**24 px tall** for `wardrobe` and `tenant`),
3. hard-thresholds alpha, because pixel art has no soft edges,
4. snaps every colour to the **nearest palette colour** (the game palette plus a few wood and fabric shades),
5. adds a **1 px dark outline** so silhouettes read in the dark.

It requires Python 3 and Pillow (`pip install pillow`).

## 4. Step-by-step commands

From the project root, with Blender 3.6 LTS or 4.x:

```bat
:: 1. Render all props (Blender runs without opening a window)
"C:\Program Files\Blender Foundation\Blender 4.2\blender.exe" --background --python blender\render_props.py -- --out assets_src\renders

::    or only some of them:
blender --background --python blender\render_props.py -- --out assets_src\renders --only wardrobe,tenant

:: 2. Convert renders to game sprites
python tools\pixelate_sprites.py assets_src\renders assets\sprites

:: 3. Rebuild (CMake copies assets\ next to the .exe on every build)
cmake --build build --config Release
```

If you don't want one of the converted sprites (as with `shelf`), delete it from `assets/sprites/`. The game falls back to the procedural version.

To see every procedural sprite as a reference sheet for artists, run:
```bat
afterimage.exe --export-sprites
```
This writes all 21 procedural sprites to `sprites_procedural/`.

## 5. Naming convention and folders

```
assets_src/renders/<key>.png     raw Blender renders (source art, not loaded by the game)
assets/sprites/<key>.png         game-ready sprites (loaded by the game)
```
- `<key>` is lowercase with underscores and must match a key in `src/Art.cpp`. The full list is: `player tenant door door_open exit_locked exit_open cabinet crate fusebox fusebox_on wardrobe alcove note bed table chair shelf switch_off switch_on terminal terminal_on`.
- Sprites are 16 px wide. Height is 16 px for floor props and 24 px for tall ones. Any size works technically, because sprites are drawn bottom-anchored on their tile, but 16 px wide keeps the art consistent.
- State variants use a suffix, for example `fusebox` and `fusebox_on`, or `door` and `door_open`.

## 6. How the game loads them (`src/Art.cpp`)

```cpp
void Art::load() {
    for (int i = 0; i < kKeyCount; ++i) {
        std::string key = kKeys[i];
        std::string file = "assets/sprites/" + key + ".png";
        Texture2D t{};
        bool fromFile = false;
        if (FileExists(file.c_str())) {
            t = LoadTexture(file.c_str());          // Blender-rendered sprite
            fromFile = t.id != 0;                   // a corrupt PNG also falls back
        }
        if (!fromFile) {
            Image im = Generate(key);               // procedural pixel-art fallback
            t = LoadTextureFromImage(im);
            UnloadImage(im);
        }
        SetTextureFilter(t, TEXTURE_FILTER_POINT);  // keep pixels sharp when scaled
        tex[key] = t;
        loadedFromFile[key] = fromFile;
    }
}
```
`main.cpp` changes the working directory to the executable's folder, so `assets/` is found however the game is launched. At start-up the console prints a line such as `ART: 21 sprites (8 loaded from assets/sprites, rest procedural)`, which confirms which art is in use.
