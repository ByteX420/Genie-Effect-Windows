# Verbindliches Phase-0-Inventar: Application, Bootstrap, PCH und Ressourcen

Stand der Bestandsaufnahme: Branch `dev`, Commit `bda74a9`, 2026-07-17. Dieses Dokument ist
reine Phase-0-Analyse. Es wurden keine Produktionsdateien verändert, formatiert, gebaut oder
ausgeführt. Der gemeinsam genutzte Arbeitsbaum war während der Analyse bereits durch
`docs/.refactor_inventory/` untracked/dirty; diese parallelen Änderungen wurden nicht angefasst.
Insbesondere blieben `app/src/app/settings_window.hpp` und `docs/REFACTORING_MASTERPLAN.md`
unverändert.

## Umfang und Vollständigkeitsregel

Vollständig gelesen wurden:

- `app/src/main.cpp`
- `app/src/pch.cpp`, `app/src/pch.hpp`, `app/src/pch.h`
- `app/src/app/application.cpp`, `app/src/app/application.hpp`
- `app/src/app/resource.hpp`
- `app/src/app/startup_manager.cpp`, `app/src/app/startup_manager.hpp`

Repo-weit read-only geprüft wurden die direkten Includes und Aufrufer, die Projekt-/Resource-
Einträge, die drei Property-Konsumenten im Hook/Overlay sowie die Verträge der unmittelbar
besessenen Plattform-, Rendering-, Settings- und UI-Typen. Das Funktionsprotokoll weiter unten
enthält **120 benannte bzw. explizit deklarierte Funktionseinheiten genau einmal**:

- 2 in `main.cpp`
- 2 in `startup_manager.cpp`
- 36 freie Hilfsfunktionen im anonymen Namespace von `application.cpp`
- 2 Methoden des lokalen `TopmostRestorer`
- 78 explizite `Application`-Member einschließlich Default-/gelöschter Special Members

Captureless Callback-Lambdas werden zusätzlich in einem eigenen Callback-Abschnitt geführt; sie
sind nicht ein zweites Mal als benannte Funktionen gezählt. Compiler-generierte Special Members
von Aggregaten ohne explizite Deklaration sind keine alten, zu migrierenden Funktionen.

## Datei- und Include-Inventar

| Quelle | Heutige Rolle und aktuelle Nutzer | Verbindliches Ziel |
| --- | --- | --- |
| `app/src/main.cpp` | Entry Point plus DPI, Prozess-Mutex, COM-STA, Console-Control-Thunk und globaler `Application*`. In `GenieEffect.vcxproj` als Compile Unit. | Nur `wmain`; Bootstrap nach `app/process_bootstrap.*`, Mutex nach `platform/windows/single_instance_guard.*`, Console-Control eng in `ProcessBootstrap`. |
| `app/src/pch.cpp` | Einzige Zeile `#include "pch.hpp"`; im Projekt mit `PrecompiledHeader=Create`. | Als reine PCH-Erzeugungs-Unit behalten. |
| `app/src/pch.hpp` | Direkte First-Party-Includes: `main.cpp`, `pch.cpp`, `animation/genie_mesh.cpp`, `app/application.cpp`, `settings_store.cpp`, `settings_ui_theme.cpp`, `settings_ui_widgets.cpp`, `settings_window.cpp`, `startup_manager.cpp`, alle vier Plattform- und drei Rendering-`.cpp` sowie indirekt `pch.h`. Enthält STL, Win32, COM, D3D/DXGI/DComp/DWM, WRL sowie `imconfig.h`; definiert `IMGUI_ENABLE_FREETYPE`. | Root-PCH behalten, aber auf häufige stabile STL-/Windows-Header begrenzen; keine Projekt-Header. ImGui/FreeType- und fachlich seltene Grafikdetails an explizite Konsumenten verlagern, soweit der unveränderte Vendor-Build es zulässt. |
| `app/src/pch.h` | Kompatibilitätsheader: inkludiert `pch.hpp`, definiert `IMGUI_DEFINE_MATH_OPERATORS`, inkludiert `imgui_internal.h`. Direkt genutzt von `src/menu/motion/motion.cpp` und den vorhandenen ImGui-Fork-Dateien (`imgui*.cpp`, `imgui_extra.h`, Win32-/DX11-Backends), deren Projekt-PCH überwiegend deaktiviert ist. | Kein allgemeiner Projekt-PCH. UI-Motion erhält explizite ImGui-Includes. Da `third_party` unverändert bleiben muss und dessen aktuelle Quellen `pch.h` direkt verlangen, ist dieser minimale Vendor-Kompatibilitätsvertrag bewusst zu erhalten oder über Projektmetadaten ohne Vendor-Edit gleichwertig bereitzustellen; kein fachlicher Code hinein. |
| `app/src/app/application.hpp` | Wird direkt nur von `main.cpp` und `application.cpp` inkludiert. Zieht heute Settings UI/Store, Plattform und Rendering transitiv in `main.cpp`; `main.cpp` nutzt dadurch sogar `SettingsWindow::ActivateExistingInstance()` ohne eigenen Include. | Kleiner Vertrag der Kompositionswurzel (`Initialize`, `Run`, `RequestShutdown`, idempotentes privates `Shutdown`), Forward Declarations und besitzende Komponenten; kein Run-/D3D-/UI-/Settings-Innenzustand. |
| `app/src/app/application.cpp` | Einziger Implementierer von `Application`; inkludiert zusätzlich Resource, Startup, Logger und `window_util`. Rund 3.120 Zeilen mit Bootstrap-Resten, Runtime, Policy, UI-Actions, Hook, Minimize/Restore und Recovery. | Kleine Kompositionswurzel. Der anonyme Block wird vollständig auf die unten genannten präzisen Komponenten verteilt. |
| `app/src/app/resource.hpp` | Von `GenieEffect.rc`, `application.cpp` und `settings_window.cpp` inkludiert. Definiert fünf numerisch stabile RCDATA-IDs und drei derzeit unbenutzte Back-compat-Aliase. | Kleiner gemeinsamer Resource-ID-Vertrag bleibt für RC und C++ erreichbar. Font-Loading nach `ui/rendering`; Hook-ID/Extraktion nach `CbtHookManager` bzw. präzisem Resource-Loader. Alte JetBrains-Aliase nach vollständiger Aufruferumstellung bewusst entfernen, nicht als dauerhaften Shim behalten. |
| `app/src/app/startup_manager.*` | Header wird nur von `application.cpp` inkludiert; freie Funktion wird beim Start-Reparieren und bei UI-Änderung aufgerufen. | Nach `platform/windows/startup_manager.*`, in `genie::platform::windows`; eindeutiger Registry-Handle-Besitz. |

Projektmetadaten: `GenieEffect.vcxproj` verwendet global `pch.hpp`; `pch.cpp` erzeugt die PCH.
Release ist Windows-Subsystem mit `wmainCRTStartup`, definiert beim Resource-Compile
`GENIE_EMBED_RELEASE_HOOK` und bindet den Release-Build von `GenieHookPost.dll` als RCDATA ein.
Debug ist Console-Subsystem und hat diese Hook-Resource nicht.

## Eigene Typen und Zustandsbesitz

### Typen aus den untersuchten Dateien

| Typ | Quelle | Heutiger Inhalt/Besitz | Ziel |
| --- | --- | --- | --- |
| `Application` | `application.hpp:23` | Nicht kopierbare Sammelklasse. Besitzt D3D/Capture per `unique_ptr`, `deque<AnimationRun>`, Plattformadapter, UI-Fenster, Hook-/Timer-Handles, Snapshot-Maps und sämtliche Policy-/Sessionzustände. `HINSTANCE`, `HWND`, `HMONITOR` sind geliehene Prozess-/OS-Handles. | Nur Kompositionswurzel und Lebensdauer in `app/application.*`; Komponenten in Abhängigkeitsreihenfolge konstruieren, transaktional initialisieren, umgekehrt idempotent herunterfahren. |
| `Application::RunState` | `application.hpp:38` | `kIdle`, `kCapturing`, `kWaitingForNativeMinimize`, `kAnimating`, `kRestoring`, `kAborting`, `kCleaningUp`. Heute ist jede Transition erlaubt; `SetRunState` validiert nicht. | `runtime/run_state.hpp`; explizit erlaubte Transitionen, ein gemeinsamer Cleanup-Pfad. |
| `Application::CachedSnapshot` | `application.hpp:48` | Geliehenes `HWND`; `CapturedTexture` besitzt D3D-Textur/SRV via `ComPtr`; Bounds/Taskbar/Placement/PID/Zeitwert. Wird kopiert, wodurch COM-Referenzen geteilt werden. | Präziser Snapshot-Werttyp plus Besitz in `runtime/snapshot_cache.*`. |
| `Application::AnimationRun` | `application.hpp:60` | Besitzt `OverlayWindow` (und dessen HWND/DComp/D3D-Ressourcen); Ziel-/Monitor-HWND/HMONITOR geliehen; Pacing-, Capture- und Run-State. | `runtime/animation_run.*`; Container und Slot-Erzeugung nach `animation_run_pool.*`. |
| `Translation` | `application.cpp:45` | Lokales POD für VERSIONINFO-Sprache/Codepage; Pointer darauf ist in den lokalen Version-Buffer geliehen. | Implementierungsdetail von `platform/windows/process_info.*`. |
| `EmbeddedResource` | `application.cpp:140` | Nicht besitzende Sicht auf vom EXE-Modul gehaltene RCDATA-Bytes. `LoadResource`/`LockResource` benötigen kein `FreeResource`. | Interner Werttyp des Hook-Resource-Loaders in/bei `cbt_hook_manager.*`. |
| `TopmostRestorer` | `application.cpp:2208` | Lokaler RAII-Typ mit geliehenem Ziel-`HWND`; setzt ein nur temporär angehobenes Fenster genau einmal auf `HWND_NOTOPMOST` zurück. | Teil von `features/minimize/minimize_transaction.*` bzw. schmale `window_state`-Operation. |
| `CbtProc` | `application.cpp:114` | Funktionspointer-Typ auf die fremde DLL-Callback-ABI `LRESULT CALLBACK(int,WPARAM,LPARAM)`. | Privater ABI-Typ von `platform/windows/cbt_hook_manager.*`. |

### Heutige `Application`-Member und zukünftiger Besitzer

