#pragma once

#include <string>

namespace genie::platform {

[[nodiscard]] bool GrantAppContainerPermissions(const std::wstring& path);

}  // namespace genie::platform
