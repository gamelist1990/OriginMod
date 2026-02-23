#include <atomic>
#include <random>

#include "mc/client/player/LocalPlayer.h"
#include "mc/deps/core/math/Vec2.h"

#include "ll/api/memory/Hook.h"

#include "mod/OriginMod.h"
#include "mod/features/FeatureRegistrar.h"
#include "mod/features/FeatureRegistry.h"
#include "mod/features/IFeature.h"

// ---------------------------------------------------------------------------
// Hook: LocalPlayer::_applyTurnDelta
// ---------------------------------------------------------------------------

namespace {

std::atomic_bool g_aimJitterEnabled{false};

constexpr float kAmplitude = 0.75f;

float jitterComponent(std::mt19937& rng) {
    std::uniform_real_distribution<float> dist(-kAmplitude, kAmplitude);
    return dist(rng);
}

} // namespace

LL_TYPE_INSTANCE_HOOK(
    AimJitterApplyTurnDeltaHook,
    ll::memory::HookPriority::Normal,
    LocalPlayer,
    &LocalPlayer::_applyTurnDelta,
    void,
    ::Vec2 const& turnOffset
) {
    if (g_aimJitterEnabled.load(std::memory_order_relaxed)) {
        thread_local std::mt19937 rng{std::random_device{}()};
        ::Vec2 modified{turnOffset.x + jitterComponent(rng), turnOffset.y + jitterComponent(rng)};
        origin(modified);
        return;
    }
    origin(turnOffset);
}

// ---------------------------------------------------------------------------
// Feature
// ---------------------------------------------------------------------------

namespace origin_mod::features {

class AimJitterFeature final : public IFeature {
public:
    [[nodiscard]] std::string_view id() const noexcept override { return "aimjitter"; }
    [[nodiscard]] std::string_view name() const noexcept override { return "AimJitter"; }
    [[nodiscard]] std::string_view description() const noexcept override { return "視点がブレます (applyTurnDelta hook)"; }

    void onEnable(origin_mod::OriginMod& mod) override {
        if (!mHooked) {
            AimJitterApplyTurnDeltaHook::hook();
            mHooked = true;
            mod.getSelf().getLogger().debug("AimJitter: hook installed");
        }
        g_aimJitterEnabled.store(true, std::memory_order_relaxed);
        mod.getSelf().getLogger().debug("AimJitter enabled");
    }

    void onDisable(origin_mod::OriginMod& mod) override {
        g_aimJitterEnabled.store(false, std::memory_order_relaxed);
        if (mHooked) {
            AimJitterApplyTurnDeltaHook::unhook();
            mHooked = false;
            mod.getSelf().getLogger().debug("AimJitter: hook removed");
        }
        mod.getSelf().getLogger().debug("AimJitter disabled");
    }

private:
    bool mHooked{false};
};

} // namespace origin_mod::features

namespace {
void registerAimJitter(origin_mod::features::FeatureRegistry& reg) {
    (void)reg.registerFeature<origin_mod::features::AimJitterFeature>("aimjitter");
}
} // namespace

ORIGINMOD_REGISTER_FEATURE(registerAimJitter);
