# Authoritative Phase-0 inventory: `common/debug_log.hpp`

This file was read in full. It is a 222-line header-only implementation shared by the app and hook
DLL. No build, test, or program run was performed.

| Old symbol/state | Current callers and behavior | Required target | Ownership / cleanup |
| --- | --- | --- | --- |
| `GenieEnvFlagEnabled()` | Logger flags plus app/settings-renderer controlled device-recovery tests | Logger-private flag parsing for `GENIE_TRACE`/`GENIE_LOG_SYNC`; a precise `platform/windows/environment` function for non-logging flags | Reads a borrowed environment-variable name into a stack buffer. Must not remain a generic global supplied by the logger. |
| `IsTraceLoggingEnabled()` | Hook, overlay, and logging macros | `core::IsTraceLoggingEnabled()` in `logger.cpp/.hpp` | Debug-only, process-local cached value from `GENIE_TRACE`; Release always false. |
| `IsSynchronousLoggingEnabled()` | log write path | Private `LoggerState` setting in `logger.cpp` | Debug-only, process-local cached value from `GENIE_LOG_SYNC`; Release false. |
| `GenieDebugLogPath()` | application startup display, diagnostics/open-log, cleanup, logger | `core::DebugLogPath()` | Process-local cached string. `GENIE_DEBUG_LOG` wins; fallback expands `%LOCALAPPDATA%\GenieEffect\genie_debug.log`; final fallback `genie_debug.log`. Creates the immediate parent directory. |
| `GenieLogFolderSize()` | diagnostics snapshot and log cleanup | `core::DebugLogFolderSize()` | Enumerates regular files whose names begin `genie_debug`; no retained handles. Error-code based failure remains non-throwing. |
| `CleanupGenieLogs()` | app initialization | `core::CleanupDebugLogs()` | Defaults: maximum 5 files and 10 MiB total. Rotates active log over 2 MiB to timestamped name, sorts old logs oldest-first, deletes until limits hold. No open logger handle exists at current startup call. |
| `GenieLogToUtf8()` | line writer | Private logger implementation | UTF-16 to UTF-8 using `WideCharToMultiByte`; owns temporary string only. |
| `GenieLogProcessName()` | line prefix | Private logger implementation | Process-local cached basename from `GetModuleFileNameW`. |
| `GenieLogFileHandle()` static `HANDLE` | line writer | Private RAII `LoggerState` in `logger.cpp` | Lazily owns append handle. Current implementation closes only after write failure and otherwise relies on process teardown. New logger must expose/use idempotent shutdown and close the handle deterministically. |
| `GenieLogFileLock()` static `SRWLOCK` | line writer | Private logger implementation | Process-local synchronization primitive; not a service locator or public handle. |
| `LogDebugLine()` | all app/platform/rendering/settings diagnostics and hook DLL | `core::Log()` implementation behind small `logger.hpp` contract | Debug-only. Prefix includes local timestamp with milliseconds, process name/PID/TID, level and module. Opens with append plus read/write/delete sharing, writes UTF-8 BOM to an empty file, appends CRLF, optionally flushes, and closes/resets on write failure. |
| `LogDebug` macro | all first-party modules and hook | Small zero-overhead Debug/Release logging contract in `core/logger.hpp` | Preserve Release compile-out behavior and existing message content while moving state/implementation to `.cpp`. |
| `LogTrace` macro | verbose app/hook/render/platform traces | Small trace contract in `core/logger.hpp` | Must consult cached trace flag in Debug and compile out in Release. |

## Cross-project constraint

`hook/hook.cpp` currently includes the same header-only implementation. Once implementation moves to
`core/logger.cpp`, both `GenieEffect.vcxproj` and `GenieHook.vcxproj` must compile/link that source (or
an equally real shared implementation) so the header does not become a forwarding shim. The two
processes/DLL contexts retain independent function-local logger state and append safely using the
existing sharing flags and SRW lock within each module.

## Required compatibility and safety

- Preserve environment variables `GENIE_DEBUG_LOG`, `GENIE_TRACE`, and `GENIE_LOG_SYNC`.
- Preserve the default log path, UTF-8 BOM/encoding, CRLF line format, timestamp/process/PID/TID/
  level/module prefix, rotation thresholds, and Debug-only logging behavior.
- Logger public headers must not expose `HANDLE`, `SRWLOCK`, `<windows.h>`, filesystem streams, or
  implementation globals.
- App shutdown must close its owned log handle idempotently after all component shutdown logging.
- Hook unload must not perform unsafe complex work under the loader lock; its logger state must at
  least release automatically without introducing calls back into the app.
