# Authoritative Phase-0 inventory: animation, Windows platform adapters, rendering, and hook DLL

This is a source-only migration inventory for the following files, all of which were read in full:

- `app/src/animation/easing.hpp`
- `app/src/animation/genie_mesh.cpp/.hpp`
- `app/src/animation/geometry.hpp`
- `app/src/platform/native_animation_blocker.cpp/.hpp`
- `app/src/platform/taskbar_target_provider.cpp/.hpp`
- `app/src/platform/window_event_monitor.cpp/.hpp`
- `app/src/platform/window_util.cpp/.hpp`
- `app/src/rendering/d3d_device.cpp/.hpp`
- `app/src/rendering/desktop_capture.cpp/.hpp`
- `app/src/rendering/overlay_window.cpp/.hpp`
- `hook/hook.cpp`
- `hook/GenieHook.vcxproj`

Repository-wide includes and callers were inspected read-only, including the relevant parts of
`application.cpp/.hpp`, settings storage/UI, logger, `pch.hpp`, app project/resource metadata,
`GenieEffect.slnx`, README/architecture documentation, and the otherwise unused
`app/shaders/genie_mesh.hlsl`. No production file was edited and no build, test, formatter, or
application run was performed.

Each old callable symbol is assigned exactly once in the function ledgers below. Local lambdas are
listed under their one owning function rather than treated as independent migration units.
Implicit compiler-generated special members are described in the type ledger; explicitly defaulted
or deleted special members remain part of the target type contract.

## Mandatory target boundaries

| Existing responsibility | Required owner after migration |
| --- | --- |
| Rectangles, points, easing math, Bezier evaluation, mesh topology/positions | `animation/geometry`, `animation/easing.cpp/.hpp`, `animation/genie_mesh.cpp/.hpp`; platform-free leaf code |
| D3D11 device/context/factory | `rendering/d3d_device.cpp/.hpp` |
| Captured GPU texture value | `rendering/texture.hpp` (the current `CapturedTexture`) |
| DXGI output enumeration, `IDXGIOutputDuplication`, frame acquire/release/access-lost reset | `rendering/desktop_duplication_session.cpp/.hpp` |
| Region/window capture, clipping, rotated-output conversion, GDI `PrintWindow` fallback | `rendering/desktop_capture.cpp/.hpp` |
| Visual overlay HWNDs, hit testing, visible regions, target indicator | `rendering/overlay_window.cpp/.hpp` |
| Composition swap chain/target/visual, RTV, present/commit/resize | `rendering/overlay_renderer.cpp/.hpp` |
| Animation shaders, buffers, constants, mesh upload and draw | `rendering/animation_renderer.cpp/.hpp` |
| Progress clock, reverse/continue/cancel handshake and active run state | `runtime/animation_run.cpp/.hpp` plus `runtime/frame_scheduler`; it invokes `AnimationRenderer` and `OverlayWindow` |
| Device-loss detection policy, teardown ordering and retry/backoff | `runtime/renderer_recovery.cpp/.hpp`; low-level objects only report typed failures |
| Window enumeration/bounds/placement/monitor queries | `platform/windows/window_state.cpp/.hpp` and narrow types in `window_types.hpp` |
| Named window properties, DWM attributes, cloak/region/transparency restoration | `platform/windows/window_properties.cpp/.hpp` |
| Process path/version/UTF conversion | `platform/windows/process_info.cpp/.hpp` |
| AppContainer ACL mutation | a precise `platform/windows/app_container_access.cpp/.hpp` adapter, used by the hook loader and logger setup |
| Native-transition suppression | `platform/windows/native_animation_blocker.cpp/.hpp` |
| WinEvent/shell observation | `platform/windows/window_event_monitor.cpp/.hpp` |
| Taskbar/monitor/icon target discovery and environment override parsing | `platform/windows/taskbar_target_provider.cpp/.hpp`; return platform `RECT`/edge data, not animation types |
| Hook resource/cache/fingerprint/load/export lookup/global hook lifetime plus stable registered-message broker | `platform/windows/cbt_hook_manager.cpp/.hpp` (with a private, precisely named embedded-resource helper if needed) |
| Injected CBT callback | Keep the separate `hook/hook.cpp` DLL module and exact ABI/behavior |

Required dependency corrections:

- `animation` must not include or semantically depend on Win32, D3D, ImGui, hooks, UI, or
  `AppSettings`. Its public headers currently satisfy that rule, but `genie_mesh.cpp` includes
  `pch.hpp`, which transitively includes Windows/D3D/DirectComposition/ImGui configuration. The
  migrated leaf implementation must build from explicit STL headers and remain independently
  testable.
- `platform/windows` may not depend on `animation`. The current `TaskbarTarget` exposes
  `animation::RectF` and `animation::GenieEdge`; replace them with a platform rectangle and a
  platform taskbar-edge value, then map to animation geometry in `MinimizeFeature`/`RestoreFeature`.
- `rendering` may depend on `animation` and narrow Windows types, but it must not know settings
  pages, hotkeys, exclusions, pause/fullscreen/battery policy, or feature transactions.
- The settings-window ImGui device/recovery remains under `ui/rendering`; it must not share or
  acquire ownership of the animation D3D device.

## Current call and lifetime graph

```text
Application
  owns D3dDevice
    ├─ borrowed by DesktopCapture
    └─ borrowed by every OverlayWindow in Application::runs_
  owns DesktopCapture
    └─ owns per-output duplication + cached desktop textures
  owns AnimationRun slots
    └─ each owns OverlayWindow
       ├─ owns overlay/indicator HWNDs
       ├─ owns swap chain + DirectComposition graph
       ├─ owns shaders/buffers/states
       └─ owns current CapturedTexture references + mesh/progress state
  owns NativeAnimationBlocker, WindowEventMonitor, TaskbarTargetProvider
  owns HMODULE hook_dll_ and HHOOK cbt_hook_ (to move to CbtHookManager)

GenieHookPost.dll::CBTProc
  finds "GenieEffectOverlayWindow"
  ├─ posts "GenieMinimizeAttempt"
  └─ synchronously sends "GenieRestoreAttempt"
       → OverlayWindow::WindowProc/HandleMessage
       → callbacks into Application minimize/restore feature logic
```

The current safe graphics teardown order is overlays first, then capture sessions/textures, then
the shared D3D device. `Application::CreateAnimationRenderer()`, renderer recovery, and final
shutdown already mostly follow that order. The new owners must encode it structurally rather than
depending on raw-pointer discipline.

## Type, enum, state, and resource inventory

### Animation value types and state

| Old declaration/state | Current role and users | Required target / disposition | Ownership and constraints |
| --- | --- | --- | --- |
| `RectF` | Source/target/mesh geometry; app, taskbar provider and overlay | `animation/geometry.hpp` | Four floats; value only. Platform adapters must stop returning it directly. |
| `PointF` | `GenieMeshGenerator::screen_positions_` | `animation/geometry.hpp` | Value only. |
| `SizeF` | Only `CapturedTexture::size` | Prefer rendering-owned texture extent in `rendering/texture.hpp`; remove from animation if no genuine animation user remains | Value only; its current placement creates a semantic rendering-to-animation dependency for texture metadata. |
| `EasingCurve` | Persisted-name mapping and frame easing | `animation/easing.hpp` | Preserve all values: Linear, EaseIn, EaseOut, EaseInOut, Cubic, Back, Elastic, Custom. |
| `CubicBezier` | Settings model/UI/editor and overlay easing | `animation/easing.hpp` as a pure value | Four floats; no resources. Stored compatibility is handled by settings serializer/validator. |
| `GenieEdge` | Mesh orientation and current taskbar result | `animation/genie_mesh.hpp`; platform edge must be a separate value mapped by features | Preserve Top/Bottom/Left/Right semantics. |
| `GenieDirection` | Mesh progress orientation | `animation/genie_mesh.hpp` | Preserve Minimize/Maximize; the current renderer always passes Minimize and obtains restore by decreasing progress. |
| `AnimationStyle` | Classic/curvy/squash mesh choice | `animation/genie_mesh.hpp` | Preserve Classic/Curvy/Squash. Persisted historical spelling is a settings concern. |
| `MeshVertex` | Dynamic vertex buffer payload | `animation/genie_mesh.hpp` | Four floats in position/UV order; layout must continue matching D3D input offsets 0 and 8. |
| `GenieMesh` | Reusable CPU vertex/index storage | `animation/genie_mesh.hpp` | Owns vectors; indices are `uint16_t`. |
| `GenieMeshGenerator` fields | Cached screen positions/index-layout dimensions, segment count, strength | `animation/genie_mesh.cpp/.hpp` | Owns vectors/value state. Segment count clamps 2..100. Current overlay buffers only fit 50 segments (102 vertices/300 indices), so the new renderer must derive capacity from the same maximum or enforce one shared limit. |
| Mesh constants | `kSlideAnimationEndFraction=.5`, translate start `.4`, curvy shape `.20`, stretch factor `.70` | Private constants in `animation/genie_mesh.cpp` | Preserve behavior; no stateful ownership. |

### Platform types and static callback state

| Old declaration/state | Current role | Required target | Ownership / cleanup |
| --- | --- | --- | --- |
| `NativeAnimationBlocker` | Forces DWM transitions off globally while effect active | `platform/windows/native_animation_blocker` | Borrows ignored HWND and stores non-owning HWND identities. Destructor calls `Disable()`. Target must be idempotent and restore the prior per-window attribute rather than blindly forcing `FALSE`. |
| `WindowEventMonitor::WindowCallback`, `WindowSeenCallback` | Minimize, restore and generic seen-event dispatch | `platform/windows/window_event_monitor` | Owns `std::function` closures (currently capturing `Application`). Target callback contract stays observational and must not own feature logic. |
| Five `HWINEVENTHOOK` fields | Minimize start/end, show, foreground, state change | Same monitor | Owned handles; each must be unhooked exactly once. |
| `message_window_`, `shell_hook_message_` | Message-only shell-hook window and registered `SHELLHOOK` ID | Same monitor | Owns HWND; shell registration is deregistered before destroy. Registered message ID is process-global value. |
| `WindowEventMonitor::active_monitor_` | Static Win32 callback thunk target | Narrow private callback state in same monitor | The only global system pointer in this scope. Allowed only as an encapsulated C-callback thunk; enforce one active instance, clear during stop, and never expose as service locator. |
| `TaskbarTarget` | Animation `RectF` plus `GenieEdge` | Platform `TaskbarTarget` in `window_types.hpp` or provider header using `RECT` plus `TaskbarEdge` | Value only; remove animation dependency. |
| `TaskbarTargetProvider` | Environment, UI Automation, shell/monitor fallback | `platform/windows/taskbar_target_provider` | Stateless object; all HWNDs are borrowed. |
| UI Automation `Translation` and raw COM locals | File-description lookup and taskbar subtree query | Private provider/process-info implementation | Current code manually `Release()`s every COM interface and `SysFreeString()`s names. Migrate to `ComPtr`/BSTR RAII without changing scoring. COM apartment remains process-bootstrap responsibility. |
| Window class exclusion list | Filters Progman, WorkerW, taskbars and known shell/input helper classes | Private `window_state` constant | Preserve all eight class names until tests establish intentional changes. |

