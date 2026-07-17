# Verbindliches Phase-0-Inventar: Settings-UI, Theme und Motion

Stand: 2026-07-17. Diese Datei ist ein reines statisches Migrationsprotokoll. Es wurden keine
Produktionsdateien geändert, keine Formatierung angewandt, keine Anwendung gestartet und keine
Builds oder Tests ausgeführt.

## Gelesener Umfang und Beweislage

Vollständig gelesen:

- `docs/REFACTORING_MASTERPLAN.md`
- `app/src/app/settings_window.cpp/.hpp`
- `app/src/app/settings_ui_widgets.cpp/.hpp`
- `app/src/app/settings_ui_theme.cpp/.hpp`
- `app/src/menu/theme.hpp`
- `app/src/menu/motion/motion.cpp/.hpp`
- `app/src/menu/motion/motion_context.hpp`
- `app/src/menu/motion/motion_tokens.hpp`

Zusätzlich wurden repositoryweit alle Includes und Symbolverwendungen dieser Dateien gelesen. Die
wesentlichen externen Aufrufer liegen in `app/src/main.cpp`,
`app/src/app/application.cpp/.hpp`, die Resource-IDs in `app/src/app/resource.hpp` und
`app/GenieEffect.rc`, und die Projekteingänge in `app/GenieEffect.vcxproj`.

### Schutzprüfung für `settings_window.hpp`

`app/src/app/settings_window.hpp` wurde nur gelesen. Vor der Inventur wurden Arbeitsbaum, Index und
`HEAD` separat verglichen. Zu diesem Zeitpunkt gab es weder einen unstaged noch einen staged Diff;
Arbeitsdatei und `HEAD` hatten beide den Blob-Hash
`ddfee0be4ab583401127d5ff899365ae15a782e7`. Damit existiert aktuell kein Git-Hunk, der in diesem
Bericht nachgebildet werden könnte. Der gesamte heutige Headerinhalt wurde trotzdem als
schützenswerter Benutzerbestand behandelt und unten vollständig einem Zielbesitzer zugeordnet.
Unmittelbar vor Phase 7 ist diese Prüfung zu wiederholen, damit später hinzugekommene Änderungen
nicht überschrieben werden.

## Verbindliches Zielbild für diesen Bereich

| Ziel | Einzige Verantwortung | Besitz und Abhängigkeit |
| --- | --- | --- |
| `ui/settings_window.cpp/.hpp` | Haupt-HWND erzeugen/zerstören, Sichtbarkeit, Win32-Nachrichten routen, Fensterdragging und DPI-bedingte äußere Fenstergröße | Besitzt nur das Haupt-HWND und fensterspezifischen Win32-Zustand; hält schmale, nicht-besitzende Referenzen auf Controller/Renderer/Tray. |
| `ui/rendering/imgui_renderer.cpp/.hpp` | ImGui-Kontext, Win32-/DX11-Backends, D3D11-Gerät/Context/Swapchain/RTV, Frame begin/end, Resize, Fonts, Style-Neuanwendung und Device Recovery | Alle COM-Objekte via `ComPtr`; Shutdown idempotent; HWND nur geliehen. |
| `ui/tray/tray_icon.cpp/.hpp` | `Shell_NotifyIcon`, Tooltip, Kontextmenü, Retry-Timer und `TaskbarCreated` | Besitzt die Shell-Registrierung und den Timerzustand; Haupt-HWND und Actions nur geliehen. |
| `ui/preview/animation_preview.cpp/.hpp` | Preview-HWND, Preview-WndProc, Phasensteuerung und Dragging | Besitzt Preview-HWND; zerstört es bei Ende/Shutdown; kurzlebige GDI-Objekte werden im Paint-Pfad freigegeben. |
| `ui/settings_controller.cpp/.hpp` | Shell-/Navigationsorchestrierung, View-Model aktualisieren, Aktionen typisieren, Save-Feedback und Page-Aufruf | Besitzt UI-Präsentationszustand und `MotionSystem`; hält `SettingsActions&`, aber keine zweite fachliche Settings-Wahrheit. |
| `ui/settings_view_model.hpp` | Autoritativer, read-only UI-Snapshot plus klarer transienter Page-Zustand | Wird vom Controller erneuert; persistierte Werte stammen aus `SettingsService`. |
| `ui/settings_actions.hpp` | Einzige UI→Anwendung-Grenze (`SetEnabled`, typisierte Settings-Changes, Hotkeys, Pause, Diagnostics, Exit und Active-App-Snapshot) | Keine direkte `Application`-Abhängigkeit in UI-Dateien. |
| `ui/pages/general_page.*` | Effect-Hauptschalter, Close-Verhalten und Startup-Optionen | Nur Darstellung und typisierte Events. |
| `ui/pages/motion_page.*` | Dauer, Link, Presets, Easing/Bezier, Style, Qualität, Stärke, Fade, Target Indicator und Preview-Start | Transienter Drag-/Dirty-Zustand lokal; Persistenz nur über Controller/Actions. |
| `ui/pages/applications_page.*` | Filter und Exclusion-Liste darstellen | Aktive Apps kommen aus dem View Model; keine Win32-Prozessenumeration in der Page. |
| `ui/pages/windows_page.*` | Fullscreen- und Battery-Saver-Optionen | Nur Darstellung und typisierte Events. |
| `ui/pages/hotkeys_page.*` | Hotkey-Anzeige, Capture-UI und Feedback | Registrierung/Lebensdauer bleibt im `GlobalHotkeyManager`. |
| `ui/pages/diagnostics_page.*` | Diagnose-Snapshot anzeigen und Diagnoseaktionen auslösen | Keine Renderer-/Clipboard-/Shell-Implementierung in der Page. |
| `ui/pages/about_page.*` | Produktversion und Font-Lizenz anzeigen | Lizenztext als seitenbezogene Resource; keine allgemeinen UI-Ressourcenhelfer. |
| `ui/components/combo.*` | Combo einschließlich Öffnen/Schließen und zustandsgebundener Animation | Zustand an ImGui-Kontext/Instanz binden, keine process-globalen Maps. |
| `ui/components/controls.*` | Toggle, Slider, CompactButton, SegmentSelector, Tooltip, Traffic Lights und Sidebar Item | Wiederverwendbare Darstellung, keine Settings-Persistenz. |
| `ui/components/easing_editor.*` | Cubic-Bezier-Graph und vier Werteingaben | Mutiert nur den übergebenen Arbeitswert; Commit entscheidet die Motion-Page. |
| `ui/components/page_layout.*` | Page-Flow, Gruppen, Zeilen, Karten, Separatoren, Scroll-Fades/-Thumb | Keine Theme-Tokens definieren und keine Settings kennen. |
| `ui/theme/theme.cpp/.hpp` | ImGui-Style und kleine Far-/Typografieoperationen | Liest ausschließlich `theme_tokens`. |
| `ui/theme/theme_tokens.hpp` | Farben, Typografiegrößen und Layout-Tokens | Konstante Daten; keine Zeichenlogik, keine veränderlichen Globals. |
| `ui/motion/*` | Motion-Werttypen, Tracks, Easing und Tokens | `MotionSystem` wird vom `SettingsController` besessen und als `MotionContext` geliehen. |
| `platform/windows/single_instance_guard.*` | Bestehende Instanz aktivieren | `main.cpp` kennt `SettingsWindow` nicht mehr. |
| `platform/windows/application_list_provider.*` | Top-Level-Fenster/Prozesse zu normalisierten App-Namen abbilden | Ergebnis gelangt über Actions/ViewModel zur UI; UI inkludiert keinen Plattformadapter. |

## Include- und Aufruferoberfläche

| Alte Datei | Direkte First-Party-Includes/Aufrufer | Migrationsfolge |
| --- | --- | --- |
| `app/settings_window.hpp` | `app/application.hpp`; der statische Aktivierungseinstieg wird zusätzlich aus `main.cpp` verwendet | `Application` besitzt künftig `SettingsController`; `main` verwendet `SingleInstanceGuard`. |
| `app/settings_window.cpp` | inkludiert Store, Theme, Widgets, Motion-Globals, altes Theme, Debug-Log, Window-Util und beide ImGui-Backends | Nach der Trennung darf `ui/settings_window.cpp` weder D3D/ImGui-Backends noch Settings-Store oder Pages inkludieren. |
| `app/settings_ui_widgets.hpp` | nur `settings_window.cpp`; inkludiert Theme und dadurch Motion/ImGui | Aufrufer werden auf konkrete Component-Header verteilt. |
| `app/settings_ui_theme.hpp` | Widgets, Theme-Implementierung und SettingsWindow | `theme_tokens`, `theme`, `page_layout` und `controls` werden gezielt inkludiert. |
| `menu/theme.hpp` | nur die drei heutigen UI-CPPs; die Icon-Externs haben repositoryweit keine Definition und keinen Aufrufer | Palette nach `ui/theme/theme_tokens.hpp`; tote Icon-Deklarationen entfernen. |
| `menu/motion/motion.hpp` | Motion-CPP, Tokens und Context | echter Pfad `ui/motion/motion.hpp`; öffentliches Naming an Google-Stil anpassen. |
| `menu/motion/motion_tokens.hpp` | Motion-Context | echter Pfad `ui/motion/motion_tokens.hpp`. |
| `menu/motion/motion_context.hpp` | Theme-Header, Widgets und SettingsWindow | globale Fassade entfernen; `MotionContext` explizit vom Controller an Views/Components geben. |

`app/GenieEffect.vcxproj` führt aktuell alle elf alten Produktionsdateien auf
(`settings_window`, `settings_ui_widgets`, `settings_ui_theme`, `menu/theme` und vier Motion-Dateien).
Beim Umzug sind diese Einträge atomar durch die echten neuen Pfade zu ersetzen. Ein
`GenieEffect.vcxproj.filters` existiert im aktuellen Arbeitsbaum nicht.

