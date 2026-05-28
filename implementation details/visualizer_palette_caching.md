# Visualizer Palette Caching — Implementation Details

**Date:** 2026-05-28
**Last updated:** 2026-05-28 (revised)
**Scope:** New `visualizer_color_type` modes 4, 5, 6; unified palette pre-baking for modes 0, 2, 3; per-bar magnitude coloring

---

## 1. Overview

The visualizer previously computed every bar's color on every frame (60 fps) by calling gradient/luminosity math inline in the render loop. This implementation introduces a **palette pre-baking** architecture: all color computation runs once per song change, and the 60 fps render loop becomes an array lookup with smooth interpolation.

**Key design decisions:**
- Per-bar coloring for album-art modes (4, 5, 6): each bar's magnitude maps to a palette index via linear interpolation
- Row-based coloring for legacy modes (0, 2, 3): each row has one color, same as before
- Mode 1 (per-bar variance) is the only mode that still computes color per-frame
- All palettes capped at 8 colors for consistency
- Modes 4 and 5: sorted by luminosity so low magnitude → dark, high magnitude → bright
- Mode 6: two-band selection (dark + bright) with peak vibrance at mid-magnitude, tapering at both ends

---

## 2. Data Model Changes

### 2.1 New struct: `ColorPalette`

**File:** `src/common/model.h`, lines 359–362

```c
typedef struct {
    PixelData colors[16];  // up to 16 slots (8 used)
    int count;             // number of valid entries (0–8)
} ColorPalette;
```

`PixelData` is `{ unsigned char r, g, b; int a; }` (defined in `src/utils/img_utils.h`).

The array is 16 elements for alignment simplicity, but all modes are capped at 8 colors.

### 2.2 Extended struct: `UIState`

**File:** `src/common/model.h`, line 442

```c
typedef struct {
    // ... existing fields ...
    int chroma_height;

    ColorPalette visualizer_palettes[7];  // one per color type 0–6
} UIState;
```

Full path: `model->state.ui.visualizer_palettes[color_type]`

Array has 7 entries. Index 1 is unused (`count == 0`) because type 1 is per-bar variance computed per-frame.

### 2.3 Memory cost

7 × 16 × 8 bytes = 896 bytes on the stack inside `UIState`. Negligible.

---

## 3. Palette Generation

### 3.1 Trigger: song change detection

**File:** `src/ui/visuals.c`, `draw_spectrum_visualizer_to_buf()`, lines 935–938

```c
static size_t cached_palette_song_hash = PALETTE_SONG_HASH_UNSET; // (size_t)-2

if (model->current_hash != cached_palette_song_hash) {
    cached_palette_song_hash = model->current_hash;
    generate_all_visualizer_palettes(model, height);
}
```

Runs one check per frame; regeneration only triggers once per song change.

### 3.2 Entry point: `generate_all_visualizer_palettes()`

**File:** `src/ui/visuals.c`, lines 329–369

1. Resolves the base color from `ui->color` (album) or `ui->theme.trackview_visualizer.rgb` (theme)
2. Calls `generate_legacy_palette()` for modes 0, 2, 3
3. Sets mode 1's count to 0
4. Reads `model->songdata->cover` (RGBA pixel data, 4 channels)
5. If cover exists: generates modes 4, 5, 6 with luminosity capping and sorting
6. If no cover: sets all album-art palette counts to 0

### 3.3 Legacy palette (modes 0, 2, 3)

**File:** `src/ui/visuals.c`, lines 49–60

Pre-bakes the existing math functions, preserving pixel-perfect parity:

- Mode 0: `increase_luminosity_for_height(base, height, j, false)` — bright top-to-bottom
- Mode 2: `increase_luminosity_for_height(base, height, j, true)` — bright bottom-to-top
- Mode 3: `get_gradient_color(base, j, height, 1, 0.6f)` — gradient fade

Palette has `min(height, 16)` entries (typically 4–5 for a default visualizer height of 5).

### 3.4 K-Means palette (mode 4)

**File:** `src/ui/visuals.c`, lines 66–142

- Downsamples cover to 400 pixels, runs K-Means with K=8 for 20 iterations
- Initialization: evenly-spaced pixels (deterministic, no randomness)
- Sorted by luminosity after generation via `sort_palette_by_luminosity()`

### 3.5 Color binning palette (mode 5)

**File:** `src/ui/visuals.c`, lines 148–205

