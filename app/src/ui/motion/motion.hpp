#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>

#include "imgui.h"

namespace genie::ui::motion {
enum class MotionEasing {
  kLinear,
  kEaseInQuad,
  kEaseOutQuad,
  kEaseInOutQuad,
  kEaseOutCubic,
  kEaseOutExpo,
  kEaseOutBack,
  kSmoothStep,
  kSmootherStep,
  kSpringSoft,
  kSpringSnappy,
};

struct MotionSpec {
  float duration = 0.18f;
  float delay = 0.0f;
  float response = 12.0f;
  MotionEasing easing = MotionEasing::kEaseOutQuad;
  bool snap_on_complete = true;

  static MotionSpec Timed(float duration_seconds,
                          MotionEasing easing_mode = MotionEasing::kEaseOutQuad,
                          float delay_seconds = 0.0f);
  static MotionSpec Spring(float response_strength = 12.0f,
                           MotionEasing spring_mode = MotionEasing::kSpringSoft);
};

struct MotionKey {
  std::string storage;

  MotionKey() = default;
  explicit MotionKey(std::string_view raw_key);
  MotionKey(std::string_view scope, std::string_view id, std::string_view channel);

  [[nodiscard]] const std::string& GetValue() const { return storage; }
};

struct MotionStats {
  std::size_t scalar_tracks = 0;
  std::size_t vector_tracks = 0;
  std::size_t color_tracks = 0;
  std::size_t active_tracks = 0;
  unsigned frame_index = 0;
  float delta_time = 0.0f;
};

class MotionSystem {
public:
  void BeginFrame(float delta_time);
  void Clear();
  void SetReducedMotion(bool enabled);
  [[nodiscard]] bool ReducedMotion() const;

  float AnimateValue(std::string_view key, float target, const MotionSpec& spec,
                     std::optional<float> initial_value = std::nullopt);
  float AnimateValue(const MotionKey& key, float target, const MotionSpec& spec,
                     std::optional<float> initial_value = std::nullopt);

  ImVec2 AnimateVector(std::string_view key, const ImVec2& target, const MotionSpec& spec,
                       std::optional<ImVec2> initial_value = std::nullopt);
  ImVec2 AnimateVector(const MotionKey& key, const ImVec2& target, const MotionSpec& spec,
                       std::optional<ImVec2> initial_value = std::nullopt);

  ImVec4 AnimateColor(std::string_view key, const ImVec4& target, const MotionSpec& spec,
                      std::optional<ImVec4> initial_value = std::nullopt);
  ImVec4 AnimateColor(const MotionKey& key, const ImVec4& target, const MotionSpec& spec,
                      std::optional<ImVec4> initial_value = std::nullopt);

  void Set(std::string_view key, float value);
  void Set(std::string_view key, const ImVec2& value);
  void Set(std::string_view key, const ImVec4& value);
  void Set(const MotionKey& key, float value);
  void Set(const MotionKey& key, const ImVec2& value);
  void Set(const MotionKey& key, const ImVec4& value);

  void Forget(std::string_view key);
  void Forget(const MotionKey& key);

  [[nodiscard]] bool IsActive(std::string_view key) const;
  [[nodiscard]] bool IsActive(const MotionKey& key) const;
  [[nodiscard]] MotionStats GetStats() const;

private:
  template <typename T>
  struct Track {
    T current{};
    T source{};
    T target{};
    T velocity{};
    MotionSpec spec{};
    float elapsed = 0.0f;
    unsigned last_touched_frame = 0;
    bool initialized = false;
    bool active = false;
  };

  template <typename T>
  using TrackMap = std::unordered_map<std::string, Track<T>>;

  template <typename T>
  T Animate(TrackMap<T>& tracks, std::string_view key, const T& target, const MotionSpec& spec,
            std::optional<T> initial_value);

  template <typename T>
  void SetTrackValue(TrackMap<T>& tracks, std::string_view key, const T& value);

  template <typename T>
  [[nodiscard]] bool IsTrackActive(const TrackMap<T>& tracks, std::string_view key) const;

  template <typename T>
  void CleanupTracks(TrackMap<T>& tracks);

  [[nodiscard]] MotionSpec Sanitize(const MotionSpec& spec) const;

  TrackMap<float> scalar_tracks_;
  TrackMap<ImVec2> vector_tracks_;
  TrackMap<ImVec4> color_tracks_;
  unsigned frame_index_ = 0;
  float delta_time_ = 0.0f;
  bool reduced_motion_ = false;
  unsigned active_track_count_ = 0;
};
}  // namespace genie::ui::motion
