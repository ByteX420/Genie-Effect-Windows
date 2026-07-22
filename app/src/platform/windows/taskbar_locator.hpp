#pragma once

#include <windows.h>

namespace minimize::platform {

[[nodiscard]] HWND FindTaskbarWindowForRect(const RECT& rect);

}  // namespace minimize::platform
