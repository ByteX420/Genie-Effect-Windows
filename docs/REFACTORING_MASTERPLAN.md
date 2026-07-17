# Verbindlicher Masterplan: Genie Effect vollständig modularisieren

## Auftrag an die ausführende KI

Arbeite diesen Plan selbstständig, vollständig und in der angegebenen Reihenfolge ab. Der Auftrag
endet nicht nach einer Analyse, einem Teilumbau, einem erfolgreichen Build oder einigen Tests. Er
endet erst, wenn:

1. die gesamte Produktionsstruktur umgebaut ist,
2. sämtlicher bestehender First-Party-Code einer klaren Verantwortung zugeordnet ist,
3. alle alten Sammeldateien und Übergangslösungen entfernt sind,
4. Projektdateien und Dokumentation den neuen Stand abbilden,
5. danach alle technischen Tests erfolgreich sind,
6. danach ein echter Computer-Use-Test der laufenden Anwendung erfolgreich ist,
7. alle dabei gefundenen Probleme behoben und erneut geprüft wurden,
8. und du anhand der unten definierten Abnahmekriterien mit dem Ergebnis zufrieden bist.

Führe die Arbeit durch. Liefere nicht nur Vorschläge oder Beispielcode. Bitte den Benutzer nicht nach
jedem Abschnitt um Fortsetzung. Triff bei kleinen Detailfragen eine begründete, konsistente
Entscheidung. Halte nur dann an, wenn eine echte externe Sperre vorliegt, die du nach Ausschöpfung
aller sicheren Möglichkeiten nicht selbst lösen kannst.

## Strikte Reihenfolge: erst alles umbauen, dann testen

Während der Struktur- und Implementierungsphasen sind keine Builds, Testläufe oder Programmstarts
auszuführen. Das gilt ausdrücklich auch für vermeintlich hilfreiche Zwischen-Builds.

Erlaubt sind in dieser Zeit:

- Dateien und Aufrufpfade lesen,
- Querverweise und Includes durchsuchen,
- Code verschieben, aufteilen und schreiben,
- Projekt- und Filterdateien aktualisieren,
- Formatierung anwenden,
- statisch prüfen, ob jede alte Funktion genau einem neuen Besitzer zugeordnet wurde.

Erst wenn der Abschnitt **„Produktionsumbau vollständig“** lückenlos erfüllt ist, beginnt der erste
Build. Ab diesem Zeitpunkt gilt eine Reparatur- und Testschleife: Fehler beheben, betroffene und
übergeordnete Tests wiederholen und erst bei vollständig grünem Ergebnis fortfahren.

## Unverhandelbare Randbedingungen

- Das bestehende sichtbare Verhalten muss erhalten bleiben. Dieser Auftrag ist ein Strukturumbau,
  kein Produkt-Redesign.
- Verwende den aktuellen Repository-Inhalt als Wahrheit. Lies vor dem Verschieben jede betroffene
  First-Party-Datei vollständig.
- Bewahre vorhandene, nicht zu diesem Auftrag gehörende Benutzeränderungen.
- Verändere `app/third_party/` nicht. Eigene ImGui-Erweiterungen gehören ausschließlich unter
  `app/src/ui/`.
- Bewahre Namen und Laufzeitvertrag von `GenieEffect.exe` und `GenieHookPost.dll`.
- Bewahre die Hook-ABI, insbesondere den exportierten CBT-Einstieg, die Resource-IDs und die
  Einbettungs-/Extraktionslogik.
- Bewahre Pfad, Format und Rückwärtskompatibilität von
  `%LOCALAPPDATA%\GenieEffect\settings.json`.
- Bewahre Debug- und Release-Ausgabeorte unter `build/`.
- Bewahre x64, D3D11, DXGI Desktop Duplication, DirectComposition, Win32, ImGui und FreeType als
  technische Basis.
- Behalte `pch.hpp` nur für häufig verwendete, stabile STL-/Windows-Header. Keine eigenen
  Projekt-Header in den PCH aufnehmen.
- Keine globalen Systempointer. Ein unvermeidbarer Win32-C-Callback-Thunks darf eng gekapselten
  Zustand besitzen, aber nicht als allgemeiner Service-Locator dienen.
- Keine neuen Sammeldateien wie `utils`, `helpers`, `common`, `misc`, `manager` ohne fachliche
  Präzisierung.
- Keine leeren Platzhalter, keine auskommentierten Altimplementierungen, keine dauerhaften
  Weiterleitungsdateien und keine offenen `TODO`-Marker als Ersatz für fertigen Code.
- Keine künstlichen Interfaces für jede Klasse. Eine Schnittstelle ist nur gerechtfertigt, wenn sie
  eine echte Modulgrenze bildet, mehrere Implementierungen ermöglicht oder Tests wesentlich
  verbessert.
- Ressourcenbesitz muss eindeutig und RAII-basiert sein. Shutdown und Recovery müssen idempotent
  bleiben.

## Was „alles fertig“ konkret bedeutet

Der Produktionsumbau ist nicht vollständig, solange auch nur einer dieser Punkte offen ist:

- Eine Funktion aus einer alten Datei wurde noch nicht migriert oder bewusst entfernt.
- Dieselbe Logik existiert parallel in alter und neuer Form.
- `Application` enthält noch Minimize-/Restore-Algorithmen, D3D-Details, Hotkey-Registrierung,
  Settings-Serialisierung, Diagnoseerstellung oder ImGui-Zeichencode.
- `SettingsWindow` besitzt noch gleichzeitig Win32-Fenster, D3D/ImGui-Renderer, Tray,
  Animation-Preview, Seitendarstellung und sämtliche Anwendungsaktionen.
- Der anonyme Hilfsbereich am Anfang von `application.cpp` oder `window_util.*` dient noch als
  Sammelablage.
- Reale Ordner und Visual-Studio-Filter unterscheiden sich.
- Eine entfernte Datei steht noch in `.vcxproj`, `.slnx`, Includes, Dokumentation oder Ressourcen.
- Eine neue Datei fehlt in der Projektdatei.
- Es gibt noch unnötige zyklische Includes oder Abhängigkeiten gegen die unten definierte Richtung.
- Ein Header zieht unnötig Windows-, D3D- oder ImGui-Implementierungsdetails in fachlich reine
  Module.
- Veralteter, unerreichbarer oder duplizierter Code ist noch vorhanden.
- README und `docs/architecture.md` beschreiben weiterhin die alte Struktur.

## Festgestellter Ist-Zustand

Die groben Ordner sind bereits sinnvoll angelegt, die Verantwortlichkeiten sind aber noch nicht
sauber getrennt. Besonders relevant:

