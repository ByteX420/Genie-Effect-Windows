#include "pch.hpp"

#include "app/settings_store.hpp"

#include <cerrno>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <iomanip>
#include <sstream>
#include <string_view>

#include "common/debug_log.hpp"

namespace genie::app {
namespace {

constexpr float kMinimumDuration = 0.10f;
constexpr float kMaximumDuration = 2.00f;
constexpr size_t kMaximumJsonNestingDepth = 64;
constexpr std::uint32_t kSupportedHotkeyModifiers = 0x000fu;

struct HotkeyField {
  size_t action_index;
  bool is_modifier;
};

std::optional<HotkeyField> FindHotkeyField(std::string_view key) {
  constexpr std::array prefixes = {
      std::string_view{"toggleEffectHotkey"},
      std::string_view{"openSettingsHotkey"},
      std::string_view{"repairWindowsHotkey"},
  };
  for (size_t index = 0; index < prefixes.size(); ++index) {
    if (key.starts_with(prefixes[index]) && key.substr(prefixes[index].size()) == "Modifiers") {
      return HotkeyField{.action_index = index, .is_modifier = true};
    }
    if (key.starts_with(prefixes[index]) && key.substr(prefixes[index].size()) == "Key") {
      return HotkeyField{.action_index = index, .is_modifier = false};
    }
  }
  return std::nullopt;
}

bool IsValidEasingName(std::string_view value) {
  constexpr std::array names = {
      std::string_view{"Linear"},      std::string_view{"Ease In"}, std::string_view{"Ease Out"},
      std::string_view{"Ease In Out"}, std::string_view{"Cubic"},   std::string_view{"Back"},
      std::string_view{"Elastic"},
  };
  return std::find(names.begin(), names.end(), value) != names.end();
}

bool IsValidAnimationStyle(std::string_view value) {
  return value == "Gienie classic" || value == "Gienie curvy" || value == "Squash" ||
         value == "Classic Genie";
}

void AppendUtf8(std::string* output, unsigned int code_point) {
  if (code_point <= 0x7f) {
    output->push_back(static_cast<char>(code_point));
  } else if (code_point <= 0x7ff) {
    output->push_back(static_cast<char>(0xc0 | (code_point >> 6)));
    output->push_back(static_cast<char>(0x80 | (code_point & 0x3f)));
  } else {
    output->push_back(static_cast<char>(0xe0 | (code_point >> 12)));
    output->push_back(static_cast<char>(0x80 | ((code_point >> 6) & 0x3f)));
    output->push_back(static_cast<char>(0x80 | (code_point & 0x3f)));
  }
}

class SettingsJsonParser {
public:
  explicit SettingsJsonParser(std::string_view json) : json_(json) {}

