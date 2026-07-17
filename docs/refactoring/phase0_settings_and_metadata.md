# Authoritative Phase-0 inventory: settings, project metadata, resources, and documentation

This report is a source-only inventory. No build, test, or application run was performed.

## Files read in full

- `app/src/app/settings_store.hpp`
- `app/src/app/settings_store.cpp`
- `app/GenieEffect.vcxproj`
- `app/GenieEffect.rc`
- `app/shaders/genie_mesh.hlsl`
- `GenieEffect.slnx`
- `README.md`
- `docs/architecture.md`
- `app/assets/fonts/README.md`
- `app/assets/fonts/OFL.txt`

`app/GenieEffect.vcxproj.filters` does not currently exist.

## `settings_store.hpp` value types and contracts

| Old declaration | Current callers/consumers | Required target | Ownership and compatibility |
| --- | --- | --- | --- |
| `kDefaultMinimizeDuration` | `AppSettings` default and application/UI duration fallbacks | `settings/app_settings.hpp` | Value only; preserve `0.70f`. |
| `kDefaultRestoreDuration` | `AppSettings` default and application/UI duration fallbacks | `settings/app_settings.hpp` | Value only; preserve `0.70f`. |
| `HotkeyAction` | `Application`, `SettingsWindow`, persisted hotkey array | `settings/hotkey_binding.hpp` | Value enum; preserve ordering `ToggleEffect`, `OpenSettings`, `RepairWindows`, `Count`, because JSON field mapping and command IDs use the index. |
| `HotkeyBinding` and equality | `Application` registration/update and settings UI editor | `settings/hotkey_binding.hpp` | Value type; no handle ownership. Preserve modifier/key numeric representation. |
| `AppSettings` | `Application` owns current truth; `SettingsWindow` currently copies fields; parser/save operate on it | `settings/app_settings.hpp`; owned exclusively by `SettingsService` after migration | Preserve every field, default, type, and saved meaning. UI must receive a view-model snapshot rather than own an independent settings truth. |
| Default hotkeys | `AppSettings::hotkeys` | `settings/hotkey_binding.hpp` / `app_settings.hpp` | Preserve Alt+Ctrl+G (`0x0001 | 0x0002`, `'G'`) for toggle and empty bindings for the other actions. |

Persisted defaults that must remain byte-semantically compatible at the field/value level:

- enabled `true`
- minimize/restore duration `0.70`
- link speeds `false`
- fullscreen suppression flag `false`
- battery-saver suppression flag `false`
- easing `"Ease In Out"` and Bezier `EaseInOut()`
- style historical spelling `"Gienie classic"`
- quality `"automatic"`
- strength `1.0`
- fade `"Subtle"`
- target indicator `false`
- close behavior `"exit"`
- start minimized / run at startup `false`
- empty exclusions

## `settings_store.cpp` function-level migration

### Parser, serializer, validation, and repository internals