| Heutige Datei/Gruppe | Problem |
| --- | --- |
| `app/src/app/application.hpp` | Kennt fast alle Systeme, viele Zustände und sehr viele fachfremde Methoden. |
| `app/src/app/application.cpp` | Rund 3.100 Zeilen mit Initialisierung, Message Loop, Laufzeit-State, Renderer-Recovery, Policy, Hotkeys, Diagnose, Settings-Mutationen, Hook, Minimize, Restore, Snapshots und Window-Recovery. |
| Anonymer Namespace am Anfang von `application.cpp` | Große Sammlung aus Hook-, Resource-, Prozess-, Fensterzustands-, Geometrie-, Logging- und Konvertierungshelfern. |
| `settings_store.cpp` | Modell, Normalisierung, Parsing, Serialisierung und Dateizugriff sind gekoppelt. |
| `settings_window.cpp/.hpp` | Rund 2.500 Zeilen: Win32, D3D, ImGui, Tray, Preview, UI-State, sämtliche Seiten und Anwendungsbefehle sind gekoppelt. |
| `settings_window.hpp` | Übergibt eine lange Liste einzelner Callbacks und besitzt sehr viel heterogenen Zustand. |
| `settings_ui_widgets.cpp` | Rund 1.100 Zeilen; enthält auch die Combo-Implementierung und mehrere unterschiedliche UI-Komponenten. |
| `desktop_capture.cpp` | Desktop-Duplication, Capture-Ablauf und Recovery sind zu groß gekoppelt. |
| `overlay_window.cpp` | Rund 1.000 Zeilen; Overlay-HWND, DirectComposition, Shader, Mesh-Upload und Frame-Rendering sind wieder in einer Datei vereint. |
| `window_util.*` | Fachlich unterschiedliche Windows-Helfer in einer allgemeinen Datei. |
| `debug_log.hpp` | Umfangreiche Implementierung in einem allgemeinen Header. |
| `main.cpp` | Single-Instance, COM und Console-Handler liegen zusätzlich zum Programmeinstieg dort. |

Das bloße Umbenennen dieser Dateien ist keine Lösung. Zustände und Verhalten müssen in echte,
besitzende Komponenten verschoben werden.

## Verbindliche Zielarchitektur

Die genaue Anzahl kleiner Hilfsdateien darf anhand des realen Codes angepasst werden. Die Module,
ihre Grenzen und die unten genannten Kernklassen sind jedoch verbindlich.

```text
Genie-Effect-Windows/
├── GenieEffect.slnx
├── app/
│   ├── GenieEffect.vcxproj
│   ├── GenieEffect.vcxproj.filters
│   ├── GenieEffect.rc
│   ├── assets/
│   ├── shaders/
│   ├── third_party/
│   └── src/
│       ├── main.cpp
│       ├── pch.cpp
│       ├── pch.hpp
│       ├── app/
│       │   ├── application.cpp
│       │   ├── application.hpp
│       │   ├── message_loop.cpp
│       │   ├── message_loop.hpp
│       │   ├── process_bootstrap.cpp
│       │   └── process_bootstrap.hpp
│       ├── core/
│       │   ├── logger.cpp
│       │   ├── logger.hpp
│       │   ├── timer.cpp
│       │   └── timer.hpp
│       ├── animation/
│       │   ├── easing.cpp
│       │   ├── easing.hpp
│       │   ├── genie_mesh.cpp
│       │   ├── genie_mesh.hpp
│       │   └── geometry.hpp
│       ├── settings/
│       │   ├── app_settings.hpp
│       │   ├── hotkey_binding.hpp
│       │   ├── exclusion_rules.cpp
│       │   ├── exclusion_rules.hpp
│       │   ├── settings_repository.cpp
│       │   ├── settings_repository.hpp
│       │   ├── settings_serializer.cpp
│       │   ├── settings_serializer.hpp
│       │   ├── settings_service.cpp
│       │   ├── settings_service.hpp
│       │   ├── settings_validator.cpp
│       │   └── settings_validator.hpp
│       ├── platform/
│       │   └── windows/
│       │       ├── cbt_hook_manager.cpp/.hpp
│       │       ├── fullscreen_detector.cpp/.hpp
│       │       ├── global_hotkey_manager.cpp/.hpp
│       │       ├── native_animation_blocker.cpp/.hpp
│       │       ├── power_status_monitor.cpp/.hpp
│       │       ├── process_info.cpp/.hpp
│       │       ├── session_state_store.cpp/.hpp
│       │       ├── single_instance_guard.cpp/.hpp
│       │       ├── startup_manager.cpp/.hpp
│       │       ├── taskbar_target_provider.cpp/.hpp
│       │       ├── window_event_monitor.cpp/.hpp
│       │       ├── window_properties.cpp/.hpp
│       │       ├── window_state.cpp/.hpp
│       │       └── window_types.hpp
│       ├── rendering/
│       │   ├── animation_renderer.cpp/.hpp
│       │   ├── d3d_device.cpp/.hpp
│       │   ├── desktop_capture.cpp/.hpp
│       │   ├── desktop_duplication_session.cpp/.hpp
│       │   ├── overlay_renderer.cpp/.hpp
│       │   ├── overlay_window.cpp/.hpp
│       │   └── texture.hpp
│       ├── runtime/
│       │   ├── animation_run.cpp/.hpp
│       │   ├── animation_run_pool.cpp/.hpp
│       │   ├── frame_scheduler.cpp/.hpp
│       │   ├── renderer_recovery.cpp/.hpp
│       │   ├── run_state.hpp
│       │   └── snapshot_cache.cpp/.hpp
│       ├── features/
│       │   ├── effect/
│       │   │   ├── effect_controller.cpp/.hpp
│       │   │   └── effect_policy.cpp/.hpp
│       │   ├── minimize/
│       │   │   ├── minimize_feature.cpp/.hpp
│       │   │   ├── minimize_request.hpp
│       │   │   └── minimize_transaction.cpp/.hpp
│       │   ├── restore/
│       │   │   ├── restore_feature.cpp/.hpp
│       │   │   ├── restore_request.hpp
│       │   │   └── restore_transaction.cpp/.hpp
│       │   ├── pause/
│       │   │   └── pause_controller.cpp/.hpp
│       │   ├── recovery/
│       │   │   └── window_recovery_service.cpp/.hpp
│       │   └── diagnostics/
│       │       ├── diagnostics_service.cpp/.hpp
│       │       └── diagnostics_snapshot.hpp
│       └── ui/
│           ├── settings_actions.hpp
│           ├── settings_controller.cpp/.hpp
│           ├── settings_view_model.hpp
│           ├── settings_window.cpp/.hpp
│           ├── rendering/
│           │   └── imgui_renderer.cpp/.hpp
│           ├── tray/
│           │   └── tray_icon.cpp/.hpp
│           ├── preview/
│           │   └── animation_preview.cpp/.hpp
│           ├── pages/
│           │   ├── general_page.cpp/.hpp
│           │   ├── motion_page.cpp/.hpp
│           │   ├── applications_page.cpp/.hpp
│           │   ├── windows_page.cpp/.hpp
│           │   ├── hotkeys_page.cpp/.hpp
│           │   ├── diagnostics_page.cpp/.hpp
│           │   └── about_page.cpp/.hpp
│           ├── components/
│           │   ├── combo.cpp/.hpp
│           │   ├── controls.cpp/.hpp
│           │   └── page_layout.cpp/.hpp
│           ├── motion/
│           │   ├── motion.cpp/.hpp
│           │   ├── motion_context.hpp
│           │   └── motion_tokens.hpp
│           └── theme/
│               ├── theme.cpp/.hpp
│               └── theme_tokens.hpp
├── hook/
│   ├── GenieHook.vcxproj
│   └── hook.cpp
├── tests/
│   ├── GenieEffect.Tests.vcxproj
│   └── src/
├── docs/
│   ├── architecture.md
│   └── REFACTORING_MASTERPLAN.md
├── README.md
└── LICENSE.txt
```