- Quantizes all pixels into a 32×32×32 grid (`BIN_SHIFT = 5`)
- Extracts top 12 most populated bins, then caps to 8
- Sorted by luminosity after generation via `sort_palette_by_luminosity()`

### 3.6 Two-band vibrant palette (mode 6)

**File:** `src/ui/visuals.c`, lines 257–310

The most complex generator. Addresses the problem that the vibrance formula `chroma * (1 - |lum - 128| / 128)` heavily favors mid-bright pixels, excluding dark but representative album colors.

**Algorithm:**

1. Two independent top-4 selectors: `dark[]` for pixels with `lum < 80`, `bright[]` for `lum >= 80`
2. Single pass through sampled pixels. Each pixel routes to the appropriate band based on luminance
3. Within each band, insertion sorted by vibrance score (highest first, so `band[0]` = most vibrant)
4. Minimum RGB distance check (threshold = 60) — rejects pixels too similar to already-selected entries in the same band
5. Palette assembly with a **peak-vibrance-at-midpoint** layout:
   - Indices 0–3: dark band, **reversed** (least vibrant → most vibrant as magnitude increases)
   - Indices 4–7: bright band, **as-is** (most vibrant → least vibrant as magnitude increases)
6. No shared sort helper needed — ordering is achieved through assembly direction

**Palette layout relative to bar magnitude:**

```
magnitude:  low ─────────────────────────────────── high
            │                                         │
dark:       least vibrant ──────────► most vibrant   │
            └────────────────────────┘                │
                                  ┌────────────────────┘
bright:                           most vibrant ──► least vibrant
```

Vibrance peaks at the midpoint (transition from dark to bright band) and tapers at both ends. Visually: quiet bars are muted/dark, mid-volume bars are the most vivid, and the loudest bars are bright but less saturated.

**Helper functions:**

```c
typedef struct { float score; unsigned char r, g, b; } VibEntry;

void init_vib_entries(VibEntry *entries, int n);
// Sets all scores to -1.0

int insert_vib_entry(VibEntry *entries, int count, int max,
                     float vibrance, unsigned char r, unsigned char g, unsigned char b);
// Finds insertion position, checks min distance, inserts, returns new count
```

---

## 4. Rendering

### 4.1 Buffer rendering (the active path)

**File:** `src/ui/visuals.c`, `draw_spectrum_to_buf()`, lines 780 onwards

**Legacy modes (0, 2, 3) — row-based:**

```c
if (visualizer_color_type <= 3) {
    pidx = j - 1;  // direct row-to-palette mapping
    if (pidx >= palette->count) pidx = palette->count - 1;
}
row_color = palette->colors[pidx];
```

All bars in the same row share the same color. Unchanged from original behavior.

**Album-art modes (4, 5, 6) — per-bar with interpolation:**

```c
if (palette->count == 1 || height <= 1) {
    bar_color = palette->colors[0];
} else {
    // Map magnitude to fractional palette position
    if (magnitudes[i] <= 1.0f)
        scaled = 0.0f;
    else if (magnitudes[i] >= (float)height)
        scaled = (float)(palette->count - 1);
    else
        scaled = (magnitudes[i] - 1.0f) * (palette->count - 1) / (height - 1);

    int lo = (int)scaled;
    int hi = lo + 1;
    if (hi >= palette->count) hi = palette->count - 1;
    float frac = scaled - (float)lo;

    // Linear interpolation between adjacent palette entries
    bar_color = (PixelData){
        .r = (unsigned char)(c0.r + (int)((c1.r - c0.r) * frac)),
        .g = (unsigned char)(c0.g + (int)((c1.g - c0.g) * frac)),
        .b = (unsigned char)(c0.b + (int)((c1.b - c0.b) * frac)),
        .a = 255
    };
}
```

Each bar gets its own color based on its magnitude. The interpolation between `colors[lo]` and `colors[hi]` produces smooth color transitions as magnitude changes, avoiding the abrupt snapping that caused flickering in earlier versions.

Low magnitude → low palette index → dark color. High magnitude → high palette index → bright color.

**Mode 1 (per-bar variance) — unchanged:**

```c
PixelData tmp = {color.r / 2, color.g / 2, color.b / 2, 255};
tmp = increase_luminosity(tmp, (int)round(magnitudes[i] * 10 * 4));
bar_style = cell_style_fg(tmp);
```