| Old symbol | Current role/callers | Required single target | Ownership / failure behavior to preserve |
| --- | --- | --- | --- |
| `HotkeyField` | Parser-only mapping result | Private type in `settings_serializer.cpp` | Value only. |
| `FindHotkeyField()` | `SettingsJsonParser::Parse()` | Private parser helper in `settings_serializer.cpp` | Recognizes the three action prefixes and `Modifiers` / `Key` suffixes; action ordering must remain stable. |
| `IsValidEasingName()` | Parser and post-load normalization | `SettingsValidator` | Accept exactly Linear, Ease In, Ease Out, Ease In Out, Cubic, Back, Elastic, Custom. |
| `IsValidAnimationStyle()` | Parser | `SettingsValidator` | Accept current misspellings `"Gienie classic"`, `"Gienie curvy"`, `"Squash"` and legacy `"Classic Genie"`. |
| `AppendUtf8()` | JSON `\uXXXX` string decoding | Private parser helper in `settings_serializer.cpp` | Appends 1–3 byte UTF-8 for a single 16-bit code unit. Preserve accepted input behavior initially; serializer tests must pin malformed/surrogate behavior before any deliberate hardening. |
| `SettingsJsonParser::SettingsJsonParser()` | `LoadSettings()` | `SettingsSerializer` parser implementation | Borrows the JSON string view for the parse call; owns only cursor state. |
| `SettingsJsonParser::Parse()` | `LoadSettings()` | `SettingsSerializer::Deserialize()` | Mutates a supplied settings copy, skips unknown fields, tolerates invalid known values by skipping them and retaining defaults, but rejects malformed document structure. |
| `SettingsJsonParser::AtEnd()` | `Parse()` | Private serializer helper | Requires only trailing whitespace after the root object. |
| `SettingsJsonParser::SkipWhitespace()` | All parser routines | Private serializer helper | Cursor-only mutation. |
| `SettingsJsonParser::Consume()` | All parser routines | Private serializer helper | Cursor advances only on exact match. |
| `SettingsJsonParser::ParseString()` | Keys, values, arrays, skipped values | Private serializer helper | Rejects control chars and invalid escapes; handles `\" \\ \/ \b \f \n \r \t \uXXXX`. |
| `SettingsJsonParser::ParseBoolean()` | Boolean fields and skipped values | Private serializer helper | Exact lowercase `true`/`false`. |
| `SettingsJsonParser::ParseNumber()` | Durations, strength, hotkeys, arrays, skipped values | Private serializer helper | JSON numeric grammar; `strtod`; rejects ERANGE and incomplete conversion. |
| `SettingsJsonParser::ParseStringArray()` | exclusions | Private serializer helper | Replaces exclusions only after the whole value parses successfully. |
| `SettingsJsonParser::ParseFloatArray()` | cubic Bezier | Private serializer helper | Requires finite values and optionally exact element count. |
| `SettingsJsonParser::ParseCubicBezier()` | custom easing fields | Private serializer helper plus `SettingsValidator` | Four finite floats; calls `ClampHandles()`. |
| `SettingsJsonParser::SkipValue()` | Unknown fields and invalid known values | Private serializer helper | Supports strings, objects, arrays, booleans, null, numbers; preserve 64-level nesting guard and tolerant unknown-field behavior. |
| parser fields `json_`, `position_` | parser methods | Private `SettingsSerializer` parser state | Borrowed view plus owned cursor; no external resources. |
| `EscapeJsonString()` | `SaveSettings()` | `SettingsSerializer::Serialize()` helper | Escapes JSON quotes, slash, controls; leaves UTF-8 bytes otherwise unchanged. |
| `Utf8ToWide()` | executable normalization/equality | Private helper in `exclusion_rules.cpp` (implementation detail only) | Converts with `CP_UTF8 | MB_ERR_INVALID_CHARS`; no retained allocation/handle. Public settings headers must not expose Win32. |

### Public settings and exclusion functions

| Old symbol | Current callers | Required single target | Ownership / cleanup / errors |
| --- | --- | --- | --- |
| `NormalizeExecutableName()` | application exclusion mutations; UI active-app add; exclusion normalization | `settings/exclusion_rules.cpp/.hpp` | Trims ASCII whitespace; rejects empty, >255-byte, `"."`, `".."`, invalid UTF-8, control/Windows-invalid filename chars, and non-`.exe` suffix (ASCII case-insensitive); returns original trimmed spelling. |
| `ExecutableNamesEqual()` | exclusion lookup and UI active-app selection | `settings/exclusion_rules.cpp/.hpp` | Current behavior is Unicode ordinal case-insensitive via `CompareStringOrdinal`; preserve this Windows filename behavior behind a clean header. |
| `ContainsExcludedApplication()` | application policy and normalization | `settings/exclusion_rules.cpp/.hpp` | Borrowed vector/name; no resources. |
| `NormalizeExcludedApplications()` | load, save, application updates | `settings/exclusion_rules.cpp/.hpp`, invoked centrally by `SettingsValidator` | Removes invalid entries and case-insensitive duplicates while preserving first occurrence/order. Null input is a no-op. |
| `SettingsFilePath()` | app diagnostics/session log context, load/save | `SettingsRepository::DefaultPath()` | Reads `LOCALAPPDATA`; preserve exact `\GenieEffect\settings.json` path and empty-path failure. |
| `LoadSettings()` | `Application::Initialize()` | `SettingsRepository::Load()` plus `SettingsSerializer`, with `SettingsService::Load()` owning publication | Missing/unreadable/oversized/malformed file returns defaults. Limit remains 1 MiB. Logs malformed/oversized fallback. Normalize exclusions/hotkeys/style/easing/Bezier before publication. |
| `SaveSettings()` | all application setting mutations/hotkey/exclusion/startup changes | `SettingsSerializer::Serialize()` + `SettingsRepository::SaveAtomically()`, called only by `SettingsService` | Creates parent dirs, writes `<settings>.tmp`, flushes/closes, then `MoveFileExW(REPLACE_EXISTING | WRITE_THROUGH)`; deletes temp on failure. Valid in-memory state must not be published on save failure. |