`motion.cpp` inkludiert als einzige Datei dieses Inventurbereichs `pch.h`. Dieser Header ist ein
bewusster First-Party-Kompatibilitätsshim für den ImGui-Fork, inkludiert `pch.hpp`,
`imgui_internal.h` und aktiviert ImGui-Math-Operatoren. Der neue Motion-Code soll `pch.hpp` und nur
die tatsächlich benötigten Header inkludieren; seine heutigen `ImClamp`/`ImMax`-Verwendungen können
auf `std::clamp`/`std::max` wechseln. Der `pch.h`-Shim darf nicht versehentlich als öffentliche
Motion-Abhängigkeit weiterwandern.

### Header-Hygiene beim Umzug

| Alter Headerzug | Problem | Ziel |
| --- | --- | --- |
| `application.hpp → settings_window.hpp → d3d11.h/dxgi.h/wrl + settings_store.hpp` | D3D- und vollständiges Settings-Modell gelangen bis zur Kompositionswurzel | `Application` deklariert `ui::SettingsController` vor und besitzt ihn per `unique_ptr`; D3D bleibt privat im Renderer, View-Daten in `settings_view_model.hpp`. |
| `settings_window.hpp → functional` | nur für 22 Einzelcallbacks | entfällt mit `SettingsActions&`. |
| `settings_ui_widgets.hpp → settings_ui_theme.hpp → motion_context.hpp` | jede Komponente zieht globales Motion und komplettes Theme | konkrete Component-Header nehmen einen vorwärtsdeklarierbaren `MotionContext`/schmale ImGui-Werttypen; kein globaler Context-Header. |
| `settings_ui_theme.hpp → cmath + imgui.h + motion_context.hpp` | Tokens, Layout und Zeichenlogik sind gekoppelt | `theme_tokens` enthält nur Token-Typen; `theme.cpp`/`page_layout.cpp` inkludieren ImGui/Math-Implementierungsdetails. |
| `motion_tokens.hpp → motion.hpp → imgui.h` | akzeptabel nur für UI-Motion, darf aber nicht in Settings/Core gelangen | Abhängigkeit bleibt unter `ui/motion`; keine UI-Motion-Typen in fachlichen Settings-Headern. |
| `settings_window.cpp → settings_store/window_util/debug_log/resource` | Monolith greift direkt auf Fach-, Plattform-, Logging- und Resource-Details zu | Pages sehen ViewModel/Actions; Active-App-Provider und Single-Instance sind Plattformmodule; Renderer/About besitzen ihre konkreten Resources. |

## Kanonisches Funktions- und Typenprotokoll: `settings_window.*`

Jeder alte, explizit deklarierte oder definierte Funktionskörper steht in den folgenden Tabellen
genau einmal. Interne Teilblöcke von `RenderContents` werden anschließend separat nach Seiten
zugeordnet, gelten aber nicht als zusätzliche Top-Level-Funktionen.

### Typen und Callback-Verträge

| Quelle | Alter Typ/Zustand | Heutige Verwendung | Verbindliches Ziel |
| --- | --- | --- | --- |
| HPP 16–21 | `TemporaryPauseAction` | Tray-Menü → `Application::SetTemporaryPause` | `ui/settings_actions.hpp`; Werte auf `kResume`, `kTenMinutes`, `kOneHour`, `kUntilRestart` erhalten. Ausführung durch `PauseController`. |
| HPP 23–29 | `HotkeyUpdateResult` | Hotkey-Page und `Application::SetHotkey` | `ui/settings_actions.hpp`; alle fünf Ergebnisse erhalten. |
| HPP 31–37 | `DiagnosticsAction` | Diagnostics-Page und Application | `ui/settings_actions.hpp`; fachliche Ausführung in `DiagnosticsService`. |
| HPP 39–56 | `DiagnosticsSnapshot` | Page-Cache, About-Version und Application-Erzeugung | kanonisch `features/diagnostics/diagnostics_snapshot.hpp`; als Feld im `SettingsViewModel` exponieren. |
| HPP 58–258 | `SettingsWindow` | von `Application` direkt besessener Monolith | kleine `ui::SettingsWindow`; Controller besitzt/verdrahtet Renderer, Tray, Preview und Pages. |
| HPP 119–127 | `SettingsWindow::Page` | Navigation und Seitenwahl | `SettingsPage` in `ui/settings_view_model.hpp`; Controller ist Besitzer der Auswahl. |
| CPP 283–286 | `EmbeddedResource` | Fontbinärdaten und Lizenztext | alten generischen Typ entfernen: private `EmbeddedFontResource` im `ImGuiRenderer`, seitenlokaler Text-Resource-View in `AboutPage`. |
| CPP 1000–1004 | lokaler `PageEntry` | statische Navigation | private Navigationseintrag-Tabelle des `SettingsController`. |
| CPP 1556–1560 | lokaler `AppItem` | Exclusion-/Active-App-Merge | `ApplicationItemViewModel` in `settings_view_model.hpp`. |
| HPP 60 | `ToggleCallback` | `SetEnabled` | `SettingsActions::SetEnabled(bool)`. |
| HPP 61–62 | `SpeedCallback` | Live-Vorschau und Commit der beiden Dauern | Controller-Preview eines `AnimationDurationsChange`, Commit über `SettingsActions::ApplySettings`. |
| HPP 63 | `LinkCallback` | Link durations | typisierter `LinkDurationsChange` über `ApplySettings`. |
| HPP 64 | `FullscreenBehaviorCallback` | Pause in Fullscreen | typisierter `FullscreenBehaviorChange`. |
| HPP 65 | `BatterySaverCallback` | Battery Saver | typisierter `BatterySaverBehaviorChange`. |
| HPP 66 | `EasingCallback` | beide Easing-Namen | typisierter `EasingChange`. |
| HPP 67–68 | `CustomBezierCallback` | Preview/Commit je Richtung | `CustomBezierChange` mit expliziter Preview-/Commit-Grenze. |
| HPP 69 | `AnimationStyleCallback` | Mesh-Stil | typisierter `AnimationStyleChange`. |
| HPP 70 | `QualityModeCallback` | Qualitätsmodus | typisierter `QualityModeChange`. |
| HPP 71 | `StrengthCallback` | Live-Vorschau/Commit | `GenieStrengthChange` mit Preview-/Commit-Grenze. |
| HPP 72 | `FadeCallback` | Fade-Stärke | typisierter `FadeStrengthChange`. |
| HPP 73 | `TargetIndicatorCallback` | Target Indicator | typisierter `TargetIndicatorChange`. |
| HPP 74 | `CloseBehaviorCallback` | Exit/Tray | typisierter `CloseBehaviorChange`. |
| HPP 75 | `StartupCallback` | Run-at-login und Start-minimized | typisierter `StartupOptionsChange`; Anwendung orchestriert `SettingsService` und `StartupManager`. |
| HPP 76 | `ExclusionCallback` | App hinzufügen/entfernen | typisierter `ExclusionChange`. |
| HPP 77 | `PauseCallback` | Tray-Pause/Resume | `SettingsActions::SetTemporaryPause`. |
| HPP 78 | `HotkeyUpdateCallback` | Capture/Clear | `SettingsActions::SetHotkey`. |
| HPP 79 | `HotkeyActionCallback` | `WM_HOTKEY` | aus UI entfernen; `GlobalHotkeyManager` dispatcht den Anwendungsfall. |
| HPP 80 | `DiagnosticsCallback` | Snapshot-Polling | `SettingsActions::GetDiagnostics`, Ergebnis ins View Model. |
| HPP 81 | `DiagnosticsActionCallback` | Diagnosebuttons | `SettingsActions::ExecuteDiagnosticsAction`. |
| HPP 82 | `HealCallback` | Tray „Repair Windows“ | separaten Callback entfernen; `DiagnosticsAction::kRepairWindows`. |
| HPP 83 | `ExitCallback` | Close und Tray Exit | `SettingsActions::RequestExit`. |

`DiagnosticsSnapshot` enthält genau `effect`, `hook`, `renderer`, `d3d_device`,
`active_animations`, `watchdog`, `display_refresh`, `window_monitor`, `taskbar`,
`startup_repair`, `version`, `windows_version`, `graphics_adapter`, `monitor_configuration`,
`log_folder_size` und `report`. `SettingsWindow::Page` enthält `kGeneral`, `kAnimation`,
`kApplications`, `kWindowsIntegration`, `kHotkeys`, `kDiagnostics`, `kAbout`; die Zielwerte heißen
inhaltlich General, Motion, Applications, Windows, Hotkeys, Diagnostics und About.
`EmbeddedResource` besitzt nur das nicht-besitzende PE-Resource-Paar `data`/`size`.
`PageEntry` besitzt `page`/`label`/`section_gap`; `AppItem` besitzt
`name`/`is_excluded`/`is_active`. Keiner dieser drei kleinen Werttypen benötigt Cleanup.

### Freie Funktionen und `SettingsWindow`-Methoden

