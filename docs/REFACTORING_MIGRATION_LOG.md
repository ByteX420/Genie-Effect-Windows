# Refactoring migration log

This is the authoritative index for the source-to-owner migration required by
`REFACTORING_MASTERPLAN.md`. It is kept with the repository so the final
“Produktionsumbau vollständig” gate can prove that every old function was moved exactly once or
consciously removed.

## Phase 0 status

Phase 0 is complete. All 43 first-party files below `app/src` and `hook`, both product project
files, the solution, resource script, shader source, README, architecture document, and bundled-font
metadata were read in full. Repository-wide callers, includes, callbacks, mutable static state,
Window Properties, resource IDs, environment/registry/file names, and cleanup/recovery paths were
cross-referenced without building, testing, formatting, or starting the application.

The detailed ledgers are normative appendices:

- [Application, bootstrap, PCH, resources](refactoring/phase0_application.md): 120 explicitly
  declared/named function units, the complete `Application` state, all 15 Window Properties,
  bootstrap, hook extraction, session, runtime and shutdown paths.
- [Settings UI, theme, motion](refactoring/phase0_ui.md): every old UI function and render block,
  all callbacks and UI state, page/component owners, Win32 messages, D3D/ImGui/tray/preview
  resources and recovery.
- [Animation, Windows platform, rendering, hook](refactoring/phase0_render_platform.md): every
  callable in those modules, COM/GDI/HWND ownership, Desktop Duplication, DirectComposition,
  shader/mesh responsibilities and the stable hook ABI.
- [Settings and metadata](refactoring/phase0_settings_and_metadata.md): every settings model/parser/
  serializer/repository/exclusion function and all persistence/build/resource/documentation
  contracts.
- [Core logger](refactoring/phase0_core_logger.md): every old header-only logger function and its
  process-local handle/lock/rotation/encoding contract.

## Stable external contracts

- Product artifacts stay `GenieEffect.exe` and `GenieHookPost.dll` under
  `build/bin/x64/<Configuration>`.
- Hook ABI stays `extern "C" LRESULT CALLBACK CBTProc(int, WPARAM, LPARAM)` with loader fallbacks
  `CBTProc`, `_CBTProc@12`, and ordinal 1.
- Hook protocol strings stay `GenieEffectOverlayWindow`, `GenieMinimizeAttempt`,
  `GenieRestoreAttempt`, `GenieAllowMinimize`, `GenieAllowRestore`, and
  `GenieExcludedApplication`.
- Resource IDs 201–205 and Release resource 205 embedding remain stable. The unused
  `IDR_JETBRAINS_*` aliases are explicitly classified for removal after UI-resource migration.
- Settings stay at `%LOCALAPPDATA%\GenieEffect\settings.json`; camelCase fields, defaults,
  historical `"Gienie ..."` spellings, tolerant unknown/legacy-field parsing and atomic
  write-through replacement remain compatible.
- Environment variables stay `GENIE_TASKBAR_RECT`, `GENIE_DEBUG_LOG`, `GENIE_TRACE`,
  `GENIE_LOG_SYNC`, and the Debug-only `GENIE_TEST_DEVICE_RECOVERY`.
- Registry/mutex/session names stay `HKCU\...\Run\GenieEffect`,
  `Local\GenieEffect.Windows.SingleInstance`, and `session.state` with
  `running`/`safe`/`clean`.

## Structural corrections required by the inventory

These are confirmed source defects or incomplete ownership contracts, not behavior to duplicate:

- initialization must become transactional instead of relying on eventual destructor cleanup;
- the process-static `CleanupAndRestoreAll` guard must become instance-owned idempotent shutdown;
- the fixed `HWND animating_copies[2]` cleanup buffer must be replaced by the dynamic run owner;
- success, cancel, timeout, destroyed HWND, Device Lost and shutdown must converge on one
  transaction cleanup path;
- Window Property writes need complete rollback and unambiguous encode/decode handling;
- Safe Mode must actually read and act on the existing session marker;
- Desktop Duplication acquired frames, GDI objects, overlay regions/class brushes, hook handles and
  logger file handles need explicit RAII owners;