  bool Parse(AppSettings* settings) {
    SkipWhitespace();
    if (!Consume('{')) return false;
    SkipWhitespace();
    if (Consume('}')) return AtEnd();

    while (true) {
      std::string key;
      if (!ParseString(&key)) return false;
      SkipWhitespace();
      if (!Consume(':')) return false;
      SkipWhitespace();

      const size_t value_start = position_;
      bool value_valid = false;
      if (key == "enabled") {
        bool value = false;
        value_valid = ParseBoolean(&value);
        if (value_valid) settings->enabled = value;
      } else if (key == "minimizeDuration" || key == "restoreDuration") {
        double value = 0.0;
        value_valid = ParseNumber(&value);
        if (value_valid && std::isfinite(value) && value >= kMinimumDuration &&
            value <= kMaximumDuration) {
          if (key == "minimizeDuration") {
            settings->minimize_duration = static_cast<float>(value);
          } else {
            settings->restore_duration = static_cast<float>(value);
          }
        }
      } else if (key == "genieStrength") {
        double value = 0.0;
        value_valid = ParseNumber(&value);
        if (value_valid && std::isfinite(value) && value >= 0.25 && value <= 1.0) {
          settings->genie_strength = static_cast<float>(value);
        }
      } else if (key == "fadeStrength") {
        std::string value;
        value_valid = ParseString(&value);
        if (value_valid && (value == "No fade" || value == "Subtle" || value == "Strong")) {
          settings->fade_strength = std::move(value);
        }
      } else if (key == "linkSpeeds" || key == "adaptiveDuration" ||
                 key == "disableAnimationsFullscreen" || key == "reduceEffectsOnBattery" ||
                 key == "disableEffectsBatterySaver" || key == "showTargetIndicator" ||
                 key == "followWindowsAnimationPreference" || key == "startMinimized" ||
                 key == "runAtStartup") {
        bool value = false;
        value_valid = ParseBoolean(&value);
        if (value_valid) {
          if (key == "linkSpeeds") settings->link_speeds = value;
          if (key == "disableAnimationsFullscreen") {
            settings->disable_animations_fullscreen = value;
          }
          if (key == "disableEffectsBatterySaver") {
            settings->disable_effects_battery_saver = value;
          }
          if (key == "showTargetIndicator") settings->show_target_indicator = value;
          if (key == "startMinimized") settings->start_minimized = value;
          if (key == "runAtStartup") settings->run_at_startup = value;
        }
      } else if (key == "closeBehavior") {
        std::string value;
        value_valid = ParseString(&value);
        if (value_valid && (value == "exit" || value == "tray")) {
          settings->close_behavior = std::move(value);
        }
      } else if (key == "minimizeEasing" || key == "restoreEasing") {
        std::string value;
        value_valid = ParseString(&value);
        if (value_valid && IsValidEasingName(value)) {
          if (key == "minimizeEasing")
            settings->minimize_easing = std::move(value);
          else
            settings->restore_easing = std::move(value);
        }
      } else if (key == "animationStyle") {
        std::string value;
        value_valid = ParseString(&value);
        if (value_valid && IsValidAnimationStyle(value)) {
          settings->animation_style = std::move(value);
        }
      } else if (key == "excludedApplications") {
        std::vector<std::string> values;
        value_valid = ParseStringArray(&values);
        if (value_valid) settings->excluded_applications = std::move(values);
      } else if (const std::optional<HotkeyField> field = FindHotkeyField(key); field.has_value()) {
        double value = 0.0;
        value_valid = ParseNumber(&value);
        if (value_valid && std::isfinite(value) && value == std::floor(value)) {
          if (field->is_modifier && value >= 0.0 &&
              value <= static_cast<double>(kSupportedHotkeyModifiers)) {
            settings->hotkeys[field->action_index].modifiers = static_cast<std::uint32_t>(value);
          } else if (!field->is_modifier && value >= 0.0 && value <= 254.0) {
            settings->hotkeys[field->action_index].virtual_key = static_cast<std::uint32_t>(value);
          }
        }
      } else {
        value_valid = SkipValue();
      }

      if (!value_valid) {
        position_ = value_start;
        if (!SkipValue()) return false;
      }

      SkipWhitespace();
      if (Consume('}')) return AtEnd();
      if (!Consume(',')) return false;
      SkipWhitespace();
    }
  }

private:
  bool AtEnd() {
    SkipWhitespace();
    return position_ == json_.size();
  }

  void SkipWhitespace() {
    while (position_ < json_.size() && (json_[position_] == ' ' || json_[position_] == '\t' ||
                                        json_[position_] == '\r' || json_[position_] == '\n')) {
      ++position_;
    }
  }

  bool Consume(char expected) {
    if (position_ >= json_.size() || json_[position_] != expected) return false;
    ++position_;
    return true;
  }