| Quelle | Alter Funktionskörper | Aktuelle Aufrufer | Ziel, Besitz, Cleanup/Recovery |
| --- | --- | --- | --- |
| CPP 28–29 | externe Deklaration `ImGui_ImplWin32_WndProcHandler(...)` | `SettingsWindow::WindowProc` | kein eigener Funktionsbesitz und keine Migration des Third-Party-Codes; neuer `ImGuiRenderer::HandleMessage` kapselt den Aufruf, das kleine Window routet nur. |
| CPP 72–76 | `SystemUiAnimationsEnabled()` | `UpdateReducedMotion` | schmale private Win32-Abfrage in `SettingsWindow`; Ergebnis an `SettingsController::SetReducedMotion`. |
| CPP 78–85 | `SystemTransparencyEnabled()` | **kein Aufrufer** | als toten Code entfernen; nicht in das neue Theme kopieren. |
| CPP 95–120 | `HotkeyText(const HotkeyBinding&)` | Hotkeys-Teil von `RenderContents` | private Formatierung in `hotkeys_page.cpp` oder reiner Settings-Hotkey-Formatter; kein Ressourcenbesitz. |
| CPP 122–136 | `HotkeyResultText(HotkeyUpdateResult)` | Hotkey Clear und Key-Capture in `WindowProc` | private Ergebnisformatierung in `hotkeys_page.cpp`. |
| HPP 85 | `SettingsWindow::SettingsWindow() = default` | Konstruktion als `Application`-Member | expliziter kleiner Konstruktor in `ui/settings_window`; keine implizite Initialisierung fremder Systeme. |
| CPP 140 | `SettingsWindow::~SettingsWindow()` | implizite Zerstörung von `Application::settings_window_` | `ui/settings_window`; ruft idempotentes `Shutdown`, besitzt nur Haupt-HWND. |
| HPP 87 | gelöschter Copy-Konstruktor | Compile-Time-Vertrag | im neuen `SettingsWindow` erhalten. |
| HPP 88 | gelöschter Copy-Assignment-Operator | Compile-Time-Vertrag | im neuen `SettingsWindow` erhalten. |
| CPP 142–154 | `ActivateExistingInstance(DWORD)` | `main.cpp:30,38` | `platform/windows/single_instance_guard.*`; Klassenname/Show-Message als stabiler Aktivierungsvertrag. `SendMessageTimeout` und 50-ms-Polling/Deadline erhalten, aber kein `Sleep` in UI. |
| CPP 156–213 | `Initialize(...)` | `Application::Initialize:700` | aufteilen: `SettingsController::Initialize(SettingsActions&)` komponiert; `SettingsWindow`, `ImGuiRenderer`, `TrayIcon`, `AnimationPreview` initialisieren transaktional. Lange Callbackliste entfällt vollständig. Bei Fehler N werden 1…N-1 rückwärts heruntergefahren. |
| CPP 215–245 | `AddTrayIcon()` | `Show(false)`, Taskbar-Neustart, Retry-Timer | `TrayIcon::Add`; besitzt `NIM_ADD`-Registrierung und Retry-Timer. Scheitert Add beim Verstecken, muss Controller das Fenster wieder sichtbar/fokussierbar machen. |
| CPP 247–259 | `RemoveTrayIcon()` | `Shutdown`, `Show(true)` | `TrayIcon::Remove`; `KillTimer` immer, `NIM_DELETE` nur bei hinzugefügtem Icon, idempotent. |
| CPP 261–281 | `Shutdown()` | Destruktor und `Application::CleanupAndRestoreAll:3074` | Controller-Shutdown in Reihenfolge Preview → Tray → Pages/Motion → Renderer → Hauptfenster; jedes Teil-Shutdown idempotent. |
| CPP 288–299 | `LoadEmbeddedResource(int)` | Font-Rebuild und `LoadEmbeddedText` | alten generischen Körper bewusst zerlegen: Font-RCDATA privat im `ImGuiRenderer`; Lizenz-RCDATA privat im `AboutPage`. Resource-Memory ist modulbesessen und wird nicht freigegeben. |
| CPP 301–306 | `LoadEmbeddedText(int)` | About-Lizenzmodal | `about_page.cpp`; erzeugt einen besitzenden `std::string` aus `IDR_UI_FONT_LICENSE`. |
| CPP 308–384 | `PreviewWindowProc(...)` | von Preview-Fensterklasse | `AnimationPreview::WindowProc`; `GWLP_USERDATA`-Thunk bleibt eng gekapselt. GDI-Brushes/HFONT in jedem `WM_PAINT` löschen, Capture bei Button-up/Cancel lösen. |
| CPP 386–422 | `StartAnimationPreview()` | Motion-Page Preview-Button | `AnimationPreview::Start`; zerstört vorherige Preview, registriert Klasse, zentriert auf Work Area des Settings-Monitors, besitzt neues HWND. |
| CPP 424–450 | `UpdateAnimationPreview()` | pro `Render()` | `AnimationPreview::Update`; ungültiges HWND räumt Zustand auf, Phasen/Delays und aktuelle Minimize-/Restore-Dauern erhalten. |
| CPP 452–460 | `CloseAnimationPreview()` | Start, Update-Ende und Shutdown | `AnimationPreview::Close`; Zustand zuerst nullen, dann gültiges HWND zerstören; idempotent. |
| CPP 462–507 | `Show(bool)` | Startup, Open-Hotkey, Tray/Activation, Close-to-Tray | `SettingsController::Show/Hide` orchestriert; `SettingsWindow` führt Sichtbarkeit/Zentrierung aus, `TrayIcon` Add/Remove. Ein fehlgeschlagenes Tray-Add darf das Fenster nie unzugänglich lassen. |
| CPP 509–551 | `UpdateState(const AppSettings&)` | Initialisierung und alle Application-Settings-Mutatoren | `SettingsController::UpdateViewModel(SettingsSnapshot)`; keine zweite fachliche Wahrheit. Diff triggert Render; Tooltip-Update wird an Tray delegiert. |
| CPP 553–569 | `UpdateTrayTooltip()` | Enabled-/Pause-Änderung | `TrayIcon::UpdateTooltip`; Enabled, temporary pause und until-restart aus ViewModel. |
| CPP 571–577 | `UpdatePauseState(bool,bool)` | Initialisierung, Pause setzen/ablaufen | Controller aktualisiert ViewModel, Tray-Tooltip und Renderanforderung. |
| CPP 579–584 | `SetHotkeyRegistrationStatus(...)` | `Application::RegisterConfiguredHotkeys/SetHotkey` | Ergebnis des `GlobalHotkeyManager` in `SettingsViewModel::hotkey_status`; HotkeysPage neu rendern. |
| CPP 586–589 | `ShowDiagnosticsPage()` | Safe-Mode-Start | `SettingsController::SelectPage(SettingsPage::kDiagnostics)`. |
| CPP 591–612 | `FlushPendingSpeedSave()` | Shutdown, Hide und Page-Wechsel | `MotionPage::CommitPendingEdits`; Controller sendet typed commits für Dauer/Stärke und behält Dirty-State nur bei Fehler. |
| CPP 614–631 | `RecordSaveResult(bool)` | alle UI-Commitpfade | `SettingsController::RecordActionResult`; besitzt Toast-/Persistenzfeedback und setzt Motion-Key neu. |
| CPP 633–651 | `HandleCloseRequest()` | Traffic-Light Close und `WM_CLOSE` | `SettingsController::HandleCloseRequest`; bei `tray` sicher Hide+Tray, sonst `SettingsActions::RequestExit`; HWND-Routing bleibt im Fenster. |
| CPP 653–692 | `CreateRenderWindow(HINSTANCE)` | `Initialize` | `SettingsWindow::Create`; besitzt Haupt-HWND und DWM-Attribute. Per-monitor-DPI-Kontext wiederherstellen; bei Fehler kein fremder Rendererzustand. |
| CPP 694–712 | `CreateDeviceResources()` | Initialize und Recovery | `ImGuiRenderer::CreateDeviceResources`; besitzt D3D-Gerät, Context, Swapchain und anschließend RTV. |
| CPP 714–723 | `CreateRenderTarget()` | Device-Erstellung und Resize | `ImGuiRenderer::CreateRenderTarget`; temporärer Backbuffer via `ComPtr`, RTV nur bei vollständigem Erfolg veröffentlichen. |
| CPP 725 | `CleanupRenderTarget()` | Release und Resize | `ImGuiRenderer::ReleaseRenderTarget`; idempotentes `ComPtr::Reset`. |
| CPP 727–730 | `IsDeviceLostError(HRESULT)` | Present und Resize | private statische `ImGuiRenderer::IsDeviceLostError`; dieselben vier DXGI-Codes erhalten. |
| CPP 732–741 | `ReleaseDeviceResources()` | Shutdown und Recovery-Fehler | `ImGuiRenderer::ReleaseDeviceResources`; OM targets lösen, Context clearen, dann RTV/Swapchain/Context/Device resetten. |
| CPP 743–765 | `TryRecoverDeviceResources()` | Render und `HandleDeviceLost` | `ImGuiRenderer::TryRecover`; 250-ms-Start, exponentiell bis 4000 ms, DX11-Backend erst nach Gerät initialisieren; Fehlschlag räumt Teilressourcen. |
| CPP 767–777 | `HandleDeviceLost()` | Present, Resize und Debug-Injektion | `ImGuiRenderer::HandleDeviceLost`; Backend shutdown → Ressourcen release → pending/backoff setzen → sofortiger erster Versuch. |
| CPP 779 | `ApplyStyle()` | Initialize und DPI-Wechsel | Wrapper entfällt; `ImGuiRenderer` ruft `ui::theme::ApplyStyle(scale)` nach Font-/DPI-Aufbau. |
| CPP 781–857 | `RebuildFonts(UINT)` | Initialize und DPI-Wechsel | `ImGuiRenderer::RebuildFonts`; besitzt Font-Atlas und vier geliehene `ImFont*`. DX11-Objects invalidieren/neu erzeugen; embedded Inter regular/semibold/bold, Assets-Fallback und Default-Fallback erhalten. |
| CPP 859–863 | `ApplyWindowShape(int,int)` | Fenstererzeugung und Resize | kleine `SettingsWindow::ApplyWindowShape`; aktuelles Verhalten entfernt eine Region (`SetWindowRgn(nullptr)`), HWND bleibt Besitzer. |
| CPP 865–869 | `UpdateDpi(UINT)` | `WM_DPICHANGED` | Fenster setzt suggested rect; Renderer aktualisiert Fonts/Style/Scale; gleiche DPI ist No-op. |
| CPP 871–874 | `UpdateReducedMotion()` | Initialize und `WM_SETTINGCHANGE` | `SettingsWindow` fragt OS, Controller aktualisiert seine Motion-Instanz/Tokens; keine globale Fassade. |
| CPP 876–916 | `Render()` | `Application::Run:942`, zusätzlich sofort nach Show | split: Controller aktualisiert Preview/Motion/Pages, `ImGuiRenderer::BeginFrame/EndFrame/Present`. Inaktivitäts-Gating bleibt Controller/Renderer-Vertrag; Device-Lost bleibt Renderer. |
| CPP 918 | `ForceRender()` | State-Updates, Input/WindowProc, Show, Drag | `SettingsController::RequestRender`; atomarer/einthreadiger Präsentationsflag, kein D3D-Besitz. |
| CPP 920–927 | `WantsContinuousRendering() const` | `Application::Run:1099` | `SettingsController::WantsContinuousRendering`; berücksichtigt sichtbares Fenster, Enter-Motion, Preview, Vordergrund, aktive Tracks und Request-Flag. |
| HPP 115 | `hwnd() const` | Hotkey-Registrierung, Overlay-Filter, ShellExecute, Clipboard | `SettingsWindow::hwnd()` bleibt schmaler geliehener Handle-Zugriff; Hotkey-/Diagnostics-Besitzer sollen langfristig eigene geeignete Owner-HWNDs erhalten. |
| CPP 929–2209 | `RenderContents()` | nur `Render()` | vollständig auf Controller/Shell, sieben Pages und Components verteilen; genaue Blockzuordnung unten. Kein entsprechender neuer Monolith. |
| CPP 2211–2485 | `WindowProc(...)` | Win32-Klassenproc des Hauptfensters | kleine `SettingsWindow::WindowProc`; nur Thunk und Routing. Tray, Renderer, Controller und GlobalHotkeyManager erhalten ihre Nachrichten über schmale Methoden. |
| CPP 2487–2503 | `GetActiveApplications()` | Apps-Page alle 2 s | `platform/windows/ApplicationListProvider::GetActiveApplications`; Controller erhält Snapshot über Actions, Page nur ViewModel. |