- the two shader implementations must become one canonical, behavior-compatible source;
- UI process-global widget maps and global motion state must become context/controller-owned;
- the stable CBT broker must no longer be an arbitrary visual overlay from a dynamic run pool.

## Conscious removals

The ledgers mark the following current dead code for removal rather than migration:

- unused application rectangle/foreground/maximize/hide helpers and identity duration wrapper;
- unused `RectF` center accessors and `CubicBezier::Ease`;
- unused taskbar clamp/shell-rect method and duplicate capture device-loss forwarder/reset;
- unused UI transparency query, theme gradient/blend duplicates, icon declarations and layout
  accessors;
- unused motion debug overlay/wrappers/overloads after the final caller audit;
- compatibility Font-ID aliases after all real resource users move.

No item in this section may be removed if a new, real target caller is introduced during migration;
the final static gate re-runs the caller audit.

## Phase progression

- [x] Phase 0 — complete inventory and exact owner mapping.
- [x] Phase 1 — conventions and leaf modules.
- [x] Phase 2 — Windows platform adapters.
- [x] Phase 3 — settings.
- [x] Phase 4 — runtime state.
- [x] Phase 5 — features.
- [x] Phase 6 — rendering.
- [x] Phase 7 — UI.
- [x] Phase 8 — composition root and entry point.
- [x] Phase 9 — project metadata and documentation.
- [x] Phase 10 — final production cleanup.
- [x] Static “Produktionsumbau vollständig” gate.
- [x] Debug/Release production builds succeed after restoring `app/assets/fonts` and silencing
      intentional `[[nodiscard]]` discards on best-effort platform helpers.
- [ ] Automated tests, integration tests, and final Computer-Use loop.

## Implementation ledger

### Phase 1 complete

- Replaced the header-only logger with `core/logger.hpp/.cpp`; environment parsing is isolated in
  `core/environment.hpp/.cpp`.
- Kept animation platform-free, moved easing implementation out of the public header, and removed
  the inventoried dead geometry/easing methods.
- Moved motion to `ui/motion`, converted its API and data names to Google C++ style, removed the
  unused debug overlay, moved token construction out of the header, and transferred ownership from
  the process-global facade to the settings-window instance.
- Moved theme code to `ui/theme`, centralized immutable theme tokens, removed unused global icon
  declarations, and eliminated every old motion/theme include path.
- `pch.h` remains only as the tiny integration header required by the vendored ImGui fork; it no
  longer forwards to the product PCH. `third_party` remains untouched.

### Phase 2 complete

- Moved startup, native-animation blocking, taskbar targeting and window-event monitoring under
  `platform/windows`.
- Added RAII `CbtHookManager`; embedded-resource extraction, fingerprinted cache, DLL ownership,
  hook ownership and partial-failure rollback no longer belong to `Application`.
- Centralized the stable hook/app Window Property names and moved placement, transparency and
  property cleanup into `window_properties`. The app and hook now consume the same ABI constants.
- Physically split the former `window_util` implementation into window state, Window Properties,
  process information, display information, taskbar location and AppContainer ACL adapters.
- Added owning global-hotkey and single-instance adapters, plus focused fullscreen and power-status
  queries. Product-version, elevation and foreground operations no longer live in `Application`.
- Connected the persisted session marker to Safe Mode; an unclean prior `running`/`safe` session
  now starts without hook and animation renderer instead of ignoring the marker.

### Phase 3 complete

- Removed `settings_store.*` and separated `app_settings`, `hotkey_binding`, and
  `exclusion_rules`.
- Added separate `SettingsSerializer`, `SettingsRepository`, `SettingsValidator`, and transactional
  `SettingsService`. Every application mutation now works on a copy and publishes only after an
  atomic repository save; drag previews explicitly use the service's non-persistent preview path.

### Phase 4 complete

- Extracted `runtime::RunState`, added an explicit allowed-transition table, and made the central
  transition function reject invalid transitions while tracing every accepted change.
- Extracted `CachedSnapshot`, `AnimationRun`, `AnimationRunPool`, `SnapshotCache`,
  `FrameScheduler`, and `RendererRecovery`. Snapshot pruning validates both HWND and process ID;
  timer-resolution ownership is contained in the scheduler.