  bool ParseString(std::string* output) {
    if (!Consume('"')) return false;
    output->clear();
    while (position_ < json_.size()) {
      const unsigned char ch = static_cast<unsigned char>(json_[position_++]);
      if (ch == '"') return true;
      if (ch < 0x20) return false;
      if (ch != '\\') {
        output->push_back(static_cast<char>(ch));
        continue;
      }
      if (position_ >= json_.size()) return false;
      const char escaped = json_[position_++];
      switch (escaped) {
        case '"':
          output->push_back('"');
          break;
        case '\\':
          output->push_back('\\');
          break;
        case '/':
          output->push_back('/');
          break;
        case 'b':
          output->push_back('\b');
          break;
        case 'f':
          output->push_back('\f');
          break;
        case 'n':
          output->push_back('\n');
          break;
        case 'r':
          output->push_back('\r');
          break;
        case 't':
          output->push_back('\t');
          break;
        case 'u': {
          if (position_ + 4 > json_.size()) return false;
          unsigned int code_point = 0;
          for (int i = 0; i < 4; ++i) {
            const char hex = json_[position_++];
            code_point <<= 4;
            if (hex >= '0' && hex <= '9')
              code_point += hex - '0';
            else if (hex >= 'a' && hex <= 'f')
              code_point += hex - 'a' + 10;
            else if (hex >= 'A' && hex <= 'F')
              code_point += hex - 'A' + 10;
            else
              return false;
          }
          AppendUtf8(output, code_point);
          break;
        }
        default:
          return false;
      }
    }
    return false;
  }

  bool ParseBoolean(bool* output) {
    if (json_.substr(position_, 4) == "true") {
      position_ += 4;
      *output = true;
      return true;
    }
    if (json_.substr(position_, 5) == "false") {
      position_ += 5;
      *output = false;
      return true;
    }
    return false;
  }

  bool ParseNumber(double* output) {
    const size_t start = position_;
    if (position_ < json_.size() && json_[position_] == '-') ++position_;
    if (position_ >= json_.size()) return false;
    if (json_[position_] == '0') {
      ++position_;
    } else {
      if (json_[position_] < '1' || json_[position_] > '9') return false;
      while (position_ < json_.size() && json_[position_] >= '0' && json_[position_] <= '9') {
        ++position_;
      }
    }
    if (position_ < json_.size() && json_[position_] == '.') {
      ++position_;
      const size_t fraction_start = position_;
      while (position_ < json_.size() && json_[position_] >= '0' && json_[position_] <= '9') {
        ++position_;
      }
      if (fraction_start == position_) return false;
    }
    if (position_ < json_.size() && (json_[position_] == 'e' || json_[position_] == 'E')) {
      ++position_;
      if (position_ < json_.size() && (json_[position_] == '+' || json_[position_] == '-')) {
        ++position_;
      }
      const size_t exponent_start = position_;
      while (position_ < json_.size() && json_[position_] >= '0' && json_[position_] <= '9') {
        ++position_;
      }
      if (exponent_start == position_) return false;
    }
    const std::string token(json_.substr(start, position_ - start));
    char* end = nullptr;
    errno = 0;
    *output = std::strtod(token.c_str(), &end);
    return errno != ERANGE && end == token.c_str() + token.size();
  }

  bool ParseStringArray(std::vector<std::string>* output) {
    if (!Consume('[')) return false;
    SkipWhitespace();
    if (Consume(']')) return true;
    while (true) {
      std::string value;
      if (!ParseString(&value)) return false;
      output->push_back(std::move(value));
      SkipWhitespace();
      if (Consume(']')) return true;
      if (!Consume(',')) return false;
      SkipWhitespace();
    }
  }