### Zerlegung von `RenderContents`

| Alter Block | Enthaltenes Verhalten | Exaktes Ziel |
| --- | --- | --- |
| 929–984 | Root-ImGui-Window, Shellflächen, Traffic Lights | `SettingsController`/kleine Shell-View; Traffic Lights nach `components/controls`. |
| 986–1043 | Brand, `PageEntry`, Navigation, Page-Wechsel und Pending-Commit | Controller-Shell; `SettingsPage` im ViewModel. |
| 1045–1074 | Status-Chip Enabled/Paused/Off | Controller-Shell aus ViewModel. |
| 1076–1138 | Page-Child, Tab-Motion, Layoutaufbau, `selected_index` | Controller-Page-Dispatch; Layout nach `components/page_layout`; `selected_index` privat in MotionPage. |
| 1140–1217 | Effect, Close action, Launch at login, Start in tray | `pages/general_page.*`. |
| 1219–1355 | Preview/Reset, Presets, Dauer-Slider, `run_duration_slider`, Link durations | `pages/motion_page.*`; Dirty-/Active-State dort. |
| 1357–1452 | Animation style, Easing-Combos, `combo_row`, `easing_block`, Custom-Bezier Preview/Commit | MotionPage plus `components/combo` und `components/easing_editor`. |
| 1454–1543 | Quality, Strength, Fade, Target indicator | MotionPage. |
| 1545–1691 | Active-App-Refresh, Merge/Sort/Filter, Suche, Exclusion-Toggles und Fehler | `pages/applications_page.*`; Merge-Ergebnis als `ApplicationItemViewModel`; Enumeration außerhalb UI. |
| 1693–1737 | Fullscreen- und Battery-Saver-Toggles | `pages/windows_page.*`. |
| 1739–1797 | Hotkeyliste, Change/Clear und Feedback | `pages/hotkeys_page.*`; Key-Capture-Event vom Controller. |
| 1799–1891 | Diagnose-Polling, `status_row`, Status/Display/Machine/Tools | `pages/diagnostics_page.*`; Snapshot im ViewModel. |
| 1893–2023 | About, Build, Inter/OFL und Lizenzmodal mit statischem Lizenzstring | `pages/about_page.*`; der funktionslokale `static const std::string license` wird Page-Instanzzustand oder unveränderlicher Lazy-Cache mit definierter Context-Lebensdauer. |
| 2025–2135 | Content-Höhe, Fade, eigener Overlay-Scrollbar samt ImGuiStorage | `components/page_layout.*`. |
| 2137–2206 | Save-Toast und Motion-Cleanup | `SettingsController`-Shell/Feedback-Komponente. |

Die lokalen Lambdas `px`, `window_point`, `selected_index`, `run_duration_slider`, `combo_row`,
`easing_block`, die App-Vergleichs-/Lowercase-Lambdas, `status_row` und Scroll-/Toast-Berechnungen
werden nicht als neue freie Helfersammlung übernommen. Sie werden private Methoden des jeweils in
der Tabelle genannten Besitzers oder bleiben kleine lokale Zeichenoperationen.

## Kanonisches Funktionsprotokoll: `settings_ui_widgets.*`

### Statischer Zustand

| Quelle | Zustand | Heutiger Besitzer/Lebensdauer | Ziel |
| --- | --- | --- | --- |
| CPP 19 | `g_slider_value_buffers` | process-global, wächst nach ImGuiID | in `controls::Slider` an ImGuiStorage/Controller-Context binden und beim Context-Shutdown verwerfen. |
| CPP 20 | `g_combo_was_open` | process-global pro Popup-ID | `ComboState` im `combo`-Modul, contextgebunden und beim Session-Ende gelöscht. |
| CPP 21 | `g_combo_closing` | process-global pro Popup-ID | zusammen mit `ComboState`; keine getrennten globalen Maps. |

### Funktionen

| Quelle | Alter Funktionskörper | Aktuelle Aufrufer | Ziel |
| --- | --- | --- | --- |
| CPP 23–25 | `ReferenceMotionKey(...)` | Button, Toggle, Slider und Combo | entfernen; direkt `ui::motion::MotionKey` im jeweiligen Component-Besitzer konstruieren. |
| CPP 27–30 | `MixColor(...)` | Button, Toggle und Slider | `ui/theme::BlendColor` in `theme.*`. |
| CPP 32 | `ControlRounding(float)` | Combo und SegmentSelector | entfernen; `ThemeTokens::kControlRounding * scale` am Component-Aufruf. |
| CPP 34–100 | `ReferenceButton(...)` | nur `CompactButton` | Implementierung direkt nach `components/controls::CompactButton`; Wrapper entfernen. |
| CPP 104–112 | `DelayedTooltip(...)` | Custom-Bezier und Strength | `components/controls::DelayedTooltip`. |
| CPP 114–187 | `Toggle(...)` | General, Motion, Apps und Windows Pages | `components/controls::Toggle`; MotionContext explizit verwenden statt globalem `WindowMotion`. |
| CPP 189–400 | `Slider(...)` | Dauer und Strength | `components/controls::Slider`; numerischer Editorzustand contextgebunden; Rückgabe trennt `active`, `changed`, `committed` statt mehrdeutiges bool. |
| CPP 402–754 | `Combo(...)` | Style/Easing/Fade | `components/combo.*`; nicht-modales Dropdown, upward placement, connected shell, outside-click und Close-Motion erhalten. |
| CPP 756–761 | `CompactButton(...)` | Preview/Reset/Presets, Hotkeys, Diagnostics, About | `components/controls::CompactButton`. |
| CPP 763–846 | `SegmentSelector(...)` | Close behavior und Quality | `components/controls::SegmentSelector`. |
| CPP 848–1087 | `EasingGraphEditor(...)` | MotionPage Custom Easing | `components/easing_editor.*`; Clamp, Shift-Feinschritt, Overshoot-Y-Bereich, Drag- und Input-Commit erhalten. |

Die `Slider`-lokale Parse-Lambda und die `EasingGraphEditor`-Lambdas für Clamp,
Screen-Konvertierung, Hit-Test und Handle-Zeichnung bleiben private Implementierungsdetails ihrer
präzisen Komponente. Sie werden nicht zu allgemeinen Helpers.

## Kanonisches Funktions- und Typenprotokoll: Theme/Layout

### Typen, Tokens und globale Palette

| Quelle | Alter Typ/Zustand | Ziel |
| --- | --- | --- |
| `settings_ui_theme.hpp:10–13` | `settings_ui::MotionContext` | einziges `ui::motion::MotionContext` in `ui/motion/motion_context.hpp`, enthält geliehene System-/Token-Referenzen. |
| HPP 20–76 | `Metrics` mit allen Tokens | `ui/theme/theme_tokens.hpp`; alle Namen auf Google-Stil. Fenster/Shell/Page/Card/Row/Stack/Control-Werte bleiben unverändert. Ungenutzte Tokens erst nach Caller-Audit entfernen. |
| HPP 78–86 | neun `ImU32`-Farbtokens | mit der `colors`-Palette zu einem unveränderlichen `ThemeTokens`-Satz konsolidieren. |
| HPP 120–125 | `TrafficLightAction` | `ui/components/controls.hpp`. |
| HPP 131–225 | `PageLayout` | `ui/components/page_layout.cpp/.hpp`; besitzt nur Frame-lokalen Layoutzustand und geliehene ImGui/Motion-Objekte. |
| `menu/theme.hpp:7–20` | Namespace `colors`: `main`, `panel`, `border`, `panelHeader`, veränderliches `accent`, `text`, `textDim`, `sidebar`, `subNamespaceBg`, `comboBg` | `ui/theme/theme_tokens.hpp`; `accent` wird konstant, Namen `snake_case`/`kPascalCase` gemäß gewählter Tokenform. `subNamespaceBg` ist aktuell unbenutzt und wird entfernt, falls bis Migration kein Caller entsteht. |
| `menu/theme.hpp:23–24` | `g_MenuArrowIcon`, `g_MenuConfigIcon` | repositoryweit weder Definition noch Verwendung: ersatzlos entfernen. |

`TrafficLightAction` enthält `kNone`, `kClose`, `kMinimize`, `kZoom`; alle vier Werte gehen in die
Controls-Schnittstelle über.

