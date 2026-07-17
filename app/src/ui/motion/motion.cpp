#include "pch.hpp"

#include "ui/motion/motion.hpp"

#include <algorithm>
#include <cmath>

namespace genie::ui::motion {
namespace {
constexpr unsigned kFinishedRetentionFrames = 2;
constexpr unsigned kActiveRetentionFrames = 180;
constexpr float kEpsilon = 0.0001f;

std::string BuildMotionKey(std::string_view scope, std::string_view id, std::string_view channel) {
  std::string key;
  key.reserve(scope.size() + id.size() + channel.size() + 2);

  auto appendPart = [&](std::string_view part) {
    if (part.empty()) {
      return;
    }
    if (!key.empty()) {
      key += '/';
    }
    key.append(part.data(), part.size());
  };

  appendPart(scope);
  appendPart(id);
  appendPart(channel);
  return key;
}

bool NearlyEqual(float lhs, float rhs) { return std::fabs(lhs - rhs) <= kEpsilon; }

bool NearlyEqual(const ImVec2& lhs, const ImVec2& rhs) {
  return NearlyEqual(lhs.x, rhs.x) && NearlyEqual(lhs.y, rhs.y);
}

bool NearlyEqual(const ImVec4& lhs, const ImVec4& rhs) {
  return NearlyEqual(lhs.x, rhs.x) && NearlyEqual(lhs.y, rhs.y) && NearlyEqual(lhs.z, rhs.z) &&
         NearlyEqual(lhs.w, rhs.w);
}

float LerpValue(float from, float to, float t) { return from + (to - from) * t; }

ImVec2 LerpValue(const ImVec2& from, const ImVec2& to, float t) {
  return ImVec2(LerpValue(from.x, to.x, t), LerpValue(from.y, to.y, t));
}

ImVec4 LerpValue(const ImVec4& from, const ImVec4& to, float t) {
  return ImVec4(LerpValue(from.x, to.x, t), LerpValue(from.y, to.y, t), LerpValue(from.z, to.z, t),
                LerpValue(from.w, to.w, t));
}

float Ease(MotionEasing easing, float t) {
  t = std::clamp(t, 0.0f, 1.0f);

  switch (easing) {
    case MotionEasing::kLinear:
      return t;
    case MotionEasing::kEaseInQuad:
      return t * t;
    case MotionEasing::kEaseOutQuad:
      return 1.0f - (1.0f - t) * (1.0f - t);
    case MotionEasing::kEaseInOutQuad:
      return t < 0.5f ? 2.0f * t * t : 1.0f - std::pow(-2.0f * t + 2.0f, 2.0f) * 0.5f;
    case MotionEasing::kEaseOutCubic: {
      const float f = t - 1.0f;
      return f * f * f + 1.0f;
    }
    case MotionEasing::kEaseOutExpo:
      return t >= 1.0f ? 1.0f : 1.0f - std::pow(2.0f, -10.0f * t);
    case MotionEasing::kEaseOutBack: {
      constexpr float c1 = 1.70158f;
      constexpr float c3 = c1 + 1.0f;
      const float f = t - 1.0f;
      return 1.0f + c3 * f * f * f + c1 * f * f;
    }
    case MotionEasing::kSmoothStep:
      return t * t * (3.0f - 2.0f * t);
    case MotionEasing::kSmootherStep:
      return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f);
    case MotionEasing::kSpringSoft:
    case MotionEasing::kSpringSnappy:
      return t;
  }

