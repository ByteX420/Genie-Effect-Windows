#pragma once

#include <windows.h>

#include "features/effect_policy.hpp"

namespace minimize::rendering {
class OverlayWindow;
}
namespace minimize::settings {
class SettingsService;
}

namespace minimize::features {

class AnimationConfiguration final {
public:
  AnimationConfiguration(const settings::SettingsService& settings, const EffectPolicy& policy);

  [[nodiscard]] float Apply(rendering::OverlayWindow& overlay, const RECT& source, bool restoring,
                            const RenderingPressure& pressure) const;

private:
  const settings::SettingsService& settings_;
  const EffectPolicy& policy_;
};

}  // namespace minimize::features
