#pragma once

#include <string>
#include <vector>

namespace genie::ui {

class ApplicationListProvider final {
public:
  [[nodiscard]] std::vector<std::string> GetActiveApplications() const;
};

}  // namespace genie::ui
