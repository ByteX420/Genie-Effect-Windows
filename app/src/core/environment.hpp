#pragma once

#include <string_view>

namespace genie::core {

[[nodiscard]] bool EnvironmentFlagEnabled(std::string_view name);

}  // namespace genie::core