  bool SkipValue(size_t depth = 0) {
    SkipWhitespace();
    if (position_ >= json_.size()) return false;
    if (depth > kMaximumJsonNestingDepth) return false;
    if (json_[position_] == '"') {
      std::string ignored;
      return ParseString(&ignored);
    }
    if (json_[position_] == '{') {
      ++position_;
      SkipWhitespace();
      if (Consume('}')) return true;
      while (true) {
        std::string ignored;
        if (!ParseString(&ignored)) return false;
        SkipWhitespace();
        if (!Consume(':') || !SkipValue(depth + 1)) return false;
        SkipWhitespace();
        if (Consume('}')) return true;
        if (!Consume(',')) return false;
        SkipWhitespace();
      }
    }
    if (json_[position_] == '[') {
      ++position_;
      SkipWhitespace();
      if (Consume(']')) return true;
      while (true) {
        if (!SkipValue(depth + 1)) return false;
        SkipWhitespace();
        if (Consume(']')) return true;
        if (!Consume(',')) return false;
        SkipWhitespace();
      }
    }
    bool ignored_bool = false;
    if (ParseBoolean(&ignored_bool)) return true;
    if (json_.substr(position_, 4) == "null") {
      position_ += 4;
      return true;
    }
    double ignored_number = 0.0;
    return ParseNumber(&ignored_number);
  }