### Rendering types, COM objects, and handles

| Old declaration/state | Current role | Required owner | Ownership / cleanup / recovery |
| --- | --- | --- | --- |
| `D3dDevice` | Shared animation graphics device | `rendering/d3d_device` | Owns device, immediate context, `IDXGIDevice`, and factory via `ComPtr`. Accessors return borrowed pointers. Implicit destructor releases in reverse field order. |
| `CapturedTexture` | Capture result/snapshot/overlay source | Rename or retain as the concrete value in `rendering/texture.hpp` | Owns texture and SRV `ComPtr`s; copies add COM refs. Snapshot maps and active run can share the same GPU objects. |
| `DesktopCapture::OutputCapture` | One monitor/output duplication session | `DesktopDuplicationSession::Output` | Owns duplication and cached full-output texture via `ComPtr`; desktop coordinates/format are values. |
| `DesktopCapture::AcquireResult` | Frame acquisition outcome | `DesktopDuplicationSession::AcquireResult` | Preserve Acquired, NoNewFrame, AccessLost, DeviceLost, Failed as typed outcomes. |
| `DesktopCapture` | Duplication plus crop/rotate plus `PrintWindow` fallback | Split session from `rendering/desktop_capture` | Borrows non-null D3D device; vector owns output sessions; `device_lost_` is a report flag. Prefer a reference or checked non-null handle. |
| GDI objects in `CaptureWindow` | Screen DC, memory DC, DIB, selected old bitmap, stock brush | `DesktopCapture` private window-capture path | `GetDC`/`ReleaseDC`, `CreateCompatibleDC`/`DeleteDC`, select old bitmap back before `DeleteObject`. Stock brush is borrowed. Convert to scope guards. |
| Duplication acquired frame | `IDXGIResource`/texture between Acquire/ReleaseFrame | `DesktopDuplicationSession` | Current paths call `ReleaseFrame()` manually after QI/copy and on QI/create failures. Target needs an acquired-frame RAII guard so every future early return releases it. |
| Rotated staging textures/maps | CPU rotation for 90/180/270 outputs | `DesktopCapture` | Local `ComPtr`; every successful `Map` must pair with `Unmap`. |
| `FrameConstants` | HLSL cbuffer: viewport, opacity, padding | `rendering/animation_renderer` | 16-byte value copied through a mapped dynamic constant buffer. |
| `OverlayWindow::AnimationState` | Capture, geometry, progress/clock/style/fade state | Split into `runtime::AnimationRun` state and immutable `AnimationRenderer` frame input | Owns a `CapturedTexture`; all other members values. Runtime state, not the HWND wrapper, owns the clock/transition lifecycle. |
| `OverlayWindow` | Currently visual window, hook broker, DirectComposition, D3D animation renderer and run clock in one class | Retain a small `rendering::OverlayWindow`; move the other roles to `CbtHookManager`, `OverlayRenderer`, `AnimationRenderer`, and `runtime::AnimationRun` | Noncopyable owner today. After split each owner has one resource family and explicit initialization order. |
| Overlay HWNDs | Main overlay and target indicator | `rendering/overlay_window` | Owned and destroyed idempotently. Window classes are registered process-wide; class registration lifetime needs a deliberate one-time owner. |
| Overlay swap chain and DirectComposition graph | `IDXGISwapChain1`, device, target, visual | `rendering/overlay_renderer` | Owned `ComPtr`s; visual content is swap chain, target root is visual. Release after unbinding RTV and before D3D device destruction. |
| Overlay D3D resources | RTV, VS/PS, input layout, vertex/index/constant buffers, sampler, blend, rasterizer | `rendering/animation_renderer` except surface RTV in `OverlayRenderer` | Owned `ComPtr`s. Dynamic buffers are remapped per frame; SRV is explicitly unbound after draw. |
| `GenieMeshGenerator`, reusable mesh, index count | CPU mesh cache and draw count | `rendering/animation_renderer` owns generator/mesh; math remains in `animation` | Value/vector ownership; reset on renderer teardown. |
| `OverlayWindow::MinimizeCallback`, `RestoreCallback` | Registered-message dispatch into `Application` | Callback boundary moves to `CbtHookManager`, wired to `EffectController`/features | `std::function` currently owns closures. Remove low-level renderer callback into `Application`; do not create a service locator. |
| Target-indicator regions | Outer/inner `HRGN` | `OverlayWindow` | `inner` is deleted locally. `SetWindowRgn` takes ownership of `outer` only on success; current code does not handle failure, so target must use the same owned-region wrapper as the main overlay. |
| Main overlay regions | Empty/visible `HRGN` | `OverlayWindow` via `window_properties` region helper | `SetOwnedWindowRegion` deletes on invalid HWND/failure and transfers ownership on success. |
| Indicator class brush | `CreateSolidBrush` assigned during class registration | One-time registered-class owner | Current repeated registration leaks newly created brushes when the class already exists and never unregisters/deletes the successful brush. Fix ownership during the structural split. |

### Hook DLL ABI and build state

| State/contract | Exact current value and required preservation |
| --- | --- |
| DLL/export | `GenieHookPost.dll`; `extern "C" __declspec(dllexport) LRESULT CALLBACK CBTProc(int, WPARAM, LPARAM)`. Loader probes `CBTProc`, `_CBTProc@12`, then ordinal 1. Preserve the C name/calling convention and loader compatibility. |
| Hook type | App installs `WH_CBT` globally (`thread_id=0`) with the loaded DLL and owns the returned `HHOOK`. Only `HCBT_MINMAX` is intercepted; all other calls chain with `CallNextHookEx(nullptr, ...)`. |
| Overlay discovery | `FindWindowW(L"GenieEffectOverlayWindow", nullptr)`. The class name is cross-module ABI and may not be renamed. |
| Registered messages | `GenieMinimizeAttempt` (async `PostMessageW`) and `GenieRestoreAttempt` (`SendMessageTimeoutW`, `SMTO_ABORTIFHUNG`, 75 ms). `wParam` is target HWND and `lParam` is the original CBT value/show command. |
| Return contract | Minimize returns nonzero/block only if the post succeeds. Restore returns nonzero/block only if the overlay synchronously returns a nonzero handled value. Failure/timeout chains and permits native behavior. |
| UIPI | Overlay calls `ChangeWindowMessageFilterEx(..., MSGFLT_ALLOW)` for both registered messages. A global hook still cannot observe elevated targets unless Genie Effect itself is elevated; preserve warning/fallback behavior. |
| Project | x64 Debug/Release dynamic library, toolset v145, MultiByte with explicit wide APIs, no PCH, static CRT, `user32.lib`, output `build/bin/x64/<Configuration>/GenieHookPost.dll`, object tree `build/obj/Hook/...`. |
| Build coupling | App target `BuildGenieHook` recursively builds the hook before app preparation. Release resource compile defines `GENIE_EMBED_RELEASE_HOOK`; resource `IDR_GENIE_HOOK` is exactly 205 and embeds the Release DLL. |
| Hook project pre-build | Force-terminates a running `GenieEffect.exe` before rebuilding. Preserve only as project behavior unless later deliberately replaced; it is not runtime cleanup. |
| Logger include | Hook currently includes app `common/debug_log.hpp` relatively. Logger migration must provide a real linkable implementation usable by both projects without moving logic into `DllMain`. |

## Function-level migration ledger: animation

### `geometry.hpp`

| Old symbol | Current callers / behavior | Required single target | Ownership / cleanup / recovery |
| --- | --- | --- | --- |
| `RectF::Width()` | Mesh curvy calculations | `animation::RectF::Width()` | Pure subtraction; no resources. |
| `RectF::Height()` | Mesh curvy calculations | `animation::RectF::Height()` | Pure subtraction; no resources. |
| `RectF::CenterX()` | No first-party caller | Remove unless a migrated pure-animation caller genuinely uses it | Pure value; consciously classified as currently dead. |
| `RectF::CenterY()` | No first-party caller | Remove unless a migrated pure-animation caller genuinely uses it | Pure value; consciously classified as currently dead. |

### `easing.hpp`

