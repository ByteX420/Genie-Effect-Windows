#include "pch.h"
#include "menu/motion/motion.hpp"

#include <cmath>

namespace ui::motion {
  namespace {
    constexpr unsigned kFinishedRetentionFrames = 2;
    constexpr unsigned kActiveRetentionFrames   = 180;
    constexpr float    kEpsilon                 = 0.0001f;

    std::string BuildMotionKey( std::string_view scope, std::string_view id, std::string_view channel ) {
      std::string key;
      key.reserve( scope.size() + id.size() + channel.size() + 2 );

      auto appendPart = [&]( std::string_view part ) {
        if ( part.empty() ) {
          return;
        }
        if ( !key.empty() ) {
          key += '/';
        }
        key.append( part.data(), part.size() );
      };

      appendPart( scope );
      appendPart( id );
      appendPart( channel );
      return key;
    }

    bool NearlyEqual( float lhs, float rhs ) {
      return std::fabs( lhs - rhs ) <= kEpsilon;
    }

    bool NearlyEqual( const ImVec2& lhs, const ImVec2& rhs ) {
      return NearlyEqual( lhs.x, rhs.x ) && NearlyEqual( lhs.y, rhs.y );
    }

    bool NearlyEqual( const ImVec4& lhs, const ImVec4& rhs ) {
      return NearlyEqual( lhs.x, rhs.x ) &&
             NearlyEqual( lhs.y, rhs.y ) &&
             NearlyEqual( lhs.z, rhs.z ) &&
             NearlyEqual( lhs.w, rhs.w );
    }

    float LerpValue( float from, float to, float t ) {
      return from + ( to - from ) * t;
    }

    ImVec2 LerpValue( const ImVec2& from, const ImVec2& to, float t ) {
      return ImVec2( LerpValue( from.x, to.x, t ), LerpValue( from.y, to.y, t ) );
    }

    ImVec4 LerpValue( const ImVec4& from, const ImVec4& to, float t ) {
      return ImVec4(
          LerpValue( from.x, to.x, t ),
          LerpValue( from.y, to.y, t ),
          LerpValue( from.z, to.z, t ),
          LerpValue( from.w, to.w, t ) );
    }

    float Ease( MotionEasing easing, float t ) {
      t = ImClamp( t, 0.0f, 1.0f );

      switch ( easing ) {
        case MotionEasing::Linear:
          return t;
        case MotionEasing::EaseInQuad:
          return t * t;
        case MotionEasing::EaseOutQuad:
          return 1.0f - ( 1.0f - t ) * ( 1.0f - t );
        case MotionEasing::EaseInOutQuad:
          return t < 0.5f ? 2.0f * t * t : 1.0f - std::pow( -2.0f * t + 2.0f, 2.0f ) * 0.5f;
        case MotionEasing::EaseOutCubic: {
          const float f = t - 1.0f;
          return f * f * f + 1.0f;
        }
        case MotionEasing::EaseOutExpo:
          return t >= 1.0f ? 1.0f : 1.0f - std::pow( 2.0f, -10.0f * t );
        case MotionEasing::EaseOutBack: {
          constexpr float c1 = 1.70158f;
          constexpr float c3 = c1 + 1.0f;
          const float     f  = t - 1.0f;
          return 1.0f + c3 * f * f * f + c1 * f * f;
        }
        case MotionEasing::SmoothStep:
          return t * t * ( 3.0f - 2.0f * t );
        case MotionEasing::SmootherStep:
          return t * t * t * ( t * ( t * 6.0f - 15.0f ) + 10.0f );
        case MotionEasing::SpringSoft:
        case MotionEasing::SpringSnappy:
          return t;
      }

      return t;
    }

    bool IsSpring( MotionEasing easing ) {
      return easing == MotionEasing::SpringSoft || easing == MotionEasing::SpringSnappy;
    }

    float SpringTowardVal( float current, float target, float& velocity, float response, float deltaTime ) {
      const float omega    = ImMax( response, 0.001f );
      const float dt       = ImMax( deltaTime, 0.0001f );
      const float exp_term = std::exp( -omega * dt );
      const float x        = current - target;
      const float temp     = ( velocity + omega * x ) * dt;
      velocity             = ( velocity - omega * temp ) * exp_term;
      return target + ( x + temp ) * exp_term;
    }

    template <typename T>
    T SpringToward( const T& current, const T& target, T& velocity, float response, float deltaTime );

