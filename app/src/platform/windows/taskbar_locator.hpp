#pragma once

#include <windows.h>

namespace genie::platform {

[[nodiscard]] HWND FindTaskbarWindowForRect(const RECT& rect);

}  // namespace genie::platform
