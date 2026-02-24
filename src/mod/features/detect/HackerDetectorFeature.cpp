#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>

#include <fmt/format.h>
#include <nlohmann/json.hpp>

#include "mod/OriginMod.h"
#include "mod/api/Player.h"
#include "mod/api/World.h"
#include "mod/config/ConfigManager.h"
#include "mod/features/FeatureRegistrar.h"
#include "mod/features/FeatureRegistry.h"
#include "mod/features/IFeature.h"

#include "ll/api/service/Bedrock.h"

#include "mc/client/game/ClientInstance.h"
#include "mc/client/player/LocalPlayer.h"
#include "mc/deps/core/math/Vec3.h"
#include "mc/world/actor/Actor.h"
#include "mc/world/level/Level.h"

namespace origin_mod::features {

namespace {
using Clock = std::chrono::steady_clock;
using json  = nlohmann::json;

enum class MessageMode {
    Client,
    Server,
};

struct Settings {
    MessageMode mode{MessageMode::Client};

    bool immobileCheck{false};
    bool speedCheck{true};
    bool derpCheck{true};
    bool flyCheck{false};

    int strictness{2};           // 0..5 (higher = stricter)
    int allowedTicksBase{13};    // original reference
    float allowedGroundBase{14.5f};
    float allowedAirBase{13.5f};

    int cooldownMs{1200};        // anti-spam
    int minFailsToReport{2};     // require consecutive fails
};

struct Tracked {
    Vec3 lastPos{};
    Clock::time_point lastSeen{};
    Clock::time_point lastSample{};

    int speedFails{0};
    int derpFails{0};
    int offGroundTicks{0};

    Clock::time_point lastReport{};
};

static Settings loadSettings(origin_mod::OriginMod& mod) {
    Settings s;

    try {
        auto& cm = origin_mod::config::ConfigManager::instance();

        // Create default if missing
        json def;
        def["messages"] = "client";
        def["immobileCheck"] = false;
        def["speedCheck"] = true;
        def["derpCheck"] = true;
        def["flyCheck"] = false;
        def["strictness"] = 2;
        def["cooldownMs"] = 1200;
        def["minFailsToReport"] = 2;
        cm.ensureDefaultConfig("hackerdetector.json", def);

        auto cfgOpt = cm.loadConfig("hackerdetector.json");
        if (!cfgOpt) return s;

        auto const& cfg = *cfgOpt;
        if (!cfg.is_object()) return s;

        if (auto it = cfg.find("messages"); it != cfg.end() && it->is_string()) {
            auto m = it->get<std::string>();
            std::transform(m.begin(), m.end(), m.begin(), [](unsigned char c) { return (char)std::tolower(c); });
            if (m == "server") s.mode = MessageMode::Server;
            else s.mode = MessageMode::Client;
        }

        if (auto it = cfg.find("immobileCheck"); it != cfg.end() && it->is_boolean()) s.immobileCheck = it->get<bool>();
        if (auto it = cfg.find("speedCheck"); it != cfg.end() && it->is_boolean()) s.speedCheck = it->get<bool>();
        if (auto it = cfg.find("derpCheck"); it != cfg.end() && it->is_boolean()) s.derpCheck = it->get<bool>();
        if (auto it = cfg.find("flyCheck"); it != cfg.end() && it->is_boolean()) s.flyCheck = it->get<bool>();

        if (auto it = cfg.find("strictness"); it != cfg.end() && it->is_number_integer()) {
            s.strictness = std::clamp(it->get<int>(), 0, 5);
        }
        if (auto it = cfg.find("cooldownMs"); it != cfg.end() && it->is_number_integer()) {
            s.cooldownMs = std::max(0, it->get<int>());
        }
        if (auto it = cfg.find("minFailsToReport"); it != cfg.end() && it->is_number_integer()) {
            s.minFailsToReport = std::clamp(it->get<int>(), 1, 20);
        }

    } catch (const std::exception& e) {
        mod.getSelf().getLogger().debug("HackerDetector: loadSettings failed: {}", e.what());
    } catch (...) {
    }

    return s;
}

static void sendReport(origin_mod::api::Player& player, MessageMode mode, std::string_view msg) {
    if (mode == MessageMode::Server) player.sendMessage(msg);
    else player.localSendMessage(msg);
}

static std::string sanitizeName(std::string name) {
    name = origin_mod::api::World::removeColorCodes(name);
    name.erase(std::remove(name.begin(), name.end(), '\n'), name.end());
    name.erase(std::remove(name.begin(), name.end(), '\r'), name.end());
    while (!name.empty() && (name.back() == ' ' || name.back() == '\t')) name.pop_back();
    while (!name.empty() && (name.front() == ' ' || name.front() == '\t')) name.erase(name.begin());
    if (name.empty()) return "Invalid Name";
    return name;
}

static int64_t actorUid(::Actor& a) {
    try {
        return a.getOrCreateUniqueID().rawID;
    } catch (...) {
        return reinterpret_cast<int64_t>(&a);
    }
}

} // namespace

class HackerDetectorFeature final : public IFeature {
public:
    [[nodiscard]] std::string_view id() const noexcept override { return "hackerdetect"; }
    [[nodiscard]] std::string_view name() const noexcept override { return "Hacker Detector"; }
    [[nodiscard]] std::string_view description() const noexcept override {
        return "他プレイヤーの異常な移動/回転を検知して通知します（誤検知あり）";
    }

