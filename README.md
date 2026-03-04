# Alarm

A Windows desktop alarm application built with C++20, Dear ImGui, and Vulkan.

Each alarm stores a time, label, repeat days, and an optional YouTube URL. When triggered, the application opens the URL in Chrome. The window minimises to the system tray rather than closing.

---

## Tech Stack

| Component | Library |
|---|---|
| GUI | [Dear ImGui](https://github.com/ocornut/imgui) (immediate mode) |
| Renderer | Vulkan + `imgui_impl_vulkan` |
| Window / Tray | Win32 API |
| Serialisation | [nlohmann/json](https://github.com/nlohmann/json) |
| Build | CMake 3.20+ |

---

## Building

**Requirements:** MSVC or MinGW, CMake ≥ 3.20, Vulkan SDK.

```bash
cmake -S . -B build
cmake --build build --config Release
```

The build step copies `data/` (JSON storage) next to the executable automatically.

---

## Project Structure

```
Alarm/
├── include/
│   ├── Application.h
│   ├── models/
│   │   ├── AlarmModel.h
│   │   └── SettingsModel.h
│   ├── controllers/
│   │   ├── AlarmController.h
│   │   └── PersistenceService.h
│   └── views/
│       └── AlarmView.h
├── src/
│   ├── Application.cpp
│   ├── main.cpp
│   ├── controllers/
│   │   ├── AlarmController.cpp
│   │   └── PersistenceService.cpp
│   └── views/
│       └── AlarmView.cpp
└── data/
    ├── alarms.json
    └── settings.json
```

---

## Architecture

The codebase uses a layered architecture. Dependencies only flow **downward**: View knows about Controller, Controller knows about Model, nothing flows back up.

```
┌──────────────────────────────────────────────────────┐
│                    Application                       │  Win32 + Vulkan lifecycle
│             owns Controller and View                 │
└────────────────┬───────────────────┬─────────────────┘
                 │                   │
                 ▼                   ▼
┌──────────────────────────┐  ┌──────────────────┐
│  Controller layer        │◄─│      View        │  View holds a ref to AlarmController
│                          │  │                  │  Controller does not know View exists
│  ┌───────────────────┐   │  └──────────────────┘
│  │  AlarmController  │   │
│  │  (business logic, │   │
│  │   owns state)     │   │
│  └────────┬──────────┘   │
│           │ calls        │
│  ┌────────▼──────────┐   │
│  │ PersistenceService│   │  Pure I/O utility; reads/writes JSON files
│  │ (static, no state)│   │  only reachable from within the Controller layer
│  └───────────────────┘   │
└────────────┬─────────────┘
             │ uses as value objects
             ▼
┌──────────────────────────┐
│       Model layer        │  Plain data structs; no knowledge of anything above
│  AlarmModel / Settings   │
└──────────────────────────┘
```

---

### Model & Controller

**Model** (`alarm::model`) is intentionally thin. `AlarmModel` and `SettingsModel` are plain structs — they carry data and know how to serialise themselves to/from JSON, but contain no business logic.

```cpp
struct AlarmModel {
    std::string id;
    std::string label;
    int hour, minute;
    bool enabled;
    std::vector<int> repeat_days;
    std::string youtube_url;

    static AlarmModel fromJson(const nlohmann::json &);
    nlohmann::json toJson() const;
};
```

`fromJson` / `toJson` live on the struct because they describe the data's own representation — this is self-description, not business logic, and keeps serialisation code close to the fields it touches.

**Controller** (`alarm::controller`) is where all business logic lives. `AlarmController` is the single source of truth for runtime state: it owns the in-memory `alarms_` vector and the `settings_` value.

```
AlarmController
├── alarms()          — read-only access to the alarm list
├── addAlarm()        — assigns a UUID v4 id, appends, sorts, persists
├── updateAlarm()     — finds by id, overwrites, sorts, persists
├── deleteAlarm()     — removes by id, persists
├── setEnabled()      — flips enabled flag, persists
├── settings()        — read-only access to settings
└── saveSettings()    — replaces settings, persists
```

Every mutation follows the same three-step pattern: **mutate in memory → sort (if needed) → persist to disk**. This keeps the JSON files always in sync without a separate "save" concept visible to the rest of the application.

`PersistenceService` is a pure I/O utility, deliberately separated from `AlarmController`. It only knows how to read/write JSON files; it has no knowledge of sorting, id generation, or any other logic. This separation means that swapping the storage format (e.g., SQLite) would only touch `PersistenceService`.

```
PersistenceService  (static methods, no state)
├── loadAlarms()    / saveAlarms()
└── loadSettings()  / saveSettings()
```

Alarm IDs are UUID v4 strings generated entirely in-process using `<random>`. The 128-bit random value has version bits (4) and variant bits (RFC 4122) forced into the appropriate positions before being formatted as a hex string.

---

### View & Controller

**View** (`alarm::view`) holds a non-owning reference to `AlarmController` and is the only layer that directly calls its mutation methods. The relationship is one-directional: View reads from and writes to Controller; Controller never calls back into View.

```cpp
class AlarmView {
    controller::AlarmController &ctrl_;   // reference, not pointer — always valid
    ...
};
```

Because this application uses **Dear ImGui** (immediate mode), there is no observer pattern, no data binding, and no "model changed → notify view" mechanism. Instead, every frame the View reads the current state directly from the Controller and rebuilds the entire UI from scratch:

```
Every frame:
  ctrl_.alarms()        → iterate and render each row
  ctrl_.settings()      → populate the settings dialog fields
  ctrl_.setEnabled()    → called immediately when the ON/OFF button is clicked
  ctrl_.addAlarm()      → called when the add dialog is confirmed
  ctrl_.updateAlarm()   → called when the edit dialog is confirmed
  ctrl_.deleteAlarm()   → called when the delete confirm dialog is accepted
```

**Communicating upward to Application** — the View needs to signal two things to `Application` without holding a reference to it: window resize requests (from Settings) and close decisions (minimise vs. exit). Rather than callbacks or events, it exposes two poll methods:

```cpp
bool pollWindowResize(int &w, int &h);   // true once after a resize is requested
CloseDecision pollCloseDecision();        // None / Minimize / Exit
```

`Application::mainLoop_()` calls these every frame. This keeps the dependency direction clean: View still has no knowledge of Application.

**Dialog state** is owned entirely by the View as private member variables (`editHour_`, `editLabel_`, `pendingDeleteId_`, etc.). The Controller is only touched on confirmed actions, never during intermediate editing. This means a user can open the edit dialog, change values, and cancel — the Controller state is never dirtied.

---

## Data Files

Stored in `data/` next to the executable. Created with default values on first run if absent.

**`data/alarms.json`**
```json
{
  "alarms": [
    {
      "id": "550e8400-e29b-4000-8000-446655440000",
      "label": "Morning",
      "hour": 7,
      "minute": 30,
      "enabled": true,
      "repeat_days": [1, 2, 3, 4, 5],
      "youtube_url": ""
    }
  ]
}
```

**`data/settings.json`**
```json
{
  "chrome_path": "C:\\Program Files\\Google\\Chrome\\Application\\chrome.exe",
  "close_to_tray": true,
  "suppress_minimize_hint": false,
  "window_width": 1280,
  "window_height": 800
}
```

`repeat_days` uses `0 = Sunday … 6 = Saturday`. An empty array means the alarm fires every day.