| Zustand | Ownership heute | Zielbesitzer / Cleanup-Vertrag |
| --- | --- | --- |
| `d3d_device_` | `unique_ptr`, besitzt COM-Geräte indirekt. | `rendering::D3dDevice`; vom Rendering-/Recovery-Verbund besessen, vor abhängigen Capture/Overlay-Ressourcen zerstören. |
| `desktop_capture_` | `unique_ptr`; hält geliehenen `D3dDevice*`, besitzt Duplication/Textures via `ComPtr`. | `rendering::DesktopCapture` plus `DesktopDuplicationSession`; vor Device zerstören/resetten. |
| `runs_` | `deque`; jedes Element besitzt ein Overlay. | `runtime::AnimationRunPool`; jeder Run führt Erfolg, Cancel, Timeout, Device Lost und Shutdown in denselben Cleanup. |
| `native_animation_blocker_` | Wertobjekt/RAII, geliehene HWNDs und eigene Menge blockierter Fenster. | `platform/windows/native_animation_blocker`; `Disable()` vor Hook/Window-Ende und im Destruktor. |
| `window_event_monitor_` | Wertobjekt/RAII; besitzt WinEvent-Hooks und Message-HWND, hält Callbacks. | `platform/windows/window_event_monitor`; `Stop()` vor Callback-Ziele sterben. |
| `taskbar_target_provider_` | Zustandsloses Wertobjekt. | `platform/windows/taskbar_target_provider`. |
| `instance_` | Geliehenes Prozess-`HINSTANCE`. | `Application`/UI-Konstruktion; nicht freigeben. |
| `main_thread_id_` | Wert, keine Resource. | `MessageLoop`/`ProcessBootstrap` für thread-sicheres Wakeup. |
| `hook_dll_`, `cbt_hook_` | `Application` besitzt `HMODULE` und `HHOOK`. | `CbtHookManager`; immer erst `UnhookWindowsHookEx`, dann `FreeLibrary`; idempotent. |
| `pre_minimize_snapshots_`, `restore_snapshots_`, `last_snapshot_refresh_ms_` | Maps besitzen `CapturedTexture`-COM-Refs; HWND-Keys geliehen. | `SnapshotCache`; PID-Abgleich gegen wiederverwendete HWNDs, Limit/Pruning und deterministisches Freigeben. |
| Timer-/Pacing-Felder | `animation_frame_timer_` ist eigenes `HANDLE`; `timeBeginPeriod` ist prozessweiter auszugleichender Zustand. | `runtime::FrameScheduler`; `timeEndPeriod` genau einmal, Timer vor Close wecken, Handle per RAII schließen. |
| Renderer-Recovery-Felder | Backoff und Statuswerte. | `runtime::RendererRecovery`; zentraler Device-Lost-Cleanup und begrenzter Backoff 250–4000 ms. |
| `in_restore_window_state_` | Reentrancy-Bool, derzeit nicht RAII. | `RestoreTransaction`/`WindowRecoveryService`; scope-gebundener Guard. |
| `last_foreground_window_` | Geliehenes HWND ohne Lebenszeitgarantie, vor Nutzung mit `IsWindow` geprüft. | `EffectController` oder Diagnostics-Kontext; nie besitzen, PID/Validität prüfen. |
| Enabled-/Runtime-/Pause-/Fullscreen-/Power-Felder | Fachliche Policy und Poll-Timestamps. | `SettingsService`, `PauseController`, `FullscreenDetector`, `PowerStatusMonitor`, zusammengeführt durch `EffectPolicy`/`EffectController`. |
| Performance-Metriken | Missed frames, device failures, capture duration. | `FrameScheduler`, `RendererRecovery` und typisierte Animation-Qualitätskonfiguration; nicht in `Application`. |
| Safe-Mode-/Session-/Repair-Felder | Dateizustand und Diagnostics-Text. | `SessionStateStore`, `WindowRecoveryService`, `DiagnosticsService`; `Application` orchestriert nur. |
| Dauer-/Settings-Felder | Zweite fachliche Wahrheit neben `settings_`. | Ein `SettingsService`; Laufzeit erhält validierten Snapshot/typisierte Konfiguration. |
| `shutting_down_` | Atomischer, threadübergreifender Stop-Flag. | `Application`/`MessageLoop`; Request ist thread-safe, eigentlicher Shutdown auf Owner-Thread. |
| `settings_window_` | Wertobjekt besitzt HWND, UI-D3D/ImGui/Tray/Preview und Callbacks. | `ui::SettingsController` plus getrennte UI-Besitzer; `Application` hält nur oberste Komponente. |

## Statische Zustände und Konstanten

### Veränderlicher statischer Zustand

| Zustand | Verwendung | Migration |
| --- | --- | --- |
| `static Application* g_application` (`main.cpp:9`) | Geliehener Pointer auf das Stackobjekt in `wmain`, nur für `ConsoleHandler`; wird vor Handler-Registrierung gesetzt und vor Deregistrierung/Destruktor genullt. Kein Synchronisationsschutz außer der faktischen Sequenz. | Globalen Systempointer entfernen. Enger Console-Control-Thunk in `ProcessBootstrap` darf gekapselten Callbackzustand halten, aber keinen Service-Locator. |
| `static std::atomic<bool> cleaned_up` in `CleanupAndRestoreAll` (`application.cpp:3011`) | Prozessweit, nicht pro `Application`; verhindert jeden späteren Cleanup einer zweiten Instanz im selben Prozess. | Instanzmember/idempotenter `Application::Shutdown()` plus idempotente Owner-Shutdowns; kein funktionsstatischer Cross-Instance-Zustand. |

Der angrenzende `WindowEventMonitor` besitzt zusätzlich außerhalb dieses Dateiscopes
`static WindowEventMonitor* active_monitor_` als Win32-Callback-Thunk. Die neue Plattformkomponente
muss diesen Zustand weiterhin eng kapseln und vor `Unhook`/Fensterzerstörung nullen; er darf nicht
zum allgemeinen Zugriff auf `Application` werden.

### Vertraglich relevante Konstanten

| Konstante | Exakter Wert / Semantik | Ziel |
| --- | --- | --- |
| `kSingleInstanceMutexName` | `Local\GenieEffect.Windows.SingleInstance` | `SingleInstanceGuard`; Name unverändert, damit alte/neue Instanz sich gegenseitig erkennen. |
| `kHookDllName` | `GenieHookPost.dll` | `CbtHookManager`; Laufzeitvertrag unverändert. |
| `kCbtProcName` / `kDecoratedCbtProcName` | `CBTProc`, `_CBTProc@12`; zusätzlich Ordinal 1 | `CbtHookManager`; ABI-Fallbacks beibehalten. |
| 15 `k...Property`-Strings | Siehe Property-Tabelle unten. | `window_properties`; die drei Hook-geteilten Stringwerte sind ABI. |
| `kMaxPreMinimizeSnapshots` | 4 | `SnapshotCache`. Restore-Snapshots sind heute nicht entsprechend begrenzt. |
| Renderer-Backoff | 250 ms initial, 4000 ms maximal | `RendererRecovery`. |
| `kHotkeyBaseId` | 4100 | `GlobalHotkeyManager`; IDs 4100 bis `4100 + kCount - 1`. |
| `kSupportedHotkeyModifiers` | ALT \| CONTROL \| SHIFT \| WIN | `settings::HotkeyBinding`-Validierung und `GlobalHotkeyManager`. |
| Live-Capture-Refresh | lokales `constexpr 16 ms` | `FrameScheduler`/Capture-Refresh-Policy. |
| Fullscreen-Toleranz | lokales `constexpr 2 px` | `FullscreenDetector`. |
| gültige Easing-Namen | Linear, Ease In, Ease Out, Ease In Out, Cubic, Back, Elastic, Custom | `SettingsValidator`; Schreibweisen bleiben dateikompatibel. |
| Startup Registry | `HKCU\Software\Microsoft\Windows\CurrentVersion\Run`, Wert `GenieEffect` | `platform/windows/startup_manager`. |

## Window-Property-Vertrag

Alle Ziel-`HWND` sind geliehen. Property-Werte werden als integerkodierte `HANDLE` gespeichert und
besitzen keine Kernel-Resource. Die Strings `GenieAllowMinimize`, `GenieAllowRestore` und
`GenieExcludedApplication` werden auch in `hook/hook.cpp` ausgewertet; `GenieAllowMinimize` wird
zusätzlich in `rendering/overlay_window.cpp` gesetzt. Ihre Schreibweise ist damit
prozess-/DLL-übergreifende ABI und darf nicht divergieren.

| Property | Writer / Leser heute | Bedeutung, Cleanup und Sonderfall |
| --- | --- | --- |
| `GenieAllowMinimize` | Application und Overlay setzen transient; Hook und `OnMinimizeStart` lesen. | Reentrancy-/Bypass-Token, damit der von Genie selbst ausgelöste Native-Minimize nicht erneut blockiert wird. Sofort nach `ShowWindow*` entfernen; globaler Cleanup entfernt es. |
| `GenieAllowRestore` | `RestoreWindowFromGenieState` setzt transient; Hook liest. | Analoges Restore-Bypass-Token; nach `ShowWindow` entfernen. |
| `GenieExcludedApplication` | `ApplyExclusionTransitionOverrides`/`OnWindowSeen` setzen oder entfernen; Hook liest. | Lässt Minimize/Restore für ausgeschlossene Prozesse nativ passieren. Bei Settings-/Policy-Wechsel aktualisieren; Heal/Shutdown entfernt es systemweit. |
| `GenieIsMinimizing` | `OnMinimizeStart` setzt; Minimize-/Restore-Erkennung liest. | Markiert eine Genie-Minimize-Transaktion, auch wenn Snapshot/Eventfolge unvollständig ist. Recovery entfernt es. |
| `GenieOriginalPlacementLeft` | Placement-Helper schreibt; Restore/Heal liest bzw. erkennt. | Linke Kante der Normalposition als `INT_PTR`. Wert 0 wird zu `nullptr`; deshalb darf Existenz allein nicht als vollständiger Rect-Vertrag gelten. |
| `GenieOriginalPlacementTop` | wie Left | Obere Kante; gemeinsam atomar als logischer Vierer-Satz behandeln. |
| `GenieOriginalPlacementRight` | wie Left | Rechte Kante. |
| `GenieOriginalPlacementBottom` | wie Left | Untere Kante. |
| `GenieMovedOffscreen` | Minimize/Placement-Preserve setzt; Restore liest. | Historischer Marker für versteckten/minimierten Genie-Zustand; aktueller Code verschiebt nicht zwingend Koordinaten, sondern cloakt/transparenziert. Recovery entfernt. |
| `GenieWasMaximized` | Placement-Helper setzt nur bei `true`; Restore liest. | Bestimmt `SW_SHOWMAXIMIZED` statt `SW_RESTORE`; bei `false` entfernen. |
| `GenieTransparencySaved` | `MakeWindowTransparent` setzt; Restore/Heal liest. | Idempotenzmarker für die Transparenzmutation. Muss erst nach erfolgreichem Speichern gesetzt bleiben und im Restore entfernt werden. |
| `GenieOriginalExStyle` | Transparenz-Helper speichert den damaligen ExStyle, sofern ungleich 0. | Wird heute bewusst nur als Recovery-Marker verwendet; der komplette Style wird **nicht** zurückgespielt, damit temporäres `WS_EX_TOPMOST`/fremde Style-Änderungen nicht resurrected/überschrieben werden. |
| `GenieWasLayered` | Transparenz-Helper schreibt 1/0; Restore liest. | Nur das von Genie veränderte `WS_EX_LAYERED`-Bit wird restauriert. Der 0-Wert ist als nuller HANDLE nicht als vorhandene Property unterscheidbar; Verhalten beim Migrieren exakt modellieren, nicht über Property-Existenz für `false`. |
| `GenieOriginalAlpha` | Transparenz-Helper schreibt ursprüngliches Byte; Restore liest. | Layered-Alpha, danach Property entfernen. Alpha 0 wird ebenfalls als nuller HANDLE gelesen. |
| `GenieOriginalFlags` | Transparenz-Helper schreibt ursprüngliche `GetLayeredWindowAttributes`-Flags. | Nur relevant, wenn das Fenster vorher layered war; danach entfernen. Flagwert 0 ist nullkodiert. |

Zusätzlicher, nicht als Property gespeicherter Zustand:

- `DWMWA_CLOAKED` wird über `SetWindowCloaked` gesetzt/gelöscht.
- Nach abgeschlossener Minimize-Animation erhält das Ziel eine leere, an Windows übertragene
  `HRGN`; `SetOwnedWindowRegion` löscht sie bei Übergabefehler, bei Erfolg besitzt Windows sie.
  Recovery ruft `SetWindowRgn(..., nullptr, ...)`.
- DWM-Transitions werden über `NativeAnimationBlocker`/`SetDwmTransitionsDisabled` verwaltet.
- `ClearGenieWindowProperties` entfernt Placement/Move/Maximize/Minimize/Allow-Marker, aber nicht
  die fünf Transparenz-Marker; deshalb muss vorher/zusätzlich `RestoreWindowTransparency` laufen.

## Resource-IDs und Hook-Resource

| ID | Zahl | Aktuelle Resource / Nutzer | Ziel und Erhaltung |
| --- | ---: | --- | --- |
| `IDR_UI_FONT_REGULAR` | 201 | `Inter-Regular.ttf`; `SettingsWindow::RebuildFonts`. | UI-Resource-Loader/`ui/rendering::ImGuiRenderer`; Zahl beibehalten. |
| `IDR_UI_FONT_SEMIBOLD` | 202 | `Inter-SemiBold.ttf`; UI-Fontaufbau. | Wie oben; Zahl beibehalten. |
| `IDR_UI_FONT_BOLD` | 203 | `Inter-Bold.ttf`; UI-Fontaufbau. | Wie oben; Zahl beibehalten. |
| `IDR_UI_FONT_LICENSE` | 204 | `OFL.txt`; About-Seite lädt Text. | UI-About/Resource-Loader; Zahl beibehalten. |
| `IDR_GENIE_HOOK` | 205 | Nur Release: `build/bin/x64/Release/GenieHookPost.dll`; `ExtractEmbeddedHookDll`. | `CbtHookManager`/präziser Embedded-Resource-Loader; Einbettungs- und Fallback-Vertrag beibehalten. |
| `IDR_JETBRAINS_MONO_REGULAR` | Alias 201 | Repo-weit kein aktueller Nutzer. | Bewusst als unbenutzten Back-compat-Shim entfernen, sobald Resource/UI-Migration abgeschlossen ist. |
| `IDR_JETBRAINS_MONO_BOLD` | Alias 202 | Repo-weit kein aktueller Nutzer. | Wie oben. |
| `IDR_JETBRAINS_MONO_LICENSE` | Alias 204 | Repo-weit kein aktueller Nutzer. | Wie oben. |

