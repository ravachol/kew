# Visualizer Mode Cycling — Implementation Details

**Date:** 2026-05-29
**Last updated:** 2026-05-29
**Scope:** Mode cycling hotkey (`v`), `visualizer_mode` state, config migration from `visualizerEnabled`/`visualizer_color_type`, footer notification

---

## 1. Overview

The visualizer previously had a binary on/off toggle bound to `v`. This was replaced with a **6-state cycle**: off → lighten → reversed → k-means → binning → vibrant → off. Each press of `v` advances to the next mode, and a transient notification in the footer displays the new mode name.

The old `visualizerEnabled` (bool) + `visualizer_color_type` (int) were consolidated into a single `visualizer_mode` field (int 0–5). The old config key `visualizerColorType` is migrated on load so existing `kewrc` files continue to work.

**Key design decisions:**
- Single `visualizer_mode` int replaces two separate fields (`visualizerEnabled` + `visualizer_color_type`)
- `visualizer_color_type` (palette index 0–4) is derived as `visualizer_mode - 1` when mode > 0
- Mode 0 = off was not representable by the old `visualizerColorType` key (which only existed when the visualizer was on), so migration maps old color type N → mode N+1
- Notification uses the existing `set_error_message()` / footer system — no new UI components needed
- Key binding is user-configurable via `toggleVisualizer=<key>` in `kewrc`, not hardcoded

---

## 2. Data Model Changes

### 2.1 Replaced fields in `UISettings`

**File:** `src/common/model.h`, line 299

Old:
```c
bool visualizerEnabled;   /**< Show spectrum visualizer. */
int  visualizer_color_type; /**< Color layout mode for the spectrum visualizer. */
```

New:
```c
int visualizer_mode;  /**< Visualizer mode: 0=off, 1=lighten, 2=reversed, 3=k-means, 4=binning, 5=vibrant. */
```

### 2.2 Replaced fields in `AppSettings`

**File:** `src/common/model.h`, lines 481–482

Old:
```c
char visualizerEnabled[2];
char visualizer_color_type[2];
```

New:
```c
char visualizer_mode[2];
char visualizer_color_type[2];  // kept for config migration only
```

`visualizer_color_type` remains in `AppSettings` solely to read the legacy `visualizerColorType` config key. It is never written out and is not present in `UISettings`.

### 2.3 Derived field at runtime

`visualizer_color_type` (palette index) is not stored in `UISettings`. It is derived on the fly:

```c
int visualizer_color_type = ui->visualizer_mode > 0 ? ui->visualizer_mode - 1 : 0;
```

This is computed locally in `draw_spectrum_to_buf()` (`src/ui/visuals.c`, line 851) and in the now-dead `print_spectrum()` (line 714).

---

## 3. Message and Command Flow (MVU Architecture)

The implementation follows the project's Model-View-Update pattern:

```
User presses 'v'
  → input.c: map_tb_key_to_event() → MSG_CYCLE_VISUALIZER_MODE
    → update.c: update() handles MSG_CYCLE_VISUALIZER_MODE
        ├── model->state.settings.visualizer_mode++ (with wrap at 5→0)
        ├── persist to settings->visualizer_mode
        ├── set_dirty(DIRTY_ALL)
        └── result.cmd.type = CMD_CYCLE_VISUALIZER_MODE
      → kew.c: player_tick() calls run_command(result)
        → effects.c: run_command() handles CMD_CYCLE_VISUALIZER_MODE
            ├── looks up mode name from static array
            ├── formats "visualizer: <name>" string
            └── set_error_message(buf) → displayed in footer
```

### 3.1 Message type

**File:** `src/common/events.h`, line 65

```c
MSG_CYCLE_VISUALIZER_MODE
```

### 3.2 Command type

**File:** `src/update/effects.h`, line 20

```c
CMD_CYCLE_VISUALIZER_MODE,
```

### 3.3 State mutation (update.c)

**File:** `src/update/update.c`, lines 450–459

```c
case MSG_CYCLE_VISUALIZER_MODE:
        model->state.settings.visualizer_mode++;
        if (model->state.settings.visualizer_mode > 5)
                model->state.settings.visualizer_mode = 0;
        snprintf(settings->visualizer_mode,
                 sizeof(settings->visualizer_mode), "%d",
                 model->state.settings.visualizer_mode);
        set_dirty(DIRTY_ALL);
        result.cmd.type = CMD_CYCLE_VISUALIZER_MODE;
        break;
```

