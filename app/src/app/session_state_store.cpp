#include "pch.hpp"

#include "app/session_state_store.hpp"

#include <filesystem>
#include <fstream>

#include "settings/settings_repository.hpp"

namespace genie::app {
namespace {

std::filesystem::path SessionStatePath() {
  std::filesystem::path path(settings::SettingsRepository::Path());
  if (!path.empty()) path.replace_filename(L"session.state");
  return path;
}

}  // namespace

std::string SessionStateStore::Read() const {
  const std::filesystem::path path = SessionStatePath();
  if (path.empty()) return {};
  std::ifstream input(path, std::ios::binary);
  std::string state;
  if (input) std::getline(input, state);
  return state;
}

bool SessionStateStore::Write(std::string_view state) const {
  const std::filesystem::path path = SessionStatePath();
  if (path.empty()) return false;
  std::error_code error;
  std::filesystem::create_directories(path.parent_path(), error);
  if (error) return false;
  const std::filesystem::path temporary = path.wstring() + L".tmp";
  {
    std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
    if (!output) return false;
    output << state << '\n';
    output.flush();
    if (!output) {
      output.close();
      std::filesystem::remove(temporary, error);
      return false;
    }
  }
  if (!MoveFileExW(temporary.c_str(), path.c_str(),
                   MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
    std::filesystem::remove(temporary, error);
    return false;
  }
  return true;
}

}  // namespace genie::app