  return t;
}

bool IsSpring(MotionEasing easing) {
  return easing == MotionEasing::kSpringSoft || easing == MotionEasing::kSpringSnappy;
}

float SpringTowardVal(float current, float target, float& velocity, float response,
                      float delta_time) {
  const float omega = std::max(response, 0.001f);
  const float dt = std::max(delta_time, 0.0001f);
  const float exp_term = std::exp(-omega * dt);
  const float x = current - target;
  const float temp = (velocity + omega * x) * dt;
  velocity = (velocity - omega * temp) * exp_term;
  return target + (x + temp) * exp_term;
}

template <typename T>
T SpringToward(const T& current, const T& target, T& velocity, float response, float delta_time);

template <>
float SpringToward<float>(const float& current, const float& target, float& velocity,
                          float response, float delta_time) {
  return SpringTowardVal(current, target, velocity, response, delta_time);
}

template <>
ImVec2 SpringToward<ImVec2>(const ImVec2& current, const ImVec2& target, ImVec2& velocity,
                            float response, float delta_time) {
  return ImVec2(SpringTowardVal(current.x, target.x, velocity.x, response, delta_time),
                SpringTowardVal(current.y, target.y, velocity.y, response, delta_time));
}

template <>
ImVec4 SpringToward<ImVec4>(const ImVec4& current, const ImVec4& target, ImVec4& velocity,
                            float response, float delta_time) {
  return ImVec4(SpringTowardVal(current.x, target.x, velocity.x, response, delta_time),
                SpringTowardVal(current.y, target.y, velocity.y, response, delta_time),
                SpringTowardVal(current.z, target.z, velocity.z, response, delta_time),
                SpringTowardVal(current.w, target.w, velocity.w, response, delta_time));
}
}  // namespace

MotionSpec MotionSpec::Timed(float duration_seconds, MotionEasing easing_mode,
                             float delay_seconds) {
  MotionSpec spec{};
  spec.duration = duration_seconds;
  spec.delay = delay_seconds;
  spec.easing = easing_mode;
  spec.response = 12.0f;
  return spec;
}

MotionSpec MotionSpec::Spring(float response_strength, MotionEasing spring_mode) {
  MotionSpec spec{};
  spec.duration = 0.0f;
  spec.delay = 0.0f;
  spec.easing = spring_mode;
  spec.response = response_strength;
  return spec;
}

MotionKey::MotionKey(std::string_view raw_key) : storage(raw_key) {}

MotionKey::MotionKey(std::string_view scope, std::string_view id, std::string_view channel)
    : storage(BuildMotionKey(scope, id, channel)) {}

void MotionSystem::BeginFrame(float delta_time) {
  delta_time_ = std::max(delta_time, 0.0f);
  ++frame_index_;
  active_track_count_ = 0;

  CleanupTracks(scalar_tracks_);
  CleanupTracks(vector_tracks_);
  CleanupTracks(color_tracks_);
}

void MotionSystem::Clear() {
  scalar_tracks_.clear();
  vector_tracks_.clear();
  color_tracks_.clear();
  active_track_count_ = 0;
}

void MotionSystem::SetReducedMotion(bool enabled) { reduced_motion_ = enabled; }

bool MotionSystem::ReducedMotion() const { return reduced_motion_; }

float MotionSystem::AnimateValue(std::string_view key, float target, const MotionSpec& spec,
                                 std::optional<float> initial_value) {
  return Animate(scalar_tracks_, key, target, spec, initial_value);
}

float MotionSystem::AnimateValue(const MotionKey& key, float target, const MotionSpec& spec,
                                 std::optional<float> initial_value) {
  return AnimateValue(key.GetValue(), target, spec, initial_value);
}

ImVec2 MotionSystem::AnimateVector(std::string_view key, const ImVec2& target,
                                   const MotionSpec& spec, std::optional<ImVec2> initial_value) {
  return Animate(vector_tracks_, key, target, spec, initial_value);
}

ImVec2 MotionSystem::AnimateVector(const MotionKey& key, const ImVec2& target,
                                   const MotionSpec& spec, std::optional<ImVec2> initial_value) {
  return AnimateVector(key.GetValue(), target, spec, initial_value);
}

ImVec4 MotionSystem::AnimateColor(std::string_view key, const ImVec4& target,
                                  const MotionSpec& spec, std::optional<ImVec4> initial_value) {
  return Animate(color_tracks_, key, target, spec, initial_value);
}

ImVec4 MotionSystem::AnimateColor(const MotionKey& key, const ImVec4& target,
                                  const MotionSpec& spec, std::optional<ImVec4> initial_value) {
  return AnimateColor(key.GetValue(), target, spec, initial_value);
}