Pure state transition: increments the mode, persists to `AppSettings`, marks UI dirty, emits a command. No side effects.

### 3.4 Side effect — notification (effects.c)

**File:** `src/update/effects.c`, lines 200–211

```c
case CMD_CYCLE_VISUALIZER_MODE: {
        static const char *names[] = {
                "off", "lighten", "reversed",
                "k-means", "binning", "vibrant"
        };
        int mode = model->state.settings.visualizer_mode;
        if (mode < 0 || mode > 5) mode = 0;
        char buf[64];
        snprintf(buf, sizeof(buf), "visualizer: %s", names[mode]);
        set_error_message(buf);
        break;
}
```

All non-state-manipulating code lives in `effects.c`, as required by the MVU architecture. The notification is displayed via the existing footer error-message system (`set_error_message` → `component_error_row`).

---

## 4. Key Binding

### 4.1 Default binding

**File:** `src/ui/settings.c`, line 867

```c
c_strcpy(settings->toggle_visualizer, "v",
         sizeof(settings->toggle_visualizer));
```

The default key is `"v"`, set in `set_default_config()`. This is **not** hardcoded in the `key_bindings` array — it flows through the config system so users can override it.

### 4.2 Event mapping

**File:** `src/ui/settings.c`, lines 660–661

```c
{"toggleVisualizer", MSG_CYCLE_VISUALIZER_MODE},
{"cycleVisualizerMode", MSG_CYCLE_VISUALIZER_MODE},
```

Both config key names map to the same message. `toggleVisualizer` is the canonical name; `cycleVisualizerMode` is accepted as an alias.

### 4.3 User configuration

Users can rebind in `kewrc`:

```
toggleVisualizer = x
```

If absent, the default `"v"` from `set_default_config()` applies.

### 4.4 Legacy binding migration

The old `bind = v, toggleVisualizer` lines in existing `kewrc` files still work — the event map now routes `toggleVisualizer` to `MSG_CYCLE_VISUALIZER_MODE` instead of the removed `MSG_TOGGLEVISUALIZER`.

---

## 5. Config Migration

### 5.1 New config key

**File:** `src/ui/settings.c`, lines 2194–2196

```
[visualizer]
# Visualizer mode: 0=off, 1=lighten, 2=reversed, 3=k-means, 4=binning, 5=vibrant.
visualizerMode=1
```

Written by both `set_config()` (first write path, line 1915) and `set_prefs()` (second write path, line 2104). The old `visualizerEnabled` and `visualizerColorType` keys are **never written** — they are only read for backward compatibility.

### 5.2 Reading new key

**File:** `src/ui/settings.c`, lines 1137–1140

```c
} else if (strcmp(lowercase_key, "visualizermode") == 0) {
        snprintf(settings->visualizer_mode,
                 sizeof(settings->visualizer_mode), "%s",
                 pair->value);
}
```

### 5.3 Legacy key fallback

**File:** `src/ui/settings.c`, lines 1141–1144 (reading) and lines 1549–1156 (mapping)

The old `visualizercolortype` key is still read:

```c
} else if (strcmp(lowercase_key, "visualizercolortype") == 0) {
        snprintf(settings->visualizer_color_type,
                 sizeof(settings->visualizer_color_type), "%s",
                 pair->value);
}
```

After the config parsing loop, if `visualizerMode` was not set but `visualizerColorType` was:

```c
if (settings->visualizer_mode[0] == '\0' &&
    settings->visualizer_color_type[0] != '\0') {
        int ct = get_number(settings->visualizer_color_type);
        if (ct >= 0 && ct <= 4)
                snprintf(settings->visualizer_mode,
                         sizeof(settings->visualizer_mode), "%d", ct + 1);
}
```

Old color type 0–4 maps to mode 1–5. Mode 0 (off) was not representable by the old key.

### 5.4 Transfer to UI

**File:** `src/ui/common_ui.c`, line 123

```c
ui->visualizer_mode = get_number(settings->visualizer_mode);
```

Loaded once at startup from `AppSettings` into `UISettings`.

---

## 6. Rendering Integration

### 6.1 Mode check (visualizer on/off)

