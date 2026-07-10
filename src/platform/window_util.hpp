#pragma once

#include <optional>
#include <vector>
#include <windows.h>

namespace genie::platform {

[[nodiscard]] std::optional<RECT> GetExtendedFrameBounds(HWND window);
[[nodiscard]] bool IsInterestingTopLevelWindow(HWND window, HWND ignored_window = nullptr);
[[nodiscard]] std::vector<HWND> EnumerateTopLevelWindows(HWND ignored_window = nullptr);
void SetDwmTransitionsDisabled(HWND window, bool disabled);
void SetWindowCloaked(HWND window, bool cloaked);
bool SetOwnedWindowRegion(HWND window, HRGN region, bool redraw);
[[nodiscard]] RECT GetVirtualScreenRect();
[[nodiscard]] HWND FindTaskbarWindowForRect(const RECT& rect);
[[nodiscard]] std::optional<double> GetMonitorRefreshRateHz(HMONITOR monitor);
bool GrantAppContainerPermissions(const std::wstring& path);

}  // namespace genie::platform
