#pragma once

#include <string_view>

namespace minimize::core {

[[nodiscard]] bool EnvironmentFlagEnabled(std::string_view name) noexcept;

}  // namespace minimize::core
