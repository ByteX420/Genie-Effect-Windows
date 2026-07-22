#pragma once

#include <string>
#include <vector>

namespace minimize::ui {

class ApplicationListProvider final {
public:
  [[nodiscard]] std::vector<std::string> GetActiveApplications() const;
};

}  // namespace minimize::ui