| Old symbol | Current callers / behavior | Required single target | Ownership / cleanup / recovery |
| --- | --- | --- | --- |
| `CubicBezier::operator==()` | Settings-window state comparison also uses generated `!=` | `animation/easing.cpp/.hpp` value contract | Pure comparison of all four floats. |
| `CubicBezier::ClampHandles()` | Settings load/editor/application setter and overlay setter | `animation/easing.cpp/.hpp` | Clamps x to 0..1 and y to -0.5..1.5. Ensure finite-value validation occurs before calling it because NaN is not repaired by `std::clamp`. |
| `CubicBezier::Ease()` | No first-party caller | Remove unless retained as an explicitly used public preset | Current dead preset `(0.25,0.1,0.25,1)`. |
| `CubicBezier::EaseInOut()` | Settings defaults and overlay/runtime defaults | `animation/easing.cpp/.hpp` | Pure preset `(0.42,0,0.58,1)`. |
| `EasingCurveFromName()` | Application minimize/restore renderer setup | Prefer typed settings/view model conversion in `settings_serializer`/`settings_validator`; otherwise a pure named-preset adapter beside easing, never runtime UI code | Exact names: Ease In, Ease Out, Ease In Out, Cubic, Back, Elastic, Custom; unknown maps Linear. Persisted compatibility remains settings-owned. |
| `detail::BezierComponent()` | Derivative solver, curve evaluation and UI point sampling | Private `animation/easing.cpp` helper | Pure Bernstein component with fixed endpoints 0/1. |
| `detail::BezierComponentDerivative()` | Newton solve | Private `animation/easing.cpp` helper | Pure derivative. |
| `detail::SolveBezierT()` | `EvaluateCubicBezier()` | Private `animation/easing.cpp` helper | Clamps x, tries 8 Newton iterations then 12 bisection iterations at `1e-6`; deterministic for finite handles. |
| `EvaluateCubicBezier()` | `ApplyEasing()` custom mode | `animation/easing.cpp/.hpp` | Solves CSS x semantics then evaluates y; may return y outside 0..1 before caller clamp. |
| `CubicBezierPoint()` | UI easing graph | `animation/easing.cpp/.hpp` pure sampling API | Clamps parameter; either output pointer may be null. No UI type dependency. |
| `ApplyEasing()` | Existing overlay render path | `animation/easing.cpp/.hpp` | Clamp input/output to 0..1; preserve Linear/quadratic/smoothstep/cubic/Back/Elastic/Custom formulas. Current final clamp intentionally removes overshoot. |

### `genie_mesh.cpp/.hpp`

| Old symbol | Current callers / behavior | Required single target | Ownership / cleanup / recovery |
| --- | --- | --- | --- |
| anonymous `Clamp01()` | Mesh timing helpers and oriented progress | Private `animation/genie_mesh.cpp` helper | Pure finite clamp. |
| anonymous `QuadraticEaseInOut()` | Classic branch-local curves | Private `animation/genie_mesh.cpp` helper | Pure piecewise easing. |
| anonymous `SineEaseInOut()` | Curvy shape scaling | Private `animation/genie_mesh.cpp` helper | Pure cosine easing. |
| anonymous `Lerp()` | Curvy/squash geometry | Private `animation/genie_mesh.cpp` helper | Pure interpolation. |
| anonymous `SafeDivisor()` | Classic/curvy normalization | Private `animation/genie_mesh.cpp` helper | Replaces magnitudes below `0.0001` with signed epsilon; document finite-input precondition. |
| anonymous `AppendCellIndices()` | `GenerateInto()` when topology changes | Private `animation/genie_mesh.cpp` helper | Appends six `uint16_t` indices per grid cell; caller clears first. Validate grid/index limits centrally. |
| `GenieMeshGenerator::SetStrength()` | Overlay/runtime configuration | `animation::GenieMeshGenerator` | Stores raw value; Classic generation later clamps 0..1, Curvy/Squash ignore it. |
| `GenieMeshGenerator::SetLongGridSegmentCount()` | Quality selection through overlay | `animation::GenieMeshGenerator` | Clamp 2..100. Share maximum with GPU-buffer sizing. |
| `GenieMeshGenerator::GenerateInto()` | Overlay render each frame | `animation::GenieMeshGenerator` | Owns/reuses internal positions and resizes caller mesh vertices. Returns “indices changed”, not success; null mesh also returns false. Replace with an unambiguous result while preserving topology caching. Remove ignored `viewport_height` parameter. Local row/column UV and linear-strength blending remain here. |
| `GenieMeshGenerator::GenerateCurvyPositions()` | `GenerateInto()` Curvy | Private `animation::GenieMeshGenerator` implementation | Produces target-neck stretch/squash for all four edges. Owns no external resources. |
| `GenieMeshGenerator::GenerateSquashPositions()` | `GenerateInto()` Squash | Private `animation::GenieMeshGenerator` implementation | Four linearly interpolated corners. |
| `GenieMeshGenerator::GenerateClassicPositions()` | `GenerateInto()` Classic | Private `animation::GenieMeshGenerator` implementation | Four edge-specific paths. Its eight branch-local Bezier-position lambdas remain local implementation details, not new module functions. |

Animation acceptance constraints: for validated finite rectangles/progress/control points and legal
segment counts, every output must be deterministic and finite; index/vertex counts must match the
grid; Minimize and Maximize must retain reversed-progress semantics. Invalid/NaN settings must be
rejected or normalized before entering this leaf module.

## Function-level migration ledger: Windows platform

### `window_util.cpp/.hpp`

The old file must be deleted after every row below has moved; it must not survive as a forwarding
header or a new generic helper collection.

| Old symbol | Current callers / behavior | Required single target | Ownership / cleanup / recovery |
| --- | --- | --- | --- |
| anonymous `WideToUtf8()` | `GetWindowExecutableName()` | Private `platform/windows/process_info.cpp` helper | Stack/owned strings only; `WC_ERR_INVALID_CHARS`, empty on conversion failure. |
| anonymous `IsExcludedClassName()` | `IsInterestingTopLevelWindow()` | Private `platform/windows/window_state.cpp` helper | Preserve exclusions: Progman, WorkerW, Shell_TrayWnd, Shell_SecondaryTrayWnd, DV2ControlHost, Windows.UI.Core.CoreWindow, XamlExplorerHostIslandWindow, ApplicationFrameInputSinkWindow. |
| anonymous `CollectWindowsProc()` | `EnumWindows()` thunk in `EnumerateTopLevelWindows()` | Private callback in `platform/windows/window_state.cpp` | Borrows a stack context pair only for synchronous enumeration; no global state. |
| `GetWindowExecutableName()` | Application exclusion policy and settings active-app list | `platform/windows/process_info.cpp/.hpp` | Borrows HWND, opens process with `PROCESS_QUERY_LIMITED_INFORMATION`, closes handle on every queried path, returns UTF-8 basename only. Failure for protected/elevated processes remains non-fatal. |
| `GetExtendedFrameBounds()` | Capture, fullscreen, pacing, snapshots, window filtering/recovery | `platform/windows/window_state.cpp/.hpp` | Borrows HWND. For non-iconic windows prefers DWM extended bounds, then `GetWindowRect`; for iconic/offscreen sentinel coordinates uses normal placement adjusted from work-area to monitor coordinates. No retained handles. |
| `IsInterestingTopLevelWindow()` | Application policy/events, blocker, UI app enumeration | `platform/windows/window_state.cpp/.hpp` | Preserve validity/visible-or-iconic/root/unowned/non-tool/non-cloaked/class/48x48 filters. Borrowed ignored HWND. |
| `EnumerateTopLevelWindows()` | Blocker, exclusions, settings active-app list | `platform/windows/window_state.cpp/.hpp` | Returns borrowed HWND identities from synchronous `EnumWindows`; callers must revalidate before later use. |
| `SetDwmTransitionsDisabled()` | Native blocker and application exclusion/recovery paths | `platform/windows/window_properties.cpp/.hpp` | Borrows HWND; currently ignores DWM failure. Target blocker needs query/store/restore semantics or a documented best-effort result. |
| `SetWindowCloaked()` | Application minimize/restore/healing/recovery | `platform/windows/window_properties.cpp/.hpp` | Borrows HWND; wraps `DWMWA_CLOAKED`, currently ignores error. Must participate in one idempotent window transaction cleanup. |
| `SetOwnedWindowRegion()` | Overlay visibility and real-window hide/restore | `platform/windows/window_properties.cpp/.hpp` | Takes ownership of non-null `HRGN`: OS owns it after successful `SetWindowRgn`; helper deletes it on invalid HWND/failure. A null region clears the window region. Preserve this explicit ownership contract. |
| `GetVirtualScreenRect()` | Overlay initialization, clipping, diagnostics | `platform/windows/window_state.cpp/.hpp` or a precise monitor-query adapter | Uses SM_X/Y/CX/CYVIRTUALSCREEN and supports negative multi-monitor origins. Value only. |
| `FindTaskbarWindowForRect()` | Taskbar provider and overlay taskbar cutout/z-order | Private/public narrow helper in `platform/windows/taskbar_target_provider` | Borrows returned HWND. Chooses primary or enumerated secondary taskbar on the rect's nearest monitor; if none match, returns primary taskbar even if on another monitor. |
| `GetMonitorRefreshRateHz()` | Frame pacing and diagnostics | `platform/windows/window_state.cpp/.hpp` or precise display-mode adapter | Borrows HMONITOR. Up to three QueryDisplayConfig attempts preserve rational rates (for example 59.94); fallback is monitor-specific `EnumDisplaySettingsExW`. All vectors are local. |
| `GrantAppContainerPermissions()` | Debug log setup and CBT hook directory/DLL before load | `platform/windows/app_container_access.cpp/.hpp` | Merges read/execute ACEs for S-1-15-2-1 and S-1-15-2-2. Owns/frees security descriptor, converted SIDs, and new ACL with `LocalFree`; old DACL is borrowed from descriptor. Preserve existing ACLs and inheritable flags. |

### `native_animation_blocker.cpp/.hpp`

| Old symbol | Current callers / behavior | Required single target | Ownership / cleanup / recovery |
| --- | --- | --- | --- |
| `NativeAnimationBlocker::~NativeAnimationBlocker()` | Owning `Application` destruction | `platform/windows/native_animation_blocker` | Must continue invoking idempotent restoration. |
| `NativeAnimationBlocker::Enable()` | Effect activation and post-renderer recovery | Same class | Calls `Disable()` if already active, borrows ignored overlay HWND, enumerates interesting windows, disables transitions and tracks HWNDs. It always returns true today even when DWM calls fail; replace with useful status without making partial cleanup unsafe. |
| `NativeAnimationBlocker::Disable()` | Effect pause/disable, renderer recovery and final shutdown | Same class | Restores tracked valid windows, clears set, then enumerates all current interesting windows and forces transitions enabled. Target must restore only changes it owns and remain safe for destroyed/reused HWNDs. |
| `NativeAnimationBlocker::SetTransitionsDisabledForWindow()` | Event-seen, minimize/restore and abort paths | Same class | When disabling, requires blocker enabled and interesting window, then tracks it. When enabling, restores valid window and erases it. Target should retain per-window prior value/identity needed for reliable rollback. |

### `window_event_monitor.cpp/.hpp`

