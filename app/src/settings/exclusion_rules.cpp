#include "pch.hpp"

#include "settings/exclusion_rules.hpp"

#include <algorithm>
#include <windows.h>

namespace minimize::settings {
namespace {

std::optional<std::wstring> Utf8ToWide(std::string_view value) {
  if (value.empty()) return std::wstring{};
  const int length = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(),
                                         static_cast<int>(value.size()), nullptr, 0);
  if (length <= 0) return std::nullopt;
  std::wstring result(static_cast<std::size_t>(length), L'\0');
  if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(),
                          static_cast<int>(value.size()), result.data(), length) != length) {
    return std::nullopt;
  }
  return result;
}

}  // namespace

std::optional<std::string> NormalizeExecutableName(std::string_view name) {
  while (!name.empty() && (name.front() == ' ' || name.front() == '\t' || name.front() == '\r' ||
                           name.front() == '\n')) {
    name.remove_prefix(1);
  }
  while (!name.empty() && (name.back() == ' ' || name.back() == '\t' || name.back() == '\r' ||
                           name.back() == '\n')) {
    name.remove_suffix(1);
  }
  if (name.empty() || name.size() > 255 || name == "." || name == ".." ||
      !Utf8ToWide(name).has_value()) {
    return std::nullopt;
  }
  constexpr std::string_view kInvalidCharacters = "<>:\"/\\|?*";
  for (const unsigned char character : name) {
    if (character < 0x20 ||
        kInvalidCharacters.find(static_cast<char>(character)) != std::string_view::npos) {
      return std::nullopt;
    }
  }
  if (name.size() < 5) return std::nullopt;
  constexpr std::string_view kExecutableSuffix = ".exe";
  for (std::size_t index = 0; index < kExecutableSuffix.size(); ++index) {
    const unsigned char character =
        static_cast<unsigned char>(name[name.size() - kExecutableSuffix.size() + index]);
    const char folded = character >= 'A' && character <= 'Z'
                            ? static_cast<char>(character + ('a' - 'A'))
                            : static_cast<char>(character);
    if (folded != kExecutableSuffix[index]) return std::nullopt;
  }
  return std::string(name);
}

bool ExecutableNamesEqual(std::string_view left, std::string_view right) {
  const std::optional<std::wstring> wide_left = Utf8ToWide(left);
  const std::optional<std::wstring> wide_right = Utf8ToWide(right);
  if (!wide_left.has_value() || !wide_right.has_value()) return false;
  return CompareStringOrdinal(wide_left->data(), static_cast<int>(wide_left->size()),
                              wide_right->data(), static_cast<int>(wide_right->size()),
                              TRUE) == CSTR_EQUAL;
}

bool ContainsExcludedApplication(const std::vector<std::string>& applications,
                                 std::string_view name) {
  return std::any_of(applications.begin(), applications.end(), [name](const std::string& entry) {
    return ExecutableNamesEqual(entry, name);
  });
}

void NormalizeExcludedApplications(std::vector<std::string>* applications) {
  if (applications == nullptr) return;
  std::vector<std::string> normalized;
  normalized.reserve(applications->size());
  for (const std::string& application : *applications) {
    std::optional<std::string> name = NormalizeExecutableName(application);
    if (name.has_value() && !ContainsExcludedApplication(normalized, *name)) {
      normalized.push_back(std::move(*name));
    }
  }
  *applications = std::move(normalized);
}

}  // namespace minimize::settings
