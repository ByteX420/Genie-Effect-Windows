#include "pch.hpp"

#include "core/environment.hpp"

#include <cstring>

namespace minimize::core {

bool EnvironmentFlagEnabled(std::string_view name) noexcept {
  if (name.empty() || name.size() >= 256) {
    return false;
  }

  char stack_name[256];
  std::memcpy(stack_name, name.data(), name.size());
  stack_name[name.size()] = '\0';

  char value[8]{};
  const DWORD length =
      GetEnvironmentVariableA(stack_name, value, static_cast<DWORD>(sizeof(value)));
  if (length == 0 || length >= sizeof(value)) {
    return false;
  }

  const char c = value[0];
  return c == '1' || c == 'y' || c == 'Y' || c == 't' || c == 'T';
}

}  // namespace minimize::core