Die `Metrics`-Tokens sind vollständig:
`kWindowWidth`, `kWindowHeight`, `kWindowRounding`, `kTitlebarHeight`, `kSidebarWidth`,
`kSidebarMargin`, `kSidebarContentWidth`, `kSidebarBrandY`, `kNavigationY`,
`kNavigationRowHeight`, `kNavigationSpacing`, `kSidebarStatusBottom`, `kPageInset`,
`kCardInnerInset`, `kContentInset`, `kLabelControlGap`, `kRelatedSpacing`, `kGroupSpacing`,
`kScrollGutter`, `kScrollBottomPadding`, `kScrollFadeHeight`, `kMainInset`, `kMainTop`,
`kCardWidth`, `kCardRounding`, `kControlRounding`, `kCardSpacing`, `kRowHeight`,
`kRowHeightTall`, `kRowHeightHero`, `kRowHeightDense`, `kStackPadTop`, `kStackPadBottom`,
`kStackTitleGap`, `kSectionCaptionGap`, `kTitleBlockHeight`, `kToggleWidth`, `kToggleHeight`,
`kButtonHeight`, `kComboHeight`, `kSegmentHeight`, `kSliderHeight` und `kMinLabelWidth`.
Die neun Header-Farbtokens sind `kMainBackground`, `kSidebarBackground`, `kCardBackground`,
`kPanelHeader`, `kActiveItem`, `kText`, `kMutedText`, `kBorder` und `kSeparator`.

`PageLayout` besitzt ausschließlich geliehenen/frame-lokalen Zustand:
`draw_`, `origin_`, `page_width_`, `scale_`, `alpha_`, `local_alpha_`, `motion_y_shift_`, `y_`,
`motion_`, `page_scope_`, `reveal_serial_`, `in_group_`, `group_start_`, `group_row_`,
`row_top_`, `row_height_`, `row_open_`, `stack_row_`, `stack_title_band_`,
`stack_control_band_`, `stack_pad_top_`, `stack_gap_`, `reserved_control_w_`, `dual_line_`,
`dual_title_box_y_`, `dual_title_box_h_`, `dual_sub_box_y_` und `dual_sub_box_h_`. Es besitzt
weder ImGui-Context noch DrawList oder MotionSystem; deshalb gibt es keinen separaten Cleanup,
aber jeder `ChannelsSplit`-Pfad muss vor Funktionsende durch `ChannelsMerge` geschlossen werden.

### Funktionen und alle `PageLayout`-Methoden

| Quelle | Alter Funktionskörper | Aktuelle Aufrufer | Ziel |
| --- | --- | --- | --- |
| HPP 88–92 | `WithAlpha(ImU32,float)` | Settings shell, PageLayout | `ui/theme::WithAlpha`. |
| HPP 94–101 | `Blend(ImU32,ImU32,float)` | **kein Aufrufer** | entfernen; nicht neben `BlendColor` duplizieren. |
| HPP 105–110 | `CenteredTextTop(font,box_y,box_h)` | Shell, Layout, Widgets | `ui/theme::CenteredTextTop`. |
| HPP 112–118 | `CenteredTextTop(font,font_size,box_y,box_h)` | Button | gleiche Theme-Typografieeinheit. |
| HPP 136 | `PageLayout::scale() const` | **kein Aufrufer** | entfernen. |
| HPP 137 | `PageLayout::alpha() const` | PageLayout intern | `components/page_layout`. |
| HPP 138 | `PageLayout::page_width() const` | **kein Aufrufer** | entfernen. |
| HPP 139 | `PageLayout::y() const` | Apps/Hotkeys/Diagnostics Feedback | `PageLayout::Y`. |
| HPP 140 | `PageLayout::content_bottom() const` | Page-Content-Ende | `PageLayout::ContentBottom`. |
| HPP 141 | `PageLayout::content_left() const` | Pages/Layout | `PageLayout::ContentLeft`. |
| HPP 142–144 | `PageLayout::content_right() const` | Pages/Layout | `PageLayout::ContentRight`. |
| HPP 145 | `PageLayout::content_width() const` | Pages/Layout | `PageLayout::ContentWidth`. |
| HPP 146 | `PageLayout::group_left() const` | Layout | `PageLayout::GroupLeft`. |
| HPP 147 | `PageLayout::group_right() const` | Layout/MotionPage Header | `PageLayout::GroupRight`. |
| HPP 149 | `PageLayout::motion_y_shift() const` | **kein Aufrufer** | entfernen. |
| HPP 183 | `PageLayout::RowTop() const` | **kein Aufrufer** | entfernen. |
| HPP 184 | `PageLayout::RowHeight() const` | **kein Aufrufer** | entfernen. |
| HPP 185 | `PageLayout::RowCenterY() const` | **kein Aufrufer** | entfernen. |
| HPP 186 | `PageLayout::IsStackRow() const` | **kein Aufrufer** | entfernen. |
| CPP 14–17 | `Alpha(...)` | Separator und Traffic Lights | mit `WithAlpha` in `ui/theme` vereinigen; alter Doppelpfad entfällt. |
| CPP 19–25 | `Mix(...)` | Traffic Lights | `ui/theme::BlendColor`/`BlendU32`; genau eine Farbmischfunktion pro Repräsentation. |
| CPP 28–42 | `DrawSymmetricX(...)` | Traffic-Light Close | private Funktion in `components/controls.cpp`. |
| CPP 46–95 | `ApplyStyle(float)` | SettingsWindow-Wrapper | `ui/theme::ApplyStyle`; Renderer ruft sie bei Init/DPI auf. |
| CPP 97–103 | `DrawGradientShadow(...)` | **kein Aufrufer** | entfernen. |
| CPP 105–113 | `DrawCard(...)` | `PageLayout::EndGroup` | private Zeichenfunktion in `components/page_layout.cpp`. |
| CPP 115–117 | `DrawSeparator(...)` | `PageLayout::BeginRow` | private Zeichenfunktion in `components/page_layout.cpp`. |
| CPP 119–202 | `DrawTrafficLights(...)` | Shell | `components/controls::DrawTrafficLights`. |
| CPP 204–213 | `PageLayout::PageLayout(...)` | `RenderContents` | `components/page_layout`. |
| CPP 215–228 | `PageLayout::Reveal(...)` | Title, SectionCaption, BeginGroup | private Methode in `components/page_layout`; MotionContext geliehen. |
| CPP 230 | `PageLayout::Gap(...)` | drei Feedbackblöcke | `components/page_layout`. |
| CPP 232 | `PageLayout::AdvanceScaled(...)` | **kein Aufrufer** | entfernen. |
| CPP 234–253 | `PageLayout::Title(...)` | alle sieben Pages | `components/page_layout`. |
| CPP 255–268 | `PageLayout::SectionCaption(...)` | Pages | `components/page_layout`. |
| CPP 270–279 | `PageLayout::BeginGroup()` | Pages und BeginRow | `components/page_layout`; ChannelsSplit wird in EndGroup immer zusammengeführt. |
| CPP 281–293 | `PageLayout::EndGroup()` | Pages/Caption/BeginGroup | `components/page_layout`; offenen Row zuerst schließen. |
| CPP 295–312 | `PageLayout::BeginRow(...)` | Pages/BeginStackRow | `components/page_layout`. |
| CPP 314–329 | `PageLayout::BeginStackRow(...)` | General/Motion/Apps | `components/page_layout`. |
| CPP 331–333 | `PageLayout::ReserveControl(...)` | Pages | `components/page_layout`. |
| CPP 335–342 | `PageLayout::EndRow()` | Pages/Group | `components/page_layout`. |
| CPP 344–350 | `PageLayout::LabelMaxWidth() const` | RowTitle/RowSubtitle | private Methode in `components/page_layout`. |
| CPP 352–354 | `PageLayout::StackControlY() const` | Pages/ControlCursor | `components/page_layout`. |
| CPP 356 | `PageLayout::StackControlHeight() const` | Pages/ControlCursor | `components/page_layout`. |
| CPP 358–370 | `PageLayout::DrawClippedText(...) const` | Title/Row text | private Methode in `components/page_layout`. |
| CPP 372–401 | `PageLayout::RowTitle(...)` | Pages | `components/page_layout`. |
| CPP 403–418 | `PageLayout::RowSubtitle(...)` | Pages | `components/page_layout`. |
| CPP 420–437 | `PageLayout::RowValue(...)` | Diagnostics/About | `components/page_layout`. |
| CPP 439–446 | `PageLayout::ControlCursor(...) const` | Pages | `components/page_layout`. |
| CPP 448–455 | `PageLayout::ControlMaxWidth(...) const` | MotionPage | `components/page_layout`. |
| CPP 457–459 | `PageLayout::ToScreen(...) const` | Layout/Feedback | `components/page_layout`. |
| CPP 461–464 | `PageLayout::SetCursor(...) const` | Pages | `components/page_layout`. |
| CPP 466–508 | `SidebarItem(...)` | Shell-Navigation | `components/controls::SidebarItem`. |

## Kanonisches Funktions- und Typenprotokoll: `menu/motion/*`

Alle Dateien wechseln ohne Weiterleitungsheader nach `app/src/ui/motion`. Dabei werden öffentliche
Methoden PascalCase, Parameter/Member `snake_case` und Enum-Werte `kPascalCase`.

### Typen und Zustand

| Quelle | Alter Typ/Zustand | Ziel/Umbenennung |
| --- | --- | --- |
| `motion.hpp:11–23` | `MotionEasing` mit elf unpräfigierten Werten | gleicher Typ in `ui/motion/motion.hpp`; Werte `kLinear` … `kSpringSnappy`. |
| 25–37 | `MotionSpec` | Felder `duration`, `delay`, `response`, `easing`, `snap_on_complete`; Verhalten erhalten. |
| 39–47 | `MotionKey::storage` | `MotionKey` mit privatem `value_`; nur `Value()` exponieren. |
| 49–56 | `MotionStats` | Felder `scalar_tracks`, `vec2_tracks`, `color_tracks`, `active_tracks`, `frame_index`, `delta_time`. |
| 58–135 | `MotionSystem` | gleiches fachliches Ziel; Maps/Framezustand instanzgebunden. |
| 98–108 | `MotionSystem::Track<T>` | private Track-Struktur; `last_touched_frame`, `snap_on_complete` über Spec. |
| 110–111 | `TrackMap<T>` | private Alias im MotionSystem. |
| `motion_tokens.hpp:6–80` | `MotionTokens` und 20 camelCase-Felder | `ui/motion/motion_tokens.hpp`; `hover_fast` … `spring_snappy`. |
| `motion_context.hpp:7` | inline global `WindowMotion::g_system` | entfernen; `SettingsController::motion_system_` ist eindeutiger Besitzer. |
| `motion_context.hpp:8` | inline global `WindowMotion::g_tokens` | entfernen; `SettingsController::motion_tokens_` wird bei Reduced Motion ersetzt. |
| `motion.cpp:9–11` | `kFinishedRetentionFrames`, `kActiveRetentionFrames`, `kEpsilon` | private Konstanten in neuem `ui/motion/motion.cpp`, Namen Google-konform. |

