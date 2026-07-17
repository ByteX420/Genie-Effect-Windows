#pragma once

#include <windows.h>

#include "features/effect_policy.hpp"

namespace genie::rendering {
class OverlayWindow;
}
namespace genie::settings {
class SettingsService;
}

namespace genie::features {

class AnimationConfiguration final {
public:
  AnimationConfiguration(const settings::SettingsService& settings, const EffectPolicy& policy);

  [[nodiscard]] float Apply(rendering::OverlayWindow& overlay, const RECT& source, bool restoring,
                            const RenderingPressure& pressure) const;

private:
  const settings::SettingsService& settings_;
  const EffectPolicy& policy_;
};

}  // namespace genie::features
