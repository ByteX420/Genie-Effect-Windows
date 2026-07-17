#include "pch.hpp"

#include "ui/application_list_provider.hpp"

#include <algorithm>
#include <unordered_set>

#include "platform/windows/process_info.hpp"
#include "platform/windows/window_state.hpp"
#include "settings/exclusion_rules.hpp"

namespace genie::ui {

std::vector<std::string> ApplicationListProvider::GetActiveApplications() const {
  std::unordered_set<std::string> unique_applications;
  for (HWND window : platform::EnumerateTopLevelWindows()) {
    if (!platform::IsInterestingTopLevelWindow(window)) continue;
    const std::optional<std::string> executable = platform::GetWindowExecutableName(window);
    if (!executable.has_value() || executable->empty()) continue;
    const std::optional<std::string> normalized = settings::NormalizeExecutableName(*executable);
    if (normalized.has_value()) unique_applications.insert(*normalized);
  }
  std::vector<std::string> result(unique_applications.begin(), unique_applications.end());
  std::sort(result.begin(), result.end());
  return result;
}

}  // namespace genie::ui
