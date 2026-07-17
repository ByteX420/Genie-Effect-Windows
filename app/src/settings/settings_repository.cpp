#include "pch.hpp"

#include "settings/settings_repository.hpp"

#include <filesystem>
#include <fstream>

#include "core/logger.hpp"
#include "settings/settings_serializer.hpp"
#include "settings/settings_validator.hpp"

namespace genie::settings {

std::wstring SettingsRepository::Path() {
  DWORD required = GetEnvironmentVariableW(L"LOCALAPPDATA", nullptr, 0);
  if (required == 0) return {};
  std::wstring local_app_data(required, L'\0');
  const DWORD written = GetEnvironmentVariableW(L"LOCALAPPDATA", local_app_data.data(), required);
  if (written == 0 || written >= required) return {};
  local_app_data.resize(written);
  return local_app_data + L"\\GenieEffect\\settings.json";
}

AppSettings SettingsRepository::Load() const {
  const std::wstring path = Path();
  if (path.empty()) return {};
  std::ifstream input(std::filesystem::path(path), std::ios::binary);
  if (!input) return {};
  std::string json((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
  if (json.size() > 1024 * 1024) {
    core::LogDebug(L"Settings", L"Ignoring settings file larger than 1 MiB");
    return {};
  }
  auto deserialized = SettingsSerializer::Deserialize(json);
  if (!deserialized.has_value()) {
    core::LogDebug(L"Settings", L"Ignoring malformed settings file");
    return {};
  }
  return SettingsValidator::Normalize(std::move(*deserialized));
}

bool SettingsRepository::Save(const AppSettings& settings) const {
  const std::wstring path = Path();
  if (path.empty()) return false;
  const std::filesystem::path settings_path(path);
  std::error_code error;
  std::filesystem::create_directories(settings_path.parent_path(), error);
  if (error) return false;

  const std::filesystem::path temporary_path = settings_path.wstring() + L".tmp";
  {
    std::ofstream output(temporary_path, std::ios::binary | std::ios::trunc);
    if (!output) return false;
    output << SettingsSerializer::Serialize(SettingsValidator::Normalize(settings));
    output.flush();
    if (!output) {
      output.close();
      std::filesystem::remove(temporary_path, error);
      return false;
    }
  }
  if (!MoveFileExW(temporary_path.c_str(), settings_path.c_str(),
                   MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
    std::filesystem::remove(temporary_path, error);
    return false;
  }
  return true;
}

}  // namespace genie::settings