`cpp/.hpp` in diesem Baum bedeutet ein echtes Dateipaar. Erzeuge keine Datei nur deshalb, weil sie
im Baum steht, wenn der reale Inhalt trivial wäre; lege dann die Verantwortung in die engste
fachlich passende Nachbardatei. Umgekehrt darfst du eine Datei weiter teilen, wenn sonst erneut eine
Sammeldatei entstehen würde.

Die vorhandenen Produktprojekte `app` und `hook` reichen zunächst aus. Erzwinge keine zusätzliche
Produktbibliothek nur um der Architektur willen. Das Testprojekt wird erst in der Testphase
hinzugefügt. Sollte das Testprojekt sonst Produktionsquellen duplizieren müssen, darfst du die
tatsächlich plattformunabhängigen Module in eine kleine statische `GenieEffect.Core`-Bibliothek
extrahieren. Tue das nur bei echtem Nutzen und halte Windows, D3D und ImGui außerhalb dieser
Bibliothek.

## Verantwortlichkeiten der Module

### `app`

Kompositionswurzel und Lebensdauer. `Application` erstellt Komponenten, verdrahtet sie, startet die
Message Loop und fährt sie in umgekehrter Reihenfolge herunter. Keine fachliche Minimize-/Restore-
Logik, keine ImGui-Widgets, keine JSON-Logik und keine direkten D3D-Zeichenoperationen.

### `core`

Kleine, allgemein verwendbare Infrastruktur. Nur wirklich allgemeine, benannte Komponenten.
`logger` darf nicht wieder zu `debug_log.hpp` als Header-Monolith werden. Kein Win32, ImGui oder D3D
in öffentlichen Core-Headern, sofern es nicht technisch unvermeidbar und dokumentiert ist.

### `animation`

Reine Geometrie, Easing und Mesh-Berechnung. Keine Fenster, Hooks, Geräte, Einstellungen oder UI.
Alle Ergebnisse müssen für gültige Eingaben endlich und deterministisch sein.

### `settings`

Das Datenmodell, Validierung, Normalisierung, Serialisierung, atomarer Dateizugriff und der
besitzende `SettingsService`. Die UI besitzt keine zweite unabhängige Kopie der fachlichen Wahrheit.
Die UI erhält einen View-Model-Snapshot und sendet typisierte Änderungen.

### `platform/windows`

Schmale Windows-Adapter: HWND-Zustand, Window Properties, Hook-Lebensdauer, Hotkeys, Taskbar,
Fullscreen, Energiezustand, Startup, Sessionstatus, Prozessinformationen und Single-Instance.
Keine ImGui-Ansicht und keine Genie-Mesh-Berechnung.

### `rendering`

D3D11-Gerät, Desktop Duplication, Capture, Overlay und Mesh-Rendering. Das Modul kennt keine
Settings-Seite und registriert keine Hotkeys. Geräteverlust und Wiederherstellung sind gekapselt.
Das ImGui-Rendering des Einstellungsfensters bleibt separat unter `ui/rendering`.

### `runtime`

Laufende Animationen, State Machine, Snapshot-Cache, Frame-Taktung und Renderer-Recovery. Dieses
Modul enthält Mechanik und Besitz, aber keine UI. Ein `AnimationRun` ist ein klarer Datentyp, nicht
versteckter Zustand in `Application`.

### `features`

Anwendungsfälle. `MinimizeFeature` und `RestoreFeature` orchestrieren Capture, Fensterzustand,
Overlay und Laufzeitkomponenten. `EffectPolicy` entscheidet anhand von Enabled, Pause, Fullscreen,
Battery Saver und Exclusions, ob ein Effekt erlaubt ist. Recovery und Diagnose sind eigene
Anwendungsfälle.

### `ui`

Win32-Einstellungsfenster, ImGui-Renderer, Seiten, Komponenten, Tray und Preview. Seiten zeichnen
nur ihre Seite. Das Fenster routet Nachrichten. Der Renderer besitzt Grafikressourcen. Der
Controller übersetzt zwischen View Model und `SettingsActions`.

## Erlaubte Abhängigkeitsrichtung

```text
main
  ↓
app
  ├──→ ui
  ├──→ features
  ├──→ runtime
  ├──→ settings
  └──→ platform/windows

ui ─────────────→ settings-Datentypen + settings_actions
features ───────→ runtime + settings + rendering + platform/windows
runtime ────────→ animation + rendering + platform/windows
rendering ──────→ animation + schmale Windows-Typen
settings ───────→ core + reine animation-Datentypen
platform/windows → core
animation ──────→ core/STL
core ───────────→ STL
```

Verbotene Beispiele:

- `rendering` inkludiert eine UI-Seite,
- `animation` kennt `HWND`, ImGui oder `AppSettings`,
- `SettingsWindow` inkludiert `Application`,
- `Application` zeichnet Widgets oder erzeugt Mesh-Vertices,
- ein Low-Level-Modul ruft zurück in `Application`,
- gegenseitige Includes zwischen Feature und Renderer,
- UI-Seiten greifen direkt auf D3D-Geräte oder Hook-Handles zu.

## Einheitliche Namens- und Dateiregeln

Das Repository orientiert sich bereits überwiegend am Google-C++-Stil. Vereinheitliche sämtlichen
First-Party-Code darauf:

| Element | Schreibweise | Beispiel |
| --- | --- | --- |
| Typen/Klassen/Structs | `PascalCase` | `AnimationRunPool` |
| Methoden/Funktionen | `PascalCase` | `StartAnimation()` |
| lokale Variablen | `snake_case` | `window_handle` |
| Member | `snake_case_` | `renderer_` |
| Konstanten | `kPascalCase` | `kMaximumRuns` |
| Enum-Werte | `kPascalCase` | `RunState::kAnimating` |
| Namespaces | klein | `genie::features` |
| Dateien/Ordner | `lower_snake_case` | `frame_scheduler.cpp` |

