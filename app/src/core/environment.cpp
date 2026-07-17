#include "pch.hpp"

#include "core/environment.hpp"

#include <array>
#include <string>

namespace genie::core {

bool EnvironmentFlagEnabled(std::string_view name) {
  if (name.empty()) {
    return false;
  }

  const std::string variable_name(name);
  std::array<char, 2> value{};
  if (GetEnvironmentVariableA(variable_name.c_str(), value.data(),
                              static_cast<DWORD>(value.size())) == 0) {
    return false;
  }

  return value[0] == '1' || value[0] == 'y' || value[0] == 'Y' || value[0] == 't' ||
         value[0] == 'T';
}

}  // namespace genie::core