  std::string_view json_;
  size_t position_ = 0;
};

std::string EscapeJsonString(std::string_view value) {
  std::string escaped;
  escaped.reserve(value.size() + 8);
  for (const unsigned char ch : value) {
    switch (ch) {
      case '"':
        escaped += "\\\"";
        break;
      case '\\':
        escaped += "\\\\";
        break;
      case '\b':
        escaped += "\\b";
        break;
      case '\f':
        escaped += "\\f";
        break;
      case '\n':
        escaped += "\\n";
        break;
      case '\r':
        escaped += "\\r";
        break;
      case '\t':
        escaped += "\\t";
        break;
      default:
        if (ch < 0x20) {
          constexpr char kHex[] = "0123456789abcdef";
          escaped += "\\u00";
          escaped.push_back(kHex[ch >> 4]);
          escaped.push_back(kHex[ch & 0xf]);
        } else {
          escaped.push_back(static_cast<char>(ch));
        }
    }
  }
  return escaped;
}

std::optional<std::wstring> Utf8ToWide(std::string_view value) {
  if (value.empty()) return std::wstring{};
  const int length = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(),
                                         static_cast<int>(value.size()), nullptr, 0);
  if (length <= 0) return std::nullopt;
  std::wstring result(static_cast<size_t>(length), L'\0');
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
  for (const unsigned char ch : name) {
    if (ch < 0x20 || kInvalidCharacters.find(static_cast<char>(ch)) != std::string_view::npos) {
      return std::nullopt;
    }
  }
  if (name.size() < 5) return std::nullopt;
  constexpr std::string_view kExeSuffix = ".exe";
  for (size_t i = 0; i < kExeSuffix.size(); ++i) {
    const unsigned char ch = static_cast<unsigned char>(name[name.size() - 4 + i]);
    const char folded =
        ch >= 'A' && ch <= 'Z' ? static_cast<char>(ch + ('a' - 'A')) : static_cast<char>(ch);
    if (folded != kExeSuffix[i]) return std::nullopt;
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

std::wstring SettingsFilePath() {
  DWORD required = GetEnvironmentVariableW(L"LOCALAPPDATA", nullptr, 0);
  if (required == 0) return {};
  std::wstring local_app_data(required, L'\0');
  const DWORD written = GetEnvironmentVariableW(L"LOCALAPPDATA", local_app_data.data(), required);
  if (written == 0 || written >= required) return {};
  local_app_data.resize(written);
  return local_app_data + L"\\GenieEffect\\settings.json";
}

AppSettings LoadSettings() {
  AppSettings defaults;
  const std::wstring path = SettingsFilePath();
  if (path.empty()) return defaults;

  std::ifstream input(std::filesystem::path(path), std::ios::binary);
  if (!input) return defaults;
  std::string json((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
  if (json.size() > 1024 * 1024) {
    LogDebug(L"Settings", L"Ignoring settings file larger than 1 MiB");
    return defaults;
  }

  AppSettings loaded;
  SettingsJsonParser parser(json);
  if (!parser.Parse(&loaded)) {
    LogDebug(L"Settings", L"Ignoring malformed settings file");
    return defaults;
  }
  NormalizeExcludedApplications(&loaded.excluded_applications);
  for (HotkeyBinding& binding : loaded.hotkeys) {
    binding.modifiers &= kSupportedHotkeyModifiers;
    if (binding.virtual_key == 0) binding.modifiers = 0;
  }
  if (loaded.animation_style == "Classic Genie") {
    loaded.animation_style = "Gienie classic";
  }
  if (loaded.animation_style == "Gienie classic") {
    loaded.minimize_easing = "Ease In Out";
    loaded.restore_easing = "Ease In Out";
  } else {
    loaded.minimize_easing = "Linear";
    loaded.restore_easing = "Linear";
  }
  return loaded;
}

bool SaveSettings(const AppSettings& settings) {
  const std::wstring path = SettingsFilePath();
  if (path.empty()) return false;
  const std::filesystem::path settings_path(path);
  std::error_code error;
  std::filesystem::create_directories(settings_path.parent_path(), error);
  if (error) return false;

  const std::filesystem::path temporary_path = settings_path.wstring() + L".tmp";
  std::ofstream output(temporary_path, std::ios::binary | std::ios::trunc);
  if (!output) return false;
  std::vector<std::string> excluded_applications = settings.excluded_applications;
  NormalizeExcludedApplications(&excluded_applications);
  output << std::boolalpha << std::fixed << std::setprecision(2);
  output << "{\n"
         << "  \"enabled\": " << settings.enabled << ",\n"
         << "  \"minimizeDuration\": " << settings.minimize_duration << ",\n"
         << "  \"restoreDuration\": " << settings.restore_duration << ",\n"
         << "  \"linkSpeeds\": " << settings.link_speeds << ",\n"
         << "  \"disableAnimationsFullscreen\": " << settings.disable_animations_fullscreen << ",\n"
         << "  \"disableEffectsBatterySaver\": " << settings.disable_effects_battery_saver << ",\n"
         << "  \"minimizeEasing\": \"" << EscapeJsonString(settings.minimize_easing) << "\",\n"
         << "  \"restoreEasing\": \"" << EscapeJsonString(settings.restore_easing) << "\",\n"
         << "  \"animationStyle\": \"" << EscapeJsonString(settings.animation_style) << "\",\n"
         << "  \"genieStrength\": " << settings.genie_strength << ",\n"
         << "  \"fadeStrength\": \"" << EscapeJsonString(settings.fade_strength) << "\",\n"
         << "  \"showTargetIndicator\": " << settings.show_target_indicator << ",\n"
         << "  \"closeBehavior\": \"" << EscapeJsonString(settings.close_behavior) << "\",\n"
         << "  \"startMinimized\": " << settings.start_minimized << ",\n"
         << "  \"runAtStartup\": " << settings.run_at_startup << ",\n";
  constexpr std::array hotkey_names = {
      std::string_view{"toggleEffectHotkey"},
      std::string_view{"openSettingsHotkey"},
      std::string_view{"repairWindowsHotkey"},
  };
  for (size_t index = 0; index < hotkey_names.size(); ++index) {
    const HotkeyBinding& binding = settings.hotkeys[index];
    output << "  \"" << hotkey_names[index] << "Modifiers\": " << binding.modifiers << ",\n"
           << "  \"" << hotkey_names[index] << "Key\": " << binding.virtual_key << ",\n";
  }
  output << "  \"excludedApplications\": [";
  for (size_t i = 0; i < excluded_applications.size(); ++i) {
    if (i != 0) output << ", ";
    output << '"' << EscapeJsonString(excluded_applications[i]) << '"';
  }
  output << "]\n}\n";
  output.flush();
  if (!output) {
    output.close();
    std::filesystem::remove(temporary_path, error);
    return false;
  }
  output.close();

  if (!MoveFileExW(temporary_path.c_str(), settings_path.c_str(),
                   MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
    std::filesystem::remove(temporary_path, error);
    return false;
  }
  return true;
}

}  // namespace genie::app
