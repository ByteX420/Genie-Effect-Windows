# Genie Effect for Windows

**Genie Effect** is an open-source Windows desktop app that replaces the stock minimize and restore transition with a **macOS-style Genie animation**. When you minimize a window, the live desktop region is captured and warped into a deformable mesh that sucks into the taskbar (and expands back out on restore).

It is a native **C++ / Direct3D 11 / DirectComposition** project with a polished ImGui settings UI — not a shell theme pack or AutoHotkey script.

| | |
| --- | --- |
| **Platform** | Windows 10 / 11 (x64) |
| **License** | [MIT](LICENSE.txt) |
| **Language** | C++ (latest MSVC) |
| **UI** | Dear ImGui + FreeType (Inter) |
| **Graphics** | D3D11, DXGI Desktop Duplication, DirectComposition |

---

## Features

- **Genie minimize & restore** — mesh-based deformation toward the taskbar (or a custom rect)
- **Concurrent animations** — multiple windows can animate without blocking each other
- **Separate motion controls** — minimize vs restore duration, linked or independent speeds
- **Easing & style options** — classic Genie, strength, fade, and related motion tweaks
- **App exclusions** — skip the effect for specific executables
- **System integration** — run at startup, start minimized, tray icon, close-to-tray or exit
- **Hotkeys** — toggle the effect, open settings, repair hooks (configurable)
- **Settings UI** — dark macOS-inspired shell (traffic lights, sidebar, cards, motion)
- **Repair / diagnostics** — status for effect, hook, renderer, D3D device, display
- **Native animation suppression** — disables classic shell + DWM transitions while running
- **Device-lost recovery** — recreates capture/overlay/settings renderers after GPU resets

---

## How it works (high level)

Windows does **not** expose a public API that means “replace this DWM minimize animation before the compositor runs it.” Genie Effect uses the strongest **documented** path available:

1. **Detect** minimize/restore with `SetWinEventHook` (`EVENT_SYSTEM_MINIMIZESTART` / related events).
2. **Suppress** the stock transition with `DwmSetWindowAttribute(DWMWA_TRANSITIONS_FORCEDISABLED)` and temporary `SystemParametersInfo(SPI_SETANIMATION)` changes (restored on exit).
3. **Capture** the visible window region via **DXGI Desktop Duplication** into an `ID3D11Texture2D`.
4. **Composite** a transparent topmost overlay with **DirectComposition** + a D3D11 swap chain.
5. **Deform** a textured mesh (Genie curve / squash) each frame until the window lands at the taskbar target.

A separate **CBT hook DLL** (`GenieHookPost.dll`) helps observe and coordinate window events. Elevated processes are only visible to the hook if Genie Effect itself runs elevated (UIPI).

For a deeper technical write-up, see [`docs/architecture.md`](docs/architecture.md).

---

## Requirements

### Run

- Windows 10 or Windows 11 (64-bit)
- A GPU with Direct3D 11 support
- Desktop Duplication available on the target session (normal interactive desktop)

### Build

- **Visual Studio 2022 or newer** (or VS 18 / Build Tools) with:
  - Desktop development with C++
  - MSBuild
  - MSVC toolset (project uses `v145` / latest)
- **x64** platform only
- No separate vcpkg step for core deps — **ImGui** and **FreeType** are vendored under `app/third_party/`

---

## Build

### Visual Studio

1. Open `GenieEffect.slnx`
2. Select configuration **Release** (or **Debug**) and platform **x64**
3. Build **Solution** (`Ctrl+Shift+B`)

The app project builds the hook DLL first, then links/embeds it as needed.

### Command line (Developer PowerShell)

```powershell
MSBuild.exe GenieEffect.slnx /p:Configuration=Release /p:Platform=x64 /m
```

### Outputs

| Path | Contents |
| --- | --- |
| `build\bin\x64\Release\` | `GenieEffect.exe`, `GenieHookPost.dll` (+ PDBs) |
| `build\bin\x64\Debug\` | Debug binaries |
| `build\obj\App\x64\<Config>\` | App intermediates |
| `build\obj\Hook\x64\<Config>\` | Hook intermediates |

**Runtime:** keep `GenieEffect.exe` and `GenieHookPost.dll` in the **same folder**. Release builds can also use the embedded hook resource when configured with `GENIE_EMBED_RELEASE_HOOK`.

---

## Usage

1. Build (or obtain) a Release binary pair.
2. Run `GenieEffect.exe` (optionally **as Administrator** if you want the effect on elevated windows).
3. Open the settings window from the tray or hotkey.
4. Enable the effect (sidebar status chip shows **On** / **Off** / **Paused**).
5. Minimize any eligible window — it should Genie into the taskbar.

### Settings overview

| Page | What you control |
| --- | --- |
| **Effect** | Master enable, close behavior, startup options |
| **Motion** | Durations, easing, style, strength, fade, preview |
| **Apps** | Exclude executables from the effect |
| **System** | Windows integration helpers |
| **Hotkeys** | Global shortcuts |
| **Repair** | Live diagnostics (hook, renderer, display, etc.) |
| **About** | Product version, licenses |

Settings persist to:

```text
%LOCALAPPDATA%\GenieEffect\settings.json
```

---

## Configuration & environment

### Environment variables

| Variable | Purpose |
| --- | --- |
| `GENIE_TASKBAR_RECT` | Override minimize target as `left,top,right,bottom` (physical screen coords). Useful for custom taskbars. |
| `GENIE_DEBUG_LOG` | Override path of the debug log file |
| `GENIE_TRACE=1` | Verbose timing traces (noisy; for debugging) |
| `GENIE_LOG_SYNC=1` | Flush every log line (helps after hangs/crashes) |
| `GENIE_TEST_DEVICE_RECOVERY=1` | Debug: one controlled D3D teardown/recreate after startup |

Example custom taskbar target:

```powershell
$env:GENIE_TASKBAR_RECT = "100,980,1820,1070"
.\GenieEffect.exe
```

### Debug log

Debug builds write diagnostics to:

```text
%LOCALAPPDATA%\GenieEffect\genie_debug.log
```

---

## Project layout

```text
Genie-Effect-Windows/
├── GenieEffect.slnx              # Solution
├── app/
│   ├── GenieEffect.vcxproj       # Main app
│   ├── assets/fonts/             # Inter (OFL)
│   ├── shaders/                  # HLSL (genie mesh reference)
│   ├── src/
│   │   ├── animation/            # Mesh math, easing (platform-independent)
│   │   ├── app/                  # Application, settings UI, store, startup
│   │   ├── common/               # Shared helpers (logging)
│   │   ├── menu/                 # Motion system + UI theme tokens
│   │   ├── platform/             # Win32, DWM, taskbar, hooks helpers
│   │   └── rendering/            # D3D11, capture, overlay
│   └── third_party/
│       ├── imgui/                # Dear ImGui + Win32/DX11 backends
│       └── freetype/             # Headers + static lib for crisp UI text
├── hook/
│   ├── GenieHook.vcxproj
│   └── hook.cpp                  # CBT hook DLL
├── docs/
│   └── architecture.md           # Technical boundary & design notes
├── build/                        # Local build outputs (not source of truth)
├── .clang-format                 # Google-based C++ style
└── LICENSE.txt
```

### Code style

- C++ style follows **Google C++** conventions via [`.clang-format`](.clang-format) (2-space indent, 100 columns).
- Format first-party sources (not `third_party/`):

```powershell
$cf = "…\clang-format.exe"   # e.g. VS LLVM x64 clang-format
Get-ChildItem app\src, hook -Recurse -Include *.cpp,*.hpp,*.h |
  ForEach-Object { & $cf -i --style=file $_.FullName }