void MotionSystem::Set(std::string_view key, float value) {
  SetTrackValue(scalar_tracks_, key, value);
}

void MotionSystem::Set(std::string_view key, const ImVec2& value) {
  SetTrackValue(vector_tracks_, key, value);
}

void MotionSystem::Set(std::string_view key, const ImVec4& value) {
  SetTrackValue(color_tracks_, key, value);
}

void MotionSystem::Set(const MotionKey& key, float value) { Set(key.GetValue(), value); }

void MotionSystem::Set(const MotionKey& key, const ImVec2& value) { Set(key.GetValue(), value); }

void MotionSystem::Set(const MotionKey& key, const ImVec4& value) { Set(key.GetValue(), value); }

void MotionSystem::Forget(std::string_view key) {
  scalar_tracks_.erase(std::string(key));
  vector_tracks_.erase(std::string(key));
  color_tracks_.erase(std::string(key));
}

void MotionSystem::Forget(const MotionKey& key) { Forget(key.GetValue()); }

bool MotionSystem::IsActive(std::string_view key) const {
  return IsTrackActive(scalar_tracks_, key) || IsTrackActive(vector_tracks_, key) ||
         IsTrackActive(color_tracks_, key);
}

bool MotionSystem::IsActive(const MotionKey& key) const { return IsActive(key.GetValue()); }

MotionStats MotionSystem::GetStats() const {
  MotionStats result{};
  result.scalar_tracks = scalar_tracks_.size();
  result.vector_tracks = vector_tracks_.size();
  result.color_tracks = color_tracks_.size();
  result.active_tracks = active_track_count_;
  result.frame_index = frame_index_;
  result.delta_time = delta_time_;
  return result;
}

MotionSpec MotionSystem::Sanitize(const MotionSpec& spec) const {
  MotionSpec result = spec;
  result.duration = std::max(result.duration, 0.0f);
  result.delay = std::max(result.delay, 0.0f);
  result.response = std::max(result.response, 0.0f);

  if (reduced_motion_) {
    result.delay = 0.0f;

    if (IsSpring(result.easing)) {
      result.response = std::max(result.response, 20.0f);
    } else {
      result.duration = result.duration <= 0.0f ? 0.0f : std::min(result.duration, 0.08f);
      if (result.easing == MotionEasing::kEaseOutBack) {
        result.easing = MotionEasing::kEaseOutQuad;
      }
    }
  }

  return result;
}

template <typename T>
T MotionSystem::Animate(TrackMap<T>& tracks, std::string_view key, const T& target,
                        const MotionSpec& spec, std::optional<T> initial_value) {
  const MotionSpec applied_spec = Sanitize(spec);
  Track<T>& track = tracks[std::string(key)];

  if (!track.initialized) {
    const T start_value = initial_value.has_value() ? *initial_value : target;
    track.current = start_value;
    track.source = start_value;
    track.target = target;
    track.spec = applied_spec;
    track.elapsed = 0.0f;
    track.initialized = true;
    track.active = !NearlyEqual(start_value, target);
    track.last_touched_frame = frame_index_;

    if (!track.active) {
      track.current = target;
      track.source = target;
    }
  } else if (!NearlyEqual(track.target, target)) {
    track.source = track.current;
    track.target = target;
    track.spec = applied_spec;
    track.elapsed = 0.0f;
    track.active = !NearlyEqual(track.current, track.target);
  } else if (!NearlyEqual(track.spec.duration, applied_spec.duration) ||
             !NearlyEqual(track.spec.delay, applied_spec.delay) ||
             !NearlyEqual(track.spec.response, applied_spec.response) ||
             track.spec.easing != applied_spec.easing ||
             track.spec.snap_on_complete != applied_spec.snap_on_complete) {
    track.spec = applied_spec;
  }

  track.last_touched_frame = frame_index_;

  if (IsSpring(track.spec.easing)) {
    const float response = track.spec.response > 0.0f
                               ? track.spec.response
                               : (track.spec.easing == MotionEasing::kSpringSnappy ? 14.0f : 8.0f);

    track.current =
        SpringToward(track.current, track.target, track.velocity, response, delta_time_);
    if (NearlyEqual(track.current, track.target) && NearlyEqual(track.velocity, T{})) {
      if (track.spec.snap_on_complete) {
        track.current = track.target;
        track.velocity = T{};
      }
      track.active = false;
    } else {
      track.active = true;
    }
  } else {
    track.elapsed += delta_time_;

    if (track.elapsed < track.spec.delay) {
      track.current = track.source;
      track.active = !NearlyEqual(track.current, track.target);
    } else {
      const float effective_duration = std::max(track.spec.duration, 0.0001f);
      const float progress =
          std::clamp((track.elapsed - track.spec.delay) / effective_duration, 0.0f, 1.0f);
      track.current = LerpValue(track.source, track.target, Ease(track.spec.easing, progress));
      track.active = progress < 1.0f && !NearlyEqual(track.current, track.target);

      if (progress >= 1.0f && track.spec.snap_on_complete) {
        track.current = track.target;
        track.active = false;
      }
    }
  }

  if (track.active) {
    ++active_track_count_;
  }

  return track.current;
}