Passe auch den aktuell abweichenden UI-Motion-Code an diese Regel an. Formatiere nur
First-Party-Code mit der vorhandenen `.clang-format`.

Weitere Regeln:

- Eine wichtige Klasse erhält normalerweise genau ein `.hpp`/`.cpp`-Paar.
- Kleine, eng gekoppelte Werttypen dürfen gemeinsam in einer präzise benannten Headerdatei stehen.
- Nutze Forward Declarations, wo vollständige Typen im Header nicht nötig sind.
- Definiere Destruktoren von Klassen mit `std::unique_ptr` auf vorwärtsdeklarierten Typen außerhalb
  des Headers.
- Öffentliche Header zeigen den Vertrag, keine Implementierungssammlung.
- Idealer Richtwert: produktinterne `.cpp`-Dateien unter etwa 500 Zeilen und Header unter etwa
  250 Zeilen. Überschreitungen sind nur mit einer echten, zusammenhängenden Verantwortung erlaubt.
- `third_party` ist von diesen Größen- und Stilregeln ausgenommen.

## So soll der zentrale Code aussehen

Die Beispiele definieren Form und Verantwortungsgrenzen. Passe Signaturen an reale Anforderungen
an, ohne wieder Verantwortlichkeiten zusammenzuziehen.

### Kleiner Programmeinstieg

```cpp
#include "pch.hpp"

#include "app/application.hpp"
#include "app/process_bootstrap.hpp"

int wmain() {
  genie::app::ProcessBootstrap bootstrap;
  if (!bootstrap.Initialize()) {
    return bootstrap.exit_code();
  }

  genie::app::Application application(bootstrap.instance());
  bootstrap.SetShutdownHandler([&application] { application.RequestShutdown(); });

  if (!application.Initialize()) {
    return EXIT_FAILURE;
  }
  return application.Run();
}
```

Single-Instance, COM, DPI und Console-Control-Handler gehören in `ProcessBootstrap` beziehungsweise
den passenden Windows-Adapter. `main.cpp` enthält keine Geschäftslogik und keinen globalen
`Application*`.

### Schlanke Kompositionswurzel

```cpp
namespace genie::app {

class Application final {
public:
  explicit Application(HINSTANCE instance);
  ~Application();

  Application(const Application&) = delete;
  Application& operator=(const Application&) = delete;

  bool Initialize();
  int Run();
  void RequestShutdown();

private:
  void Shutdown();

  HINSTANCE instance_ = nullptr;
  std::unique_ptr<settings::SettingsService> settings_;
  std::unique_ptr<runtime::AnimationRunPool> runs_;
  std::unique_ptr<features::EffectController> effect_;
  std::unique_ptr<features::DiagnosticsService> diagnostics_;
  std::unique_ptr<ui::SettingsController> settings_ui_;
  std::unique_ptr<MessageLoop> message_loop_;
  bool initialized_ = false;
};

}  // namespace genie::app
```

Die echte Memberliste darf zusätzliche besitzende Komponenten enthalten. Sie darf aber nicht
wieder deren interne Zustände aufnehmen. Initialisierung ist transaktional: Wenn Schritt N
fehlschlägt, werden 1 bis N-1 sicher rückgängig gemacht. Shutdown darf mehrfach aufgerufen werden.

### Explizite Laufzeit-State-Machine

```cpp
enum class RunState {
  kIdle,
  kCapturing,
  kWaitingForNativeMinimize,
  kAnimating,
  kRestoring,
  kAborting,
  kCleaningUp,
};

struct AnimationRun {
  HWND window = nullptr;
  RunState state = RunState::kIdle;
  std::optional<WindowSnapshot> snapshot;
  std::unique_ptr<rendering::OverlayWindow> overlay;
  FrameSchedule frame_schedule;
  std::chrono::steady_clock::time_point state_entered_at{};
};
```

Erlaube nur definierte Zustandswechsel. Zentrale Transition-Funktionen protokollieren alten und
neuen Zustand. Timeout, Abbruch, Geräteverlust, zerstörtes Ziel-HWND und normaler Abschluss müssen
alle in denselben verlässlichen Cleanup-Pfad führen.

### Minimize und Restore als eigenständige Anwendungsfälle

```cpp
class MinimizeFeature final {
public:
  MinimizeFeature(rendering::DesktopCapture& capture,
                  runtime::AnimationRunPool& runs,
                  runtime::SnapshotCache& snapshots,
                  platform::WindowState& windows,
                  EffectPolicy& policy);

  bool Execute(const MinimizeRequest& request);
  void Update();
  void Cancel(HWND window);

private:
  rendering::DesktopCapture& capture_;
  runtime::AnimationRunPool& runs_;
  runtime::SnapshotCache& snapshots_;
  platform::WindowState& windows_;
  EffectPolicy& policy_;
};
```

Die Transaktion merkt sich jede Veränderung am echten Fenster. Bei Fehler oder Abbruch stellt sie
Placement, Sichtbarkeit, Transparenz, Layered-Flags und Genie-Properties wieder her. Restore nutzt
eine eigene, analoge Transaktion und dupliziert diesen Cleanup nicht.

### Ein besitzender Settings-Service

```cpp
class SettingsService final {
public:
  explicit SettingsService(SettingsRepository repository);

  bool Load();
  [[nodiscard]] const AppSettings& Get() const;
  bool Update(const SettingsChange& change);

private:
  SettingsRepository repository_;
  AppSettings settings_;
};
```

Ein Update arbeitet auf einer Kopie, validiert und normalisiert sie, speichert atomar und
veröffentlicht sie erst danach. Ladefehler, unbekannte Felder und beschädigte Dateien erhalten das
bisherige robuste Fallback-Verhalten. Easing-Namen, der bestehende Schreibfehler in historischen
Werten und andere bereits gespeicherte Werte müssen rückwärtskompatibel gelesen werden.

### Eine echte UI-Grenze statt vieler Einzelcallbacks

```cpp
class SettingsActions {
public:
  virtual ~SettingsActions() = default;

  virtual bool SetEnabled(bool enabled) = 0;
  virtual bool ApplySettings(const settings::SettingsChange& change) = 0;
  virtual HotkeyUpdateResult SetHotkey(HotkeyAction action, HotkeyBinding binding) = 0;
  virtual void SetTemporaryPause(TemporaryPauseAction action) = 0;
  virtual DiagnosticsSnapshot GetDiagnostics() const = 0;
  virtual bool ExecuteDiagnosticsAction(DiagnosticsAction action) = 0;
  virtual void RequestExit() = 0;
};
```