`MotionEasing` enthält genau `Linear`, `EaseInQuad`, `EaseOutQuad`, `EaseInOutQuad`,
`EaseOutCubic`, `EaseOutExpo`, `EaseOutBack`, `SmoothStep`, `SmootherStep`, `SpringSoft` und
`SpringSnappy`. `MotionStats` enthält `scalarTracks`, `vec2Tracks`, `colorTracks`, `activeTracks`,
`frameIndex`, `deltaTime`; `MotionSpec` enthält `duration`, `delay`, `response`, `easing`,
`snapOnComplete`; `MotionKey` enthält `storage`. Diese alten Feldnamen werden wie oben angegeben
umbenannt, ohne Semantik zu verlieren.

Die `MotionTokens`-Felder sind vollständig:
`hoverFast`, `hoverSoft`, `pressFast`, `fadeFast`, `fadeMedium`, `fadeSlow`, `slideSoft`,
`slideMedium`, `slideLarge`, `panelEnterFade`, `panelEnterOffset`, `popupOpen`, `popupClose`,
`tabFade`, `tabSlide`, `searchFade`, `searchSlide`, `selectSharp`, `springSoft` und
`springSnappy`. Der neue Name jedes Felds ist die entsprechende `snake_case`-Form.

`MotionSystem::Track<T>` besitzt `current`, `source`, `target`, `velocity`, `spec`, `elapsed`,
`lastTouchedFrame`, `initialized` und `active`; alles ist reiner Wertzustand ohne externes
Cleanup. `MotionSystem` besitzt `scalarTracks_`, `vec2Tracks_`, `colorTracks_`, `frameIndex_`,
`deltaTime_`, `reducedMotion_` und `activeTrackCount_`. `Clear`/Destruktion geben die Maps frei;
`CleanupTracks` begrenzt ihre Laufzeitlebensdauer. `MotionSpec`, `MotionKey` und `MotionStats`
besitzen ebenfalls nur Werte/Strings und keine externen Ressourcen.

### Freie Funktionen, Methoden und Templatekörper

| Quelle | Alter Funktionskörper | Aktuelle Aufrufer | Ziel |
| --- | --- | --- | --- |
| `motion.cpp:13–31` | `BuildMotionKey(...)` | dreiteiliger `MotionKey`-Konstruktor | private `BuildMotionKey` im neuen Motion-CPP. |
| 33 | `NearlyEqual(float,float)` | Track-Logik und andere Overloads | private Motion-CPP-Funktion. |
| 35–37 | `NearlyEqual(ImVec2,ImVec2)` | Track-Logik | private Motion-CPP-Funktion. |
| 39–42 | `NearlyEqual(ImVec4,ImVec4)` | Track-Logik | private Motion-CPP-Funktion. |
| 44 | `LerpValue(float,float,float)` | Timed-Track und andere Overloads | private Motion-CPP-Funktion. |
| 46–48 | `LerpValue(ImVec2,ImVec2,float)` | Timed-Track | private Motion-CPP-Funktion. |
| 50–53 | `LerpValue(ImVec4,ImVec4,float)` | Timed-Track | private Motion-CPP-Funktion. |
| 55–89 | `Ease(MotionEasing,float)` | `Animate` | private `EvaluateEasing`; alle elf Fälle erhalten. |
| 91–93 | `IsSpring(MotionEasing)` | `Sanitize`/`Animate` | private Motion-CPP-Funktion. |
| 95–104 | `SpringTowardVal(...)` | drei Spring-Spezialisierungen | private `SpringTowardValue`. |
| 106–107 | primäre Deklaration `SpringToward<T>(...)` | Spezialisierungen/Animate | private Template-Deklaration im Motion-CPP. |
| 109–113 | `SpringToward<float>(...)` | Float-Track | private Spezialisierung. |
| 115–120 | `SpringToward<ImVec2>(...)` | Vec2-Track | private Spezialisierung. |
| 122–129 | `SpringToward<ImVec4>(...)` | Color-Track | private Spezialisierung. |
| 132–139 | `MotionSpec::Timed(...)` | Token-Fabriken, Reveal, SetTrackValue | `MotionSpec::Timed` mit snake_case-Parametern. |
| 141–148 | `MotionSpec::Spring(...)` | Token-Fabriken | `MotionSpec::Spring`. |
| `motion.hpp:42` | `MotionKey() = default` | Defaultkonstruktion möglich, kein expliziter Caller | erhalten, sofern Werttyp weiter Default-Konstruktion benötigt; sonst entfernen. |
| `motion.cpp:150` | `MotionKey(rawKey)` | aktuell kein produktiver direkter Caller | in neuem Typ erhalten nur falls Component-Migration Raw Keys nutzt; sonst entfernen. |
| 152–153 | `MotionKey(scope,id,channel)` | alle UI-Motion-Keys | erhalten, Parameter umbenennen. |
| `motion.hpp:46` | `MotionKey::value() const` | alle MotionKey-Overloads | `MotionKey::Value() const`. |
| `motion.cpp:155–163` | `MotionSystem::beginFrame(float)` | `WindowMotion::BeginFrame` | `MotionSystem::BeginFrame`; Controller ruft einmal pro ImGui-Frame. |
| 165–170 | `MotionSystem::clear()` | nur unbenutzter `WindowMotion::Reset` | `MotionSystem::Clear`; Controller-Shutdown/Reset darf es explizit verwenden. |
| 172 | `MotionSystem::setReducedMotion(bool)` | Context-Fassade | `SetReducedMotion`. |
| 174 | `MotionSystem::reducedMotion() const` | Context-Fassade | `ReducedMotion`. |
| 176–179 | `value(string_view,float,...)` | MotionKey-Overload | `Value`-String-Overload, intern. |
| 181–184 | `value(MotionKey,float,...)` | Shell, Controls, Layout | `Value`-Key-Overload. |
| 186–189 | `vec2(string_view,ImVec2,...)` | MotionKey-Overload | `Vec2`-String-Overload. |
| 191–194 | `vec2(MotionKey,ImVec2,...)` | Settings shell | `Vec2`-Key-Overload. |
| 196–199 | `color(string_view,ImVec4,...)` | MotionKey-Overload | `Color`-String-Overload. |
| 201–204 | `color(MotionKey,ImVec4,...)` | Combo, Segment, Sidebar | `Color`-Key-Overload. |
| 206–208 | `set(string_view,float)` | Key-Overload | `Set`-String/Float. |
| 210–212 | `set(string_view,ImVec2)` | Key-Overload | `Set`-String/Vec2. |
| 214–216 | `set(string_view,ImVec4)` | Key-Overload | `Set`-String/Color. |
| 218 | `set(MotionKey,float)` | Shell, Toast, Combo | `Set`-Key/Float. |
| 220 | `set(MotionKey,ImVec2)` | Settings shell | `Set`-Key/Vec2. |
| 222 | `set(MotionKey,ImVec4)` | aktuell kein externer Caller | `Set`-Key/Color nur behalten, wenn Migration einen Caller hat. |
| 224–228 | `forget(string_view)` | Key-Overload | `Forget`-String; löscht alle drei Tracktypen. |
| 230 | `forget(MotionKey)` | Combo und Toast | `Forget`-Key. |
| 232–235 | `isActive(string_view) const` | Key-Overload | `IsActive`-String. |
| 237 | `isActive(MotionKey) const` | aktuell kein externer Caller | nur behalten, wenn neuer Render-Gate es nutzt. |
| 239–248 | `stats() const` | Settings Render-Gating und DebugOverlay | `Stats`. |
| 250–271 | `debugOverlay(const char*) const` | **kein Aufrufer** | entfernen, sofern kein explizites Diagnose-Feature es übernimmt; kein verstecktes Produktions-Debugfenster. |
| 273–293 | `sanitize(const MotionSpec&) const` | `Animate` | private `Sanitize`; Reduced-Motion-Regeln erhalten. |
| 295–372 | `animate<T>(...)` | Value/Vec2/Color | private `Animate`; Init/Retarget/Spec-Wechsel, Spring/Timed und Active-Count erhalten. |
| 374–386 | `setTrackValue<T>(...)` | sechs Set-Overloads | private `SetTrackValue`. |
| 388–392 | `isTrackActive<T>(...) const` | `IsActive` | private `IsTrackActive`. |
| 394–406 | `cleanupTracks<T>(...)` | `BeginFrame` | private `CleanupTracks`; Retention 2/180 Frames erhalten. |
| 408–434 | explizite Instantiierungen für `Animate`, `SetTrackValue`, `IsTrackActive`, `CleanupTracks` je Float/Vec2/Vec4 | Linkage der Templates | zusammen mit den Templatekörpern im neuen Motion-CPP erhalten oder durch nicht-template private Overloads ersetzen; keine alte Instantiierung bleibt zurück. |
| `motion_tokens.hpp:28–54` | `MotionTokens::Default()` | globaler Token-Init und Reduced-Motion-Umschaltung | `MotionTokens::Default`; Werte unverändert. |
| 56–79 | `MotionTokens::Reduced()` | Reduced-Motion-Umschaltung | `MotionTokens::Reduced`; Werte unverändert. |
| `motion_context.hpp:10` | `WindowMotion::System()` | alle heutigen UI-Dateien | Wrapper entfernen; `MotionContext.system` injizieren. |
| 12 | `WindowMotion::Tokens()` | alle heutigen UI-Dateien | Wrapper entfernen; `MotionContext.tokens` injizieren. |
| 14 | `WindowMotion::BeginFrame(float)` | `SettingsWindow::Render` | entfernen; Controller ruft `motion_system_.BeginFrame`. |
| 16 | `WindowMotion::Reset()` | **kein Aufrufer** | entfernen; bei Bedarf direkter Owner-Aufruf. |
| 18–21 | `WindowMotion::SetReducedMotion(bool)` | `UpdateReducedMotion` | `SettingsController::SetReducedMotion`, aktualisiert System und Tokeninstanz. |
| 23 | `WindowMotion::ReducedMotion()` | **kein Aufrufer** | entfernen oder direkter Owner-Zugriff. |