**File:** `src/ui/components.c`, line 1723

```c
if (ui->visualizer_mode == 0)
```

Replaces the old `if (!ui->visualizerEnabled)`. When mode is 0, the visualizer component returns immediately.

### 6.2 Palette index derivation

**File:** `src/ui/visuals.c`, line 851

```c
int visualizer_color_type = ui->visualizer_mode > 0 ? ui->visualizer_mode - 1 : 0;
```

Maps mode 1→palette 0, mode 2→palette 1, ..., mode 5→palette 4. The palette array indices are contiguous 0–4, while the user-facing modes are 1–5 (with 0 being off).

### 6.3 Layout calculation

**File:** `src/ui/render_ui.c`, line 829

```c
model->state.settings.visualizer_mode > 0 ? model->state.settings.visualizer_height : 0
```

Replaces the old `visualizerEnabled ? visualizer_height : 0`. When mode is 0, the visualizer takes zero rows.

---

## 7. Notification Display

Mode changes are shown in the footer via the existing error-message system.

### 7.1 Setting the message

**File:** `src/common/common.c`, lines 28–38

```c
void set_error_message(const char *message)
{
        if (message == NULL)
                return;
        strncpy(current_error_message, message, ERROR_MESSAGE_LENGTH - 1);
        current_error_message[ERROR_MESSAGE_LENGTH - 1] = '\0';
        has_printed_error = false;
        set_dirty(DIRTY_FOOTER);
}
```

### 7.2 Rendering the message

**File:** `src/ui/components.c`, lines 1139–1161

`component_error_row()` checks `has_error_message()` and `!has_printed_error_message()`. If a message exists and hasn't been displayed yet, it renders the message in the footer with the appropriate color style. After one render frame, `mark_error_message_as_printed()` clears it.

### 7.3 Lifecycle

1. User presses `v` → `effects.c` calls `set_error_message("visualizer: k-means")`
2. `set_dirty(DIRTY_FOOTER)` triggers a re-render
3. `component_error_row()` displays the message
4. On the next `MSG_RENDERED`, `mark_error_message_as_printed()` clears it
5. The message auto-clears after one frame — no user dismissal needed

---

## 8. Files Changed

| File | Change |
|---|---|
| `src/common/events.h` | Added `MSG_CYCLE_VISUALIZER_MODE` |
| `src/common/model.h` | Replaced `visualizerEnabled` + `visualizer_color_type` with `visualizer_mode` in `UISettings`; replaced `visualizerEnabled` with `visualizer_mode` in `AppSettings`; kept `visualizer_color_type` in `AppSettings` for migration |
| `src/update/update.c` | Added `MSG_CYCLE_VISUALIZER_MODE` handler; emits `CMD_CYCLE_VISUALIZER_MODE` |
| `src/update/effects.h` | Added `CMD_CYCLE_VISUALIZER_MODE` |
| `src/update/effects.c` | Added `CMD_CYCLE_VISUALIZER_MODE` handler with mode name lookup and `set_error_message` |
| `src/ui/settings.c` | Added `toggle_visualizer = "v"` default; event map `toggleVisualizer` → `MSG_CYCLE_VISUALIZER_MODE`; reads `visualizermode` and legacy `visualizercolortype` config keys; fallback mapping from old key to new; writes `visualizerMode` instead of `visualizerEnabled`/`visualizerColorType` |
| `src/ui/common_ui.c` | Loads `ui->visualizer_mode` from `settings->visualizer_mode` |
| `src/ui/components.c` | Changed `!ui->visualizerEnabled` to `ui->visualizer_mode == 0` |
| `src/ui/render_ui.c` | Changed `visualizerEnabled` checks to `visualizer_mode > 0` |
| `src/ui/visuals.c` | Derives `visualizer_color_type` from `visualizer_mode` instead of reading a stored field |
| `src/kew.c` | Changed initial state from `visualizerEnabled = true` to `visualizer_mode = 1` |

---

## 9. Possible Future Improvements

1. **Reverse cycling** — add a shift+`v` or separate binding to cycle backward through modes
2. **Direct mode selection** — number keys 0–5 to jump to a specific mode
3. **Persistent notification** — show the current mode name in the status bar continuously, not just on change
4. **Per-song mode memory** — remember the last mode used per song/album