| Old symbol | Current callers / behavior | Required single target | Ownership / cleanup / recovery |
| --- | --- | --- | --- |
| anonymous `EventName()` | Trace output from `OnWinEvent()` | Private monitor/log formatting helper | Pure mapping for the five watched events, numeric fallback. |
| anonymous `WindowBrief()` | Trace output for shell/WinEvent routes | Private monitor/log formatting helper or core logger formatter | Borrows HWND and reads class/title/visibility/iconic/placement. |
| `WindowEventMonitor::~WindowEventMonitor()` | Owning `Application` destruction | `platform/windows/window_event_monitor` | Calls `Stop()`; idempotent. |
| `WindowEventMonitor::Start()` | `Application::Initialize()` and `ExitSafeMode()` | Same class, wired by `EffectController` | First stops old state, stores callbacks, installs five out-of-context hooks with `WINEVENT_SKIPOWNPROCESS`, registers/creates message-only shell window, registers shell hook/message. If any WinEvent hook fails, calls `Stop()` and returns false. Preserve transactional rollback. |
| `WindowEventMonitor::Stop()` | Startup rollback/destructor/final shutdown | Same class | Unhooks each non-null handle, deregisters/destroys message HWND, clears callback thunk pointer. Also clear callbacks/message ID in target for a completely quiescent state. |
| `WindowEventMonitor::HandleWinEvent()` | Win32 `SetWinEventHook` callback | Private static callback thunk in same class | Ignores hook/thread/time values and dispatches through the narrow active instance. No feature logic. |
| `WindowEventMonitor::MessageWindowProc()` | Message-only shell HWND callback | Private static window proc in same class | Dispatches to active monitor then default proc. Target should use the actual callback HWND when defaulting, rather than relying on the stored one. |
| `WindowEventMonitor::HandleMinimizeStart()` | Direct WinEvent and validated shell `HSHELL_GETMINRECT` | Private monitor dispatcher | Logs then invokes callback if present. |
| `WindowEventMonitor::HandleRestoreStart()` | `EVENT_SYSTEM_MINIMIZEEND` | Private monitor dispatcher | Logs then invokes callback if present. |
| `WindowEventMonitor::OnShellMessage()` | `MessageWindowProc()` | Same monitor | For registered `SHELLHOOK`/`HSHELL_GETMINRECT`, interprets `SHELLHOOKINFO`, checks placement/iconic state, then emits minimize; all other codes default. |
| `WindowEventMonitor::OnWinEvent()` | `HandleWinEvent()` | Same monitor | Null HWND ignored. Minimize start/end dispatch regardless of object IDs; show/foreground/state-change require `OBJID_WINDOW` and `CHILDID_SELF`, then call window-seen callback with event. |

Window-event behavior to preserve:

- Hooks cover `EVENT_SYSTEM_MINIMIZESTART`, `EVENT_SYSTEM_MINIMIZEEND`, `EVENT_OBJECT_SHOW`,
  `EVENT_SYSTEM_FOREGROUND`, and `EVENT_OBJECT_STATECHANGE`.
- The message-only class is `GenieShellHookWindow`; the registered shell message name is
  `SHELLHOOK`. These are internal platform names, unlike the hook/overlay ABI strings.
- Shell registration failure currently does not fail `Start()` if all WinEvent hooks succeeded;
  represent that degraded mode explicitly rather than silently treating the shell channel as live.
- Feature policy/exclusions and minimize/restore mechanics must remain outside the monitor.

### `taskbar_target_provider.cpp/.hpp`

| Old symbol | Current callers / behavior | Required single target | Ownership / cleanup / recovery |
| --- | --- | --- | --- |
| anonymous `ToLower()` | UIA matching | Private provider helper | Owns/mutates passed string using wide lowercase. |
| anonymous `Tokenize()` | Title/class/process/description/button matching | Private provider helper | Returns lowercase alphanumeric tokens; punctuation separates. |
| anonymous `GetProcessDescription()` | UIA matching | `platform/windows/process_info.cpp` or private provider helper | Reads version resource `FileDescription` using first translation. Local byte buffer only. |
| anonymous `FindTaskbarIconUIAutomation()` | `GetTargetForWindow()` unless environment override exists | Private provider strategy | Borrows target/taskbar HWND; opens/closes process handle; creates/releases UI Automation, elements, conditions, arrays and BSTRs. Preserve matching/scoring and make COM ownership RAII. |
| anonymous `ToRectF()` | Converts shell/UIA RECT to animation target | Remove from platform; perform platform-to-animation conversion in minimize/restore feature | Pure conversion but violates target dependency direction. |
| anonymous `Clamp()` | No caller | Remove as dead code | Pure unused helper. |
| `TaskbarTargetProvider::GetTargetForWindow()` | Application diagnostics and minimize setup | `platform/windows/taskbar_target_provider` returning platform data | Strategy order: environment override; otherwise UIA icon; otherwise actual taskbar HWND; otherwise monitor/work-area estimate. Determines top/bottom/left/right and returns exact matched icon or centered fallback target. No retained handles. |
| `TaskbarTargetProvider::TryGetEnvironmentTarget()` | `GetTargetForWindow()` | Private deterministic parser in same provider (separately testable) | Reads `GENIE_TASKBAR_RECT` into 128 wchar stack buffer; parses four comma-separated LONGs; rejects missing/oversized/malformed/non-positive extent. Current parser accepts trailing text after four conversions and does no clipping; pin behavior in tests before hardening. |
| `TaskbarTargetProvider::GetShellTaskbarRect()` | No caller | Remove as dead code | Existing ABM_GETTASKBARPOS/work-area fallback is bypassed by all live paths. |

Taskbar and monitor behavior to preserve in the live strategy:

1. The animated window rect chooses the nearest monitor.
2. `FindTaskbarWindowForRect()` selects `Shell_TrayWnd` or a
   `Shell_SecondaryTrayWnd` on that monitor.
3. UI Automation searches the selected taskbar subtree for Button/ListItem/TabItem controls. Exact
   title, executable stem, file description, substring, token, then class matches determine a
   best score; only a score above zero wins.
4. If no icon matches, taskbar bounds come from the shell HWND. If unavailable, the difference
   between `rcMonitor` and `rcWork` determines side; final fallback is a 48-pixel bottom taskbar.
5. A horizontal fallback target is 72 pixels wide and spans taskbar height; a vertical target is
   48 pixels high and spans taskbar width. A matched UIA target uses the exact button rectangle.
6. `GENIE_TASKBAR_RECT=left,top,right,bottom` skips UI Automation and supplies both target bounds
   and edge inference. It uses physical virtual-screen coordinates and is intentionally useful for
   custom/exotic taskbars.
7. Multi-monitor coordinates, including negative origins, must remain unmodified. Platform data is
   converted to animation-local coordinates only at the feature/render boundary.

## Function-level migration ledger: rendering

### `d3d_device.cpp/.hpp`

| Old symbol | Current callers / behavior | Required single target | Ownership / cleanup / recovery |
| --- | --- | --- | --- |
| anonymous `CreateHardwareDevice()` | `D3dDevice::Create()` debug and fallback attempts | Private `rendering/d3d_device.cpp` helper | Calls `D3D11CreateDevice` for hardware with feature levels 11.1/11.0. Returned COM pointers transfer to caller-provided addresses. |
| `D3dDevice::D3dDevice()` | Only `Create()` through private `new` | `rendering::D3dDevice` | Private default construction; object is invalid until factory method completes. |
| `D3dDevice::Create()` | `Application::CreateAnimationRenderer()` | `rendering::D3dDevice::Create()` | Transactionally owns partial `ComPtr`s. Uses BGRA support; Debug first requests debug layer then retries without it. Queries DXGI device, adapter and factory; any failure returns null and RAII unwinds. |
| `D3dDevice::device()` | Capture/overlay/application diagnostics | Same class accessor | Returns borrowed `ID3D11Device*`; owner must outlive consumer. |
| `D3dDevice::context()` | Capture/overlay | Same class accessor | Returns borrowed immediate context; current architecture is single-thread/message-loop use. |
| `D3dDevice::dxgi_device()` | Duplication, DirectComposition, diagnostics | Same class accessor | Returns borrowed pointer. |
| `D3dDevice::factory()` | Composition swap-chain creation | Same class accessor | Returns borrowed pointer. |
| `D3dDevice::IsDeviceLostError()` | Capture helper and device check | Same class static classification or typed rendering error helper | Preserve Removed, Reset, Hung, DriverInternalError classification. |
| `D3dDevice::DeviceRemovedReason()` | Overlay logging and `IsDeviceLost()` | Same class | Returns `GetDeviceRemovedReason`; null-device fallback is `DXGI_ERROR_DEVICE_REMOVED`. |
| `D3dDevice::IsDeviceLost()` | Application recovery polling, capture maps, overlay failures | Same class | True when supplied operation is a classified loss or device removed reason is failed. No mutation. |

The D3D device remains one owner shared by animation rendering/capture via checked references. Do
not create a second animation device in `OverlayRenderer` or `DesktopDuplicationSession`.

### `desktop_capture.cpp/.hpp`