- Normal completion, cancellation/watchdog timeout, device loss, and shutdown now converge on the
  same run-cleanup path. Run detachment happens before window restoration to make synchronous
  WinEvent re-entry safe.

### Phase 5 complete

- Extracted `EffectPolicy`; enabled state, Safe Mode, timed/restart pause, fullscreen suppression,
  battery-saver suppression, and executable exclusions no longer have independent truth in
  `Application`.
- Extracted `WindowRecoveryService`; window healing and restoration re-entry state now have one
  owner.
- Extracted `PauseController`, `DiagnosticsService`, and `AnimationConfiguration`. Diagnostics
  collection/actions and adaptive animation configuration no longer live in the UI or application
  loop.
- Added explicit `MinimizeRequest`/`RestoreRequest` transactions with hand-off, completion,
  cancellation, and shutdown rollback. Feature failures now converge on the Phase-4 cleanup path.
- Added `EffectController` as owner of the event monitor and connector for policy, pause, minimize,
  and restore.
- Physically moved the minimize and restore algorithms, pre-minimize capture, restore-placement
  transaction, native-minimize completion, surprise-restore handling, and exclusion transitions
  out of `Application`. The composition root now supplies only run allocation, state-transition,
  frame-pacing, and cleanup ports.

### Phase 6 complete

- Extracted `DesktopDuplicationSession`; DXGI output enumeration, duplication lifetime, cached
  desktop frames, access-lost reset, and duplication-specific device-loss state no longer belong
  to region/window capture.
- Extracted `AnimationRenderer`; captured-texture lifetime, timeline state, easing, reversal,
  completion, opacity, and mesh generation no longer belong to the overlay HWND.
- Extracted `OverlayRenderer`; shader compilation, input layout, mesh buffers, frame constants,
  sampler/blend/rasterizer state, and draw submission now have one D3D owner. `OverlayWindow`
  retains HWND, swapchain, DirectComposition, visible-region, and target-indicator behavior.
- Duplication, draw-pipeline, swapchain, and DirectComposition device-loss signals remain visible
  through the shared renderer-recovery and Phase-4 cleanup path.

### Phase 7 started

- Added the single `ui::SettingsActions` boundary and removed the entire `SettingsWindow`
  callback-type/member/initialization list. The window now receives one non-owning action
  interface; `Application` implements that contract explicitly.
- Added `ui::SettingsViewModel` and transferred settings, pause, hotkey-registration, exclusion,
  and diagnostics display state out of the Win32/ImGui host. `SettingsWindow::UpdateState` now
  publishes through the view model instead of maintaining a second set of domain members.
- Added `ui::SettingsController` as the single owner of the action boundary and view model;
  `SettingsWindow` no longer owns either settings-domain state or an application callback pointer.
- Added `ui::TrayIcon`; Explorer restart recovery, retry timing, tooltip state, and tray-menu
  command mapping no longer live in `SettingsWindow`.
- Added `ui::AnimationPreview`; preview-window lifetime, painting, dragging, and phase timing are
  encapsulated behind a small UI component.
- Added `ui::rendering::ImguiRenderer`; ImGui context/backend lifetime, D3D11 swap-chain resources,
  resize handling, DPI font rebuilding, presentation, and device-loss recovery are delegated by
  the Win32 host.
- Added the dedicated `ui::ApplicationListProvider`; active-window enumeration and executable-rule
  normalization no longer live in the settings window.
- Began the required per-page split with `ui::pages::GeneralPage` and
  `ui::pages::WindowsIntegrationPage`; both page implementations now own their rendering and
  action wiring outside the host translation unit.
- Added `ui::pages::HotkeysPage` and `ui::pages::DiagnosticsPage`. Shared hotkey formatting now
  lives in `ui/hotkey_presenter`, while diagnostics refresh and recovery-tool dispatch live with
  the diagnostics page.
- Added `ui::pages::ApplicationsPage` and `ui::pages::AboutPage`. Application filtering and
  exclusion mutations are now page-owned; product/version and the complete Inter license modal
  are likewise outside the host.
- Added `ui::pages::AnimationPage`, retaining preview/reset actions, timing presets, linked live
  duration updates, commit-on-release behavior, custom easing graphs, quality, strength, fade,
  and target-indicator settings. Every settings page now has its own source/header pair.
