#include <string>
#include <string_view>

#include "mod/OriginMod.h"
#include "mod/api/Player.h"
#include "mod/features/FeatureRegistrar.h"
#include "mod/features/FeatureRegistry.h"
#include "mod/features/IFeature.h"

namespace origin_mod::features {

class AutoSprintFeature final : public IFeature {
public:
    [[nodiscard]] std::string_view id() const noexcept override { return "autosprint"; }
    [[nodiscard]] std::string_view name() const noexcept override { return "Auto Sprint"; }
    [[nodiscard]] std::string_view description() const noexcept override {
        return "常にダッシュ状態を維持します";
    }

    void onTick(origin_mod::OriginMod& mod) override {
        // Keep sprinting every tick. If not in-game, Player wrapper no-ops.
        api::Player{mod}.setSprinting(true);
    }
};

} // namespace origin_mod::features

namespace {
void registerAutoSprint(origin_mod::features::FeatureRegistry& reg) {
    (void)reg.registerFeature<origin_mod::features::AutoSprintFeature>("autosprint");
}
} // namespace

ORIGINMOD_REGISTER_FEATURE(registerAutoSprint);
