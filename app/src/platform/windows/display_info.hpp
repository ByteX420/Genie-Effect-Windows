#pragma once

#include <optional>
#include <windows.h>

namespace genie::platform {

[[nodiscard]] RECT GetVirtualScreenRect();
[[nodiscard]] std::optional<double> GetMonitorRefreshRateHz(HMONITOR monitor);
[[nodiscard]] std::optional<RECT> GetMonitorWorkArea(
    HWND window, const std::optional<RECT>& fallback = std::nullopt);
[[nodiscard]] bool IsFullscreenForegroundWindow(HWND ignored_window = nullptr,
                                                HWND second_ignored_window = nullptr);

}  // namespace genie::platform