- Removed `app/settings_ui_widgets.*`. Reusable rendering is now split into
  `ui/components/controls.*`, `combo.*`, and `easing_editor.*`; shared component math is confined
  to a private helper header.
- Moved `PageLayout` out of the theme module into `ui/components/page_layout.*`. Theme now owns
  style and shell drawing only, while page flow/layout is an explicit reusable component.
- Moved the settings host to `ui/settings_window.*` and the non-Win32 shell/navigation rendering to
  `ui/settings_shell.*`; the old `app/settings_window.*` files no longer exist.
- Moved existing-instance activation into `SingleInstanceGuard`, and replaced `main.cpp`'s raw
  mutex, COM, DPI, and console-handler ownership with `SingleInstanceGuard` and `ProcessRuntime`.
- Added `app::MessageLoop`; message pumping and wait policy now delegate update, settings rendering,
  runtime ticking, display-change handling, and animation waits through explicit callbacks.
- Moved window/rectangle diagnostic formatting and trace emission out of `Application` into
  `platform/windows/window_diagnostics.*`, eliminating the former anonymous helper namespace.
- Restored embedded-text loading as the focused `core::LoadEmbeddedText` utility; the prior
  settings-window-local resource helper is no longer required.

The masterplan's build/test/application-start prohibition remains active. No build, test,
shader compiler, or executable has been run.

### Phase 8 complete

- `app/application.cpp` is now a small composition/lifetime facade. Initialization and runtime
  behavior live in `ApplicationRuntime`; run/state operations and per-frame effect coordination
  are split between focused runtime translation units.
- `MessageLoop` owns only message dispatch and wait selection. Product updates, display changes,
  settings rendering, runtime ticks, and animation waits are supplied as callbacks.
- Settings mutation validation/persistence moved to `features::SettingsMutationService`; global
  hotkey registration and rollback moved to `features::HotkeyController`.
- Raw process mutex, existing-instance activation, DPI awareness, COM lifetime, and console-control
  routing no longer live in `main.cpp`.
- The old anonymous window-description block moved to
  `platform/windows/window_diagnostics.*`.

### Phase 9 started

- Regenerated `app/GenieEffect.vcxproj` from the real production source/header set while retaining
  vendor PCH exceptions, hook build ordering, and Release resource embedding.
- Created `app/GenieEffect.vcxproj.filters` with exact project-entry parity and folder-aligned
  filters.
- Rewrote `docs/architecture.md` for the new dependency direction, ownership, state machine,
  minimize/restore transactions, renderer recovery, and Windows integration boundaries.
- Updated the README project tree and contributor layer table.

### Phases 9 and 10 complete

- Rechecked project and filter metadata against the filesystem: all 82 first-party production
  translation units and all 87 first-party headers occur exactly once in both metadata views.
- Added focused capture geometry and window-mask owners, leaving desktop duplication, capture,
  image masking, and device-loss handling as distinct responsibilities.
- Moved `MotionContext` to `ui/motion` and removed the remaining `app::settings_ui` namespace
  leakage. Theme, motion, page layout, controls, and pages now follow the documented UI boundary.
- Split Win32 message routing into `ui/settings_window_proc.cpp`; the settings host no longer
  combines lifecycle implementation and the complete message dispatcher in one translation unit.
- Removed obsolete resource aliases and reran old-path, TODO/FIXME/stub, duplicate-owner, and
  third-party-change scans. No production shim, placeholder, stale include, or `third_party`
  modification remains.
- Applied the repository formatter to all 169 first-party C++ source/header files under
  `app/src` and `hook`.

### Static production gate complete

- The Phase-0 ledgers account for every original function and the implementation ledger records
  each target owner or conscious removal exactly once.
- `Application` is a small lifetime facade; `main.cpp` contains only bootstrap and entry flow.
- The old application, window utility, settings store, settings UI, theme/widget, and rendering
  monolith paths are gone, with no source, project, filter, README, or architecture references.
- Settings, features, runtime state, UI concerns, D3D animation, and ImGui rendering have separate
  owners matching the masterplan dependency direction.
- Project metadata has exact filesystem parity, documentation describes the production tree, and
  the source-only scans found no remaining structural work. The build/test gate is now open.