    template <>
    float SpringToward<float>( const float& current, const float& target, float& velocity, float response, float deltaTime ) {
      return SpringTowardVal( current, target, velocity, response, deltaTime );
    }

    template <>
    ImVec2 SpringToward<ImVec2>( const ImVec2& current, const ImVec2& target, ImVec2& velocity, float response, float deltaTime ) {
      return ImVec2( SpringTowardVal( current.x, target.x, velocity.x, response, deltaTime ),
                     SpringTowardVal( current.y, target.y, velocity.y, response, deltaTime ) );
    }

    template <>
    ImVec4 SpringToward<ImVec4>( const ImVec4& current, const ImVec4& target, ImVec4& velocity, float response, float deltaTime ) {
      return ImVec4( SpringTowardVal( current.x, target.x, velocity.x, response, deltaTime ),
                     SpringTowardVal( current.y, target.y, velocity.y, response, deltaTime ),
                     SpringTowardVal( current.z, target.z, velocity.z, response, deltaTime ),
                     SpringTowardVal( current.w, target.w, velocity.w, response, deltaTime ) );
    }
  }  // namespace

  MotionSpec MotionSpec::Timed( float durationSeconds, MotionEasing easingMode, float delaySeconds ) {
    MotionSpec spec{};
    spec.duration = durationSeconds;
    spec.delay    = delaySeconds;
    spec.easing   = easingMode;
    spec.response = 12.0f;
    return spec;
  }

  MotionSpec MotionSpec::Spring( float responseStrength, MotionEasing springMode ) {
    MotionSpec spec{};
    spec.duration = 0.0f;
    spec.delay    = 0.0f;
    spec.easing   = springMode;
    spec.response = responseStrength;
    return spec;
  }

  MotionKey::MotionKey( std::string_view rawKey )
      : storage( rawKey ) {
  }

  MotionKey::MotionKey( std::string_view scope, std::string_view id, std::string_view channel )
      : storage( BuildMotionKey( scope, id, channel ) ) {
  }

  void MotionSystem::beginFrame( float deltaTime ) {
    deltaTime_ = ImMax( deltaTime, 0.0f );
    ++frameIndex_;
    activeTrackCount_ = 0;

    cleanupTracks( scalarTracks_ );
    cleanupTracks( vec2Tracks_ );
    cleanupTracks( colorTracks_ );
  }

  void MotionSystem::clear() {
    scalarTracks_.clear();
    vec2Tracks_.clear();
    colorTracks_.clear();
    activeTrackCount_ = 0;
  }

  void MotionSystem::setReducedMotion( bool enabled ) {
    reducedMotion_ = enabled;
  }

  bool MotionSystem::reducedMotion() const {
    return reducedMotion_;
  }

  float MotionSystem::value( std::string_view key, float target, const MotionSpec& spec, std::optional<float> initialValue ) {
    return animate( scalarTracks_, key, target, spec, initialValue );
  }

  float MotionSystem::value( const MotionKey& key, float target, const MotionSpec& spec, std::optional<float> initialValue ) {
    return value( key.value(), target, spec, initialValue );
  }

  ImVec2 MotionSystem::vec2( std::string_view key, const ImVec2& target, const MotionSpec& spec, std::optional<ImVec2> initialValue ) {
    return animate( vec2Tracks_, key, target, spec, initialValue );
  }

  ImVec2 MotionSystem::vec2( const MotionKey& key, const ImVec2& target, const MotionSpec& spec, std::optional<ImVec2> initialValue ) {
    return vec2( key.value(), target, spec, initialValue );
  }

  ImVec4 MotionSystem::color( std::string_view key, const ImVec4& target, const MotionSpec& spec, std::optional<ImVec4> initialValue ) {
    return animate( colorTracks_, key, target, spec, initialValue );
  }

  ImVec4 MotionSystem::color( const MotionKey& key, const ImVec4& target, const MotionSpec& spec, std::optional<ImVec4> initialValue ) {
    return color( key.value(), target, spec, initialValue );
  }

  void MotionSystem::set( std::string_view key, float value ) {
    setTrackValue( scalarTracks_, key, value );
  }

  void MotionSystem::set( std::string_view key, const ImVec2& value ) {
    setTrackValue( vec2Tracks_, key, value );
  }

  void MotionSystem::set( std::string_view key, const ImVec4& value ) {
    setTrackValue( colorTracks_, key, value );
  }