Hook-Extraktionsvertrag:

1. Release lädt RCDATA 205 als geliehene Modulbytes; Debug hat die Resource regulär nicht.
2. Cache-Verzeichnis ist `<Settings-Verzeichnis>\hooks`; wenn `SettingsFilePath()` nicht
   verfügbar ist, `%TEMP%\GenieEffect\hooks`.
3. Dateiname ist `GenieHookPost-<16-stelliger FNV-1a-64-Hash>.dll`.
4. Bestehende Datei wird über Größe und vollständigen Bytevergleich validiert.
5. Neu geschrieben wird `<ziel>.<PID>.tmp`, danach `MoveFileExW` mit
   `REPLACE_EXISTING|WRITE_THROUGH`; bei Race/Move-Fehler wird das Ziel erneut bytegenau geprüft.
   Temporärdateien werden auf bekannten Fehlerpfaden best effort entfernt; versionierte
   Cache-Dateien bleiben bewusst liegen.
6. Wenn Resource/Extraktion fehlt, wird `GenieHookPost.dll` neben der EXE versucht.
7. Vor `LoadLibraryW` erhalten Verzeichnis und Datei AppContainer-Berechtigungen.
8. Exportauflösung: `CBTProc`, `_CBTProc@12`, dann Ordinal 1. Danach globaler
   `SetWindowsHookExW(WH_CBT, ..., thread=0)`.
9. Fehler nach `LoadLibrary` laufen über `UninstallCbtHook`; Cleanup unhookt vor dem FreeLibrary.

## Environment-Variablen und persistente externe Namen

| Name | Aktueller Pfad | Semantik / Ziel |
| --- | --- | --- |
| `LOCALAPPDATA` | Indirekt über `SettingsFilePath`; Hook-Cache und `session.state` verwenden dessen Parent. | `%LOCALAPPDATA%\GenieEffect\settings.json` bleibt kompatibel; Settings Repository und `SessionStateStore` teilen nur eine präzise App-Data-Pfadkomponente. |
| `TEMP`/`GetTempPathW` | Fallback des Hook-Caches, falls Settings-Pfad leer. | Im Hook-Resource-Loader beibehalten. |
| `GENIE_TASKBAR_RECT` | `Application::Initialize` druckt Hinweis; tatsächlicher Parser ist `TaskbarTargetProvider::TryGetEnvironmentTarget`: `left,top,right,bottom`, 128-Wchar-Puffer, genau vier Longs, positive Größe. | `platform/windows/taskbar_target_provider`; Parsing separat testbar und String unverändert. |
| `GENIE_TEST_DEVICE_RECOVERY` | Nur `_DEBUG`; `Application::Run` löst einmal Animation-Renderer-Recovery aus. Die Settings-UI liest denselben Flag separat für ihre eigene Device-Recovery. | Getrennte Debug-Testhooks in `runtime::RendererRecovery` und `ui/rendering::ImGuiRenderer`; kein Produktions-Policyzustand. |
| Registry-Wert `GenieEffect` | HKCU Run-Key; Wert ist quoted absolute EXE path. | `platform/windows/startup_manager`; Name/Command-Format erhalten. |
| Session-Datei `session.state` | Neben `settings.json`; Werte `running`, `safe`, `clean`. | `platform/windows/session_state_store`; siehe Safe-Mode-Befund unten. |

## Win32-/Callback-Inventar

| Callback/Thunk | Registrierung und heutiger Aufrufer | Zustand/Ownership | Ziel |
| --- | --- | --- | --- |
| `ConsoleHandler(DWORD)` | `SetConsoleCtrlHandler(..., TRUE)` in `wmain`; Signale CLOSE, C, BREAK, LOGOFF, SHUTDOWN. | Liest geliehenes `g_application`; ruft nur `RequestShutdown`, führt keinen Cleanup auf dem Control-Thread aus. | Eng gekapselter Thunk in `ProcessBootstrap`, der eine thread-sichere Shutdown-Funktion besitzt. |
| Settings-Action-Lambdas in `Application::Initialize` | 22 `std::function`-Callbacks an `SettingsWindow::Initialize`. | Alle capturen `this`; UI darf sie nach `Application`-Tod nicht aufrufen. Heute beendet `settings_window_.Shutdown()` vor Memberzerstörung. | Eine `ui::SettingsActions`-Grenze/`SettingsController`, kein Callback-Bündel. |
| WindowEvent-Lambdas in `Initialize` und `ExitSafeMode` | `WindowEventMonitor::Start` ruft Minimize/Restore/Seen aus WinEvent-/Shell-Callbacks. | Capturen `this`; Monitor besitzt Callbackkopien, muss vor Feature-Zielen stoppen. | `EffectController` als Callbackziel; Plattformmonitor bleibt dünn. |
| Overlay-Lambdas in `InitializeRun` | `OverlayWindow` ruft bool Minimize/Restore bei registrierten Hook-Nachrichten. | Capturen `this`; Overlay besitzt Callbackkopien. | `EffectController`/Minimize-/Restore-Feature; Renderer darf nicht in `Application` zurückrufen. |
| `EnumDisplayMonitors`-Lambda | Lokal in `BuildDiagnosticsSnapshot`, erhöht `int*`. | Kein persistenter Zustand, `LPARAM` zeigt geliehen auf lokalen Zähler. | `DiagnosticsService`/Monitor-Info-Adapter. |
| `EnumWindows`-Lambda in `CleanupAndRestoreAll` | Entfernt Exclusion und heilt Property-markierte Fenster inline. | Captureless; arbeitet über globale Property-Konstanten, kein Context. | `WindowRecoveryService::RestoreAll`; keine duplizierte Inline-Restore-Logik. |
| `EnumWindows`-Lambda in `HealLeftoverWindows` | Context ist geliehener Pointer auf lokales `pair<Application*, size_t*>`. | Synchroner `EnumWindows`-Call; Pointer nur während Call gültig. | `WindowRecoveryService`; Context enthält Service, nicht `Application`. |
| `abort_pending_minimize`-Lambda | Lokal in `CompletePendingNativeMinimize`. | Capturt `this`, Index und geliehenes HWND; räumt Cloak/Alpha/Region/Props/Overlay/Snapshot/Run auf. | `MinimizeTransaction::Rollback`; gemeinsamer idempotenter Cleanup. |

Single-Instance-Aktivierung ist aktuell zusätzlich an UI gekoppelt:
`SettingsWindow::ActivateExistingInstance(5000)` pollt alle 50 ms nach Fensterklasse
`GenieEffectImGuiSettings` und sendet `WM_APP+101` mit `SendMessageTimeout` (1 s,
`SMTO_ABORTIFHUNG|SMTO_BLOCK`). Das Fenster erlaubt diese Message per
`ChangeWindowMessageFilterEx`, was für unterschiedliche Integrity Levels/UIPI relevant ist.
Dieser Aktivierungsvertrag muss an der Single-Instance-/Window-Grenze erhalten bleiben.

## Funktions-Migrationsprotokoll

Die Spalte „Ownership/Cleanup“ beschreibt die heute berührten Ressourcen und den beim Verschieben
zu erhaltenden Fehler-/Recovery-Vertrag. „Kein Aufrufer“ bedeutet einen durch repo-weite Suche
bestätigten internen Dead-Code-Kandidaten; wegen interner Linkage kann er bewusst entfernt werden,
statt als unbenutzte Funktion mitzuwandern.

### `main.cpp` — 2/2

| ID | Alte Funktion | Aktuelle direkte Aufrufer | Verbindliches Ziel | Ownership/Cleanup und Verhalten |
| --- | --- | --- | --- | --- |
| M01 | `BOOL WINAPI ConsoleHandler(DWORD)` (`main.cpp:12`) | Windows nach Registrierung durch `wmain` | Console-Control-Thunk in `app::ProcessBootstrap` | Besitzt nichts; `g_application` ist geliehen. Für fünf Shutdownsignale atomaren Shutdown anfordern und `TRUE` liefern, sonst `FALSE`. Kein Win32-/D3D-/Window-Cleanup auf dem Control-Thread; der Main Thread wird durch Timer/`WM_QUIT` geweckt. |
| M02 | `int wmain()` (`main.cpp:23`) | CRT Entry Point | Kleiner Entry Point; `ProcessBootstrap`, `SingleInstanceGuard`, dann `Application` | Setzt Per-Monitor-V2-DPI (Rückgabe heute ignoriert). Erstellt/behält Mutex-`HANDLE`; bei `ERROR_ALREADY_EXISTS` **oder** `ERROR_ACCESS_DENIED` Handle ggf. schließen, bestehende UI bis 5 s aktivieren, Exit 0. Bei anderem Mutexfehler Exit 1. Initialisiert COM STA; nur bei Erfolg `CoUninitialize`. Registriert Handler erst mit gültigem Stack-`Application`, entfernt ihn und nullt Pointer vor Destruktion. Auf Initfehler: Handler ab, COM uninit, Mutex close. Normal: `Run`, gleiche Freigaben; `Application`-Destruktor folgt beim Scope-Ende. |

Zusatz zum Entry Point: Nicht-Debug erzwingt per `#pragma comment(linker, ...)`
`/subsystem:windows /ENTRY:wmainCRTStartup`; das Projekt setzt denselben Release-Vertrag bereits
explizit. Die Zielimplementierung hält diese Buildentscheidung in Projektmetadaten und keine
Geschäftslogik in `main.cpp`.

### `startup_manager.cpp` — 2/2

| ID | Alte Funktion | Aktuelle direkte Aufrufer | Verbindliches Ziel | Ownership/Cleanup und Verhalten |
| --- | --- | --- | --- | --- |
| S01 | `CurrentExecutablePath()` (`startup_manager.cpp:13`) | `ConfigureRunAtStartup` | Private Methode/Helfer von `platform/windows/startup_manager.*` | Besitzt nur `std::vector<wchar_t>`. Start 512 Zeichen, Verdopplung bis maximal 32768; leeres Ergebnis bei API-/Längenfehler. Kein Win32-Handle. |
| S02 | `ConfigureRunAtStartup(bool)` (`startup_manager.cpp:29`) | `Application::Initialize` (Reparatur) und `SetStartupOptions` | `platform::windows::StartupManager` | Besitzt `HKEY` aus `RegCreateKeyExW(HKCU, ..., QUERY|SET)` und schließt ihn auf allen Pfaden. Enable lehnt leeren oder quote-enthaltenden EXE-Pfad ab und schreibt `REG_SZ` als `"absolute.exe"` inklusive NUL. Disable löscht; `ERROR_FILE_NOT_FOUND` gilt als Erfolg. Kein offener Registry-Handle bei Fehler. |

### Anonymer Namespace in `application.cpp` — 36/36