Still computes color per-bar per-frame using the original `increase_luminosity()` function.

### 4.2 Terminal rendering (dead code)

**File:** `src/ui/visuals.c`, `print_spectrum()`

Renders directly via ANSI escape sequences. Not called from the component system. **Not modified.**

---

## 5. Architecture and Data Flow

### 5.1 Data flow diagram

```
update.c (MSG_TICK)
  └── sets model->current_hash, model->songdata (including cover pixels)
        │
        ▼
components.c (component_visualizer)
  └── draw_spectrum_visualizer_to_buf(Model *model, ...)     ← non-const
        │
        ├── [if song changed:] generate_all_visualizer_palettes(model, height)
        │       ├── reads model->state.settings.color or .theme
        │       ├── reads model->songdata->cover (RGBA pixels)
        │       ├── writes model->state.ui.visualizer_palettes[0,2,3,4,5,6]
        │       │       └── mode 4,5: sorted by luminosity after generation
        │       │       └── mode 6: two-band dark→bright (already ordered)
        │       └── if no cover: modes 4,5,6 have count=0
        │
        └── draw_spectrum_to_buf(const Model *model, ...)   ← const (read-only)
              ├── legacy modes: reads palette per-row
              ├── album-art modes: reads palette per-bar with interpolation
              └── mode 1: computes per-bar per-frame (unchanged)
```

### 5.2 The `const` violation

`component_visualizer()` takes `const Model *model` — the component system declares components read-only. `draw_spectrum_visualizer_to_buf` needs `Model *` to populate the palette cache. Fixed with a cast in `components.c`:

```c
draw_spectrum_visualizer_to_buf((Model *)model, buf, sound_sys, ...);
```

This is safe because the palettes are a cache (idempotent, hash-guarded). A cleaner alternative would be moving palette generation to the update tick.

---

## 6. Graceful Degradation

When cover art is unavailable: palettes 4, 5, 6 have `count = 0`. The render loop's `palette->count > 0` guard causes a fallback to the base color (same as modes 0, 2, 3). Legacy modes always work since they only need the base color.

---

## 7. Bug Fixes During Development

### 7.1 Top-N first-pixel rejection (mode 6)

The original implementation had a bug where `pos` was initialized to `-1` and only set when `vibrance > top[p].score`. When `top_count == 0`, the loop never executed and `pos` stayed `-1` — meaning the first pixel was never inserted, and `top_count` never grew from 0. The entire palette was always empty.

**Fix:** After the loop, if `pos == -1` and there's still room (`count < max`), set `pos = count` to append at the end.

### 7.2 Flickering colors (modes 4, 5, 6)

The first implementation snapped each bar to the nearest palette index via integer truncation:
```c
pidx = (int)(magnitudes[i] * palette_count / height);
```
As magnitude changed smoothly, `pidx` jumped discretely between entries, causing visible flickering.

**Fix:** Fractional interpolation between the two nearest palette entries:
```c
scaled = (magnitudes[i] - 1.0f) * (palette->count - 1) / (height - 1);
lo = (int)scaled;
frac = scaled - (float)lo;
color = lerp(colors[lo], colors[lo+1], frac);
```

### 7.3 Undeclared variable scope (modes 4, 5, 6)

`bar_color` was declared inside `if`/`else` branches but referenced outside them for the `cell_style_fg()` call.

**Fix:** Declared `bar_color` at the top of the block, assigned within branches.

---

## 8. Files Changed

| File | Change |
|---|---|
| `src/common/model.h` | Added `ColorPalette` struct; added `visualizer_palettes[7]` to `UIState` |
| `src/ui/visuals.h` | `draw_spectrum_visualizer_to_buf` signature: `const Model *` → `Model *` |
| `src/ui/visuals.c` | Added 1 public + 7 static functions; song-change trigger; per-bar interpolation rendering; luminosity sorting; two-band vibrant selection |
| `src/ui/components.c` | `(Model *)` cast in `component_visualizer` |

---

## 9. Possible Future Improvements

1. **Move palette generation to the update tick** — restores `const` correctness for the entire rendering layer.
2. **Update `print_spectrum()` to use palettes** or remove it if dead code.
3. **Make palette size configurable** — currently fixed at 8.
4. **Add more color modes** — the architecture makes it trivial: add a generator and wire it in.
5. **Move `VibEntry` and helpers to `img_utils`** if other parts of the code need color analysis.