  void MotionSystem::set( const MotionKey& key, float value ) {
    set( key.value(), value );
  }

  void MotionSystem::set( const MotionKey& key, const ImVec2& value ) {
    set( key.value(), value );
  }

  void MotionSystem::set( const MotionKey& key, const ImVec4& value ) {
    set( key.value(), value );
  }

  void MotionSystem::forget( std::string_view key ) {
    scalarTracks_.erase( std::string( key ) );
    vec2Tracks_.erase( std::string( key ) );
    colorTracks_.erase( std::string( key ) );
  }

  void MotionSystem::forget( const MotionKey& key ) {
    forget( key.value() );
  }

  bool MotionSystem::isActive( std::string_view key ) const {
    return isTrackActive( scalarTracks_, key ) ||
           isTrackActive( vec2Tracks_, key ) ||
           isTrackActive( colorTracks_, key );
  }

  bool MotionSystem::isActive( const MotionKey& key ) const {
    return isActive( key.value() );
  }

  MotionStats MotionSystem::stats() const {
    MotionStats result{};
    result.scalarTracks = scalarTracks_.size();
    result.vec2Tracks   = vec2Tracks_.size();
    result.colorTracks  = colorTracks_.size();
    result.activeTracks = activeTrackCount_;
    result.frameIndex   = frameIndex_;
    result.deltaTime    = deltaTime_;
    return result;
  }

  void MotionSystem::debugOverlay( const char* title ) const {
    if ( title == nullptr ) {
      title = "Motion Debug";
    }

    ImGui::SetNextWindowBgAlpha( 0.92f );
    if ( !ImGui::Begin( title, nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings ) ) {
      ImGui::End();
      return;
    }

    const MotionStats summary = stats();
    ImGui::Text( "Frame: %u", summary.frameIndex );
    ImGui::Text( "Delta: %.4f", summary.deltaTime );
    ImGui::Text( "Active: %zu", summary.activeTracks );
    ImGui::Separator();
    ImGui::Text( "Scalars: %zu", summary.scalarTracks );
    ImGui::Text( "Vec2: %zu", summary.vec2Tracks );
    ImGui::Text( "Colors: %zu", summary.colorTracks );
    ImGui::End();
  }

  MotionSpec MotionSystem::sanitize( const MotionSpec& spec ) const {
    MotionSpec result = spec;
    result.duration   = ImMax( result.duration, 0.0f );
    result.delay      = ImMax( result.delay, 0.0f );
    result.response   = ImMax( result.response, 0.0f );

    if ( reducedMotion_ ) {
      result.delay = 0.0f;

      if ( IsSpring( result.easing ) ) {
        result.response = ImMax( result.response, 20.0f );
      } else {
        result.duration = result.duration <= 0.0f ? 0.0f : ImMin( result.duration, 0.08f );
        if ( result.easing == MotionEasing::EaseOutBack ) {
          result.easing = MotionEasing::EaseOutQuad;
        }
      }
    }

    return result;
  }