Diese Schnittstelle ist gerechtfertigt, weil sie UI und Anwendung trennt. Die lange
`SettingsWindow::Initialize(...)`-Callbackliste wird vollständig entfernt. `SettingsWindow` erhält
den Controller oder die `SettingsActions` als Referenz und hält keine eigene fachliche Wahrheit.

## Vollständige Migrationszuordnung

Führe für jede alte Datei eine Funktionsliste. Markiere jede Funktion erst dann als migriert, wenn
Definition, Deklaration, Aufrufer, Besitz und Cleanup im Ziel geklärt sind.

| Alte Quelle | Verbindliches Ziel |
| --- | --- |
| `main.cpp` | Nur Einstieg; Bootstrap-Logik nach `process_bootstrap` und `single_instance_guard`. |
| `app/application.hpp` | Auf kleine Kompositionswurzel reduzieren; interne Zustände in Besitzer verschieben. |
| Anonymer Namespace in `app/application.cpp` (aktuell ungefähr Zeile 28–599) | Vollständig auflösen: Produktversion und Prozessdaten nach `process_info`; eingebettete Hook-Resource, Fingerprint, Cache und Extraktion nach `cbt_hook_manager` beziehungsweise einem präzisen Resource-Helper; Window Properties und Transparenz nach `window_properties`; Placement, Monitorfläche, Bounds und Foreground-Operationen nach `window_state`; Style-/Duration-Mapping nach Animation oder Policy; Trace-Formatierung zum Logger. Kein vergleichbarer anonymer Sammelblock darf verbleiben. |
| `app/application.cpp`: Destruktor, Sessionstatus und Initialisierung (aktuell ungefähr Zeile 600–765) | Kleine `Application`-Kompositionswurzel, `session_state_store` und klarer transaktionaler Initialisierungs-/Shutdown-Ablauf. |
| `app/application.cpp`: Run-Slots, Renderer-Recovery, Message Loop, Frame-Pacing, Timer und Runtime-Policy (aktuell ungefähr Zeile 766–1496) | `MessageLoop`, `AnimationRun`, `AnimationRunPool`, `FrameScheduler`, `RendererRecovery` und `EffectPolicy`. |
| `app/application.cpp`: Pause, Hotkeys und Diagnose (aktuell ungefähr Zeile 1497–1805) | `PauseController`, `GlobalHotkeyManager`, `DiagnosticsService` und kleine ausführende Diagnoseaktionen. |
| `app/application.cpp`: Settings-Mutationen, Qualitäts-/Dauerberechnung und Exclusions (aktuell ungefähr Zeile 1806–2075) | `SettingsService`, `SettingsValidator`, `EffectPolicy`, Animation-Konfiguration und Exclusion-Regeln. |
| `app/application.cpp`: CBT-Hook-Lebensdauer (aktuell ungefähr Zeile 2076–2140) | `platform/windows/cbt_hook_manager.*`; Hook-Handle und DLL-Lebensdauer verlassen `Application`. |
| `app/application.cpp`: Minimize, Restore, Events, Snapshots und Fensterwiederherstellung (aktuell ungefähr Zeile 2141–Dateiende) | `MinimizeFeature`, `RestoreFeature`, ihre Transaktionen, `EffectController`, `SnapshotCache` und `WindowRecoveryService`. `CleanupAndRestoreAll()` wird auf Besitzer delegiert statt erneut alles selbst zu kennen. |
| `app/settings_store.*` | `app_settings`, `settings_repository`, `settings_serializer`, `settings_validator`, `settings_service`, `exclusion_rules`, `hotkey_binding`. |
| `app/startup_manager.*` | `platform/windows/startup_manager.*`. |
| `app/settings_window.cpp/.hpp`: Lebensdauer, Aktivierung, Tray und Preview (aktuell ungefähr Zeile 140–652) | Win32-Fenster nach `ui/settings_window`; Tray nach `ui/tray`; Preview nach `ui/preview`; Aktivierung einer vorhandenen Instanz an die Single-Instance-/Window-Grenze. |
| `app/settings_window.cpp/.hpp`: D3D/ImGui-Gerät, Fonts, DPI und Render-Lebensdauer (aktuell ungefähr Zeile 653–928) | `ui/rendering/imgui_renderer`; `SettingsWindow` delegiert Ressourcenaufbau, Frame-Beginn/-Ende und Device Recovery. |
| `SettingsWindow::RenderContents()` in `app/settings_window.cpp` (aktuell ungefähr Zeile 929–2210) | Shell/Navigation in `settings_controller` beziehungsweise eine kleine Shell-View; Effect, Motion, Apps, System, Hotkeys, Repair und About jeweils in ein eigenes Page-Dateipaar. Gemeinsamer Seitenkontext wird präzise typisiert. |
| `SettingsWindow::WindowProc()` und Active-App-Ermittlung in `app/settings_window.cpp` (aktuell ungefähr Zeile 2211–Dateiende) | Win32-Nachrichtenrouting bleibt im kleinen `settings_window`; aktive Prozesse nach `process_info` beziehungsweise einen dedizierten Application-List-Provider verschieben. |
| `app/settings_window.hpp` | Callback-Typen und die lange `Initialize(...)`-Signatur durch `SettingsActions`; fachlichen Zustand durch `SettingsViewModel`; Grafik-, Tray- und Preview-Zustand in deren Besitzer verschieben. |
| `app/settings_ui_widgets.*` einschließlich `Combo()` | In fachlich benannte Komponenten unter `ui/components` aufteilen, darunter `combo.*`, Controls und Easing-Editor; keine neue Widget-Sammelhalde. |
| `app/settings_ui_theme.*` und `menu/theme.hpp` | `ui/theme` und `ui/components/page_layout`; Tokens von Zeichenlogik trennen. |
| `menu/motion/*` | Nach `ui/motion`; Naming vereinheitlichen; keine fachfremde Logik ergänzen. |
| `common/debug_log.hpp` | `core/logger.hpp/.cpp`; Header klein halten. |
| `platform/window_util.*` | Vollständig auf `window_state`, `window_properties`, `process_info` und weitere konkrete Adapter verteilen; alte Datei löschen. |
| `platform/window_event_monitor.*` | Nach `platform/windows`; nur Event-Beobachtung und Callback-Vertrag. |
| `platform/native_animation_blocker.*` | Nach `platform/windows`; RAII und verlässliche Wiederherstellung. |
| `platform/taskbar_target_provider.*` | Nach `platform/windows`; Environment-Parsing von Shell-/Monitorermittlung intern klar trennen. |
| `rendering/d3d_device.*` | Behalten und Besitz-/Recovery-Vertrag schärfen. |
| `rendering/desktop_capture.*` | Desktop-Duplication-Session und eigentlichen Capture-Vorgang trennen. |
| `rendering/overlay_window.cpp/.hpp` | HWND/DirectComposition-Fenster, D3D-Renderressourcen und Mesh-Animation in `OverlayWindow`, `OverlayRenderer` und `AnimationRenderer` trennen; keine Feature-Policy. |
| `animation/*` | Plattformfrei halten; Implementierungen aus übergroßen Headern bei Bedarf in `.cpp` verschieben. |
| `hook/hook.cpp` | Separates DLL-Modul erhalten; nur aufräumen, wenn ABI und Verhalten vollständig gewahrt bleiben. |