## Vollständige Zustands- und Ressourcenmigration

### `SettingsWindow`-Member

| Neuer Besitzer | Alte Member, genau einmal zugeordnet | Lebensdauer/Recovery |
| --- | --- | --- |
| `SettingsWindow` | `hwnd_`, `window_dragging_`, `window_drag_offset_` | HWND in `Create`, `DestroyWindow` in idempotentem Shutdown; Capture bei Up/Cancel lösen. |
| `ImGuiRenderer` | `device_`, `context_`, `swap_chain_`, `render_target_view_`, `imgui_ready_`, `imgui_context_ready_`, `imgui_win32_ready_`, `imgui_dx11_ready_`, `device_recovery_pending_`, `next_device_recovery_ms_`, `device_recovery_delay_ms_`, `device_recovery_test_pending_`, `current_dpi_`, `ui_scale_`, `font_small_`, `font_body_`, `font_medium_`, `font_title_` | COM/Backend/Font-Lebensdauer wie oben; Recovery/Resize ausschließlich hier. `ImFont*` sind vom Atlas geliehen und nach Context-Destroy ungültig. |
| `TrayIcon` | `tray_icon_added_`, `taskbar_created_message_` | Shell-Registrierung und Retry-Timer; nach Explorer-Neustart `added=false` und bei verborgenem Fenster neu hinzufügen. |
| `AnimationPreview` | `preview_active_`, `preview_window_`, `preview_phase_`, `preview_phase_started_ms_`, `preview_dragging_`, `preview_drag_offset_` | Preview-HWND wird bei ungültigem Handle, Abschluss und Shutdown bereinigt. |
| `SettingsController`/Shell | `selected_page_`, `reset_page_scroll_`, `persistence_error_`, `save_feedback_`, `save_feedback_until_ms_`, `save_feedback_error_`, `render_requested_`, `shown_at_ms_` | rein transient; keine fachliche Persistenz. |
| `SettingsViewModel` | `is_enabled_`, `temporarily_paused_`, `paused_until_restart_`, `hotkeys_`, `hotkey_available_`, `diagnostics_`, `minimize_duration_seconds_`, `restore_duration_seconds_`, `link_speeds_`, `disable_animations_fullscreen_`, `disable_effects_battery_saver_`, `minimize_easing_`, `restore_easing_`, `minimize_custom_bezier_`, `restore_custom_bezier_`, `animation_style_`, `quality_mode_`, `genie_strength_`, `fade_strength_`, `show_target_indicator_`, `close_behavior_`, `run_at_startup_`, `start_minimized_`, `excluded_applications_` | Snapshot wird nur nach erfolgreichem Settings-Service-Update veröffentlicht; Controller darf temporäre Editwerte separat halten. |
| `MotionPage` | `minimize_bezier_dirty_`, `restore_bezier_dirty_`, `minimize_bezier_active_`, `restore_bezier_active_`, `strength_slider_active_`, `strength_slider_dirty_`, `minimize_slider_active_`, `minimize_slider_dirty_`, `restore_slider_active_`, `restore_slider_dirty_` | Preview während Aktivität; Commit bei active→inactive, Page-Wechsel, Hide und Shutdown. |
| `ApplicationsPage`/ViewModel | `exclusion_input_`, `exclusion_error_`, `last_active_apps_refresh_ms_`, `cached_active_apps_` | Textfilter Page-lokal; Active-App-Snapshot wird vom Controller aktualisiert. |
| `HotkeysPage` | `editing_hotkey_`, `hotkey_feedback_` | Capture bei Esc/Erfolg beenden; Key-Events kommen über Controller. |
| `DiagnosticsPage` | `last_diagnostics_refresh_ms_`, `diagnostics_feedback_` | Snapshot-Refresh getaktet; Aktionen invalidieren Refreshzeit. |
| entfällt | alle 22 Callback-Member von `toggle_callback_` bis `exit_callback_` | genau eine geliehene `SettingsActions&` im Controller ersetzt die Liste. |

### Konstanten und statischer Zustand aus `settings_window.cpp`

| Ziel | Alte Namen |
| --- | --- |
| `SettingsWindow`/Single-Instance-Vertrag | `kSettingsWindowClass`, `kShowSettingsMessage`, `kWindowWidth`, `kWindowHeight`, `kMinimumWindowWidth`, `kMinimumWindowHeight`, `kHeaderHeight` |
| `AnimationPreview` | `kPreviewWindowClass` |
| `MotionPage` beziehungsweise Settings-Validator | `kMinimumAnimationDurationSeconds`, `kMaximumAnimationDurationSeconds` |
| `TrayIcon` | `kTrayMessage`, `kTrayIconId`, `kTrayRetryTimerId`, `kTrayToggleEnabled`, `kTrayShowSettings`, `kTrayRepairWindows`, `kTrayExit`, `kTrayPauseTenMinutes`, `kTrayPauseOneHour`, `kTrayPauseUntilRestart`, `kTrayResume` |
| `GlobalHotkeyManager` | `kHotkeyBaseId` |
| `theme_tokens` | `kPrimaryTextColor`, `kSecondaryTextColor`, `kPageTitleTextSize`, `kPageSubtitleTextSize`, `kSectionTitleTextSize`, `kLabelTextSize`, `kValueTextSize`, `kHelperTextSize`, `kCaptionTextSize` |
| `ImGuiRenderer`/FontSet | `kSmallFontSize`, `kBodyFontSize`, `kTitleFontSize` |
| `ImGuiRenderer` | `kInitialDeviceRecoveryDelayMs`, `kMaximumDeviceRecoveryDelayMs` |
| `AboutPage` | funktionslokaler statischer Lizenzstring aus `RenderContents:1992` |

### Externe Ressourcen

| Resource | Aktueller Besitz | Zielvertrag |
| --- | --- | --- |
| Haupt-HWND | `SettingsWindow` | bleibt beim kleinen Fenster; genau einmal `DestroyWindow`. |
| Preview-HWND | `SettingsWindow` | `AnimationPreview`; getrennte WndProc und idempotentes `Close`. |
| Preview `HBRUSH`/`HFONT` | pro Paint erzeugt | weiterhin pro Paint `DeleteObject`; ausgewähltes Fontobjekt vorher zurücktauschen. |
| Mouse capture | Haupt- und Preview-WndProc | jeweiliger HWND-Besitzer löst bei Up, `WM_CAPTURECHANGED`, `WM_CANCELMODE` und Shutdown. |
| Tray-Icon | bool in SettingsWindow | `TrayIcon` besitzt NIM_ADD/NIM_DELETE und Retry-Timer; `LoadIcon(nullptr, IDI_APPLICATION)` ist shared und wird nicht zerstört. |
| ImGui Context | SettingsWindow | `ImGuiRenderer`; genau ein Create/Destroy. |
| ImGui Win32/DX11 Backends | SettingsWindow | Renderer; Shutdown nur wenn Init erfolgreich war. |
| D3D COM-Objekte | SettingsWindow | Renderer/`ComPtr`; RTV vor Swapchain/Context/Device lösen. |
| Font-RCDATA IDs 201–203 | PE-Resource, nicht freigebbar | Renderer liest View, Atlas besitzt Daten ausdrücklich nicht; Fallback-Dateidaten werden vom Atlas besessen. |
| OFL-RCDATA ID 204 | PE-Resource | AboutPage kopiert in `std::string`. |
| ImGuiStorage/Widgetmaps | teils Context, teils globale Maps | ausschließlich Context-/Controller-Lebensdauer; bei Renderer-Shutdown keine process-globalen Reste. |

## Win32-Nachrichtenrouting nach der Trennung

### Hauptfenster

| Nachricht/Pfad | Heutiges Verhalten | Zielroute |
| --- | --- | --- |
| `WM_NCCREATE` | `this` in `GWLP_USERDATA` | bleibt enger `SettingsWindow`-Thunk. |
| dynamisches `TaskbarCreated` | `tray_icon_added_=false`, bei hidden Add | `SettingsWindow` → `TrayIcon::HandleTaskbarCreated`; Explorer-Neustart vollständig erhalten. |
| `WM_LBUTTONDOWN/MOVE/UP`, `WM_CAPTURECHANGED`, `WM_CANCELMODE` im Header | blockierungsfreies manuelles Fensterdragging | bleibt SettingsWindow; keine modale Caption-Move-Schleife. |
| `WM_KEYDOWN/WM_SYSKEYDOWN` während Hotkey-Capture | Esc, Modifier ignorieren, Binding bilden | Window → Controller → HotkeysPage/Actions; keine Hotkey-Persistenz im Window. |
| `kTrayMessage` | Links öffnet; Rechtsmenü toggelt/pause/settings/repair/exit | vollständig `TrayIcon`; Actions/Controller werden injiziert. Popup-`HMENU` immer `DestroyMenu`. |
| ImGui-Eingaben und Paint | Backend-Handler plus Renderrequest | Window → `ImGuiRenderer::HandleMessage`; Controller erhält RequestRender. |
| `WM_HOTKEY` | Callback auf `HotkeyAction` | aus SettingsWindow nach `GlobalHotkeyManager`. |
| `kShowSettingsMessage` | `Show(true)` | Window routet an Controller; Message-Filter/UIPI-Allow bleibt erhalten. |
| `WM_GETMINMAXINFO` | DPI-skalierte 800×580 Mindestgröße | SettingsWindow mit aktuellem Renderer-/DPI-Scale. |
| `WM_TIMER` | Tray-Retry | TrayIcon. |
| `WM_PAINT/WM_ERASEBKGND` | validate + render request/kein Erase | Window/Renderer-Vertrag. |
| `WM_DPICHANGED` | suggested rect, Fonts/Style rebuild | Window setzt rect; Renderer `OnDpiChanged`. |
| `WM_SETTINGCHANGE` | Reduced Motion neu lesen | Window → Controller/Motion. |
| `WM_SIZE` | Targets lösen, ResizeBuffers, RTV, Device-Lost | vollständig Renderer `Resize`; Window routet nur Größe, ignoriert minimiert/0. |
| `WM_CLOSE` | Close-to-tray oder Exit | Window → Controller. |
| `WM_NCHITTEST` | immer `HTCLIENT` | SettingsWindow erhalten. |