| Old symbol | Current callers / behavior | Required single target | Ownership / cleanup / recovery |
| --- | --- | --- | --- |
| anonymous `RectWidth()` | All capture validation/cropping | Private precise rectangle helper in `desktop_capture.cpp` | Pure LONG-to-int difference. |
| anonymous `RectHeight()` | All capture validation/cropping | Private precise rectangle helper in `desktop_capture.cpp` | Pure LONG-to-int difference. |
| anonymous `ContainsRect()` | Output selection | Private `desktop_duplication_session.cpp` helper | Pure containment. |
| anonymous `ClampRectToOutput()` | Region copy/refresh | Private `desktop_capture.cpp` helper | Pure intersection-by-min/max; caller validates non-empty. |
| anonymous `IsDeviceLostError()` | D3D create/acquire paths | Remove duplicate and call `D3dDevice::IsDeviceLostError()` or typed error classifier directly | Pure forwarding helper. |
| anonymous `GetWindowCornerRadius()` | `CaptureWindow()` | Private `desktop_capture.cpp` window-capture helper | Borrows HWND. Maximized/invalid returns zero; reads numeric DWMWA 33, honors do-not-round/small preferences, DPI-scales 12/8 px. |
| anonymous `ApplyRoundedCornerMask()` | `CaptureWindow()` after successful PrintWindow | Private `desktop_capture.cpp` helper | Mutates owned BGRA byte vector: opaque alpha, transparent DWM shadow margins, antialiased corners. Its `clear_pixel`/`apply_alpha` lambdas remain local. |
| `DesktopCapture::DesktopCapture()` | `Application::CreateAnimationRenderer()` | `rendering::DesktopCapture` | Current raw D3D pointer is borrowed and unchecked; use a non-null reference/lifetime token. |
| `DesktopCapture::CaptureRegion()` | Minimize capture and pre-minimize snapshots | `rendering::DesktopCapture` | Validates output, asks session for first/cached frame (120 ms first timeout), allocates cropped texture/SRV. Returns false without altering caller result on failure. |
| `DesktopCapture::CaptureWindow()` | Fallback in minimize and pre-minimize snapshots | `rendering::DesktopCapture` | Intersects window/requested rect or falls back to full window; uses `PrintWindow(FULLCONTENT)` then plain PrintWindow, applies alpha mask, creates BGRA texture/SRV, returns actual screen rect. All GDI/COM temporaries must unwind on every branch. |
| `DesktopCapture::RefreshCapturedTexture()` | Active live capture and same-size pre-snapshot refresh | `rendering::DesktopCapture` | Non-blocking acquire timeout zero; reuses existing destination texture. False disables live refresh but does not by itself destroy snapshot. |
| `DesktopCapture::ClearHistory()` | Effect disable and before minimize capture | Delegate to `DesktopDuplicationSession::ClearHistory()` | Resets only cached full-output frames; duplication sessions remain. |
| `DesktopCapture::device_lost()` | `Application::AnimationRendererDeviceLost()` | Low-level typed status exposed by capture/session | Value read only. Runtime recovery consumes it. |
| `DesktopCapture::ClearDeviceLost()` | No first-party caller | Remove; recovery replaces the entire capture/device graph | Current dead flag reset would be unsafe without recreating resources. |
| `DesktopCapture::AcquireFrameForRect()` | Capture/refresh | `DesktopDuplicationSession::AcquireFrameForRect()` | Lazily initializes outputs, chooses output, acquires with first-frame timeout only when no cache. On AccessLost clears all outputs and retries once; DeviceLost aborts; timeout may reuse cache. |
| `DesktopCapture::TryAcquireLatestFrame()` | `AcquireFrameForRect()` | `DesktopDuplicationSession::TryAcquireLatestFrame()` | Handles WAIT_TIMEOUT, ACCESS_LOST, classified device loss and generic failure. Acquired resource is QI'd to texture, copied to a size/format-matched cache, and `ReleaseFrame()` is mandatory. |
| `DesktopCapture::CopyRegionFromFrame()` | `CaptureRegion()` | `rendering::DesktopCapture` | Identity output uses GPU crop and new SRV. Rotated output maps a staging crop, CPU-rotates 90/180/270, unmaps, then creates destination texture/SRV. Its `copy_pixel` lambda remains local. |
| `DesktopCapture::CopyRegionIntoTexture()` | `RefreshCapturedTexture()` | `rendering::DesktopCapture` | Requires same destination width/height. Identity output GPU-copies into destination; rotated output repeats staging/CPU rotation then `UpdateSubresource`. Its separate `copy_pixel` lambda remains local. |
| `DesktopCapture::InitializeOutputs()` | Lazy acquire | `DesktopDuplicationSession::InitializeOutputs()` | Gets adapter from shared DXGI device; enumerates every output; QIs `IDXGIOutput1`; calls `DuplicateOutput`; skips individual failures and succeeds if any output remains. Device-loss results from duplication creation should be surfaced explicitly. |
| `DesktopCapture::FindOutputForRect()` | Frame acquire | `DesktopDuplicationSession::FindOutputForRect()` | Prefers full containment, otherwise monitor containing center point. A spanning rect is therefore clipped to one output; preserve/document or deliberately improve only with dedicated multi-output tests. |
| `DesktopCapture::ResetOutputs()` | Access-lost recovery | `DesktopDuplicationSession::Reset()` | Clears vector, releasing duplication and cached-frame COM refs. Must only run after an acquired-frame guard has released its frame. |
| `DesktopCapture::MarkDeviceLost()` | Acquire/create/map failures | Split: session/capture report typed DeviceLost; `runtime::RendererRecovery` owns policy/log/retry | Sets flag and logs operation HRESULT plus removed reason. It does not recreate anything locally. |

Desktop Duplication details that tests and migration must pin:

- A cache exists per enumerated output and is updated with `CopyResource` while the duplication
  frame is held; the frame is released immediately after the copy.
- First acquisition may wait 120 ms; subsequent and refresh acquisition use zero timeout and may
  reuse the prior cached frame.
- `DXGI_ERROR_ACCESS_LOST` destroys/recreates all output sessions and retries once. Device loss is a
  different outcome and escalates to common renderer recovery.
- Source rectangles outside a single output are clipped. Output rotation is read from
  `IDXGIOutputDuplication::GetDesc()`. R16G16B16A16 variants use 8 bytes per pixel; current code
  assumes 4 bytes for all other formats.
- `CaptureWindow()` is independent of Desktop Duplication and is the fallback for iconic/capture
  failures. It also strips DWM shadow margins and Windows 11 rounded corners through alpha.
- Texture format/dimensions must remain compatible between initial capture, snapshot copies and
  refresh. Identity refresh currently checks dimensions but not destination format; make the
  contract explicit during split.

### `overlay_window.cpp/.hpp`