### Validation and legacy-read details currently embedded in `Parse()` / `LoadSettings()`

These become explicit, reusable `SettingsValidator` rules rather than remaining parser side effects:

- Duration range is `[0.10, 2.00]`; non-finite/out-of-range values keep defaults.
- Genie strength range is `[0.25, 1.0]`.
- Fade is exactly `"No fade"`, `"Subtle"`, or `"Strong"`.
- Quality is exactly `"automatic"`, `"best_quality"`, or `"power_saving"`.
- Close behavior is exactly `"exit"` or `"tray"`.
- Hotkey modifiers are integral `0..0x000f`; key is integral `0..254`; post-load masks modifiers and clears modifiers when key is zero.
- Bezier arrays have exactly four finite floats and handles are clamped.
- Legacy style `"Classic Genie"` is normalized to historical persisted spelling `"Gienie classic"`.
- Parser accepts legacy boolean keys `adaptiveDuration`, `reduceEffectsOnBattery`, and
  `followWindowsAnimationPreference` as syntactically known but intentionally ignores them.
- Parser accepts current boolean keys `linkSpeeds`, `disableAnimationsFullscreen`,
  `disableEffectsBatterySaver`, `showTargetIndicator`, `startMinimized`, and `runAtStartup`.
- `reduceEffectsOnBattery` is currently parsed but not copied to
  `disable_effects_battery_saver`; this exact compatibility behavior must be covered by a test before
  deciding whether to map it as a historical alias.
- Unknown fields of any supported JSON value type are skipped.
- Invalid known values retain the field's default when the value itself can be skipped.
- Output formatting is fixed with two decimals and the current camelCase field names/order.

### Future `SettingsService` mutation boundary

Current direct mutation sites are concentrated in `Application` (enable, pause-related display
updates, durations, link speeds, easing, Bezier, style, quality, strength, fade, target indicator,
fullscreen, battery saver, close behavior, start-minimized, startup, hotkeys, exclusions) and are
presented by `SettingsWindow` callbacks. They must become typed `SettingsChange` operations.

For every update:

1. copy current `AppSettings`;
2. apply the typed change;
3. validate and normalize the copy;
4. serialize and atomically save;
5. publish the copy only after successful save;
6. notify the UI/controller and policy after publication.

`run_at_startup` also has a Windows startup side effect. That action must remain transactional:
failed registry update or failed settings save restores the previous registry/settings state rather
than leaving the two truths inconsistent.

Hotkey duplicate detection and OS registration availability currently live in `Application`;
binding validity/duplicate detection moves to `hotkey_binding`, while registration/rollback and
owned `RegisterHotKey` IDs move to `GlobalHotkeyManager`.

## Settings resource and environment ownership

- `LOCALAPPDATA`: read by the settings repository for the stable settings path.
- Settings file: repository owns file operations only; `SettingsService` owns the valid in-memory
  model.
- Temporary file: repository owns and removes `<settings>.tmp` on all failed write/replace paths.
- No persistent Win32 handle is owned by the current settings store.

## Project, resource, shader, and documentation inventory

### `GenieEffect.slnx`

- Contains x64 only.
- Startup project and sole explicit solution project is `app/GenieEffect.vcxproj`.
- The app project invokes the hook project through a custom pre-build target; the final solution
  must additionally contain the test project only when the masterplan opens the test phase.

### `app/GenieEffect.vcxproj`