| ID | Alte Funktion | Aktuelle direkte Aufrufer | Verbindliches Ziel | Ownership/Cleanup und Verhalten |
| --- | --- | --- | --- | --- |
| H01 | `QueryExecutableProductVersion()` (`application.cpp:33`) | `BuildDiagnosticsSnapshot` | `platform/windows/process_info.*` | Besitzt lokalen Byte-Buffer; `Translation*`, ProductVersion-String und `VS_FIXEDFILEINFO*` sind geliehene Sichten hinein. Bevorzugt ProductVersion-String, trimmt NUL/Space, konvertiert UTF-8; Fallback fixed version mit optionalem Buildteil; leer bei Fehler. |
| H02 | `WindowProcessId(HWND)` (`:118`) | `IsFullscreenApplicationActive`, `OnMinimizeStart`, `OnWindowSeen`, `UpdatePreMinimizeSnapshot`, `PruneSnapshots` | `platform/windows/process_info.*` | HWND geliehen; 0 für null/Fehler. PID ist wesentlich gegen HWND-Reuse im Snapshot-Pruning. |
| H03 | `IsProcessElevated()` (`:126`) | `Application::Initialize` | `platform/windows/process_info.*` (von `ProcessBootstrap`/Diagnostics genutzt) | Besitzt Token-`HANDLE` nach erfolgreichem `OpenProcessToken`; schließt immer. Fehlende Abfrage wird als nicht erhöht behandelt und löst UIPI-Warnung aus. |
| H04 | `LoadEmbeddedResource(int)` (`:145`) | `ExtractEmbeddedHookDll` | Hook-spezifischer Resource-Loader bei `CbtHookManager` | `HRSRC`/`HGLOBAL` und Bytepointer sind vom EXE-Modul geliehen; kein Free. Liefert leere Sicht bei Find/Load/Lock/Size-Fehler. |
| H05 | `ResourceFingerprint(const EmbeddedResource&)` (`:158`) | `ExtractEmbeddedHookDll` | Hook-Resource-Loader | Keine Resource; deterministisches FNV-1a 64 über jedes Byte, Initialwert/Multiplikator exakt erhalten und isoliert testbar machen. |
| H06 | `FileMatchesResource(path, resource)` (`:167`) | `ExtractEmbeddedHookDll` (vor Schreiben und nach Move-Race) | Hook-Resource-Loader | `ifstream` RAII; erst Größe, dann kompletter Bytevergleich. Keine Dateiänderung. |
| H07 | `HookCacheDirectory()` (`:177`) | `ExtractEmbeddedHookDll` | Hook-Resource-Loader / `CbtHookManager` | Keine Handle-Resource. Primär Settings-Parent plus `hooks`, sonst `GetTempPathW` plus `GenieEffect/hooks`; leer bei Tempfehler. |
| H08 | `ExtractEmbeddedHookDll()` (`:190`) | `InstallCbtHook` | Hook-Resource-Loader innerhalb/bei `CbtHookManager` | Besitzt Streams/temporäre Datei nur während Operation. Erstellt Verzeichnis; validiert Cache; schreibt PID-Temp und atomaren Replace/Write-through. Entfernt Temp best effort auf Fehler, akzeptiert konkurrent korrekt geschriebenes Ziel. Finaler Cache bleibt. |
| H09 | `GetExecutableDirectory()` (`:227`) | `InstallCbtHook` als Sibling-DLL-Fallback | Private Pfadfunktion von `CbtHookManager` bzw. `process_info` | Nur `wstring`; dynamische Erweiterung, Fallback `.\` bei Fehler/fehlendem Slash. Kein Handle. |
| H10 | `ToRectF(const RECT&)` (`:247`) | `OnMinimizeStart`, `OnRestoreAttempt` | `animation/geometry.*` | Reine wertweise Konvertierung, keine Resource. |
| H11 | `RectTraceString(const RECT&)` (`:256`) | `WindowTraceString`, `OnMinimizeStart`, `UpdatePreMinimizeSnapshot` | `core/logger.*` als benannter Formatter | Nur Stream/String; keine Resource. |
| H12 | `RectFTraceString(const RectF&)` (`:262`) | `OnMinimizeStart` | `core/logger.*` bzw. animationnaher Formatter | Nur Stream/String; keine Resource. |
| H13 | `WindowTraceString(HWND)` (`:268`) | `TraceWindowEvent`, `BringWindowForwardForCapture`, `OnMinimizeStart`, `UpdatePreMinimizeSnapshot` | `core/logger` + schmale Datenabfrage aus `window_state/process_info` | HWND geliehen; prüft `IsWindow`, liest Class/Title/Styles/Cloak/Rect/Placement. Keine Mutation/Cleanup. |
| H14 | `TraceWindowEvent(name, HWND)` (`:301`) | `BringWindowForwardForCapture`, Transparenzhelper, `Run`, `OnMinimizeStart`, `FinishActiveAnimation`, `CompletePendingNativeMinimize`, `OnRestoreAttempt`, `UpdatePreMinimizeSnapshot` | `core/logger.*` | Keine Resource; zentrale Trace-Formatierung statt anonymer Sammelhelper. |
| H15 | `ClipRectToVirtualScreen(const RECT&)` (`:307`) | `ResolveAnimationBounds` | `platform/windows/window_state.*` | Reine Rect-Operation mit geliehenem Virtual-Screen-Wert; `nullopt` bei leerem Schnitt. |
| H16 | `GetPlacement(HWND)` (`:317`) | `WasOrWillRestoreMaximized`, `IsCurrentlyMaximized`, `OnMinimizeStart`, `UpdatePreMinimizeSnapshot` | `platform/windows/window_state.*` | HWND geliehen; `WINDOWPLACEMENT.length` gesetzt; `nullopt` bei API-Fehler. |
| H17 | `IsUsableRect(const RECT&)` (`:326`) | Property-Read, Minimize, Preserve-Restore, Snapshot, Window-Restore | `platform/windows/window_state.*` | Reines Prädikat: positive Größe und left/top größer als -30000; diese Legacy-Offscreen-Grenze erhalten/testen. |
| H18 | `IsMinimizedShowCommand(UINT)` (`:331`) | **Kein Aufrufer** | Bewusst entfernen; falls wieder benötigt, `window_state` | Keine Resource. Interne Linkage und repo-weit unreferenziert; nicht als toten Shim migrieren. |
| H19 | `RectWithSizeAt(rect,left,top)` (`:336`) | **Kein Aufrufer** | Bewusst entfernen; falls wieder benötigt, `animation/geometry` | Reine Wertfunktion; keine Resource. |
| H20 | `StoreOriginalPlacementProperties(HWND,RECT)` (`:345`) | `OnMinimizeStart`, `PreserveRestorePlacementAndMarkOffscreen` | `platform/windows/window_properties.*` | HWND geliehen; schreibt vier integerkodierte Properties. Bei partiellen `SetPropW`-Fehlern gibt es heute kein Rollback/Fehlersignal; Zielvertrag muss logisch atomaren Erfolg oder vollständigen Rollback liefern. |
| H21 | `ReadOriginalPlacementProperties(HWND)` (`:356`) | `RestoreWindowFromGenieState` | `window_properties.*` | HWND geliehen; liest vier Werte, validiert über `IsUsableRect`; keine Entfernung. Nullkodierte Koordinaten beachten. |
| H22 | `StoreWasMaximizedProperty(HWND,bool)` (`:373`) | `OnMinimizeStart`, `PreserveRestorePlacementAndMarkOffscreen` | `window_properties.*` | Setzt Marker bei true, entfernt ihn bei false; HWND geliehen. |
| H23 | `HasGenieWindowState(HWND)` (`:381`) | EnumWindows-Callbacks in `CleanupAndRestoreAll`, `HealLeftoverWindows` | `window_properties.*` | Prüft Move-, Transparency-, OriginalExStyle- oder OriginalLeft-Marker. Ist absichtlich breiter als nur Minimize-Property, aber bei nullkodiertem Left nicht allein zuverlässig. |
| H24 | `WideToUtf8(wstring_view)` (`:388`) | `BuildDiagnosticsSnapshot` (Adapter/Monitorname) | Private Konvertierung in `process_info`/`DiagnosticsService`, kein allgemeines `utils` | Keine Resource; leeres Ergebnis bei leer/Conversion-Fehler. |
| H25 | `AnimationStyleFromName(string_view)` (`:399`) | `OnMinimizeStart`, `OnRestoreAttempt` | `animation` als typisierte Style-Konvertierung/Settings-Serializer-Kompatibilität | Reine Mapping-Funktion: historisches `"Gienie curvy"`→Curvy, `"Squash"`→Squash, alles andere→Classic. Gespeicherten Schreibfehler kompatibel halten. |
| H26 | `AnimationStyleDurationScale(string_view)` (`:405`) | `OnMinimizeStart`, `OnRestoreAttempt` | Typisierte Animation-Konfiguration, angewandt vom Minimize-/Restore-Feature | Reine Policy: Curvy 0.78, Squash 0.55, sonst 1.0. |
| H27 | `ClearGenieWindowProperties(HWND)` (`:411`) | `Run`-Timeout, Minimize-Fehler/Rollback, `RestoreWindowFromGenieState`, Shutdown-Enum | `window_properties.*` | HWND geliehen, invalides Fenster no-op. Entfernt Placement/Move/Max/Minimize/Allow; Transparenzgruppe bleibt separat und muss zuvor restauriert werden. |
| H28 | `WasOrWillRestoreMaximized(HWND)` (`:426`) | **Kein Aufrufer** | Bewusst entfernen; vorhandenes benötigtes Verhalten liegt in `window_state` | Keine Resource; interne Linkage. |
| H29 | `IsCurrentlyMaximized(HWND)` (`:435`) | `ResolveAnimationBounds`, `OnMinimizeStart`, `UpdatePreMinimizeSnapshot` | `platform/windows/window_state.*` | HWND geliehen; bevorzugt Placement `SW_SHOWMAXIMIZED`, Fallback `IsZoomed`. |
| H30 | `BringWindowForwardForCapture(HWND)` (`:443`) | Initialisierung des lokalen `TopmostRestorer` in `OnMinimizeStart` | `MinimizeTransaction` mit `window_state`-Adapter | HWND geliehen; invalid/iconic→false. Setzt temporär TOPMOST, foreground/top, `DwmFlush`; gibt vorherigen Topmost-Zustand zurück. Fehler werden geloggt, Mutation wird dennoch durch RAII zurückgenommen. |
| H31 | `ForegroundIsExactWindow(HWND,ignored)` (`:464`) | **Kein Aufrufer** | Bewusst entfernen; falls benötigt, `window_state` | Geliehene HWNDs, reine Abfrage. |
| H32 | `KeepGenieMinimizedWindowHidden(HWND)` (`:472`) | **Kein Aufrufer** | Bewusst entfernen; Verhalten liegt bereits in Minimize-Transaction/Recovery | Geliehenes HWND; würde cloak/transparenzieren und ggf. mit transientem Allow-Marker minimieren. Weil tot, keine zweite Cleanup-Implementierung migrieren. |
| H33 | `GetMonitorWorkArea(HWND, optional<RECT>)` (`:488`) | `ResolveAnimationBounds` | `platform/windows/window_state.*` | Geliehenes HWND/HMONITOR; nearest window, Fallback rect; `nullopt` bei MonitorInfo-Fehler. Multi-Monitor-Verhalten erhalten. |
| H34 | `ResolveAnimationBounds(HWND)` (`:502`) | `OnMinimizeStart`, `UpdatePreMinimizeSnapshot` | `platform/windows/window_state.*` | HWND geliehen. Maximized→Monitor-Work-Area; sonst Extended Frame Bounds, auf Virtual Screen clippen. Fehler nur loggen und `nullopt`; keine Mutation. |
| H35 | `MakeWindowTransparent(HWND)` (`:531`) | toter Helper `KeepGenieMinimizedWindowHidden`, `OnMinimizeStart`, `CompletePendingNativeMinimize`, `OnRestoreAttempt` | `window_properties.*`, verwendet von Transactions | HWND geliehen. Idempotent über Marker; speichert ExStyle/Layered/Alpha/Flags, setzt `WS_EX_LAYERED` und Alpha 0. Bei erstem Property-Fehler begrenzter Rollback; spätere `SetProp`-/Style-/Alpha-Fehler werden heute nicht geprüft. |
| H36 | `RestoreWindowTransparency(HWND)` (`:564`) | `Run`-Timeout, Minimize-/Pending-Rollback, `RestoreWindowFromGenieState`, Shutdown-Enum | `window_properties.*` | HWND geliehen. Stellt nur Layered-Bit und ggf. Alpha/Flags wieder her, niemals kompletten gespeicherten ExStyle; entfernt alle fünf Transparenzproperties. Idempotenter no-op ohne Marker. |

### Lokaler `TopmostRestorer` — 2/2

| ID | Alte Methode | Aktuelle direkte Aufrufer | Verbindliches Ziel | Ownership/Cleanup und Verhalten |
| --- | --- | --- | --- | --- |
| T01 | `TopmostRestorer::RestoreNow()` (`application.cpp:2213`) | Explizit nach Capture und implizit vom Destruktor | `features/minimize/minimize_transaction.*` | Geliehenes HWND; genau einmal. Wenn Fenster noch gültig und ursprünglich nicht topmost, `HWND_NOTOPMOST` ohne Move/Size/Activate. |
| T02 | `TopmostRestorer::~TopmostRestorer()` (`:2223`) | Automatischer Scope-Exit aus `OnMinimizeStart`, auch alle Early Returns | `MinimizeTransaction`-RAII | Ruft `RestoreNow`; garantiert Best-effort-Rollback der temporären Z-Order. |

### `Application` — 78/78

| ID | Altes Member | Aktuelle direkte Aufrufer | Verbindliches Ziel | Ownership/Cleanup und Verhalten |
| --- | --- | --- | --- | --- |
| A01 | `Application() = default` (`application.hpp:25`) | Stackkonstruktion in `wmain` | Kleine `app::Application`, künftig mit `HINSTANCE`/Komponenten-Konstruktion | Default konstruiert heute alle RAII-/Wertmember; noch keine externen Handles. Zielkonstruktion darf nicht bereits teilweise aktive Callbacks veröffentlichen. |
| A02 | `~Application()` (`application.cpp:600`) | Automatisch am Ende von `wmain`, auch nach fehlgeschlagenem `Initialize` | `app::Application::~Application` | Ruft `CleanupAndRestoreAll`, schließt danach den eigenen Waitable-Timer und nullt ihn. Cleanup muss instanzbezogen/idempotent bleiben; Timer erst schließen, nachdem `Run` nicht mehr wartet. |
| A03 | `Application(const Application&) = delete` (`application.hpp:28`) | Kein Aufrufer; compile-time Ownership-Vertrag | In kleiner `Application` beibehalten | Verhindert Doppelbesitz von Handles/Komponenten. |
| A04 | `Application::operator=(const Application&) = delete` (`application.hpp:29`) | Kein Aufrufer; compile-time Ownership-Vertrag | In kleiner `Application` beibehalten | Verhindert Doppel-Cleanup. |
| A05 | `ReadSessionState() const` (`application.cpp:608`) | **Kein aktueller Aufrufer** | `platform/windows/session_state_store.*` | Liest erste Zeile von `session.state` neben Settings per RAII-Stream; leer bei Pfad-/Open-Fehler. Nicht als Dead Code löschen, bevor die im Masterplan geforderte Safe-Mode-Erkennung korrekt an diesen Zieladapter verdrahtet ist. |
| A06 | `WriteSessionState(string_view) const` (`:619`) | `Initialize`, `ExitSafeMode`, `CleanupAndRestoreAll` | `SessionStateStore::Write` | Erstellt Parent, schreibt `<path>.tmp`, flush/close, atomarer `MoveFileEx(REPLACE|WRITE_THROUGH)`; Temp best effort entfernen. Zustände heute `running`/`safe`/`clean`. |
| A07 | `Initialize(HINSTANCE)` (`:646`) | `wmain` | Kleine transaktionale `Application::Initialize` | Heute sequenziell: Logs/Settings/Startup-Reparatur, Debug-Log ACL, Timer, Heal, Elevation, Renderer, UI+22 Callbacks, Hotkeys/UI, Policy, Monitor, Sessionmarker. Bei mehreren späten `false`-Returns kein lokaler Rollback; nur späterer Destruktor-Cleanup. Ziel: Schritt N rollt 1..N-1 sofort in umgekehrter Reihenfolge zurück. `HINSTANCE` geliehen. |
| A08 | `FindRunForWindow(HWND) const` (`:766`) | Restore-Poll in `Run`, `OnMinimizeStart`, excluded branch und normaler Pfad von `OnRestoreAttempt` | `runtime::AnimationRunPool` | HWND geliehen; lineare Suche, `-1` wenn keiner. Zielcache muss HWND-Validität/PID beachten. |
| A09 | `FindAvailableRun()` (`:775`) | Restore-Poll in `Run`, `OnMinimizeStart`, `OnRestoreAttempt` | `AnimationRunPool::Acquire` | Gibt idle Slot oder emplaced/initialisiert neues Overlay; bei Fehler poppt zurück. Der reine Restore-Poll ruft es heute nur als Availability-Test auf und kann dabei einen Slot erzeugen (Seiteneffekt erhalten oder bewusst durch `CanAcquire` trennen). |
| A10 | `IsOverlayWindow(HWND) const` (`:790`) | Fullscreen, Minimize, Restore, WindowSeen, Snapshot | `AnimationRunPool`/schmaler Overlay-Window-Registry-Adapter | HWND geliehen; prüft gegen alle Overlay-HWNDs, besitzt sie nicht. |
| A11 | `InitializeRun(AnimationRun&)` (`:796`) | `FindAvailableRun`, `CreateAnimationRenderer` | `AnimationRunPool` + Rendering-Fabrik | Initialisiert das vom Run besessene Overlay mit geliehenem `HINSTANCE`/`D3dDevice*` und zwei `this`-Callbacks; setzt Dauer/State. Bei Fehler ist Overlay selbst für partiellen Init-Cleanup verantwortlich. Ziel ohne Rückruf in `Application`. |
| A12 | `CreateAnimationRenderer()` (`:808`) | `Initialize`, `TryRecoverAnimationRenderer`, `ExitSafeMode` | `runtime::RendererRecovery`/Rendering-Komposition | Shutdown aller Overlays, clears Runs, Capture, Device; erstellt Device→ersten Run→Capture. Fehler räumt neu erzeugte Runs/Device auf. Reihenfolge und geliehener Device-Pointer des Capture erhalten. |
| A13 | `AnimationRendererDeviceLost() const` (`:828`) | `Run` vor und während Run-Ticks | `RendererRecovery::IsDeviceLost` | Aggregiert Overlay-, Capture- und Device-Flags; keine Resourceänderung. |
| A14 | `BeginAnimationRendererRecovery()` (`:836`) | Debug-Testhook in `Run`, Device-Lost-Pfade in `Run`, Diagnostics Restart | `runtime::RendererRecovery::Begin` | Idempotent bei pending. Disable Blocker; pro Run Overlay shutdown, Ziel best effort restaurieren und Minimize ggf. nativ abschließen; Pacing beenden; Capture/Snapshot/Device freigeben; Backoff initialisieren und sofort Retry. Alle geliehenen HWNDs vor Nutzung prüfen. |
| A15 | `TryRecoverAnimationRenderer()` (`:881`) | `BeginAnimationRendererRecovery`, `Run` | `RendererRecovery::TryRecover` | Wartet bis Deadline. Bei Erfolg baut Renderer neu, reaktiviert Blocker nur bei aktiver Policy, reset Backoff. Bei Fehler exponentiell bis 4000 ms. Kein Busy-Retry ohne Message-Wait. |
| A16 | `Run()` (`:907`) | `wmain` | `app/message_loop.*` | Besitzt lokalen `MSG`, nicht Komponenten. Pollt Pause/Fullscreen/Power/Watchdog; dispatcht alle Messages; rendert UI; betreibt Recovery, Pending-Minimize, Live-Capture, Overlay-Tick/Completion, Snapshot-Poll/Pruning und Wait-Pacing. Ziel-Loop delegiert all diese Fachblöcke. `WM_DISPLAYCHANGE` invalidiert Monitorcache. Rückgabe `message.wParam` (bei atomic break initial 0). |
| A17 | `RequestShutdown()` (`:1112`) | ConsoleHandler und UI-Exit-Lambda | `Application`/`MessageLoop::RequestStop` | Thread-safe atomischer Flag; signalisiert ggf. geliehen/owned Timer-Handle, postet `PostQuitMessage` und `PostThreadMessage(WM_QUIT)` an gespeicherte Main-Thread-ID. Besitzt/freed nichts und führt Cleanup nicht auf Fremdthread aus. |
| A18 | `ResetAnimationFramePacing(index,HWND,RECT)` (`:1127`) | `OnMinimizeStart`, `OnRestoreAttempt` | `runtime::FrameScheduler::StartRun` | Beginnt ggf. globale Timerauflösung; schreibt Run-Pacing, liest optional aktuelle Bounds aus geliehenem HWND, setzt Deadline und Monitor. |
| A19 | `UpdateAnimationFramePacingMonitor(index)` (`:1144`) | `Run`, `ResetAnimationFramePacing` | `FrameScheduler` | HWND/HMONITOR geliehen. Aktualisiert Bounds, wählt nearest Monitor, fragt Refresh Hz. Ohne Rate deaktiviert fixed pacing; mit Rate setzt Intervall/Deadline. Displaychange setzt Cache andernorts null. |
| A20 | `IsAnimationFrameDue(index) const` (`:1185`) | `Run` | `FrameScheduler` | Reine Zeitabfrage; Zero-Intervall bedeutet immer due. |
| A21 | `AdvanceAnimationFrameDeadline(index)` (`:1191`) | `Run` nach Overlay-Tick | `FrameScheduler` | Verschiebt Deadline um alle verpassten Intervalle, begrenzt `recent_missed_frames_` auf 120 und lässt Zähler langsam sinken. Keine Handles. |
| A22 | `WaitForAnimationFrameOrMessage()` (`:1216`) | `Run`, wenn mindestens ein Overlay aktiv | `FrameScheduler`/`MessageLoop` | Wählt früheste Run-Deadline. Ohne Fixed-Rate `DwmFlush`; mit eigenem Waitable-Timer relative 100-ns Deadline und `MsgWaitForMultipleObjectsEx`; SetTimer-Fehler fällt auf millisekundengenaues Message-Wait zurück. Timer bleibt Owner-Resource. |
| A23 | `BeginFallbackTimerResolution()` (`:1263`) | `ResetAnimationFramePacing` | `FrameScheduler`-RAII | Nur wenn kein High-Resolution-Waitable-Timer und noch inaktiv. `timeGetDevCaps`, dann `timeBeginPeriod(min)`; merkt Periode nur bei Erfolg. Prozessweiten Effekt zwingend ausgleichen. |
| A24 | `EndFallbackTimerResolution()` (`:1279`) | Recovery, idle-Pfad in `Run`, `FinishActiveAnimation`, globaler Cleanup | `FrameScheduler`-RAII | Idempotent; genau ein `timeEndPeriod` für erfolgreiche Begin-Periode, danach Werte nullen. |
| A25 | `SetEnabled(bool)` (`:1288`) | Settings-UI-Toggle-Lambda, Hotkey Toggle | `settings::SettingsService` über `ui::SettingsActions`; Policy-Refresh in `EffectController` | Arbeitet auf Settings-Kopie, speichert zuerst, veröffentlicht danach; bei unverändert nur UI-Sync. Kein Resourcebesitz, aber Runtime-Enable/Disable kann Hook/Blocker/Runs verändern. |
| A26 | `RunStateName(RunState)` (`:1308`) | `CheckAnimationTimeouts` | `runtime/run_state.*` | Reines vollständiges Enum→String-Mapping, Fallback Unknown. |
| A27 | `SetRunState(index,state)` (`:1328`) | `Run`, Watchdog, Minimize/Restore/Finish/Pending | `AnimationRun`/`AnimationRunPool::Transition` | Prüft nur Index, setzt State+Zeit; validiert heute **keine** erlaubte Kante und loggt Transition nicht. Ziel muss beides tun. |
| A28 | `CheckAnimationTimeouts()` (`:1334`) | `Run` pro Iteration | `AnimationRunPool`/gemeinsamer Runtime-Cleanup | Timeouts: Capturing 2500 ms, Waiting 2000, Aborting/Cleaning 1500, sonst 10000. Cancel Overlay, nullt Slot, restauriert Ziel, löscht beide Snapshots/Pacing, zurück Idle. HWND geliehen und geprüft. Muss mit normalem/Device/Shutdown-Cleanup vereinigt werden. |
| A29 | `IsTemporarilyPaused() const` (`:1375`) | `IsEffectActive`, `SetTemporaryPause` UI-Sync | `features/pause/PauseController` | Reine Tick-Abfrage plus Until-Restart-Flag; Ablauf selbst in `UpdateTemporaryPause`. |
| A30 | `IsEffectActive() const` (`:1380`) | Recovery, Run-Snapshot/Poll, Runtime-Refresh, Diagnostics, Exclusion, Minimize/Restore/Seen | `features/effect/EffectPolicy` | Reine Entscheidung aus `!safe_mode && enabled && !pause && !fullscreen && !batterySaver`. Keine Seitenwirkung. |
| A31 | `IsFullscreenApplicationActive() const` (`:1385`) | `UpdateFullscreenSuppression` | `platform/windows/fullscreen_detector.*` | Geliehene Foreground/Settings/Overlay-HWNDs; filtert eigenen Prozess, unsichtbar/iconic/zoomed/uninteressant. Extended bounds oder WindowRect, nearest Monitor, 2px Toleranz; nur caption-/thickframe-loses Monitor-Cover gilt fullscreen. |
| A32 | `UpdateFullscreenSuppression(bool)` (`:1418`) | `Initialize(force)`, `Run`, Fullscreen-Setting-Änderung(force) | `FullscreenDetector` + `EffectPolicy/EffectController` | 500-ms Poll-Throttle; bei Zustandswechsel loggen und Runtime refresh. Keine Handles besitzen. |
| A33 | `UpdatePowerState(bool)` (`:1431`) | `Initialize(force)`, `Run`, Battery-Setting-Änderung(force) | `platform/windows/power_status_monitor.*` + `EffectPolicy` | 5-s Poll; liest `SYSTEM_POWER_STATUS`, aktualisiert on-battery/saver/suppression; Runtime nur bei Suppressionwechsel. API-Fehler lässt letzten Zustand stehen. |
| A34 | `DisableEffectRuntime()` (`:1453`) | `RefreshEffectRuntimeState` | `features/effect/EffectController::Stop` | Erzwingt jeden Run-Abschluss, unhookt, deaktiviert Blocker, restauriert alle getrackten HWNDs best effort, leert Snapshots/Capture-History, setzt active=false. Cleanup-Reihenfolge muss Owner-basiert/idempotent werden. |
| A35 | `EnableEffectRuntime()` (`:1474`) | `RefreshEffectRuntimeState` | `EffectController::Start` | Installiert Hook best effort (WinEvent fallback bleibt), aktiviert Blocker nur mit gesundem Overlay, setzt Runtime trotzdem active=true. Hook/Blocker besitzen ihre Ressourcen selbst. |
| A36 | `RefreshEffectRuntimeState()` (`:1485`) | Init, Enabled/Fullscreen/Power/Pause/SafeMode | `EffectController` mit `EffectPolicy` | Start/Stop nur bei Kantenwechsel; aktualisiert danach immer Exclusion-/DWM-Overrides. |
| A37 | `SetTemporaryPause(TemporaryPauseAction)` (`:1497`) | Settings-UI-Pause-Lambda | `PauseController` über `SettingsActions` | Resume löscht beide; 10 min/1 h setzt Tick-Deadline; UntilRestart setzt Flag. Refresh Runtime und UI-Snapshot. Keine Resource. |
| A38 | `UpdateTemporaryPause()` (`:1509`) | `Run` | `PauseController::Update` | Wenn Deadline erreicht: nullen, Runtime/UI fortsetzen; UntilRestart läuft nicht ab. |
| A39 | `UnregisterAllHotkeys()` (`:1517`) | `RegisterConfiguredHotkeys`, `CleanupAndRestoreAll` | `platform/windows/global_hotkey_manager.*` | Settings-HWND geliehen; unregistert jede feste ID, ignoriert Rückgaben. Idempotent/no-op ohne HWND. |
| A40 | `RegisterConfiguredHotkeys()` (`:1524`) | Init, erfolgreicher `SetHotkey`, `ExitSafeMode` | `GlobalHotkeyManager` | Unregister all, prüft Settings-intern Duplikate, registriert nichtleere Bindings mit `MOD_NOREPEAT`, meldet Availability an UI. OS-Registrierungen sind besessene Prozessressourcen und vor HWND/Shutdown zu entfernen. |
| A41 | `SetHotkey(HotkeyAction,HotkeyBinding)` (`:1551`) | Settings-UI-Hotkey-Update-Lambda | `SettingsService` + `GlobalHotkeyManager`, exponiert über `SettingsActions` | Validiert Index/VK/Modifier/Modifier-only/Duplikate. Unregistriert alte OS-Bindung, versucht neue; bei Register- oder Save-Fehler alte Bindung best effort erneut registrieren und UI-Status setzen. Erst nach Save Settings veröffentlichen; abschließend vollständig neu registrieren. |
| A42 | `ExecuteHotkeyAction(HotkeyAction)` (`:1603`) | SettingsWindow-Hotkey-Action-Callback; dessen `WM_HOTKEY`-Routing | `GlobalHotkeyManager` dispatcht typisierte Action an `EffectController`/`SettingsController`/`WindowRecoveryService` | Toggle ruft `SetEnabled`, Open zeigt UI, Repair heilt; `kCount` no-op. Geliehenes Settings-HWND indirekt, keine Resource. |
| A43 | `BuildDiagnosticsSnapshot() const` (`:1619`) | Settings-UI-Diagnostics-Callback, Copy-Aktion | `features/diagnostics/diagnostics_service.*` + reines `diagnostics_snapshot.hpp` | Aggregiert Policy/Hook/Recovery/D3D/Runzahl/Logs/Version/Windows/GPU/Monitor/Taskbar. Lokales `ComPtr<IDXGIAdapter>` besitzt Referenz; `RtlGetVersion`-Pointer/HWND/HMONITOR geliehen; EnumDisplayMonitors-Context lokal. Leere Version fällt auf `dev __DATE__`, Debug-Suffix. Keine Mutation außer OS-Abfragen. |
| A44 | `ExecuteDiagnosticsAction(DiagnosticsAction)` (`:1742`) | Settings-UI-Diagnostics-Action-Lambda | `DiagnosticsService` mit kleinen Commands/Delegaten | ExitSafeMode/Repair/RendererRestart delegieren. OpenLog nutzt geliehenes Settings-HWND und `ShellExecute`. Copy: öffnet Clipboard, alloziert movable `HGLOBAL`; bis erfolgreichem `SetClipboardData` besitzt App den Block und freed bei Fehler, danach besitzt Clipboard/System; `CloseClipboard` auf allen aktuellen Pfaden. |
| A45 | `ExitSafeMode()` (`:1785`) | `ExecuteDiagnosticsAction(kExitSafeMode)` | `SessionStateStore` + `EffectController`/kleine Application-Orchestrierung | No-op wenn nicht safe. Baut Renderer; startet Monitor; bei Monitorfehler Shutdown aller Overlays und reset Capture/Device. Danach safe=false, Hotkeys/Runtime, schreibt `running`. Heute praktisch unerreichbar, weil Init safe nie setzt. |
| A46 | `SetAnimationDurations(float,float,bool)` (`:1806`) | Settings-UI-Speed-Lambda | `SettingsService` + `SettingsValidator` | Clampt 0.10–2.00, mutiert Runtime und `settings_` **vor** optionalem Save; Save-Fehler lässt mutierten In-Memory-Zustand bestehen. Ziel arbeitet Copy→Validate→atomic Save→Publish; `save=false` darf nur klar definierte Preview/gestagte Änderung sein. |
| A47 | `SetLinkSpeeds(bool)` (`:1818`) | Settings-UI-Link-Lambda | `SettingsService` | Copy→Save→Publish, danach UI-Snapshot; keine Resource. |
| A48 | `SetDisableAnimationsFullscreen(bool)` (`:1830`) | Settings-UI-Fullscreen-Lambda | `SettingsService` + `EffectPolicy/FullscreenDetector` | Copy→Save→Publish; force Re-evaluation. Save-Fehler lässt alten Zustand. |
| A49 | `SetDisableEffectsBatterySaver(bool)` (`:1843`) | Settings-UI-Battery-Lambda | `SettingsService` + `EffectPolicy/PowerStatusMonitor` | Copy→Save→Publish; force Power-Poll. |
| A50 | `SetEasing(string,string)` (`:1856`) | Settings-UI-Easing-Lambda | `SettingsValidator`/`SettingsService` | Prüft beide gegen acht exakte Namen, Copy→Save→Publish. Gespeicherte Namen kompatibel halten. |
| A51 | `SetCustomEasingBezier(bool,CubicBezier,bool)` (`:1875`) | Settings-UI-Custom-Bezier-Lambda | `SettingsValidator`/`SettingsService` | Clampt Handles und mutiert gewählte Kurve sofort; `save=false` UI-Preview. Bei `save=true` bleibt Mutation heute auch bei Save-Fehler bestehen. Ziel transaktional oder explizit gestagtes ViewModel. |
| A52 | `SetAnimationStyle(string)` (`:1891`) | Settings-UI-Style-Lambda | `SettingsValidator`/`SettingsService`; typisierte Animation-Konfiguration | Erlaubt exakt `"Gienie classic"`, `"Gienie curvy"`, `"Squash"`; ändert Easing bewusst nicht; Copy→Save→Publish. Historische Schreibweise lesen/schreiben. |
| A53 | `SetQualityMode(string)` (`:1904`) | Settings-UI-Quality-Lambda | `SettingsValidator`/`SettingsService` | Erlaubt automatic/best_quality/power_saving; Copy→Save→Publish. |
| A54 | `SetGenieStrength(float,bool)` (`:1916`) | Settings-UI-Strength-Lambda | `SettingsValidator`/`SettingsService` | Clamp 0.25–1.0, mutiert sofort; optional Save. Wie Dauer/Bezier bleibt Mutation bei Save-Fehler heute bestehen; Ziel trennt Preview/Commit. |
| A55 | `SetFadeStrength(string)` (`:1923`) | Settings-UI-Fade-Lambda | `SettingsValidator`/`SettingsService` | Erlaubt No fade/Subtle/Strong; Copy→Save→Publish. Feature mappt später auf 0/0.25/0.55. |
| A56 | `SetTargetIndicator(bool)` (`:1933`) | Settings-UI-Indicator-Lambda | `SettingsService` | Copy→Save→Publish; keine Resource. |
| A57 | `CalculateAnimationDuration(float,RECT,RectF) const` (`:1942`) | Minimize und Restore | Bewusst entfernen, falls weiterhin Identität; sonst präzise Animation-Konfiguration | Ignoriert heute Source/Target und gibt nur base zurück. Kein toter Forwarder im Ziel; Style-Skalierung ist separat H26. |
| A58 | `SelectMeshSegmentCount(RECT) const` (`:1949`) | Minimize und Restore | `EffectPolicy`/typisierte Animation-Qualitätskonfiguration | Fixed: best=50, saving=20. Automatic berechnet Druck aus Battery/Saver, Pixelzahl, aktiven Runs, Capture-ms, missed frames, Device failures/recovery und liefert 20/35/50. Liest nur Zustand; keine Resource. Inputs künftig expliziter Snapshot statt Zugriff auf alle Systeme. |
| A59 | `SetCloseBehavior(string)` (`:1992`) | Settings-UI-Close-Lambda | `SettingsService` | Erlaubt exit/tray, Copy→Save→Publish. UI setzt Verhalten um. |
| A60 | `SetStartupOptions(bool,bool)` (`:2005`) | Settings-UI-Startup-Lambda | `SettingsService` orchestriert `platform::StartupManager` transaktional | Speichert Proposed **vor** Registry-Seiteneffekt. Bei Registry-Fehler versucht Settings-Rollback; meldet false auch wenn Rollback-Save scheitert. Registry-HKEY gehört StartupManager. Ziel definiert konsistenten Commit/Rollback beider Zustände. |
| A61 | `SetApplicationExcluded(string,bool)` (`:2029`) | Settings-UI-Exclusion-Lambda | `settings/exclusion_rules.*` + `SettingsService` | Normalisiert; case-insensitive Equal; add/remove mit Duplikat-/Missing-Fehler. Mutiert Vektor, speichert, stellt bei Save-Fehler vorherigen Vektor wieder her; danach Policy/Properties und UI aktualisieren. |
| A62 | `IsWindowExcluded(HWND) const` (`:2054`) | Exclusion-Apply, Minimize, Restore, WindowSeen, Snapshot | `EffectPolicy` mit `settings::ExclusionRules` und `platform::ProcessInfo` | HWND geliehen; fragt EXE-Name und vergleicht Settingsliste. Prozessabfragefehler bedeutet nicht ausgeschlossen. |
| A63 | `GetOverlayWindow() const` (`:2060`) | Recovery/Blocker, Fullscreen, Exclusion, Minimize/Restore/Seen, Shutdown | `AnimationRunPool`/Rendering-Overlay-Provider | Gibt geliehenes HWND des ersten Runs oder null. Keine Ownership; heutige Annahme „erstes Overlay ist globaler Hook-Endpunkt“ als expliziten Vertrag modellieren. |
| A64 | `ApplyExclusionTransitionOverrides()` (`:2064`) | `RefreshEffectRuntimeState`, erfolgreicher Exclusion-Update | `EffectController` koordiniert `EffectPolicy`, `WindowProperties`, `NativeAnimationBlocker` | Enumeriert geliehene Top-Level-HWNDs; setzt Excluded nur wenn Effect active und App excluded, sonst entfernt; DWM transitions nur active && !excluded deaktiviert. Keine Fenster besitzen. |
| A65 | `InstallCbtHook()` (`:2076`) | `EnableEffectRuntime` | `platform/windows/cbt_hook_manager.*` | Idempotent bei vorhandenen HHOOK. Extrahiert Resource/fällt auf Sibling zurück, vergibt ACL, besitzt `HMODULE`, löst drei ABI-Namen, besitzt globales `HHOOK`. Jeder Fehler nach Load ruft idempotenten Uninstall. Fehlender Hook ist nicht fatal für Runtime (WinEvent fallback). |
| A66 | `UninstallCbtHook()` (`:2127`) | `DisableEffectRuntime`, zwei Install-Fehlerpfade, globaler Cleanup | `CbtHookManager::Stop`/Destruktor | Idempotent; `UnhookWindowsHookEx` vor `FreeLibrary`, beide Felder null. Rückgabefehler werden heute ignoriert, aber geloggt. |
| A67 | `OnMinimizeStart(HWND)` (`:2141`) | WindowEventMonitor-Lambdas in Init/SafeMode-Exit und Overlay-Callback aus jedem Run | `features/minimize/MinimizeFeature::Execute(MinimizeRequest)` + `MinimizeTransaction` | HWND geliehen. Gate: shutdown/policy/recovery/renderer/exclusion/reentrancy/overlay/interesting. Reverses active restore oder acquires Run. Temporär topmost per RAII; Capture fallback region→window→cache; Snapshot besitzt ComPtrs; setzt Run-Konfiguration, startet Overlay; cloakt/transparenziert, blockiert transitions, persistiert Window Properties, fordert Native-Minimize mit transientem Allow an. Jeder bekannte Fehler räumt Slot/Snapshot/Windowmutationen best effort; Zieltransaktion merkt jede Mutation und rollt vollständig/idempotent zurück. |
| A68 | `FinishActiveAnimation(int)` (`:2413`) | `DisableEffectRuntime`; excluded Restore-Pfad | Gemeinsamer `AnimationRun`-Cleanup, fachliche Endschritte in Minimize-/Restore-Transaction | Setzt CleaningUp und löst Slot vor Window-Calls gegen Reentrancy. Restore: echtes Fenster restaurieren, Overlay finish, Snapshot löschen. Minimize: Pending nativ minimieren, leere Region setzen. Cancel Overlay, Capture aus, Timerauflösung ggf. end, Timer wecken, DwmFlush, Idle. Geliehene HWNDs; HRGN ownership an Wrapper/Windows. |
| A69 | `CompletePendingNativeMinimize(int)` (`:2471`) | `Run`, solange Slot ein Pending-HWND hat | `MinimizeFeature::Update`/`MinimizeTransaction` | Wartet ohne Queue-Flood, bis `IsIconic`. Lokale Rollback-Lambda uncloakt, restauriert Transparenz/Region/Props, cancelt Overlay, löscht Snapshot/Slot. Validiert Slot/Overlay/Snapshot/Placement; speichert Restore-to-max, setzt Cloak/Alpha/Move-Marker, startet Clock und State. Pending HWND geliehen. |
| A70 | `PreserveRestorePlacementAndMarkOffscreen(HWND,CachedSnapshot*)` (`:2554`) | Zwei nicht-iconic Restore-Pfade in `OnRestoreAttempt` | `features/restore/RestoreTransaction` mit `WindowState`/`WindowProperties` | HWND und optionaler Snapshot-Pointer geliehen. Placement→Snapshot→ExtendedBounds fallback, validiert; aktualisiert Snapshot, schreibt vier Placement-/Move-/Max-Marker. Gibt false vor Mutation, wenn kein brauchbares Rect; Property-Teilfehler heute nicht erkannt. |
| A71 | `IsGenieWindowRestored(HWND) const` (`:2592`) | Restore-Poll in `Run`, Surprise-Restore in `OnWindowSeen` | `RestoreFeature`/`WindowProperties` + `WindowState` | Nur relevant mit Restore-Snapshot oder IsMinimizing-Prop; gilt restored, wenn nicht iconic. HWND geliehen. |
| A72 | `OnRestoreAttempt(HWND)` (`:2600`) | WindowEventMonitor-/Overlay-Callbacks, Restore-Poll in `Run`, `OnWindowSeen` | `features/restore/RestoreFeature::Execute(RestoreRequest)` + `RestoreTransaction` | Gate policy/recovery/shutdown/exclusion/reentrancy/overlay/interesting. Excluded heilt ohne Animation. Active minimize wird rückwärts animiert; nicht-iconic Fenster werden cloak/transparent und Placement gesichert. Benötigt Snapshot mit ComPtr; acquires Run, konfiguriert Overlay und startet von Progress 1→0. Alle Fehler restaurieren echtes Fenster/löschen Slot+Snapshot best effort. |
| A73 | `OnWindowSeen(HWND,DWORD)` (`:2787`) | WindowEventMonitor-Callback aus Init/SafeMode-Exit | `features/effect/EffectController` | Filtert shutdown/policy/recovery/overlay. Merkt fremdes interessantes Foreground-HWND; pflegt Excluded-Property/DWM transition; ignoriert bereits animierte Ziele; blockiert native transitions; erkennt Surprise-Restore und delegiert; Foreground aktualisiert Pre-Snapshot. HWND geliehen und teilweise validiert. |
| A74 | `UpdatePreMinimizeSnapshot(HWND)` (`:2833`) | periodischer Foreground-Poll in `Run`, Foreground-Event in `OnWindowSeen` | `runtime/snapshot_cache.*` koordiniert `rendering::DesktopCapture` | Filtert invalid/iconic/invisible/excluded/animating/recovery. Resolve bounds; prune; refresh reused texture bei gleicher Größe, sonst region capture, dann window fallback. Snapshot besitzt Texture/SRV-ComPtrs, Placement/PID/Timestamp; Map ersetzt alte Referenzen. Capturefehler hinterlässt alten Cache. |
| A75 | `PruneSnapshots()` (`:2904`) | Vor Minimize-Capture, vor/nach Pre-Snapshot | `SnapshotCache::Prune` | Entfernt beide Maps bei invalidem HWND, PID 0 oder PID-Mismatch (HWND-Reuse). Begrenzt nur Pre-Minimize-Map auf 4 anhand ältestem Timestamp; Map-Erase gibt ComPtr-Refs frei. Restore-Map heute unbounded bis Restore/Cleanup. |
| A76 | `RestoreWindowFromGenieState(HWND,bool)` (`:2938`) | Recovery, Run-Completion/Watchdog/Disable, Minimize/Restore-Fehler, Shutdown, Heal | `features/recovery/window_recovery_service.*`; innerhalb aktiver Fälle Restore-/Minimize-Transaction | Geliehenes HWND; invalid no-op. Setzt Reentrancy-Bool, entfernt WindowRegion, wählt Maximize/Rect aus Restore→Precache→Properties, uncloakt, restauriert Transparenz, clears Props; zeigt iconic nur wenn `force_show_if_iconic`, nicht-iconic immer Restore/Maximize mit transientem Allow. Aktuell gespeichertes Restore-Rect wird nur ermittelt, aber nicht via `SetWindowPlacement` angewandt; Ziel muss **aktuelles sichtbares Verhalten** und beabsichtigten Placement-Vertrag bewusst klären. Bool-Guard muss RAII werden. |
| A77 | `CleanupAndRestoreAll()` (`:3010`) | Nur `~Application` (öffentlich deklariert, aber repo-weit kein anderer direkter Call) | Kleines `Application::Shutdown` delegiert reverse-order an Owner; Window-Healing in `WindowRecoveryService` | Funktionsstatischer Atomic macht es prozessweit one-shot; setzt shutdown, unregistert Hotkeys, stoppt Monitor, Hook, Blocker, postet Overlay close; moved Snapshot-Maps; nullt Runs; heilt systemweit per EnumWindows; restauriert tracked Fenster; shutdown UI/Overlays, reset Capture/Device, Timerresolution, weckt Timer, schreibt `clean`. Die Schleifen über die moved Snapshot-Kopien ignorieren jeweils `snapshot`; `RestoreWindowFromGenieState` sieht die nun leeren Membermaps und kann dort nur noch Properties nutzen. **Kritischer Befund:** `HWND animating_copies[2]`, aber `runs_` wächst dynamisch; Schleifen bis `runs_.size()` können bei >2 Runs out-of-bounds lesen/schreiben. Ziel-Owner-Container ohne Fixgröße und ein gemeinsamer idempotenter Cleanup. |
| A78 | `HealLeftoverWindows()` (`:3096`) | `Initialize`, UI-Heal-Lambda, Repair-Hotkey, Diagnostics Repair | `features/recovery/window_recovery_service.*` | EnumWindows synchron; entfernt Excluded-Property; bei irgendeinem Genie-State ruft Restore mit `force_show_if_iconic=false`; zählt Reparaturen und veröffentlicht Diagnostics-Status. Keine Fenster besitzen. Muss startup-/manuell sicher wiederholbar sein und UIPI-Fehler als best effort behandeln. |

## Heutige Aufruf- und Lebensdauerpfade

### Prozessstart, Single Instance, COM und Konsole

1. CRT ruft `wmain`; Release hat Windows-Subsystem, Debug Console-Subsystem.
2. DPI wird prozessweit auf `PER_MONITOR_AWARE_V2` gesetzt. Ein Fehler ändert den weiteren Pfad
   heute nicht.
3. `CreateMutexW(nullptr, TRUE, "Local\\GenieEffect.Windows.SingleInstance")`:
   - null + `ERROR_ACCESS_DENIED`: als bereits laufende, ggf. höher integrierte Instanz behandeln,
     UI-Aktivierung versuchen, Exit 0;
   - null + anderer Fehler: Fehlerausgabe, Exit 1;
   - `ERROR_ALREADY_EXISTS`: eigenes Handle schließen, Aktivierung versuchen, Exit 0;
   - neu: Handle bis Prozessende behalten. Es gibt kein `ReleaseMutex`; `CloseHandle` gibt Besitz
     beim Exit frei.
4. `CoInitializeEx(..., COINIT_APARTMENTTHREADED)` muss erfolgreich sein. Jedes `FAILED(hr)`,
   einschließlich eines möglichen Apartment-Konflikts, ist fatal. Nur erfolgreicher Init wird
   uninitialisiert.
5. Stack-`Application` wird konstruiert, globaler Borrow gesetzt, Console Handler registriert.
6. `Initialize(GetModuleHandleW(nullptr))`; Fehler führt zu Pointer-null, Handler-ab, COM-uninit,
   Mutex-close und Exit 1. Der anschließende Stackdestruktor führt dennoch Application-Cleanup aus.
7. `Run`; danach werden Pointer/Handler/COM/Mutex abgebaut, dann zerstört der Scope `Application`.

Ziel: `ProcessBootstrap` besitzt bzw. kapselt DPI-Ergebnis, `SingleInstanceGuard`, COM-Apartment und
Console-Handler. `wmain` sieht nur Bootstrap-Resultat, `Application` und Exitcode. Die Aktivierung
einer bestehenden Instanz ist kein transitive-include-Zugriff auf `SettingsWindow`.

### Application-Initialisierung

Die aktuelle Reihenfolge ist:

1. Log-Retention, `instance_`, Main-Thread-ID, Settings laden, Safe Mode hart auf `false`;
2. Enabled/Durations spiegeln;
3. aktiviertes Startup-Setting gegen Registry „reparieren“; bei Fehler Setting deaktivieren und
   best effort speichern;
4. Debug-Logfile öffnen/schließen und AppContainer-ACL vergeben;
5. High-Resolution-Waitable-Timer versuchen, sonst normalen Timer;
6. übrig gebliebene Fenster heilen;
7. Elevation prüfen und UIPI-Warnung ausgeben;
8. Animation-Renderer bauen, außer Safe Mode;
9. SettingsWindow samt 22 Callbacks initialisieren und View-State setzen;
10. Hotkeys registrieren, Pause-UI setzen, ggf. Diagnostics anzeigen, Window/Tray-Startzustand;
11. Fullscreen/Power/Runtime-Policy initial auswerten;
12. WindowEventMonitor starten, außer Safe Mode;
13. `session_started_=true`, `running` oder `safe` atomar schreiben.

Mehrere Fehler zwischen 8 und 12 geben nur `false` zurück. Ressourcen werden nicht lokal
transaktional zurückgedreht, sondern erst durch den späteren Destruktor. Das Ziel braucht einen
expliziten Initialisierungs-Stack: jeder erfolgreiche Owner wird registriert; Fehler bei N fährt
N-1..1 sofort herunter. Callbacks dürfen erst nach vollständiger Konstruktion ihrer Ziele aktiv
werden.

### Message Loop und Runtime

`Run` vermischt heute:

- Message-Pump und Stop-Wakeup,
- 500-ms-Fullscreen- und 5-s-Power-Poll,
- Pause-Ablauf und Watchdog,
- Settings-UI-Render,
- Debug-Device-Lost-Injektion,
- Renderer-Recovery/Backoff,
- Pending-Native-Minimize,
- Live-Texture-Refresh,
- Overlay-Tick und Abschluss,
- Pre-Minimize-Snapshot-Poll,
- Surprise-Restore-Poll,
- Frame-Pacing/Timerresolution.

Ziel: `MessageLoop` pumpt Messages und fragt nur eine kleine Update-/Wait-Schnittstelle ab.
`EffectController` routet Events; `AnimationRunPool`, `FrameScheduler`, `RendererRecovery`,
`SnapshotCache` und Features besitzen ihre Mechanik. `WM_DISPLAYCHANGE` wird als Plattformevent an
FrameScheduler/Rendering delegiert.

### Normaler und angeforderter Shutdown

- UI/Console rufen nur `RequestShutdown`: Atomic setzen, Timer wecken, `WM_QUIT` an Queue/Main
  posten.
- `Run` verlässt seine Wait-/Loop-Phase.
- Erst der Destruktor ruft `CleanupAndRestoreAll`.
- Aktuelle Shutdown-Reihenfolge: Hotkeys ab → Eventmonitor stop → CBT Hook ab → Native Blocker aus
  → Overlay-Close posten → Snapshotmaps aus dem sichtbaren Zustand moven/Slots nullen →
  systemweite Property-Heilung → tracked Fenster restaurieren → Overlay/UI shutdown →
  Capture/Device reset → Timerresolution end → Waitable Timer wecken → Session `clean`.
- Danach schließt der Destruktor das Timer-Handle.

Ziel: Eine instanzbezogene, mehrfach aufrufbare `Shutdown()`-Methode. Features/Runs stoppen und
Window-Transactions restaurieren zuerst; Eventquellen/Callbacks dürfen danach nichts mehr
anstoßen; Low-Level-Renderer und Plattformhandles werden in sicherer umgekehrter
Konstruktionsreihenfolge zerstört. Der heutige statische one-shot und das feste Zwei-HWND-Array sind
nicht übertragbar.

### Abbruch, Timeout und Device Lost

- Minimize-Capture-/Overlay-/ShowWindow-Fehler haben mehrere lokal duplizierte Rollbacks.
- Pending-Minimize hat eine eigene Rollback-Lambda.
- 800-ms-Warten auf Native-Minimize im Loop hat einen weiteren Inline-Cleanup.
- State-Watchdog hat wieder einen eigenen Cleanup.
- Device Lost shutdownt alle Overlays, restauriert jedes interrupted HWND, minimiert
  unterbrochene Minimize-Ziele anschließend nativ, leert alle Snapshots/Devices und probiert mit
  250–4000-ms-Backoff neu.
- Normaler Erfolg und erzwungener Abschluss verwenden weitere Pfade.

Ziel: jeder `AnimationRun`/jede Transaction führt Normalabschluss, Reverse, Cancel, Timeout,
zerstörtes HWND, Device Lost und Shutdown in denselben idempotenten Cleanup. Eine Transition
protokolliert alt/neu und lehnt illegale Kanten ab. Renderer-Recovery darf nur Rendering besitzen;
die Entscheidung, wie ein reales Fenster nach Interrupted-Minimize/-Restore endet, gehört in die
Feature-Transaction.

## Bewusst zu erhaltende Sonderfälle

| Sonderfall | Aktueller Befund | Migrationsanforderung |
| --- | --- | --- |
| UIPI / erhöhte Prozesse | Nicht erhöhte App warnt, dass erhöhte Fenster nicht global gehookt werden. Hook/ACL und `ChangeWindowMessageFilterEx` adressieren AppContainer/Integrity-Grenzen best effort. `ERROR_ACCESS_DENIED` beim Mutex gilt als zweite Instanz. | Keine falsche Erfolgsgarantie. `ProcessInfo::IsElevated`, Hook-Fehler und Window-Recovery bleiben best effort/logged. Aktivierungsmessage und Filter erhalten. |
| Mehrere Monitore | Extended bounds werden auf Virtual Screen geclippt; maximized nutzt nearest Monitor work area; TaskbarTarget ist per Fenster/Monitor; Frame-Pacing wählt Monitor und invalidiert bei `WM_DISPLAYCHANGE`; Diagnostics zählt Monitore. | `WindowState`, `TaskbarTargetProvider`, `FrameScheduler` und Capture behalten negative/monitorübergreifende Koordinaten und Monitorwechsel. |
| Device Lost | Overlay/Capture/Device werden gemeinsam erkannt; Recovery räumt Runs/Fenster, baut Ressourcen neu und backt exponentiell ab. Debug-Flag injiziert genau einmal. | `RendererRecovery` kapselt Ressourcen; Transactions kapseln reales Fenster; keine Endlosschleife/Leak und UI-Renderer-Recovery bleibt separat. |
| Taskbar-/Explorer-Neustart | Application selbst behandelt ihn nicht. Angrenzend registriert SettingsWindow `TaskbarCreated`, setzt `tray_icon_added_=false` und fügt das Icon bei verstecktem Fenster erneut hinzu. TaskbarTargetProvider ermittelt Shell-Taskbar bei jedem Target neu. | Tray-Neustart nach `ui/tray/TrayIcon`; TargetProvider darf keine stale Shell-HWNDs cachen. |
| Single Instance | Local-Mutex; `ALREADY_EXISTS` und `ACCESS_DENIED` aktivieren bestehende UI und der zweite Prozess endet 0. Aktivierung pollt 5 s und ist hang-begrenzt. | Mutexname, Session-Scope, Exitcodes, 5-s-Retry und UIPI-freigegebene Aktivierungsmessage erhalten. |
| Safe Mode | `ReadSessionState` existiert, wird aber **nirgendwo aufgerufen**. `Initialize` setzt `safe_mode_=false`; damit sind `safe`-Start, Diagnostics-Autoseite und `ExitSafeMode` im aktuellen Stand nicht erreichbar. Session wird dennoch `running`/`clean` geschrieben. | Nicht stillschweigend als funktionierend behaupten. In `SessionStateStore` das beabsichtigte Verhalten anhand Ist-Code/Docs rekonstruieren: unclean `running` erkennen, Safe Mode bewusst betreten, `safe`/`running`/`clean` atomar schreiben; Renderer/Monitor/Hotkeys erst beim Exit aktivieren. |
| Eingebettete Hook-DLL | Nur Release-RCDATA; Debug/Sonderfall fällt auf Sibling zurück. Hash-Cache, bytegenaue Prüfung, PID-Temp, atomarer Move, ACL, drei Export-Fallbacks und WH_CBT global. | Dateiname `GenieHookPost.dll`, Resource-ID 205, Export `CBTProc`, Hook-ABI und Extraktionslogik unverändert. |
| Erzwungener Prozessabbruch / Console Close | Console Handler fordert nur Shutdown an. Windows kann bei Logoff/Shutdown Zeit begrenzen; harter Crash/TerminateProcess kann Cleanup vollständig überspringen. | Control-Thread niemals D3D/Window-Cleanup ausführen. Persistente Properties plus nächster Startup-Heal sind Recovery-Netz; keine Garantie bei UIPI-geschützten Fremdfenstern erfinden. |
| Übrig gebliebene unsichtbare Fenster | Startup-Heal enumeriert alle Fenster, erkennt Property-Marker, uncloakt, entfernt leere Region, restauriert Layered-Zustand und Properties. Mit `force_show_if_iconic=false` wird ein legitimes iconic Fenster nicht aufgezwungen. | `WindowRecoveryService` muss wiederholbar sein, alle Property-Namen verstehen und Startup/Diagnostics/Hotkey bedienen. Placement/Transparenz/Region als eine Recovery-Transaktion behandeln. |
| Rasches Minimize↔Restore | Aktiver Run wird weiter Richtung Taskbar oder per `ReverseAnimation` umgedreht; Slot wird vor Window-Calls teilweise entkoppelt, um Reentrancy zu vermeiden. | Explizite erlaubte State-Transitions und Transaction-Ownership; keine stale Pending-HWNDs. |
| Mehrere gleichzeitige Runs | Pool wächst dynamisch; Snapshots teilen COM-Refs. Cleanup hat fälschlich feste Kapazität 2. | `AnimationRunPool` besitzt beliebig viele klar identifizierte Runs; Cleanup iteriert sicheren Container; Snapshot-Limit/PID-Abgleich testen. |
| Fenster-/HWND-Reuse | Snapshot speichert PID; Prune verlangt gültiges HWND und gleiche, nicht-null PID. Andere geliehene HWND-Felder prüfen teils nur `IsWindow`. | Stable ID/PID-Vertrag für Runs/Snapshots; zerstörtes oder wiederverwendetes HWND darf keinen alten Run erben. |
| Maximized/Placement | Snapshot/Properties merken Normalrect und WasMaximized. Restore berechnet `restore_rect`, wendet es heute aber nicht explizit per Placement-API an; `ShowWindow` übernimmt aktuellen OS-Zustand. | Sichtbares Ist-Verhalten nicht versehentlich ändern, aber Transaction muss Placement-Vertrag explizit dokumentieren/testen; keine temporären Topmost-/Layered-Bits zurückspielen. |

## Ziel-Aufrufgrenzen

Nach der Migration darf der direkte Kontrollfluss aus diesem Scope nur noch ungefähr so aussehen:

1. `wmain` → `ProcessBootstrap::Initialize` → kleine `Application(instance)`.
2. `Application::Initialize` konstruiert Logger/Settings/Windows-Adapter/Rendering/Runtime/Features/UI
   und zuletzt `MessageLoop`; jedes Objekt besitzt seine Ressourcen.
3. `WindowEventMonitor` und `CbtHookManager` liefern schmale Events an `EffectController`.
4. `EffectController` fragt `EffectPolicy` und delegiert an `MinimizeFeature` oder
   `RestoreFeature`.
5. Features verwenden Transactions, `SnapshotCache`, `AnimationRunPool`, Rendering und
   `WindowState/WindowProperties`, ohne Rückruf in `Application`.
6. UI spricht nur `SettingsActions`/ViewModel; Settingsänderungen laufen über `SettingsService`.
7. `MessageLoop` ruft Controller/Runtime-Updates und Wait-Scheduling, enthält aber keine
   Feature-Implementierung.
8. `Application::Shutdown` stoppt in umgekehrter Reihenfolge und kann gefahrlos erneut laufen.

## Phase-0-Gate für diesen Dateiscope

- [x] Alle neun zugewiesenen First-Party-Dateien vollständig gelesen.
- [x] Repo-weite Include-/Caller-Suche für diese Dateien und Symbole durchgeführt.
- [x] 120 benannte/explizite Funktionen genau einmal einer Zielverantwortung zugeordnet.
- [x] Klassen, Enum, Structs, Alias, statischer Zustand und Owned/Borrowed Resources erfasst.
- [x] Sämtliche Window-Property-Namen mit Hook-/Overlay-Cross-References erfasst.
- [x] Alle Resource-IDs, Aliase und RC-/UI-/Hook-Nutzer erfasst.
- [x] Environment-/Registry-/Dateinamen und Debug-Testhook erfasst.
- [x] Hook-Resource, Cache, ABI, ACL, Install/Unload erfasst.
- [x] Single-Instance, DPI, COM, Console, Message Loop, Shutdown und Sessionpfade erfasst.
- [x] Recovery für Timeout, Device Lost, Startup-Heal und aktive Fenster erfasst.
- [x] Dead-Code-Kandidaten bewusst als Entfernen statt Scheinmigration markiert.
- [x] Keine Implementierung, kein Build, kein Test, kein Start, keine Formatierung durchgeführt.

Offene Punkte sind damit keine Inventarlücken, sondern explizite Implementierungsentscheidungen für
spätere Phasen: Safe Mode ist heute unverdrahtet; Initialisierung ist nicht transaktional;
Property-Schreibfehler haben unvollständigen Rollback; Restore verwendet das ermittelte Rect nicht
direkt; globaler Cleanup ist cross-instance und bei mehr als zwei Runs speicherunsicher. Diese
Befunde müssen beim Umbau in den jeweils benannten Ownern gelöst und erst nach dem vollständigen
Produktionsumbau getestet werden.
