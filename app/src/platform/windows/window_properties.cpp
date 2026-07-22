#include "pch.hpp"

#include "platform/windows/window_properties.hpp"

namespace minimize::platform::windows::properties {
namespace {

bool IsUsableRect(const RECT& rect) {
  return rect.right > rect.left && rect.bottom > rect.top && rect.left > -30000 &&
         rect.top > -30000;
}

}  // namespace

void StoreOriginalPlacement(HWND window, const RECT& rect) {
  SetPropW(window, kOriginalPlacementLeft,
           reinterpret_cast<HANDLE>(static_cast<INT_PTR>(rect.left)));
  SetPropW(window, kOriginalPlacementTop, reinterpret_cast<HANDLE>(static_cast<INT_PTR>(rect.top)));
  SetPropW(window, kOriginalPlacementRight,
           reinterpret_cast<HANDLE>(static_cast<INT_PTR>(rect.right)));
  SetPropW(window, kOriginalPlacementBottom,
           reinterpret_cast<HANDLE>(static_cast<INT_PTR>(rect.bottom)));
}

std::optional<RECT> ReadOriginalPlacement(HWND window) {
  RECT rect{
      .left =
          static_cast<LONG>(reinterpret_cast<INT_PTR>(GetPropW(window, kOriginalPlacementLeft))),
      .top = static_cast<LONG>(reinterpret_cast<INT_PTR>(GetPropW(window, kOriginalPlacementTop))),
      .right =
          static_cast<LONG>(reinterpret_cast<INT_PTR>(GetPropW(window, kOriginalPlacementRight))),
      .bottom =
          static_cast<LONG>(reinterpret_cast<INT_PTR>(GetPropW(window, kOriginalPlacementBottom))),
  };
  return IsUsableRect(rect) ? std::optional<RECT>(rect) : std::nullopt;
}

void StoreWasMaximized(HWND window, bool was_maximized) {
  if (was_maximized) {
    SetPropW(window, kWasMaximized, reinterpret_cast<HANDLE>(1));
  } else {
    RemovePropW(window, kWasMaximized);
  }
}

bool HasMinimizeState(HWND window) {
  return GetPropW(window, kMovedOffscreen) != nullptr ||
         GetPropW(window, kTransparencySaved) != nullptr ||
         GetPropW(window, kOriginalExtendedStyle) != nullptr ||
         GetPropW(window, kOriginalPlacementLeft) != nullptr;
}

void ClearMinimizeState(HWND window) {
  if (!IsWindow(window)) return;
  RestoreTransparency(window);
  RemovePropW(window, kOriginalPlacementLeft);
  RemovePropW(window, kOriginalPlacementTop);
  RemovePropW(window, kOriginalPlacementRight);
  RemovePropW(window, kOriginalPlacementBottom);
  RemovePropW(window, kMovedOffscreen);
  RemovePropW(window, kWasMaximized);
  RemovePropW(window, kIsMinimizing);
  RemovePropW(window, kAllowMinimize);
  RemovePropW(window, kAllowRestore);
  RemovePropW(window, kExcludedApplication);
}

bool MakeTransparent(HWND window) {
  if (!IsWindow(window)) return false;
  if (GetPropW(window, kTransparencySaved) != nullptr) return true;

  const LONG_PTR extended_style = GetWindowLongPtrW(window, GWL_EXSTYLE);
  if (!SetPropW(window, kTransparencySaved, reinterpret_cast<HANDLE>(1))) return false;
  if (extended_style != 0 &&
      !SetPropW(window, kOriginalExtendedStyle, reinterpret_cast<HANDLE>(extended_style))) {
    RemovePropW(window, kTransparencySaved);
    return false;
  }

  BYTE alpha = 255;
  DWORD flags = 0;
  const bool was_layered = (extended_style & WS_EX_LAYERED) != 0;
  if (was_layered) GetLayeredWindowAttributes(window, nullptr, &alpha, &flags);
  SetPropW(window, kWasLayered,
           reinterpret_cast<HANDLE>(static_cast<INT_PTR>(was_layered ? 1 : 0)));
  SetPropW(window, kOriginalAlpha, reinterpret_cast<HANDLE>(static_cast<INT_PTR>(alpha)));
  SetPropW(window, kOriginalFlags, reinterpret_cast<HANDLE>(static_cast<INT_PTR>(flags)));

  SetLastError(ERROR_SUCCESS);
  const LONG_PTR updated_style =
      SetWindowLongPtrW(window, GWL_EXSTYLE, extended_style | WS_EX_LAYERED);
  if (updated_style == 0 && GetLastError() != ERROR_SUCCESS) {
    RestoreTransparency(window);
    return false;
  }
  if (!SetLayeredWindowAttributes(window, 0, 0, LWA_ALPHA)) {
    RestoreTransparency(window);
    return false;
  }
  return true;
}

void RestoreTransparency(HWND window) {
  if (!IsWindow(window)) return;
  if (GetPropW(window, kTransparencySaved) == nullptr &&
      GetPropW(window, kOriginalExtendedStyle) == nullptr) {
    return;
  }

  const bool was_layered = reinterpret_cast<INT_PTR>(GetPropW(window, kWasLayered)) != 0;
  const BYTE alpha = static_cast<BYTE>(reinterpret_cast<INT_PTR>(GetPropW(window, kOriginalAlpha)));
  const DWORD flags =
      static_cast<DWORD>(reinterpret_cast<INT_PTR>(GetPropW(window, kOriginalFlags)));
  const LONG_PTR current_style = GetWindowLongPtrW(window, GWL_EXSTYLE);
  if (was_layered) {
    SetWindowLongPtrW(window, GWL_EXSTYLE, current_style | WS_EX_LAYERED);
    SetLayeredWindowAttributes(window, 0, alpha, flags);
  } else {
    SetWindowLongPtrW(window, GWL_EXSTYLE, current_style & ~WS_EX_LAYERED);
  }

  RemovePropW(window, kTransparencySaved);
  RemovePropW(window, kOriginalExtendedStyle);
  RemovePropW(window, kWasLayered);
  RemovePropW(window, kOriginalAlpha);
  RemovePropW(window, kOriginalFlags);
}

}  // namespace minimize::platform::windows::properties