| Old symbol | Current callers / behavior | Required single target | Ownership / cleanup / recovery |
| --- | --- | --- | --- |
| anonymous `RectWidth()` | Animation-surface/resize validation | Private `rendering/overlay_window.cpp` geometry helper | Pure RECT difference. |
| anonymous `RectHeight()` | Animation-surface/resize validation | Private `rendering/overlay_window.cpp` geometry helper | Pure RECT difference. |
| anonymous `RectToRectF()` | `ToOverlayRect()` | Private conversion at `OverlayWindow`/animation-render boundary | Pure conversion; do not move Win32 into animation. |
| anonymous `AnimationSurfaceRect()` | `StartAnimation()` | `OverlayWindow` surface-bounds calculation | Union of source/target plus 2 px, clipped to virtual screen; pure except Win32 RECT APIs. |
| anonymous `RectFTraceString()` | Start/first-frame tracing | Core logger formatting or private runtime trace helper | String only. |
| anonymous `CompileShader()` | `OverlayWindow::CompileShaders()` | Private `rendering/animation_renderer.cpp` helper | Compiles in-memory HLSL with strictness and Debug flags; owns error blob locally and returns HRESULT. |
| anonymous `AllowCrossIntegrityMessage()` | Overlay initialization for two hook messages | Private `platform/windows/cbt_hook_manager.cpp` message-broker helper | Borrows broker HWND; calls `ChangeWindowMessageFilterEx(MSGFLT_ALLOW)`, logs status. This is required for allowed cross-integrity registered messages but does not bypass global-hook UIPI limits. |
| `OverlayWindow::~OverlayWindow()` | Run-slot/container destruction | `rendering::OverlayWindow` | Calls idempotent shutdown; after split the owning `AnimationRun` destroys animation renderer, overlay renderer, then HWND wrapper in dependency-safe order. |
| `OverlayWindow::Initialize()` | `Application::InitializeRun()` | Transactional `runtime::AnimationRun`/pool construction which initializes `OverlayWindow`, `OverlayRenderer`, then `AnimationRenderer` | Currently stores borrowed D3D/callbacks, registers hook message IDs, snapshots virtual screen, creates HWND/composition/RTV/resources, rolls back via `Shutdown()`, allows UIPI messages, clears/shows window. Move the hook-message registration/filter/callback half to `CbtHookManager`; preserve graphics transactionality without one monolithic owner. |
| `OverlayWindow::Shutdown()` | Renderer recreate, recovery, final cleanup, destructor | `runtime::AnimationRun::Shutdown()` delegating to the three rendering owners | Stops active state, destroys HWNDs, resets every D3D/DComp `ComPtr`, clears mesh, nulls borrowed device. Must be idempotent; callbacks and message IDs should also be cleared. |
| `OverlayWindow::SetAnimationDuration()` | Minimize/restore setup and initial run default | Fold into typed `runtime::AnimationRunStart`/frame schedule configuration | Stores seconds; current caller guarantees 0.10..2.00 after style scale. Runtime must guard zero/non-finite duration. |
| `OverlayWindow::SetAnimationEasing()` | Minimize/restore setup | Fold into typed `runtime::AnimationRunStart` render configuration | Stores curve/custom Bezier and clamps handles. |
| `OverlayWindow::SetAnimationStyle()` | Minimize/restore setup | Fold into typed `AnimationRenderer` configuration passed by run start | Value only. |
| `OverlayWindow::SetMeshSegmentCount()` | Application quality selection | Fold into typed `AnimationRenderer` configuration | Delegates generator clamp; buffer capacity must share the same limit. |
| `OverlayWindow::SetGenieStrength()` | Minimize/restore setup | Fold into typed `AnimationRenderer` configuration | Raw value stored; mesh generator clamps where applicable. |
| `OverlayWindow::SetFadeStrength()` | Minimize/restore setup | Fold into typed `AnimationRenderer` configuration | Raw value stored; constants path clamps 0..0.65. |
| `OverlayWindow::SetTargetIndicatorEnabled()` | Minimize/restore setup | `OverlayWindow` presentation configuration | Value only; indicator belongs to HWND layer, not animation math. |
| `OverlayWindow::window()` | Application overlay filtering/hook target/cleanup | `OverlayWindow::window()` | Returns borrowed owned HWND. |
| `OverlayWindow::active()` | Application run allocation/loop/diagnostics/cleanup | `runtime::AnimationRun::IsActive()` | Runtime state query, no HWND/render ownership. |
| `OverlayWindow::clock_started()` | Application native-minimize handshake | `runtime::AnimationRun::HasStartedClock()` or explicit run state | Runtime state query; prefer state-machine transition over an extra bool. |
| `OverlayWindow::device_lost()` | Application recovery polling | Replace with typed render-operation failure consumed by `runtime::RendererRecovery` | Current stored bool aggregates surface/shader/present failures only when device classification confirms loss. |
| `OverlayWindow::StartAnimation()` | Application minimize/restore paths | `runtime::AnimationRun::Start()` | Validate source texture/surface, configure run, render first frame, show overlay, wait for DComp commit. Delegates geometry draw to `AnimationRenderer`, surface to `OverlayRenderer`, HWND/region to `OverlayWindow`; failure uses common run cleanup. |
| `OverlayWindow::StartAnimationClock()` | Native minimize completion and restore start | Runtime run-state transition / `FrameScheduler` | Sets last tick and clock-started. |
| `OverlayWindow::ContinueMinimizeAnimation()` | Minimize during active reverse/animation | `runtime::AnimationRun::ContinueMinimize()` | Sets target progress 1 and restarts clock. |
| `OverlayWindow::ReverseAnimation()` | Restore during active minimize | `runtime::AnimationRun::ReverseToRestore()` | Sets target progress 0 and restarts clock. |
| `OverlayWindow::Tick()` | Application frame loop | `runtime::AnimationRun::Update()` using `FrameScheduler` and `AnimationRenderer::RenderFrame()` | Advances progress by elapsed/duration, renders, handles indicator deadline, and reports completion/failure. At minimize target it clears texture/hides; at restore target it deliberately leaves frame/texture until explicit finish. Preserve handshake through typed completion state. |
| `OverlayWindow::CancelAnimation()` | Timeout/abort/recovery/forced completion | `runtime::AnimationRun::Cancel()` | Clears active texture, hides/clears overlay, hides indicator. Must converge on common idempotent cleanup. |
| `OverlayWindow::FinishRestoreAnimation()` | Normal/forced restore completion | `runtime::AnimationRun::FinishRestore()` | Releases texture and hides overlay/indicator after real window has been restored. |
| `OverlayWindow::restoring()` | Application refresh/branch decisions | `runtime::AnimationRun` state query | Current predicate is active and target progress less than current progress; replace with explicit state. |
| `OverlayWindow::mutable_captured_texture()` | Desktop live-refresh destination | Narrow `runtime::AnimationRun`/snapshot texture update API | Returns pointer to owned texture today. Avoid exposing a broadly mutable renderer internals pointer; capture may update a checked texture handle. |
| `OverlayWindow::WindowProc()` | Registered overlay HWND | `rendering::OverlayWindow` static callback | On `WM_NCCREATE` stores `this` in `GWLP_USERDATA` and sets owned HWND; otherwise retrieves and dispatches. Default when absent. |
| `OverlayWindow::HandleMessage()` | `WindowProc()` | Visual-message part stays in `rendering::OverlayWindow`; registered hook-message branch moves once to `CbtHookManager`'s broker | Current registered minimize calls feature request and on failure temporarily sets `GenieAllowMinimize` then native-minimizes; restore returns handled bool. Those branches/fallback move to platform/features. Mouse activate/hit-test remain no-activate/transparent; destroy clears visual HWND. |
| `OverlayWindow::RegisterWindowClass()` | Initialization | One-time `OverlayWindow` class-registration owner | Registers overlay and indicator classes, tolerating already-exists. Own/delete the indicator brush correctly and preserve class names/cursor/no background behavior. |
| `OverlayWindow::CreateOverlayWindow()` | Initialization | `rendering::OverlayWindow` | Creates topmost/tool/noactivate/transparent/layered popup plus indicator; applies alpha 190. Owns both HWNDs and rolls back partial creation. |
| `OverlayWindow::ShowTargetIndicator()` | Run start when enabled | `rendering::OverlayWindow` | Creates 2 px frame region around target, shows topmost for 180 ms. Use owned-region wrapper so failed `SetWindowRgn` deletes outer region. |
| `OverlayWindow::HideTargetIndicator()` | Tick timeout/cancel/finish | `rendering::OverlayWindow` | Hides owned indicator HWND; idempotent. |
| `OverlayWindow::InitializeComposition()` | Initialization | `rendering::OverlayRenderer::Initialize(HWND)` | Creates premultiplied BGRA flip-sequential composition swap chain (2 buffers), DComp device/target/visual, connects graph, commits. Returns typed device/other error. |
| `OverlayWindow::CreateRenderTarget()` | Initialization/resize | `rendering::OverlayRenderer::CreateRenderTarget()` | Gets swap-chain buffer and owns RTV. Failure is classified for recovery. |
| `OverlayWindow::ResizeOverlaySurface()` | Start animation | `OverlayWindow::Resize()` orchestration delegating buffer work to `OverlayRenderer::ResizeSurface()` | Unbinds/reset RTV, resizes buffers, recreates target/constants when size changes, updates screen rect and HWND bounds. Keep HWND and swap-chain responsibilities separated behind this owner. |
| `OverlayWindow::ApplyVisibleOverlayRegion()` | First-frame show | `rendering::OverlayWindow` | Creates full surface region, subtracts overlap with selected taskbar, transfers resulting region to window. Ensures overlay never paints over target taskbar. |
| `OverlayWindow::CreateRenderResources()` | Initialization | `rendering::AnimationRenderer::Initialize()` | Creates shaders, dynamic constant/vertex/index buffers, sampler, premultiplied-alpha blend state and no-cull rasterizer. Partial `ComPtr`s unwind on owner destruction. |
| `OverlayWindow::CompileShaders()` | Render-resource creation | `rendering::AnimationRenderer::CompileShaders()` | Compiles VS/PS and creates input layout matching `MeshVertex`. Returns typed failure. |
| `OverlayWindow::UploadMesh()` | Every rendered frame | `rendering::AnimationRenderer::UploadMesh()` | Validates nonempty/fixed maximum, maps/copies/unmaps vertex buffer, maps indices only when topology changed, stores index count. Any successful map must unmap before later failure. |
| `OverlayWindow::UpdateFrameConstants()` | Resize/render | `rendering::AnimationRenderer::UpdateFrameConstants()` | Maps dynamic cbuffer. Opacity: inactive 1; Squash `1-easedProgress`; other styles use raw run progress and fade clamped 0..0.65. Preserve this subtle eased/raw distinction unless behavior tests intentionally change it. |
| `OverlayWindow::Render()` | Start first frame and runtime tick | `rendering::AnimationRenderer::RenderFrame()` with `OverlayRenderer::Present()` | Applies easing, generates/uploads mesh, binds pipeline/SRV/sampler/blend, draws indexed, unbinds SRV, then presents and commits. No feature policy or clock ownership. |
| `OverlayWindow::ClearFrame()` | Initialize/hide/shutdown path | `rendering::OverlayRenderer::ClearAndPresent()` | Clears transparent, presents and commits. Reports device loss; safe no-op without RTV/swap chain. |
| `OverlayWindow::MarkDeviceLost()` | Swap-chain/DComp/map/present failures | Replace with typed rendering errors; classification remains in D3D/rendering and policy in `runtime::RendererRecovery` | Current method only sets flag if shared D3D device reports loss; other rendering failures remain ordinary operation failures. |
| `OverlayWindow::HideOverlay()` | Start failure, completion, cancel | `rendering::OverlayWindow::Hide()` delegating clear to `OverlayRenderer` | Clears frame, installs empty region, keeps owned HWND topmost/noactivate without changing bounds. Idempotent. |
| `OverlayWindow::ToOverlayRect()` | Start-state setup | `OverlayWindow`/`AnimationRenderer` boundary conversion | Converts absolute screen `RectF` into current surface-local coordinates. Pure value; virtual/surface origin may be negative. |

### Overlay/DirectComposition behavior and shader responsibility

- The overlay class is a topmost, layered, transparent, no-activate tool popup. `WM_NCHITTEST`
  returns `HTTRANSPARENT` and `WM_MOUSEACTIVATE` returns `MA_NOACTIVATE`; it must never capture user
  input.
- Each animation surface is only the clipped union of source and target, not the whole virtual
  desktop. The HWND moves/resizes to that absolute screen rect and animation vertices are converted
  to local coordinates.
- Before showing the first frame, the taskbar HWND is placed in the topmost band and its overlap is
  removed from the overlay window region. Preserve correct primary/secondary-taskbar behavior and
  do not leave a permanent opaque/topmost surface after any failure.
- First frame order is render/present/commit, set taskbar/regions/z-order/show, `DwmFlush`, then
  `IDCompositionDevice::WaitForCommitCompletion`. This ordering ensures the real window is hidden
  only after a visible overlay exists.
- The swap chain is BGRA8, flip sequential, stretch scaling, two buffers, premultiplied alpha.
  Pixel output samples captured alpha, multiplies alpha by opacity, then premultiplies RGB by the
  resulting alpha.
- The authoritative shader today is the pair of in-memory strings in `overlay_window.cpp`, both
  with entry point `Main`. `app/shaders/genie_mesh.hlsl` is project metadata only and is not loaded
  or compiled; it uses `VertexMain`/`PixelMain` and materially different alpha behavior
  (`color.a=opacity`, `rgb*=opacity`). Migration must establish one canonical shader owned by
  `AnimationRenderer` and delete/update the stale duplicate rather than silently switching visual
  behavior.
- Current fixed GPU capacities are 102 vertices and 300 indices, exactly enough for 50 long
  segments, while the public mesh generator permits 100. Target buffers should be sized from the
  validated configuration (or share a single maximum) so no legal animation setting fails upload.

## Function-level migration ledger: injected hook DLL

### `hook/hook.cpp`

| Old symbol | Current callers / behavior | Required single target | Ownership / cleanup / recovery |
| --- | --- | --- | --- |
| anonymous `IsMinimizeCommand()` | `CBTProc()` for `HCBT_MINMAX` | Keep private in `hook/hook.cpp` | Accepts Minimize, ShowMinimized, ForceMinimize and ShowMinNoActive. Pure classification. |
| anonymous `IsRestoreCommand()` | `CBTProc()` for `HCBT_MINMAX` | Keep private in `hook/hook.cpp` | Accepts Restore, ShowNormal, Show, ShowDefault, ShowNA, ShowNoActivate, ShowMaximized and Maximize. Pure classification. |
| `DllMain()` | Windows loader | Keep minimal in `hook/hook.cpp` | Always returns TRUE and performs no loader-lock work. Do not add hook install/uninstall, COM, allocation-heavy logging, or app callbacks here. |
| exported `CBTProc()` | Installed globally by `CbtHookManager` through `SetWindowsHookExW(WH_CBT)` | Keep the exact exported callback in `hook/hook.cpp` | No owned persistent state. Reads target HWND/show command, logs, checks properties, communicates with broker, returns 1 only to block a successfully delegated operation; otherwise calls next hook. |