### Preview

| Nachricht | Zielverhalten |
| --- | --- |
| `WM_NCCREATE` | `AnimationPreview*` in `GWLP_USERDATA`. |
| `WM_PAINT` | Hintergrund, Accent und „Preview“ zeichnen; alle GDI-Objekte freigeben. |
| Mouse down/move/up/capture cancel | ausschließlich Preview-Dragging/Capture. |
| `WM_ERASEBKGND` | `1`. |
| `WM_CLOSE` | Preview-HWND zerstören; Owner erkennt ungültiges HWND spätestens beim Update. |

## Settings- und Action-Pfade

Die heutige UI mutiert häufig zuerst lokalen Zustand, ruft einen Callback und rollt nur in einigen
Pfaden zurück. Ziel ist immer:

`Page event → Controller-Arbeitswert → typed SettingsChange → SettingsActions → SettingsService
(Kopie validieren/normalisieren/atomar speichern) → neuer SettingsViewModel-Snapshot`.

| UI-Aktion | Heutiger Pfad | Zu erhaltender/zu korrigierender Vertrag |
| --- | --- | --- |
| Enabled Toggle | GeneralPage → `toggle_callback_` → `Application::SetEnabled` → Save → UpdateState | `SetEnabled`; bei Save-Fehler ViewModel unverändert, Fehler-Toast. Tray Toggle nutzt denselben Pfad und darf Ergebnis nicht still verlieren. |
| Close action | Segment → `SetCloseBehavior` | typed Change; Close liest nur ViewModel. Bei `tray` muss fehlendes Tray-Icon das HWND wieder zeigen. |
| Launch at login / Start in tray | GeneralPage → `SetStartupOptions` → Save, StartupManager, möglicher Settings-Rollback | ein orchestrierter Action-Call; bei Registry-Fehler persistierten Change atomar zurückrollen und alten Snapshot behalten. |
| Dauer Reset/Preset | MotionPage setzt beide Werte und `save=true` | gemeinsamer `AnimationDurationsChange`; Defaults 0.70/0.70. |
| Dauer Drag | UI ruft `save=false` laufend, `save=true` beim Loslassen/Flush | Preview getrennt vom Commit. Der heutige Pfad mutiert `Application::settings_` vor erfolgreichem Save; künftig bei Commitfehler autoritativen Snapshot wiederherstellen. |
| Link durations | `SetLinkSpeeds` | typed Change, UI rollback bei Fehler. |
| Style | Combo → `SetAnimationStyle` | nur drei historische Strings akzeptieren; Easing/Bezier unverändert lassen. |
| Easing | beide Namen gemeinsam → `SetEasing` | acht bestehende Namen erhalten; typed Change. |
| Custom Bezier | Drag/Input `save=false`, Ende `save=true` | Preview/Commit trennen. Der heutige Application-Pfad verändert Settings vor Save; künftig transaktional. |
| Quality | Segment → `SetQualityMode` | Werte `automatic`, `best_quality`, `power_saving` erhalten. |
| Strength | Slider Preview/Commit → `SetGenieStrength` | 0.25…1.0; heute gleiche Vorabmutation wie Dauern, künftig transaktional. |
| Fade | Combo → `SetFadeStrength` | `No fade`, `Subtle`, `Strong` erhalten. |
| Target indicator | Toggle → `SetTargetIndicator` | typed Change. |
| Exclusion | Apps Toggle → `SetApplicationExcluded` | Normalisierung/Dedupe im Settings-Modul; Page aktualisiert Liste nur nach Erfolg. |
| Fullscreen | WindowsPage → `SetDisableAnimationsFullscreen` | nach Commit EffectPolicy aktualisieren. |
| Battery Saver | WindowsPage → `SetDisableEffectsBatterySaver` | nach Commit Power/Policy aktualisieren. |
| Hotkey Change/Clear | Key-Capture → `SetHotkey`; Registration wird vor Save versucht und bei Fehler zurückgesetzt | `GlobalHotkeyManager` besitzt Registrierung; `SettingsService` und Registrierung brauchen explizite Transaktion/Rollback. |
| Tray Pause/Resume | Tray → `SetTemporaryPause` | `PauseController`; Tooltip/ViewModel sofort aktualisieren. |
| Runtime `WM_HOTKEY` | Window → `ExecuteHotkeyAction` | GlobalHotkeyManager → Effect/Settings/Recovery-Anwendungsfälle, nicht UI-WndProc. |
| Diagnose lesen | Page pollt alle 500 ms | Controller/ViewModel bekommt `DiagnosticsSnapshot`; Page ist passiv. |
| Copy/Open/Repair/Restart | Page → `ExecuteDiagnosticsAction` | `DiagnosticsService`; Clipboard-HGLOBAL geht bei erfolgreichem `SetClipboardData` an das OS, sonst `GlobalFree`; Clipboard immer schließen. |
| Tray Repair | separater Heal-Callback | gleiche `DiagnosticsAction::kRepairWindows`, keine zweite Route. |
| Exit | Close oder Tray → ExitCallback | `SettingsActions::RequestExit`. |

## Recovery- und Sonderfallverträge

- **Taskbar/Explorer-Neustart:** `TaskbarCreated` ist eine registrierte Nachricht. Der Tray-Besitzer
  verwirft seinen Added-Status und fügt das Icon nur dann neu hinzu, wenn das Settings-HWND verborgen
  ist. Bei normalem Add-Fehler läuft ein 1-s-Retry-Timer; bei sichtbarem Fenster wird der Timer
  beendet.
- **Close-to-Tray:** Verbergen gilt erst als erfolgreich, wenn `NIM_ADD` erfolgreich war. Sonst
  Fenster wieder zeigen, Render anfordern und fokussieren.
- **DPI:** Hauptfenster wird per-monitor-v2 erzeugt; `WM_DPICHANGED` übernimmt das vorgeschlagene
  Rechteck. Fonts werden an ganzzahligen Pixelgrößen neu gebacken und Style/Skalierung gemeinsam
  aktualisiert.
- **Fonts:** Inter Regular/SemiBold/Bold aus RCDATA; bei fehlender Einbettung Assets-Pfad; zuletzt
  Defaultfont. `font_small/body/medium/title` müssen nach jedem Rebuild geschlossen konsistent sein.
- **Device Lost:** DXGI removed/reset/hung/driver-internal gelten als Device Lost. DX11-ImGui-Backend
  herunterfahren, Targets/COM freigeben, sofort probieren, danach 250→500→1000→2000→4000 ms.
  Resize-Fehler ohne Device Lost versucht das RTV wiederherzustellen. `GENIE_TEST_DEVICE_RECOVERY`
  bleibt Debug-Testinjektion des Renderers.
- **Preview:** separates, sichtbares Popup am Work Area des Settings-Monitors; Minimize nach 750 ms,
  Restore nach Dauer+700 ms, Close nach Restore-Dauer+850 ms. Ungültiges/geschlossenes HWND
  beendet die Preview ohne Restzustand.
- **Reduced Motion:** `SPI_GETCLIENTAREAANIMATION` wird bei Init und `WM_SETTINGCHANGE` ausgewertet.
  Der Controller ersetzt Tokens durch `Reduced()` und setzt den Systemmodus; keine Globals.
- **Active Apps/UIPI:** alle zwei Sekunden Top-Level-Fenster ermitteln, nur interessante Fenster,
  Exe-Namen normalisieren, deduplizieren und sortieren. Prozesszugriff kann bei elevated/UIPI
  scheitern; solche Einträge werden ausgelassen, nicht als Fehlerzustand der UI behandelt.
- **Single Instance:** Klassenname und Show-Message bleiben kompatibel; `ChangeWindowMessageFilterEx`
  erlaubt die Aktivierungsnachricht über Integrity-Level-Grenzen. Timeout/Hung-Window-Pfad bleibt.
- **Shutdown:** Pending Motion-Edits committen oder sauber verwerfen, Preview schließen, Tray+Timer
  entfernen, Renderer backends/context/COM lösen und zuletzt Haupt-HWND zerstören. Wiederholter
  Shutdown darf nichts doppelt freigeben.

## Phase-0-Gate für diesen Teilbereich

- Jede explizite Funktion/Methodendefinition der elf gelesenen alten Dateien ist oben genau einem
  Ziel oder einer begründeten Entfernung zugeordnet.
- Alle Klassen, Structs, Enums, Callbacktypen, process-globalen Maps/Objekte, statischen
  Konstantengruppen und der statische Lizenzcache sind erfasst.
- Externe Aufrufer in `main` und `Application`, interne Aufrufpfade sowie Projekt-Includes sind
  erfasst.
- Haupt-HWND, Preview-HWND, Tray-Registrierung/Timer, ImGui-Context/Backends, D3D-COM, Font-Atlas,
  GDI-Objekte, Mouse Capture, Menü-Handle und Clipboard-Speicher haben einen Zielbesitzer und
  Cleanup-Vertrag.
- Taskbar-Neustart, Close-to-Tray, DPI/Fonts, Reduced Motion, Device Recovery, Preview,
  Single-Instance/UIPI, Active-App-Ermittlung und alle Settings-/Action-Mutationen sind explizit
  abgedeckt.
- Bewusst zu entfernender toter Code: `SystemTransparencyEnabled`, `Blend`,
  `DrawGradientShadow`, mehrere unbenutzte `PageLayout`-Accessor, `AdvanceScaled`,
  `g_MenuArrowIcon`, `g_MenuConfigIcon`, `subNamespaceBg` (falls weiterhin ohne Caller),
  `MotionSystem::debugOverlay` und unbenutzte Motion-Wrapper/Overloads nach abschließendem
  Caller-Audit.
- Noch keine Implementierung ist durch diese Inventur autorisiert; Phase 1 darf erst nach
  Zusammenführung mit den übrigen Phase-0-Inventuren beginnen.
