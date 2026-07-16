#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>

#include "imgui.h"

namespace ui::motion {
enum class MotionEasing {
  Linear,
  EaseInQuad,
  EaseOutQuad,
  EaseInOutQuad,
  EaseOutCubic,
  EaseOutExpo,
  EaseOutBack,
  SmoothStep,
  SmootherStep,
  SpringSoft,
  SpringSnappy
};

struct MotionSpec {
  float duration = 0.18f;
  float delay = 0.0f;
  float response = 12.0f;
  MotionEasing easing = MotionEasing::EaseOutQuad;
  bool snapOnComplete = true;

  static MotionSpec Timed(float durationSeconds,
                          MotionEasing easingMode = MotionEasing::EaseOutQuad,
                          float delaySeconds = 0.0f);
  static MotionSpec Spring(float responseStrength = 12.0f,
                           MotionEasing springMode = MotionEasing::SpringSoft);
};

struct MotionKey {
  std::string storage;

  MotionKey() = default;
  explicit MotionKey(std::string_view rawKey);
  MotionKey(std::string_view scope, std::string_view id, std::string_view channel);

  [[nodiscard]] const std::string& value() const { return storage; }
};

struct MotionStats {
  std::size_t scalarTracks = 0;
  std::size_t vec2Tracks = 0;
  std::size_t colorTracks = 0;
  std::size_t activeTracks = 0;
  unsigned frameIndex = 0;
  float deltaTime = 0.0f;
};

class MotionSystem {
public:
  void beginFrame(float deltaTime);
  void clear();
  void setReducedMotion(bool enabled);
  [[nodiscard]] bool reducedMotion() const;

  float value(std::string_view key, float target, const MotionSpec& spec,
              std::optional<float> initialValue = std::nullopt);
  float value(const MotionKey& key, float target, const MotionSpec& spec,
              std::optional<float> initialValue = std::nullopt);

  ImVec2 vec2(std::string_view key, const ImVec2& target, const MotionSpec& spec,
              std::optional<ImVec2> initialValue = std::nullopt);
  ImVec2 vec2(const MotionKey& key, const ImVec2& target, const MotionSpec& spec,
              std::optional<ImVec2> initialValue = std::nullopt);

  ImVec4 color(std::string_view key, const ImVec4& target, const MotionSpec& spec,
               std::optional<ImVec4> initialValue = std::nullopt);
  ImVec4 color(const MotionKey& key, const ImVec4& target, const MotionSpec& spec,
               std::optional<ImVec4> initialValue = std::nullopt);

  void set(std::string_view key, float value);
  void set(std::string_view key, const ImVec2& value);
  void set(std::string_view key, const ImVec4& value);
  void set(const MotionKey& key, float value);
  void set(const MotionKey& key, const ImVec2& value);
  void set(const MotionKey& key, const ImVec4& value);

  void forget(std::string_view key);
  void forget(const MotionKey& key);

  [[nodiscard]] bool isActive(std::string_view key) const;
  [[nodiscard]] bool isActive(const MotionKey& key) const;
  [[nodiscard]] MotionStats stats() const;

  void debugOverlay(const char* title = "Motion Debug") const;

private:
  template <typename T>
  struct Track {
    T current{};
    T source{};
    T target{};
    T velocity{};
    MotionSpec spec{};
    float elapsed = 0.0f;
    unsigned lastTouchedFrame = 0;
    bool initialized = false;
    bool active = false;
  };

  template <typename T>
  using TrackMap = std::unordered_map<std::string, Track<T>>;

  template <typename T>
  T animate(TrackMap<T>& tracks, std::string_view key, const T& target, const MotionSpec& spec,
            std::optional<T> initialValue);

  template <typename T>
  void setTrackValue(TrackMap<T>& tracks, std::string_view key, const T& value);

  template <typename T>
  [[nodiscard]] bool isTrackActive(const TrackMap<T>& tracks, std::string_view key) const;

  template <typename T>
  void cleanupTracks(TrackMap<T>& tracks);

  [[nodiscard]] MotionSpec sanitize(const MotionSpec& spec) const;

  TrackMap<float> scalarTracks_;
  TrackMap<ImVec2> vec2Tracks_;
  TrackMap<ImVec4> colorTracks_;
  unsigned frameIndex_ = 0;
  float deltaTime_ = 0.0f;
  bool reducedMotion_ = false;
  unsigned activeTrackCount_ = 0;
};
}  // namespace ui::motion
