#pragma once

#include <string>

namespace minimize::platform {

[[nodiscard]] bool GrantAppContainerPermissions(const std::wstring& path);

}  // namespace minimize::platform
