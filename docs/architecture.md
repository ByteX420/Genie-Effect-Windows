# Windows 11 Genie Minimize Effect

This project is a native Windows 11 prototype for replacing the visible minimize
transition with a GPU-rendered Genie-style mesh animation.

## Public API boundary

Windows does not expose a documented API that says "replace this DWM minimize
animation before the compositor starts it." The implementation therefore uses
the strongest public API path:

- `SetWinEventHook(EVENT_SYSTEM_MINIMIZESTART)` detects the minimize event.
- `DwmSetWindowAttribute(DWMWA_TRANSITIONS_FORCEDISABLED)` is applied to
  top-level windows.
- `SystemParametersInfo(SPI_SETANIMATION)` disables the classic shell window
  animation while the process is running, then restores the previous setting on
  exit.
- DXGI Desktop Duplication captures the visible desktop region directly into an
  `ID3D11Texture2D`.
- DirectComposition hosts a transparent topmost overlay backed by a D3D11 swap
  chain.
- A D3D11 textured mesh renders the Genie deformation.

For a production-grade, perfectly pre-DWM replacement, the missing layer is an
invasive shell/DWM interception component. This scaffold keeps that boundary
isolated behind `platform/native_animation_blocker.*` and
`platform/window_event_monitor.*`.

## Folder layout

- `app/src/app`: application orchestration and message loop.
- `app/src/animation`: platform-independent Genie mesh math.
- `app/src/platform`: Win32, DWM, shell taskbar, and event monitoring.
- `app/src/rendering`: Direct3D 11, DXGI capture, DirectComposition overlay.
- `app/shaders`: HLSL reference shader source.
- `hook`: the separate CBT hook DLL project.

## Custom taskbar target

By default the target rectangle is derived from `SHAppBarMessage` and points at
the Windows taskbar. A custom taskbar can set:

```powershell
$env:GENIE_TASKBAR_RECT = "100,980,1820,1070"
```

The format is `left,top,right,bottom` in physical screen coordinates.

## Reference

The animation timing and mesh-shaping logic are adapted from Harshil Shah's
MIT-licensed `Genie` SpriteKit playground:

https://github.com/HarshilShah/Genie