  template <typename T>
  T MotionSystem::animate( TrackMap<T>& tracks, std::string_view key, const T& target, const MotionSpec& spec, std::optional<T> initialValue ) {
    const MotionSpec appliedSpec = sanitize( spec );
    Track<T>&        track       = tracks[std::string( key )];

    if ( !track.initialized ) {
      const T startValue     = initialValue.has_value() ? *initialValue : target;
      track.current          = startValue;
      track.source           = startValue;
      track.target           = target;
      track.spec             = appliedSpec;
      track.elapsed          = 0.0f;
      track.initialized      = true;
      track.active           = !NearlyEqual( startValue, target );
      track.lastTouchedFrame = frameIndex_;

      if ( !track.active ) {
        track.current = target;
        track.source  = target;
      }
    } else if ( !NearlyEqual( track.target, target ) ) {
      track.source  = track.current;
      track.target  = target;
      track.spec    = appliedSpec;
      track.elapsed = 0.0f;
      track.active  = !NearlyEqual( track.current, track.target );
    } else if ( !NearlyEqual( track.spec.duration, appliedSpec.duration ) ||
                !NearlyEqual( track.spec.delay, appliedSpec.delay ) ||
                !NearlyEqual( track.spec.response, appliedSpec.response ) ||
                track.spec.easing != appliedSpec.easing ||
                track.spec.snapOnComplete != appliedSpec.snapOnComplete ) {
      track.spec = appliedSpec;
    }

    track.lastTouchedFrame = frameIndex_;

    if ( IsSpring( track.spec.easing ) ) {
      const float response = track.spec.response > 0.0f
                                 ? track.spec.response
                                 : ( track.spec.easing == MotionEasing::SpringSnappy ? 14.0f : 8.0f );

      track.current = SpringToward( track.current, track.target, track.velocity, response, deltaTime_ );
      if ( NearlyEqual( track.current, track.target ) && NearlyEqual( track.velocity, T{} ) ) {
        if ( track.spec.snapOnComplete ) {
          track.current  = track.target;
          track.velocity = T{};
        }
        track.active = false;
      } else {
        track.active = true;
      }
    } else {
      track.elapsed += deltaTime_;

      if ( track.elapsed < track.spec.delay ) {
        track.current = track.source;
        track.active  = !NearlyEqual( track.current, track.target );
      } else {
        const float effectiveDuration = ImMax( track.spec.duration, 0.0001f );
        const float progress          = ImClamp( ( track.elapsed - track.spec.delay ) / effectiveDuration, 0.0f, 1.0f );
        track.current                 = LerpValue( track.source, track.target, Ease( track.spec.easing, progress ) );
        track.active                  = progress < 1.0f && !NearlyEqual( track.current, track.target );

        if ( progress >= 1.0f && track.spec.snapOnComplete ) {
          track.current = track.target;
          track.active  = false;
        }
      }
    }

    if ( track.active ) {
      ++activeTrackCount_;
    }

    return track.current;
  }

  template <typename T>
  void MotionSystem::setTrackValue( TrackMap<T>& tracks, std::string_view key, const T& value ) {
    Track<T>& track        = tracks[std::string( key )];
    track.current          = value;
    track.source           = value;
    track.target           = value;
    track.velocity         = T{};
    track.spec             = MotionSpec::Timed( 0.0f );
    track.elapsed          = 0.0f;
    track.lastTouchedFrame = frameIndex_;
    track.initialized      = true;
    track.active           = false;
  }

  template <typename T>
  bool MotionSystem::isTrackActive( const TrackMap<T>& tracks, std::string_view key ) const {
    const auto it = tracks.find( std::string( key ) );
    return it != tracks.end() && it->second.active;
  }

  template <typename T>
  void MotionSystem::cleanupTracks( TrackMap<T>& tracks ) {
    for ( auto it = tracks.begin(); it != tracks.end(); ) {
      const unsigned age   = frameIndex_ - it->second.lastTouchedFrame;
      const bool     prune = it->second.active ? ( age > kActiveRetentionFrames ) : ( age > kFinishedRetentionFrames );
      if ( prune ) {
        it = tracks.erase( it );
      } else {
        ++it;
      }
    }
  }

  template float  MotionSystem::animate( TrackMap<float>& tracks, std::string_view key, const float& target, const MotionSpec& spec, std::optional<float> initialValue );
  template ImVec2 MotionSystem::animate( TrackMap<ImVec2>& tracks, std::string_view key, const ImVec2& target, const MotionSpec& spec, std::optional<ImVec2> initialValue );
  template ImVec4 MotionSystem::animate( TrackMap<ImVec4>& tracks, std::string_view key, const ImVec4& target, const MotionSpec& spec, std::optional<ImVec4> initialValue );

  template void MotionSystem::setTrackValue( TrackMap<float>& tracks, std::string_view key, const float& value );
  template void MotionSystem::setTrackValue( TrackMap<ImVec2>& tracks, std::string_view key, const ImVec2& value );
  template void MotionSystem::setTrackValue( TrackMap<ImVec4>& tracks, std::string_view key, const ImVec4& value );

  template bool MotionSystem::isTrackActive( const TrackMap<float>& tracks, std::string_view key ) const;
  template bool MotionSystem::isTrackActive( const TrackMap<ImVec2>& tracks, std::string_view key ) const;
  template bool MotionSystem::isTrackActive( const TrackMap<ImVec4>& tracks, std::string_view key ) const;

  template void MotionSystem::cleanupTracks( TrackMap<float>& tracks );
  template void MotionSystem::cleanupTracks( TrackMap<ImVec2>& tracks );
  template void MotionSystem::cleanupTracks( TrackMap<ImVec4>& tracks );
}  // namespace ui::motion
