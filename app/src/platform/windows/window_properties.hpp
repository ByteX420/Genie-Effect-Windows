#pragma once

#include <optional>
#include <windows.h>

namespace genie::platform::windows::properties {

inline constexpr wchar_t kAllowMinimize[] = L"GenieAllowMinimize";
inline constexpr wchar_t kAllowRestore[] = L"GenieAllowRestore";
inline constexpr wchar_t kExcludedApplication[] = L"GenieExcludedApplication";
inline constexpr wchar_t kIsMinimizing[] = L"GenieIsMinimizing";
inline constexpr wchar_t kOriginalPlacementLeft[] = L"GenieOriginalPlacementLeft";
inline constexpr wchar_t kOriginalPlacementTop[] = L"GenieOriginalPlacementTop";
inline constexpr wchar_t kOriginalPlacementRight[] = L"GenieOriginalPlacementRight";
inline constexpr wchar_t kOriginalPlacementBottom[] = L"GenieOriginalPlacementBottom";
inline constexpr wchar_t kMovedOffscreen[] = L"GenieMovedOffscreen";
inline constexpr wchar_t kWasMaximized[] = L"GenieWasMaximized";
inline constexpr wchar_t kTransparencySaved[] = L"GenieTransparencySaved";
inline constexpr wchar_t kOriginalExtendedStyle[] = L"GenieOriginalExStyle";
inline constexpr wchar_t kWasLayered[] = L"GenieWasLayered";
inline constexpr wchar_t kOriginalAlpha[] = L"GenieOriginalAlpha";
inline constexpr wchar_t kOriginalFlags[] = L"GenieOriginalFlags";

void StoreOriginalPlacement(HWND window, const RECT& rect);
[[nodiscard]] std::optional<RECT> ReadOriginalPlacement(HWND window);
void StoreWasMaximized(HWND window, bool was_maximized);
[[nodiscard]] bool HasGenieState(HWND window);
void ClearGenieState(HWND window);
[[nodiscard]] bool MakeTransparent(HWND window);
void RestoreTransparency(HWND window);

}  // namespace genie::platform::windows::properties