template <typename T>
void MotionSystem::SetTrackValue(TrackMap<T>& tracks, std::string_view key, const T& value) {
  Track<T>& track = tracks[std::string(key)];
  track.current = value;
  track.source = value;
  track.target = value;
  track.velocity = T{};
  track.spec = MotionSpec::Timed(0.0f);
  track.elapsed = 0.0f;
  track.last_touched_frame = frame_index_;
  track.initialized = true;
  track.active = false;
}

template <typename T>
bool MotionSystem::IsTrackActive(const TrackMap<T>& tracks, std::string_view key) const {
  const auto it = tracks.find(std::string(key));
  return it != tracks.end() && it->second.active;
}

template <typename T>
void MotionSystem::CleanupTracks(TrackMap<T>& tracks) {
  for (auto it = tracks.begin(); it != tracks.end();) {
    const unsigned age = frame_index_ - it->second.last_touched_frame;
    const bool prune =
        it->second.active ? (age > kActiveRetentionFrames) : (age > kFinishedRetentionFrames);
    if (prune) {
      it = tracks.erase(it);
    } else {
      ++it;
    }
  }
}

template float MotionSystem::Animate(TrackMap<float>& tracks, std::string_view key,
                                     const float& target, const MotionSpec& spec,
                                     std::optional<float> initial_value);
template ImVec2 MotionSystem::Animate(TrackMap<ImVec2>& tracks, std::string_view key,
                                      const ImVec2& target, const MotionSpec& spec,
                                      std::optional<ImVec2> initial_value);
template ImVec4 MotionSystem::Animate(TrackMap<ImVec4>& tracks, std::string_view key,
                                      const ImVec4& target, const MotionSpec& spec,
                                      std::optional<ImVec4> initial_value);

template void MotionSystem::SetTrackValue(TrackMap<float>& tracks, std::string_view key,
                                          const float& value);
template void MotionSystem::SetTrackValue(TrackMap<ImVec2>& tracks, std::string_view key,
                                          const ImVec2& value);
template void MotionSystem::SetTrackValue(TrackMap<ImVec4>& tracks, std::string_view key,
                                          const ImVec4& value);

template bool MotionSystem::IsTrackActive(const TrackMap<float>& tracks,
                                          std::string_view key) const;
template bool MotionSystem::IsTrackActive(const TrackMap<ImVec2>& tracks,
                                          std::string_view key) const;
template bool MotionSystem::IsTrackActive(const TrackMap<ImVec4>& tracks,
                                          std::string_view key) const;

template void MotionSystem::CleanupTracks(TrackMap<float>& tracks);
template void MotionSystem::CleanupTracks(TrackMap<ImVec2>& tracks);
template void MotionSystem::CleanupTracks(TrackMap<ImVec4>& tracks);
}  // namespace genie::ui::motion
