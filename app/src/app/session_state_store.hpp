#pragma once

#include <string>
#include <string_view>

namespace genie::app {

class SessionStateStore final {
public:
  [[nodiscard]] std::string Read() const;
  [[nodiscard]] bool Write(std::string_view state) const;
};

}  // namespace genie::app