## Implementierungsphasen ohne Tests

### Phase 0 – Vollständige Bestandsaufnahme

- Lies alle First-Party-Dateien unter `app/src`, `hook`, die Projektdateien, Resource-Datei,
  README und Architekturdokumentation.
- Erfasse alle Klassen, freien Funktionen, Win32-Callbacks, statischen Zustände, Window Properties,
  Resource-IDs, Environment-Variablen und Shutdown-/Recovery-Pfade.
- Erfasse für jede Funktion Quelle, Aufrufer, Zielkomponente und Ressourcenbesitz.
- Notiere bewusst erhaltene Sonderfälle: UIPI/elevated Prozesse, mehrere Monitore,
  Geräteverlust, Taskbar-Neustart, Single-Instance, Safe Mode, eingebettete Hook-DLL,
  erzwungener Prozessabbruch und übrig gebliebene unsichtbare Fenster.
- Nichts implementieren, bevor diese Zuordnung vollständig ist.

### Phase 1 – Konventionen und Blattmodule

- Richte die neuen echten Ordner ein.
- Teile `debug_log.hpp` in einen kleinen Logger-Vertrag und Implementierung.
- Bereinige `animation` als plattformfreies Blattmodul.
- Verschiebe UI Motion und Theme in die neue UI-Struktur und vereinheitliche das Naming.
- Aktualisiere Includes sofort auf echte Pfade. Keine temporären Include-Weiterleitungen.

### Phase 2 – Windows-Plattformadapter

- Verschiebe bestehende Plattformklassen nach `platform/windows`.
- Löse `window_util.*` nach fachlicher Verantwortung auf.
- Extrahiere Window Properties einschließlich aller bisherigen Property-Namen und
  Transparenz-/Placement-Wiederherstellung.
- Extrahiere Hook-Laden, eingebettete DLL, Cache/Fingerprint und Unload in `CbtHookManager`.
- Extrahiere globale Hotkeys, Fullscreen-Erkennung, Power-Status, Sessionstatus,
  Prozessinformationen, Startup und Single-Instance.
- Jeder Adapter besitzt seine Handles oder dokumentiert klar nicht-besitzende Handles.

### Phase 3 – Einstellungen

- Trenne Werttypen, Hotkeys und Exclusions vom Dateizugriff.
- Trenne Parser/Serializer von Repository und Service.
- Erhalte sämtliche Defaults und Rückwärtskompatibilität.
- Validierung und Normalisierung werden zentral und wiederverwendbar.
- Speichern erfolgt atomar und meldet Fehler ohne den gültigen In-Memory-Zustand zu beschädigen.
- Entferne `settings_store.*`, sobald alle Aufrufer umgestellt sind.

### Phase 4 – Laufzeitzustand

- Verschiebe `RunState`, `CachedSnapshot`, `AnimationRun` und zugehörige Container aus
  `Application`.
- Implementiere klar erlaubte State Transitions.
- Extrahiere Snapshot-Limits/Pruning, Frame Scheduling, Timer Resolution und Renderer Recovery.
- Verwende stabile IDs oder geprüfte Handles; ein zerstörtes Fenster darf keinen veralteten
  Laufzeitdatensatz weiterverwenden.
- Definiere einen gemeinsamen Cleanup-Pfad für Erfolg, Abbruch, Timeout, Device Lost und Shutdown.

### Phase 5 – Features

- Extrahiere Effekt-Policy: Enabled, temporäre Pause, Fullscreen, Battery Saver und Exclusion.
- Baue Minimize und Restore als getrennte Features mit expliziten Requests/Transactions.
- Verschiebe Window-Healing in `WindowRecoveryService`.
- Verschiebe Diagnose-Snapshot und Aktionen in `DiagnosticsService`.
- `EffectController` verbindet Event Monitor, Policy, Minimize, Restore und Updates, ohne ihre
  internen Details zu übernehmen.

### Phase 6 – Rendering

- Trenne Desktop-Duplication-Session von regionenbezogenem Capture.
- Trenne Overlay-Fenster, Swapchain/Renderer und Mesh-Animation.
- Erhalte Multi-Monitor- und Device-Lost-Verhalten.
- Vermeide doppelte D3D-Ownership. COM-Ressourcen werden durch `ComPtr` oder eindeutige RAII-Wrapper
  besessen.
- Fehler liefern verwertbare Ergebnisse und lösen den gemeinsamen Recovery-/Cleanup-Pfad aus.

### Phase 7 – UI

- Ersetze die lange Callbackliste durch die definierte UI-Aktionsgrenze.
- Trenne `SettingsWindow`, `ImGuiRenderer`, `TrayIcon`, `AnimationPreview`,
  `SettingsController` und `SettingsViewModel`.
- Verschiebe jede Seite in ihr eigenes Dateipaar.
- Komponenten enthalten wiederverwendbare Darstellung, keine Settings-Persistenz.
- Das Schließen, Close-to-Tray, Taskbar-Neustart, DPI, Fonts, reduzierte Bewegung,
  Device Recovery und Preview-Verhalten bleiben erhalten.
- Entferne die heutigen `app/settings_window.cpp/.hpp`, `app/settings_ui_widgets.*` und
  `app/settings_ui_theme.*` erst dann, wenn jeder enthaltene Funktionsblock in der neuen
  `ui`-Struktur angekommen ist. Gleichnamige neue, kleine `ui/settings_window.*`-Dateien dürfen
  anschließend nur Win32-Fensterlebensdauer und Nachrichtenrouting enthalten.

### Phase 8 – Application und Programmeinstieg

- Baue `Application` zuletzt auf die neuen Komponenten um.
- Konstruktion und Initialisierung folgen der Abhängigkeitsrichtung.
- Shutdown erfolgt exakt in sicherer, umgekehrter Reihenfolge und ist idempotent.
- Die Message Loop delegiert Updates und Rendering, enthält aber keine Feature-Implementierung.
- Verschiebe COM, DPI, Single-Instance und Console-Handler aus `main.cpp`.
- Ersetze den heutigen 3.100-Zeilen-Monolithen `app/application.cpp` durch eine kleine
  Kompositionswurzel. Der anonyme Sammel-Namespace und alle fachlichen Implementierungen müssen in
  ihre Zielmodule umgezogen sein; eine neue kleine `app/application.cpp` bleibt bestehen.