```

---

## Architecture notes for contributors

| Layer | Responsibility |
| --- | --- |
| `animation/` | Pure Genie mesh / geometry / easing — no Win32 |
| `platform/` | Window events, native animation blocker, taskbar rect |
| `rendering/` | Device, desktop capture, overlay window |
| `app/` | Lifetime, settings persistence, ImGui shell |
| `menu/motion/` | Shared spring/timed motion for UI chrome |
| `hook/` | Separate DLL; keep the app/DLL boundary clean |

**Important limitations (by design):**

- No official “pre-DWM replace animation” API — behavior can vary with shell updates.
- **UIPI:** non-elevated Genie Effect cannot hook elevated windows.
- Multi-monitor / exotic taskbar setups may need `GENIE_TASKBAR_RECT`.
- Fullscreen games / exclusive modes may disable or skip the effect (settings flags exist for battery saver / fullscreen-related behavior).

See [`docs/architecture.md`](docs/architecture.md) for the public-API boundary and intentional isolation of invasive shell work.

---

## Contributing

Contributions are welcome. A good PR:

1. **Builds** cleanly on x64 Release and Debug.
2. **Formats** first-party C++ with the repo `.clang-format`.
3. **Keeps** platform-independent animation code free of Win32 when possible.
4. **Documents** user-facing behavior changes in the PR description.
5. **Avoids** committing `build/`, local `*.user` noise, or secrets.

### Suggested workflow

```powershell
git checkout -b feature/my-change
# … edit …
MSBuild.exe GenieEffect.slnx /p:Configuration=Release /p:Platform=x64 /m
# format changed sources with clang-format
git commit
```

If you change minimize timing or mesh math, mention comparison against stock DWM and any multi-monitor testing you did.

### Reporting issues

Please include:

- Windows version (Win10/11 build number)
- GPU / driver if graphics-related
- Elevated vs normal process
- Steps to reproduce
- Relevant lines from `genie_debug.log` (Debug builds) if available

---

## Troubleshooting

| Symptom | Things to try |
| --- | --- |
| No animation at all | Confirm effect is **On** in settings; check Repair page (Hook / Renderer / D3D). |
| Elevated apps ignore effect | Run Genie Effect **as Administrator**. |
| Wrong suck target | Set `GENIE_TASKBAR_RECT` or check taskbar edge (top/bottom/left/right). |
| Black / stuck overlay | Restart app; check device-lost path; update GPU drivers. |
| Hook not installed | Ensure `GenieHookPost.dll` sits next to the EXE; rebuild both projects. |
| Settings not saving | Check write access to `%LOCALAPPDATA%\GenieEffect\`. |

---

## Credits & third-party

- **Genie mesh / timing inspiration** — adapted from Harshil Shah’s MIT-licensed [Genie](https://github.com/HarshilShah/Genie) SpriteKit playground  
- **[Dear ImGui](https://github.com/ocornut/imgui)** — immediate-mode UI  
- **[FreeType](https://freetype.org/)** — font rasterization for the settings UI  
- **[Inter](https://github.com/rsms/inter)** — UI typeface (SIL Open Font License; see `app/assets/fonts/OFL.txt`)

Windows, DirectX, and DirectComposition are trademarks of Microsoft Corporation. macOS is a trademark of Apple Inc. This project is not affiliated with Apple or Microsoft.

---

## License

This project is released under the **MIT License**. See [LICENSE.txt](LICENSE.txt).

```text
Copyright (c) 2026 ByteX420
```

You are free to use, modify, and distribute the software, including commercially, provided the license notice is preserved. The software is provided **as is**, without warranty.

---

## Disclaimer

Genie Effect interacts with window management, accessibility-style event hooks, and desktop capture. Use at your own risk. Behavior may change with Windows updates. Do not use this software to bypass security boundaries or capture content you are not allowed to access.