    void onEnable(origin_mod::OriginMod& mod) override {
        mEnabled = true;
        mSettings = loadSettings(mod);
        mTracked.clear();
        mod.getSelf().getLogger().info("HackerDetector enabled");
    }

    void onDisable(origin_mod::OriginMod& mod) override {
        (void)mod;
        mEnabled = false;
        mTracked.clear();
    }

    void onTick(origin_mod::OriginMod& mod) override {
        if (!mEnabled) return;

        // Refresh settings periodically (allows -config reload hackerdetector.json)
        const auto now = Clock::now();
        if (now - mLastSettingsRefresh > std::chrono::seconds(2)) {
            mSettings = loadSettings(mod);
            mLastSettingsRefresh = now;
        }

        auto ciOpt = ll::service::bedrock::getClientInstance();
        if (!ciOpt) return;
        auto* lp = ciOpt->getLocalPlayer();
        if (!lp) return;

        ::Level* level = &lp->getLevel();
        if (!level) return;

        origin_mod::api::Player me{mod};

        const float allowedGroundBps = mSettings.allowedGroundBase - (float)mSettings.strictness;
        const float allowedAirBps    = mSettings.allowedAirBase - (float)mSettings.strictness;
        const int allowedTicks       = std::max(1, mSettings.allowedTicksBase - mSettings.strictness);

        // Iterate all actors, focus on players.
        auto actors = level->getRuntimeActorList();
        for (auto* a : actors) {
            if (!a) continue;
            if (a == lp) continue;

            bool isPlayer = false;
            bool alive = false;
            try {
                isPlayer = a->isPlayer();
                alive = a->isAlive();
            } catch (...) {
                continue;
            }
            if (!isPlayer || !alive) continue;

            const int64_t uid = actorUid(*a);
            if (uid == 0) continue;

            // Name
            std::string name;
            try {
                name = a->getNameTag();
                if (name.empty()) name = a->getFormattedNameTag();
            } catch (...) {
            }
            name = sanitizeName(std::move(name));

            auto& t = mTracked[uid];
            t.lastSeen = now;

            // rotation: Vec2(x=pitch, y=yaw) in most bedrock builds
            float pitch = 0.0f;
            float yaw = 0.0f;
            try {
                auto const& rot = a->getRotation();
                pitch = rot.x;
                yaw = rot.y;
            } catch (...) {
            }

            // Ground state
            bool onGround = false;
            try {
                onGround = a->isOnGround();
            } catch (...) {
            }

            // Position sample
            Vec3 pos{};
            try {
                pos = a->getPosition();
            } catch (...) {
                continue;
            }

            if (t.lastSample.time_since_epoch().count() == 0) {
                t.lastSample = now;
                t.lastPos = pos;
                continue;
            }

            const auto dt = std::chrono::duration_cast<std::chrono::duration<float>>(now - t.lastSample).count();
            if (dt <= 0.0001f) continue;

            const float dx = pos.x - t.lastPos.x;
            const float dz = pos.z - t.lastPos.z;
            const float distXZ = std::sqrt(dx * dx + dz * dz);
            const float bps = distXZ / dt;

            t.lastSample = now;
            t.lastPos = pos;

            // === Speed Check ===
            if (mSettings.speedCheck) {
                const float allowed = onGround ? allowedGroundBps : allowedAirBps;
                if (bps >= allowed) {
                    t.speedFails++;
                } else {
                    t.speedFails = 0;
                }

                if (t.speedFails >= mSettings.minFailsToReport) {
                    const auto cooldown = std::chrono::milliseconds(mSettings.cooldownMs);
                    if (cooldown.count() == 0 || (now - t.lastReport) >= cooldown) {
                        t.lastReport = now;
                        const char* ab = onGround ? "A" : "B";
                        sendReport(
                            me,
                            mSettings.mode,
                            fmt::format(
                                "§7[Detect] §f{} §7failed §eSpeed-{} §7({:.1f} BPS) §fx{}",
                                name,
                                ab,
                                bps,
                                t.speedFails
                            )
                        );
                    }
                }
            }

            // === Derp/Invalid Rotation Check ===
            if (mSettings.derpCheck) {
                const bool invalid = (yaw <= -181.0f || yaw >= 181.0f || pitch >= 90.0f || pitch <= -90.0f);
                if (invalid) {
                    t.derpFails++;
                } else {
                    t.derpFails = 0;
                }

                if (t.derpFails >= mSettings.minFailsToReport) {
                    const auto cooldown = std::chrono::milliseconds(mSettings.cooldownMs);
                    if (cooldown.count() == 0 || (now - t.lastReport) >= cooldown) {
                        t.lastReport = now;
                        sendReport(
                            me,
                            mSettings.mode,
                            fmt::format(
                                "§7[Detect] §f{} §7failed §eInvalidRot §7(pitch={:.1f}, yaw={:.1f}) §fx{}",
                                name,
                                pitch,
                                yaw,
                                t.derpFails
                            )
                        );
                    }
                }
            }

            // === Fly Check (best-effort, disabled by default) ===
            if (mSettings.flyCheck) {
                if (onGround) {
                    t.offGroundTicks = 0;
                } else {
                    t.offGroundTicks++;
                }

                if (t.offGroundTicks > allowedTicks) {
                    Vec3 vel{};
                    try {
                        vel = a->getVelocity();
                    } catch (...) {
                        vel = Vec3{0, 0, 0};
                    }

                    const float velXZ = std::sqrt(vel.x * vel.x + vel.z * vel.z);
                    const bool hovering = std::fabs(vel.y) < 0.02f && velXZ > 0.15f;
                    if (hovering) {
                        const auto cooldown = std::chrono::milliseconds(mSettings.cooldownMs);
                        if (cooldown.count() == 0 || (now - t.lastReport) >= cooldown) {
                            t.lastReport = now;
                            sendReport(
                                me,
                                mSettings.mode,
                                fmt::format(
                                    "§7[Detect] §f{} §7failed §eFly §7(offGroundTicks={}, velY={:.2f})",
                                    name,
                                    t.offGroundTicks,
                                    vel.y
                                )
                            );
                        }
                    }
                }
            }

            // Immobile check: placeholder (kept for compatibility with config)
            (void)mSettings.immobileCheck;
        }

        // prune old tracked entries
        for (auto it = mTracked.begin(); it != mTracked.end();) {
            if (now - it->second.lastSeen > std::chrono::seconds(10)) {
                it = mTracked.erase(it);
            } else {
                ++it;
            }
        }
    }

private:
    bool mEnabled{false};
    Settings mSettings{};
    Clock::time_point mLastSettingsRefresh{};
    std::unordered_map<int64_t, Tracked> mTracked;
};

} // namespace origin_mod::features

namespace {
void registerHackerDetector(origin_mod::features::FeatureRegistry& reg) {
    (void)reg.registerFeature<origin_mod::features::HackerDetectorFeature>("hackerdetect");
}
} // namespace

ORIGINMOD_REGISTER_FEATURE(registerHackerDetector);