- Product contract: `GenieEffect.exe`; output
  `build/bin/x64/<Configuration>/`; intermediates `build/obj/App/x64/<Configuration>/`.
- Debug and Release x64, toolset `v145`, latest C++, `/utf-8`, static CRT.
- Debug is console/asInvoker; Release uses `wmainCRTStartup`, Windows subsystem, HighestAvailable,
  LTCG/WPO, CFG, and embeds the hook resource.
- PCH contract: `pch.hpp` globally, `pch.cpp` creates it; ImGui third-party files and current UI
  motion source opt out. New first-party sources should use the stable PCH unless their dependency
  boundary warrants a documented exception.
- Includes all current production `.cpp`/headers plus vendored ImGui sources. No filter file exists.
- Links FreeType, D3D11, DXGI, DWM, DirectComposition, D3DCompiler, Shell32, Ole32.
- `BuildGenieHook` invokes `hook/GenieHook.vcxproj` before app build, preserving configuration and
  platform and disabling nested parallelism.
- Phase 9 must replace old source entries exactly, retain all third-party entries untouched, mirror
  real directories in a new `.vcxproj.filters`, and keep hook build/resource ordering.

### `app/GenieEffect.rc` and `app/src/app/resource.hpp`

- PE file/product version is `1.1.0.0`; strings are `1.1.0`.
- Stable product/internal/original names include `GenieEffect.exe`.
- Resource IDs that must remain ABI-compatible:
  - `IDR_UI_FONT_REGULAR` = 201
  - `IDR_UI_FONT_SEMIBOLD` = 202
  - `IDR_UI_FONT_BOLD` = 203
  - `IDR_UI_FONT_LICENSE` = 204
  - `IDR_GENIE_HOOK` = 205
- Font aliases named `IDR_JETBRAINS_*` currently forward to Inter resource IDs. They are migration
  aliases, not external ABI; remove only after proving there are no callers.
- Release-only `GENIE_EMBED_RELEASE_HOOK` embeds
  `build/bin/x64/Release/GenieHookPost.dll`; this path/order is mandatory.

### `app/shaders/genie_mesh.hlsl`

- `FrameConstants`: viewport size, opacity, padding.
- `VertexMain()` converts pixel coordinates to clip space and passes UV.
- `PixelMain()` samples, replaces alpha with opacity, and premultiplies RGB.
- Current overlay code also contains compiled shader source/ownership; Phase 6 assigns shader
  compilation and mesh upload/draw to `AnimationRenderer`, while swap-chain/DirectComposition
  ownership belongs to `OverlayRenderer`/`OverlayWindow`. The HLSL reference remains first-party
  rendering input, not UI.

### README and architecture documentation

- Both describe the old `app/common/menu/platform/rendering` layout and monolithic ownership.
- They document required public behavior that must remain: x64 D3D11/DXGI/DirectComposition,
  concurrent animation, tray/settings/hotkeys/exclusions, device recovery, UIPI limitation,
  multi-monitor/custom taskbar target, debug/release output locations, adjacent hook DLL and
  release embedding.
- Documented environment variables to preserve:
  - `GENIE_TASKBAR_RECT`
  - `GENIE_DEBUG_LOG`
  - `GENIE_TRACE`
  - `GENIE_LOG_SYNC`
  - `GENIE_TEST_DEVICE_RECOVERY`
- Documented default debug log:
  `%LOCALAPPDATA%\GenieEffect\genie_debug.log`.
- Phase 9 must replace the old folder tree/layer table and describe owners, allowed dependencies,
  state transitions, recovery/cleanup convergence, and the UI action boundary without changing
  user-facing behavior claims.

## Phase-0 special cases captured by this slice

- Backward-compatible settings parsing and exact stable file path.
- Failed save must leave valid in-memory settings unchanged and clean temporary files.
- Startup setting has a two-resource transaction (registry plus JSON).
- Hook release resource is produced before resource compilation and keeps ID/path/name.
- Debug/Release output paths and executable/DLL names remain unchanged.
- UIGraphics font resources and license remain embedded and their IDs remain stable.
- Custom taskbar environment parsing and device recovery environment trigger are documented
  cross-module contracts, even though their implementations are inventoried in other reports.
