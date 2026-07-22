#pragma once

#include <optional>
#include <windows.h>

namespace minimize::platform::windows::properties {

inline constexpr wchar_t kAllowMinimize[] = L"MinimizeAllowMinimize";
inline constexpr wchar_t kAllowRestore[] = L"MinimizeAllowRestore";
inline constexpr wchar_t kExcludedApplication[] = L"MinimizeExcludedApplication";
inline constexpr wchar_t kIsMinimizing[] = L"MinimizeIsMinimizing";
inline constexpr wchar_t kOriginalPlacementLeft[] = L"MinimizeOriginalPlacementLeft";
inline constexpr wchar_t kOriginalPlacementTop[] = L"MinimizeOriginalPlacementTop";
inline constexpr wchar_t kOriginalPlacementRight[] = L"MinimizeOriginalPlacementRight";
inline constexpr wchar_t kOriginalPlacementBottom[] = L"MinimizeOriginalPlacementBottom";
inline constexpr wchar_t kMovedOffscreen[] = L"MinimizeMovedOffscreen";
inline constexpr wchar_t kWasMaximized[] = L"MinimizeWasMaximized";
inline constexpr wchar_t kTransparencySaved[] = L"MinimizeTransparencySaved";
inline constexpr wchar_t kOriginalExtendedStyle[] = L"MinimizeOriginalExStyle";
inline constexpr wchar_t kWasLayered[] = L"MinimizeWasLayered";
inline constexpr wchar_t kOriginalAlpha[] = L"MinimizeOriginalAlpha";
inline constexpr wchar_t kOriginalFlags[] = L"MinimizeOriginalFlags";

void StoreOriginalPlacement(HWND window, const RECT& rect);
[[nodiscard]] std::optional<RECT> ReadOriginalPlacement(HWND window);
void StoreWasMaximized(HWND window, bool was_maximized);
[[nodiscard]] bool HasMinimizeState(HWND window);
void ClearMinimizeState(HWND window);
[[nodiscard]] bool MakeTransparent(HWND window);
void RestoreTransparency(HWND window);

}  // namespace minimize::platform::windows::properties