### Phase 9 – Projektmetadaten und Dokumentation

- Aktualisiere `app/GenieEffect.vcxproj` vollständig.
- Erzeuge beziehungsweise aktualisiere `app/GenieEffect.vcxproj.filters` so, dass alle Filter die
  realen Ordner spiegeln.
- Prüfe PCH-Einstellungen für neue Dateien und Ausnahmen für Third-Party.
- Bewahre App→Hook-Buildreihenfolge und Release-Resource-Einbettung.
- Aktualisiere `GenieEffect.slnx` nur soweit durch die Struktur nötig.
- Aktualisiere README-Projektbaum und `docs/architecture.md`.
- Dokumentiere Besitz, Abhängigkeitsrichtung, State Machine und Recovery-Pfade.
- Entferne alle Verweise auf gelöschte Dateien.

### Phase 10 – Endgültige Bereinigung

- Entferne alten, duplizierten, unbenutzten und auskommentierten Code.
- Entferne temporäre Adapter, Alias-Header und Migration-Shims.
- Prüfe alle First-Party-Includes auf unnötige Abhängigkeiten.
- Stelle sicher, dass keine Produktionsdatei wieder zur Sammeldatei geworden ist.
- Formatiere alle geänderten First-Party-C++-Dateien.
- Führe noch keinen Build und keinen Test aus.

## Gate „Produktionsumbau vollständig“

Bevor der erste Test beginnen darf, bestätige durch reine Quellprüfung jeden Punkt:

- [ ] Jede alte Funktion steht im Migrationsprotokoll und wurde genau einmal migriert oder
      nachvollziehbar entfernt.
- [ ] `Application` ist nur Kompositionswurzel/Lebensdauer.
- [ ] `main.cpp` ist nur Programmeinstieg.
- [ ] Der große anonyme Hilfsbereich und die fachlichen Blöcke der heutigen
      `app/application.cpp` sind vollständig aufgelöst; nur eine kleine Kompositionswurzel trägt
      diesen Dateinamen weiter.
- [ ] `window_util.*` und die heutigen UI-/Rendering-Monolithen sind vollständig aufgelöst.
- [ ] Settings-Modell, Validierung, Serialisierung, Repository und Service sind getrennt.
- [ ] Minimize, Restore, Policy, Recovery und Diagnostics sind getrennte Features.
- [ ] Laufzeitzustand und State Machine liegen außerhalb von `Application`.
- [ ] UI-Fenster, UI-Renderer, Tray, Preview, Controller, Pages und Components sind getrennt.
- [ ] D3D-Animation und ImGui-Rendering sind getrennt.
- [ ] Keine Änderung liegt in `third_party`.
- [ ] Projektdatei und Filter enthalten exakt die real vorhandenen Produktionsdateien.
- [ ] Dokumentation beschreibt den neuen Stand.
- [ ] Keine Stubs, Übergangslösungen, offenen TODOs oder doppelten Implementierungen.
- [ ] Keine bekannten offenen Strukturarbeiten.

Erst wenn alle Kästchen wahr sind, beginne mit Tests.

## Testphase – erst jetzt ausführen

### 1. Struktur- und Build-Prüfung

1. Prüfe Projektdateien auf fehlende und verwaiste Einträge.
2. Führe einen sauberen Build der gesamten Solution für `Debug|x64` aus.
3. Behebe sämtliche Compiler-, Linker- und Ressourcenfehler.
4. Wiederhole den sauberen Debug-Build.
5. Führe einen sauberen Build der gesamten Solution für `Release|x64` aus.
6. Behebe sämtliche Fehler und relevanten Warnungen.
7. Wiederhole anschließend Debug und Release, damit eine Release-Reparatur Debug nicht
   beschädigt hat.
8. Verifiziere, dass EXE und Hook-DLL am dokumentierten Ausgabeort liegen.

Ein erfolgreicher Build ist noch kein abgeschlossenes Ergebnis.

### 2. Automatisierte Tests hinzufügen und ausführen

Lege jetzt `tests/GenieEffect.Tests.vcxproj` an und füge es der Solution hinzu. Verwende einen
kleinen, repository-eigenen Test-Runner, wenn kein bereits nutzbares Framework vorhanden ist.
Führe mindestens diese Tests aus:

#### Animation

- Easing an 0 und 1 sowie Werte dazwischen.
- Clamping beziehungsweise definiertes Verhalten außerhalb des Bereichs.
- Custom Cubic Bezier, ungültige und grenzwertige Kontrollpunkte.
- Mesh für alle Styles und Qualitäts-/Segmentstufen.
- Korrekte Vertex-/Indexanzahl.
- Keine NaN-/Inf-Werte.
- Korrekte Start- und Endgeometrie.
- Sehr kleine, große, negative und monitorübergreifende Koordinaten.

#### Settings

- Defaultwerte bei fehlender Datei.
- Roundtrip aller Felder.
- Rückwärtskompatibilität bestehender Werte und Schreibweisen.
- Unbekannte Felder.
- Beschädigte, leere und teilweise geschriebene Datei.
- Validierung und Clamping.
- Atomarer Save und simuliertes Save-Fehlschlagen.
- Exclusion-Normalisierung, Groß-/Kleinschreibung, Pfade, Duplikate und ungültige Eingaben.
- Hotkey-Gültigkeit und Duplikaterkennung.

#### Policy und State Machine

- Alle Kombinationen aus Enabled, Pause, Fullscreen, Battery Saver und Exclusion.
- Jeder erlaubte State-Übergang.
- Jeder verbotene State-Übergang.
- Timeout, Cancel, zerstörtes HWND, Device Lost und Shutdown.
- Snapshot-Limit, Ersetzung, Pruning und Prozess-ID-Abgleich.
- Mehrere gleichzeitige AnimationRuns und Slot-Wiederverwendung.

#### Plattformnahe, deterministische Logik

- Parsing von `GENIE_TASKBAR_RECT`.
- Rect-Clipping und Monitor-/Taskbar-Zielberechnung, soweit ohne reale Shell testbar.
- Session-State-Parsing.
- Hook-Resource-Fingerprint und Cache-Pfadlogik.
- Window-Property-Encode/Decode-Helfer, soweit isolierbar.

Alle Tests müssen wiederholbar sein und temporäre Daten selbst aufräumen.

### 3. Integrations- und Laufzeittests

Führe anschließend mindestens Folgendes aus:

