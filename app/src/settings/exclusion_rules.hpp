#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace minimize::settings {

[[nodiscard]] std::optional<std::string> NormalizeExecutableName(std::string_view name);
[[nodiscard]] bool ExecutableNamesEqual(std::string_view left, std::string_view right);
[[nodiscard]] bool ContainsExcludedApplication(const std::vector<std::string>& applications,
                                               std::string_view name);
void NormalizeExcludedApplications(std::vector<std::string>* applications);

}  // namespace minimize::settings
