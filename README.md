# GenieEffect

GenieEffect is a Windows desktop prototype that replaces the native minimize and restore transition with a macOS-style Genie animation rendered through Direct3D 11 and DirectComposition.

## Requirements

- Windows 10 or newer
- Visual Studio 2026 or newer with MSBuild and the Desktop development with C++ workload
- x64 build target

## Build

Open `GenieEffect.slnx` in Visual Studio or build from a Developer PowerShell:

```powershell
MSBuild.exe GenieEffect.slnx /p:Configuration=Release /p:Platform=x64 /m
```

The app project builds the hook DLL first. Build outputs are written to:

```text
build\bin\x64\Debug\
build\bin\x64\Release\
```

Intermediate files are written to:

```text
build\obj\App\x64\<Configuration>\
build\obj\Hook\x64\<Configuration>\
```

## Runtime Notes

`GenieEffect.exe` loads `GenieHookPost.dll` from the same output folder. Running elevated lets the hook observe elevated windows too; without elevation, Windows UIPI prevents hooking higher-integrity processes.

Debug builds write diagnostics to:

```text
%LOCALAPPDATA%\GenieEffect\genie_debug.log
```

Set `GENIE_DEBUG_LOG` to override the log path. Set `GENIE_TRACE=1` only when detailed timing traces are needed. Set `GENIE_LOG_SYNC=1` only when you need every log line flushed immediately after hangs or crashes.

Debug builds also support `GENIE_TEST_DEVICE_RECOVERY=1`. It performs one controlled teardown and recreation of both D3D renderers after startup so the recovery path can be smoke-tested without resetting the graphics driver.