- Normaler Start und sauberer Exit.
- Zweiter Programmstart aktiviert die bestehende Instanz und beendet sich selbst.
- Start mit fehlender Settings-Datei.
- Start mit beschädigter Settings-Datei.
- Settings ändern, neu starten und Persistenz verifizieren.
- Tray-Icon hinzufügen, Taskbar/Explorer-Neustart behandeln und Icon wiederherstellen.
- Hook-DLL neben der EXE laden.
- Release-Pfad mit eingebetteter Hook-Resource prüfen.
- Safe-Mode-/Session-Recovery-Pfad.
- Übrig gebliebene versteckte/transparente Fenster reparieren.
- Kontrollierter Device-Lost-Test über `GENIE_TEST_DEVICE_RECOVERY=1`.
- Mehrere Animationen nacheinander und gleichzeitig.
- Abbruch während Capture, Warten, Animation und Restore.
- Anwendung während aktiver Animation beenden und vollständige Fensterwiederherstellung prüfen.
- Debug-Log auf Fehler, wiederholte Recovery-Schleifen und Resource-Leaksymptome prüfen.

Bei jedem Fehler:

1. Ursache bis zur besitzenden Komponente verfolgen.
2. Korrekt beheben, nicht mit Delay-Erhöhung oder Fehlerunterdrückung kaschieren.
3. Den direkt betroffenen Test wiederholen.
4. Alle Tests des betroffenen Moduls wiederholen.
5. Debug- und Release-Build erneut ausführen.
6. Die vollständige automatisierte Suite erneut ausführen.

## Abschließender Computer-Use-Test

Nach vollständig grünen automatisierten und Integrations-Tests ist zwingend ein echter visueller
Test mit dem Computer-Use-Werkzeug durchzuführen. Ein Shell-Start oder das bloße Prüfen eines
Prozesses ersetzt diesen Schritt nicht. Lies vor der Bedienung die vorhandenen Computer-Use-
Anweisungen und steuere die reale Windows-Oberfläche.

### Vorbereitung

- Verwende den frisch gebauten Release-Stand.
- Sorge dafür, dass keine alte Genie-Effect-Instanz mehr läuft.
- Sichere für den Test nötige Benutzereinstellungen und stelle sie anschließend bei Bedarf wieder
  her.
- Öffne das Debug-/Laufzeitlog zur späteren Kontrolle.
- Verwende mindestens Notepad und Explorer als ungefährliche Testfenster.

### Visuelle Prüfliste

- [ ] Anwendung startet ohne sichtbaren Fehler oder hängen gebliebenes Overlay.
- [ ] Einstellungsfenster lässt sich öffnen, verschieben, schließen und erneut über Tray öffnen.
- [ ] Single-Instance-Aktivierung bringt das existierende Fenster nach vorn.
- [ ] Alle Seiten sind erreichbar und ohne abgeschnittene/überlappende Inhalte.
- [ ] DPI/Skalierung, Fonts, Scrollen, Combos, Toggle, Slider und Buttons reagieren korrekt.
- [ ] General-/Effect-Einstellungen funktionieren.
- [ ] Motion-Dauern, getrennte/gekoppelte Geschwindigkeit, Easing, Custom Bezier, Style,
      Qualität, Strength, Fade und Target Indicator funktionieren und bleiben gespeichert.
- [ ] Animation Preview startet, läuft, kann wiederholt werden und hinterlässt kein Fenster.
- [ ] Exclusion hinzufügen/entfernen funktioniert; ein ausgeschlossenes Programm animiert nicht.
- [ ] Hotkeys lassen sich setzen, Konflikte werden angezeigt und Aktionen funktionieren.
- [ ] Temporäre Pause für die angebotenen Zeiträume und Resume funktionieren.
- [ ] Diagnostics/Repair zeigt plausible Werte; Copy/Open Log/Repair/Renderer Restart funktionieren.
- [ ] Startup- und Close-/Tray-Verhalten funktionieren entsprechend der Auswahl.
- [ ] Notepad minimiert mit sichtbarem Genie-Effekt zum richtigen Taskbar-Ziel.
- [ ] Notepad wird mit sichtbarem Restore-Effekt korrekt wiederhergestellt.
- [ ] Explorer minimiert und restored ebenfalls korrekt.
- [ ] Maximierte und normale Fenster behalten Größe und Placement.
- [ ] Zwei oder mehr Fenster können schnell hintereinander ohne Hänger animieren.
- [ ] Abbruch/rasches Minimize-Restore hinterlässt kein unsichtbares, transparentes oder
      offscreen liegendes Fenster.
- [ ] Während einer Animation beenden: Zielanwendung wird vollständig wiederhergestellt.
- [ ] Kein schwarzes Overlay, Flackern, dauerhaftes Topmost-Fenster oder Eingabeblock.
- [ ] Tray-Icon und Einstellungsfenster funktionieren nach dem Renderer-Recovery-Test weiter.
- [ ] Nach Neustart sind Einstellungen erhalten.
- [ ] Nach dem Test zeigt das Log keine ungeklärten Fehler, Endlosschleifen oder wiederholte
      Device-Recovery.

Wenn mehrere Monitore vorhanden sind, teste auf jedem Monitor und zwischen Monitoren. Wenn kein
zweiter Monitor, kein Battery-Saver-Zustand oder kein sicher testbares erhöhtes Fenster verfügbar
ist, erfinde kein positives Ergebnis. Dokumentiere diese konkrete Umgebungsgrenze; alle
verfügbaren Varianten müssen trotzdem vollständig getestet werden.

### Zufriedenheitsschleife

Nach dem ersten Computer-Use-Durchgang:

1. Bewerte Funktion, visuelle Stabilität, Fensterwiederherstellung, Log und Prozesszustand.
2. Behebe jedes beobachtete Problem.
3. Führe danach mindestens die betroffenen automatisierten Tests aus.
4. Führe Debug- und Release-Build erneut aus.
5. Führe die vollständige automatisierte Testsuite erneut aus.
6. Wiederhole den vollständigen Computer-Use-Ablauf, nicht nur den zuvor fehlerhaften Klick.
7. Höre erst auf, wenn ein kompletter Durchgang ohne ungeklärtes Problem abgeschlossen ist.

## Abschlussbericht

Der finale Bericht muss kurz, aber beweisbar enthalten:

- welche alten Monolithen entfernt und welche Zielmodule geschaffen wurden,
- ob Verhalten oder Dateiformat bewusst kompatibel gehalten wurden,
- Ergebnis von Debug- und Release-Build,
- Anzahl und Ergebnis automatisierter Tests,
- Ergebnis der Integrationsprüfungen,
- Ergebnis des Computer-Use-Tests einschließlich getesteter Fenster und Szenarien,
- kontrollierte Logdatei und relevante Beobachtungen,
- eventuell objektiv nicht testbare Umgebungsvarianten,
- Bestätigung, dass keine TODOs, Übergangsshims oder bekannten offenen Probleme verblieben sind.

Melde nicht „fertig“, solange noch ein Fehler, ein ausgelassener verfügbarer Test, ein alter
Monolith oder eine bekannte unvollständige Migration existiert.