`CBTProc()` behavior, in order:

1. `show_cmd` is `LOWORD(lParam)` and target HWND is `wParam`.
2. Only `code == HCBT_MINMAX` is considered; trace logging reads class/title and classifies command.
3. Minimize:
   - `GenieExcludedApplication` present: allow native.
   - Else `GenieAllowMinimize` present: allow native.
   - Else find class `GenieEffectOverlayWindow`, register `GenieMinimizeAttempt`, and asynchronously
     post target HWND/original lParam. Return 1 only when `PostMessageW` succeeds.
4. Restore:
   - `GenieExcludedApplication` present: allow native.
   - Else `GenieAllowRestore` present: allow native.
   - Else find the same broker class, register `GenieRestoreAttempt`, synchronously send with a
     75 ms abort-if-hung timeout. Return 1 only when the broker writes nonzero `handled`.
5. Every unhandled/failure path chains to `CallNextHookEx`, preserving native behavior.

The target architecture should provide exactly one stable top-level broker discoverable under the
legacy `GenieEffectOverlayWindow` class while the hook is installed. Today every visual run overlay
uses that class and `FindWindowW` chooses an arbitrary matching instance; the first run is intended
as the dispatcher. Moving the broker to `CbtHookManager` removes that ambiguity and the forbidden
rendering-to-feature callback. The class/message/property strings must remain compatible with the
DLL contract even if the visual overlay receives a new internal class name.

## Cross-boundary hook-manager inputs currently in `application.cpp`

These symbols are outside this report's primary file set but were inspected because their state is
one inseparable ownership path with `hook/hook.cpp`:

| Existing app symbol/state | Current behavior | Required target / ownership |
| --- | --- | --- |
| `CbtProc` function-pointer alias | `LRESULT(CALLBACK*)(int,WPARAM,LPARAM)` | Private exact ABI alias in `CbtHookManager`. |
| `EmbeddedResource` | Borrowed byte pointer/size locked from app module | Private hook-resource value; app module owns bytes for process lifetime. |
| `LoadEmbeddedResource(IDR_GENIE_HOOK)` | Finds/loads/locks RCDATA 205 | Hook resource helper inside/under `CbtHookManager`; no `FreeResource` required. |
| `ResourceFingerprint()` | 64-bit FNV-1a over DLL bytes | Deterministic private helper; preserve filename stability and add direct tests. |
| `FileMatchesResource()` | Size then byte-for-byte compare | Private cache-validation helper; local stream/vector only. |
| `HookCacheDirectory()` | Settings directory `/hooks`; fallback temp `/GenieEffect/hooks` | Manager-owned path policy. Default resolves under `%LOCALAPPDATA%\GenieEffect\hooks`; cache persists after unload. |
| `ExtractEmbeddedHookDll()` | Creates directory, names `GenieHookPost-%016x.dll`, writes `.<pid>.tmp`, flushes and atomically replaces with `MOVEFILE_WRITE_THROUGH`; removes failed temp | Transactional resource extraction inside manager. Preserve reuse of matching cache file and race fallback that accepts an already matching destination. |
| `GetExecutableDirectory()` | Dynamic-size module path and trailing slash | `process_info`/manager path helper; used for adjacent-DLL fallback. |
| `Application::InstallCbtHook()` | Prefer embedded extracted DLL, fallback adjacent `GenieHookPost.dll`; grant AppContainer ACL; `LoadLibraryW`; lookup undecorated/decorated/ordinal export; install global hook | `CbtHookManager::Start()` owns `HMODULE`, `HHOOK`, broker HWND/message IDs and callbacks transactionally. |
| `Application::UninstallCbtHook()` | Unhook then free local DLL | `CbtHookManager::Stop()`; this order is mandatory and idempotent. |
| `hook_dll_` | Local `HMODULE` owner | Manager field; must outlive `HHOOK`. |
| `cbt_hook_` | Global `HHOOK` owner | Manager field; `UnhookWindowsHookEx` before `FreeLibrary`. |

Debug normally has no embedded hook resource and falls back to the DLL beside the EXE. Release
defines `GENIE_EMBED_RELEASE_HOOK` for resource compilation and can extract resource 205. A failed
embedded lookup/extraction is not fatal if the adjacent DLL works. ACL grants are best effort; hook
load/install failure leaves the WinEvent fallback active and must not leave a half-owned module or
hook.

## Window property ledger

All names are persistence/recovery and, for the first three, cross-DLL protocol. Define them once
under `platform/windows/window_properties` (with the hook retaining a small ABI constants header
that is safe to compile into the DLL). Do not change strings.

| Exact property | Current writer/reader and meaning | Required cleanup |
| --- | --- | --- |
| `GenieAllowMinimize` | App/old overlay sets temporarily around intentional native minimize; hook bypasses interception | Remove immediately after `ShowWindow`/`ShowWindowAsync`, and on every abort/heal/shutdown path. |
| `GenieAllowRestore` | App sets temporarily around intentional native restore/maximize; hook bypasses interception | Remove immediately after show and during general property cleanup. |
| `GenieExcludedApplication` | Application sets while effect active for excluded executable; hook allows native minimize/restore | Remove when exclusion/effect changes and from every enumerated window at shutdown/startup heal. |
| `GenieIsMinimizing` | Marks a Genie-managed minimize for restore detection/recovery | Remove on success restoration, abort, disable, heal, shutdown. |
| `GenieOriginalPlacementLeft` | Encoded `LONG` normal placement left in property HANDLE | Read before clearing; remove with placement group. Encoding zero as a null property value is ambiguous and must be covered by encode/decode tests. |
| `GenieOriginalPlacementTop` | Encoded normal placement top | Same. |
| `GenieOriginalPlacementRight` | Encoded normal placement right | Same. |
| `GenieOriginalPlacementBottom` | Encoded normal placement bottom | Same. |
| `GenieMovedOffscreen` | Historical marker that a minimized/hidden window is under Genie control (current code primarily cloaks/transparency/region-hides rather than actually moving it) | Remove after placement/visibility restoration. |
| `GenieWasMaximized` | Presence means restore/maximize semantics must be preserved | Remove after restoration. |
| `GenieTransparencySaved` | Sentinel that Genie saved/changed transparency | `RestoreWindowTransparency` must run before removal. |
| `GenieOriginalExStyle` | Saved original extended style/presence sentinel | Current recovery deliberately does not replay the whole style; it only restores the layered bit to avoid resurrecting temporary topmost/unrelated bits. Remove afterward. |
| `GenieWasLayered` | Encoded prior layered-state bool | Restore layered bit/attributes, then remove. |
| `GenieOriginalAlpha` | Encoded prior layered alpha byte | Restore if previously layered, then remove. |
| `GenieOriginalFlags` | Encoded prior layered flags | Restore if previously layered, then remove. |

`window_properties` must provide one transaction/rollback path covering cloak, region, layered style,
alpha/flags, placement/maximized intent and all markers. Success, capture failure, native-minimize
timeout, reverse, cancel, device lost, target destruction, effect disable, startup heal and process
shutdown must converge on that path.

## Explicit special-member declarations

These declarations are not repeated in the ledgers above:

| Old declaration | Current caller/meaning | Required target |
| --- | --- | --- |
| `NativeAnimationBlocker::NativeAnimationBlocker() = default` | Application member construction | Same RAII class under `platform/windows`. |
| deleted `NativeAnimationBlocker` copy constructor/assignment | Prevent duplicate restoration ownership | Preserve; move may be allowed only if handle/state transfer is correct. |
| `WindowEventMonitor::WindowEventMonitor() = default` | Application member construction | Same RAII class under `platform/windows`. |
| deleted `WindowEventMonitor` copy constructor/assignment | Prevent duplicate hook/HWND ownership | Preserve; normally non-movable while callbacks can reference address. |
| deleted `D3dDevice` copy constructor/assignment | Prevent duplicate logical device owner | Preserve in `rendering/d3d_device`. |
| deleted `DesktopCapture` copy constructor/assignment | Prevent copying borrowed device plus sessions | Preserve after split for session owner. |
| `OverlayWindow::OverlayWindow() = default` | Run-slot construction | Keep only for visual HWND wrapper; runtime/render owners initialize transactionally. |
| deleted `OverlayWindow` copy constructor/assignment | Prevent duplicate HWND/COM ownership | Preserve; define safe move only if container architecture requires and callback/userdata address remains stable. |

## Environment variables and compile-time switches affecting this scope

| Name | Reader / exact behavior | Migration requirement |
| --- | --- | --- |
| `GENIE_TASKBAR_RECT` | Taskbar provider parses `left,top,right,bottom` physical coordinates | Preserve and isolate parser for deterministic tests. |
| `GENIE_TEST_DEVICE_RECOVERY` | Debug `Application::Run()` triggers one animation-renderer teardown/recreate; Debug `SettingsWindow::Render()` independently triggers one UI renderer recovery | Preserve controlled coverage of both renderer paths, but keep animation recovery under `runtime::RendererRecovery` and UI recovery under `ui/rendering`. Release remains disabled. |
| `GENIE_TRACE` | Logger gates verbose traces used throughout hook/platform/capture/overlay | Preserve via core logger; hook and app have module-local cached flag state. |
| `GENIE_LOG_SYNC` | Logger optionally flushes every debug line | Preserve via core logger; no rendering ownership. |
| `GENIE_DEBUG_LOG` | Overrides log path used by all diagnostic calls | Preserve via core logger; hook/app write sharing remains compatible. |
| `LOCALAPPDATA` / `%LOCALAPPDATA%` | Settings path indirectly determines hook cache and default log location | Preserve `%LOCALAPPDATA%\GenieEffect` compatibility. Hook cache is its `hooks` child. |
| `GENIE_EMBED_RELEASE_HOOK` | Release resource-compiler definition, not a runtime environment variable | Preserve resource 205 embedding and adjacent-DLL fallback. |

