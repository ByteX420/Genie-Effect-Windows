#pragma once

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <windows.h>

namespace genie::features {

// Session per-window exclusions (HWND + PID) plus persisted display-device exclusions.
// Display exclusions use MONITORINFOEX.szDevice and are stored in settings.json.
class WindowExclusionService final {
public:
  bool SetExcluded(HWND window, bool excluded);
  [[nodiscard]] bool IsExcluded(HWND window) const;
  void Remove(HWND window);
  void PruneInvalidWindows();
  void Clear();

  void SetExcludedDisplays(std::vector<std::string> device_names);
  [[nodiscard]] bool IsDisplayExcluded(const std::string& device_name) const;
  [[nodiscard]] bool IsDisplayExcluded(HMONITOR monitor) const;
  bool SetDisplayExcluded(const std::string& device_name, bool excluded);
  [[nodiscard]] const std::vector<std::string>& excluded_displays() const {
    return excluded_display_devices_;
  }

  [[nodiscard]] static std::string MonitorDeviceName(HMONITOR monitor);

private:
  struct Entry {
    HWND window = nullptr;
    DWORD process_id = 0;
  };

  mutable std::unordered_map<HWND, Entry> excluded_windows_;
  std::vector<std::string> excluded_display_devices_;
  std::unordered_set<std::string> excluded_display_lookup_;
};

}  // namespace genie::features