## Shader entry-point ledger

| Old shader function/source | Current use | Required target / disposition |
| --- | --- | --- |
| in-memory HLSL `FrameConstants` cbuffer | Bound at `b0` to VS and PS | Canonical cbuffer contract matching the C++ `FrameConstants` layout. |
| in-memory HLSL `VertexInput` / `PixelInput` structs | VS input and VS-to-PS payload | Canonical shader-private structs; POSITION/TEXCOORD layout must match `MeshVertex`. |
| in-memory vertex `Main()` in `overlay_window.cpp` | Runtime-compiled as `vs_5_0` | Canonical vertex entry point owned by `rendering/animation_renderer`; preserve screen-pixel to NDC transform and UV pass-through. |
| in-memory pixel `Main()` in `overlay_window.cpp` | Runtime-compiled as `ps_5_0` | Canonical pixel entry point owned by `rendering/animation_renderer`; preserve sampled alpha, opacity multiplication and premultiplied RGB. |
| external HLSL `FrameConstants`, `VertexInput`, `PixelInput` | Duplicate types in project `<None>` file | Remove with stale file or reconcile into the one canonical shader contract. |
| `VertexMain()` in `app/shaders/genie_mesh.hlsl` | No runtime/build compiler caller; project `<None>` reference only | Remove stale duplicate or make it the canonical compiled source with verified equivalent behavior. |
| `PixelMain()` in `app/shaders/genie_mesh.hlsl` | No runtime/build compiler caller; behavior differs from live string | Do not switch to it unchanged; reconcile alpha semantics first, then keep exactly one source. |

## File-scope/static-state audit

There is only one mutable C++ static in the primary file set:
`WindowEventMonitor::active_monitor_`. It is the narrowly permitted Win32 callback thunk described
above. All other file/function-scope objects are immutable compile-time data:

| Source | Immutable static/constexpr data | Required disposition |
| --- | --- | --- |
| `genie_mesh.cpp` | Four timing/shape constants | Private animation implementation constants. |
| `d3d_device.cpp` | Feature-level array 11.1/11.0 | Private device-creation constant. |
| `window_util.cpp` | Eight excluded window-class names | Private `window_state` constant. |
| `overlay_window.cpp` | Overlay/indicator class names, `GenieAllowMinimize`, mesh capacity constants, 2 px padding, embedded VS/PS strings | Class/ABI strings move to their owners; capacity to `AnimationRenderer`; one canonical shader source. |
| `hook.cpp` | Three property names, two registered-message names, overlay broker class name | Keep exact cross-module ABI constants; ideally generated/shared from a tiny Windows-only ABI header compiled into both projects. |
| function-local constants | Hook restore timeout 75 ms; Desktop first-frame timeout 120 ms; indicator 180 ms; capture refresh 16 ms in caller; UIA target 72x48; overlay/D3D descriptors | Move beside the precise owner and preserve values unless behavior tests intentionally revise them. |

Relevant mutable static state outside the primary set: `Application::CleanupAndRestoreAll()` uses a
function-local `static std::atomic<bool> cleaned_up`. That makes cleanup once-per-process rather
than once-per-Application and must become ordinary idempotent owner state during composition-root
refactoring. It also currently copies active run HWNDs into a fixed two-element array while the run
deque can grow; the new run pool cleanup must iterate an owned dynamic collection.

## Repo-wide include/caller matrix

| Header/contract | Current first-party consumers |
| --- | --- |
| `animation/easing.hpp` | `settings_store.hpp/.cpp`, `settings_window.cpp/.hpp`, `settings_ui_widgets.cpp/.hpp`, `overlay_window.hpp/.cpp`, Application via settings/overlay |
| `animation/geometry.hpp` | `genie_mesh.hpp`, `desktop_capture.hpp`, `taskbar_target_provider.hpp`, `application.cpp`, `overlay_window.cpp` |
| `animation/genie_mesh.hpp` | `overlay_window.hpp/.cpp`, `taskbar_target_provider.hpp/.cpp` |
| `platform/window_util.hpp` | Application, SettingsWindow, NativeAnimationBlocker, TaskbarTargetProvider, OverlayWindow |
| `platform/native_animation_blocker.hpp` | `application.hpp` only |
| `platform/window_event_monitor.hpp` | `application.hpp` only |
| `platform/taskbar_target_provider.hpp` | `application.hpp` only |
| `rendering/d3d_device.hpp` | Application, DesktopCapture, OverlayWindow |
| `rendering/desktop_capture.hpp` | Application and OverlayWindow |
| `rendering/overlay_window.hpp` | `application.hpp` only |
| Hook export/messages/properties | App hook loader/window transactions and old OverlayWindow message receiver; hook project includes shared logger implementation |

No test project currently consumes these contracts. The target test project must compile pure
animation directly and test deterministic platform parsers without requiring a live desktop.

## Display, monitor, and taskbar invalidation behavior

- `TaskbarTargetProvider` caches nothing; every target request rediscovers taskbar/UIA state. A
  restarted Explorer/taskbar therefore yields fresh HWNDs on the next animation.
- Overlay start keeps the selected taskbar HWND only as a local borrowed value, subtracts its
  current rectangle, and does not retain it after the call.
- The settings/tray UI separately handles registered `TaskbarCreated`; that does not re-register
  `WindowEventMonitor`'s shell-hook window. The new monitor should explicitly handle Explorer
  restart/degraded shell registration if required rather than relying on the tray path.
- On `WM_DISPLAYCHANGE`, current Application code only clears each run's cached pacing HMONITOR.
  Existing `OverlayWindow::virtual_screen_rect_` remains the value captured at initialization, and
  duplication outputs remain until access loss/reset. The split must route display change to
  `window_state`, `DesktopDuplicationSession`, and overlay surface owners so virtual bounds,
  outputs, rotation and refresh-rate selection are refreshed safely.
- Frame pacing reselects a monitor from current non-iconic extended bounds, otherwise the saved
  animation rect, then queries rational refresh rate. Missing refresh information disables the
  fixed deadline and falls back to DWM flushing/message waits.
- Spanning-window Desktop Duplication currently chooses the fully containing output or the output
  containing the rect center and clips the capture to it. `PrintWindow` can supply a whole-window
  fallback. Multi-monitor tests must pin or intentionally improve this behavior without introducing
  out-of-bounds copies.

## Device-lost, access-lost, and cleanup paths

### Local Desktop Duplication recovery

`DXGI_ERROR_ACCESS_LOST` is session-local: return typed AccessLost, release the current output
pointer, clear all output sessions, enumerate again, and retry once. It must not be conflated with
D3D device removal. WAIT_TIMEOUT is also not failure when a cached frame exists.

### Full animation renderer recovery

Current caller behavior to preserve behind `runtime::RendererRecovery`:

1. Polls all overlays, `DesktopCapture`, and `D3dDevice` after render/capture operations.
2. On first loss, increments diagnostic failures, disables native-animation blocking, and prevents
   duplicate recovery entry.
3. For every run: remember target/direction, shut down overlay GPU/HWND resources, restore the real
   window's Genie state, and if an interrupted minimize must remain minimized, issue an allowed
   native minimize.
4. Clear run fields/frame scheduling; end fallback timer resolution.
5. Destroy DesktopCapture/duplication and snapshot GPU references, then destroy D3D device.
6. Recreate device, one initial run overlay and DesktopCapture. Retry with exponential delay from
   250 ms to 4000 ms.
7. On success clear pending state and re-enable the native blocker only if policy is active.

The global CBT hook currently remains installed during graphics recovery. While visual overlays are
destroyed, the injected hook fails to find its broker and therefore permits native behavior. A
manager-owned stable broker can make this degraded state explicit and respond “not handled” while
rendering is unavailable.

Low-level failures must be returned with enough context to distinguish DeviceLost from ordinary
shader/COM/window failures. Present, resize, buffer map, RTV creation and some DComp commits
currently call the device classifier. Other creation failures simply abort initialization/start.
Recovery policy must not live in any D3D/overlay class.

### Normal shutdown and abnormal healing

Required final order:

1. Stop dispatch/new work: shutdown flag, hotkeys, WinEvent/shell monitor, CBT hook broker/hook.
2. Disable native blocker.
3. Cancel/finish every run through one transaction cleanup, restoring all real windows and removing
   every property, cloak, region and transparency mutation.
4. Destroy animation renderers, overlay renderers and overlay HWNDs.
5. Clear snapshot/cache textures and Desktop Duplication sessions.
6. Destroy D3D device.
7. Stop timer-resolution ownership and close frame timer only after waiters leave.

Startup healing and forced shutdown additionally enumerate all top-level windows, remove exclusion
markers, and restore any window with Genie state even when no in-memory snapshot survives. This is
why property names/encoding and idempotent restoration are compatibility contracts, not private
implementation details.

## Phase handoff / completeness gate

- All callable symbols in the primary file set have one row above; explicitly defaulted/deleted
  members are separately accounted for.
- Dead code is consciously identified: `RectF::CenterX`, `RectF::CenterY`,
  `CubicBezier::Ease`, taskbar anonymous `Clamp`, `GetShellTaskbarRect`, the duplicate desktop
  device-loss forwarding helper, and `DesktopCapture::ClearDeviceLost`. Remove only when no new
  target caller was introduced.
- `window_util.*` has no valid target form and must disappear after its rows move.
- `desktop_capture.*` remains only region/window capture; duplication ownership moves out.
- Old `overlay_window.*` remains only the visual HWND boundary; composition, animation D3D, runtime
  clock/state, hook broker and feature fallback move to their named owners.
- Animation stays platform-free and deterministic.
- The hook DLL name, exported callback ABI, resource ID 205, message/property/class strings,
  x64 output locations, embedded/adjacent load behavior and UIPI fallback are preserved.
- COM, GDI, HWND, HHOOK, HMODULE, HRGN, timer and DWM mutation owners all have explicit teardown or
  recovery destinations.
- No implementation, build, test, format, or runtime action is authorized until the complete
  repository-wide Phase-0 inventory gate is satisfied.
