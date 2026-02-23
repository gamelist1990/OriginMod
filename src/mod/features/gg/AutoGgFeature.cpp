#include <chrono>
#include <random>
#include <string>
#include <string_view>

#include <fmt/format.h>

#include "mod/OriginMod.h"
#include "mod/api/Player.h"
#include "mod/api/World.h"
#include "mod/config/MessageConfig.h"
#include "mod/features/FeatureRegistrar.h"
#include "mod/features/FeatureRegistry.h"
#include "mod/features/IFeature.h"

namespace origin_mod::features {

class AutoGgFeature final : public IFeature {
public:
    [[nodiscard]] std::string_view id() const noexcept override { return "autogg"; }
    [[nodiscard]] std::string_view name() const noexcept override { return "Auto GG"; }
    [[nodiscard]] std::string_view description() const noexcept override {
        return "ゲーム終了時に自動的にGGメッセージを送信します（World Event版）";
    }

    void onEnable(origin_mod::OriginMod& mod) override {
        if (mEnabled) return;

        // World beforeEvents().chatSend にリスナーを登録
        auto& world = api::World::instance();
        mChatListenerId = world.beforeEvents().chatSend.subscribe(
            [this, &mod](api::ChatSendEvent& event) {
                onChatReceive(mod, event.message);
            }
        );

        mEnabled = true;
        mPlayer = std::make_unique<api::Player>(mod);

        mod.getSelf().getLogger().info("AutoGG enabled (World Event API)");
    }

    void onDisable(origin_mod::OriginMod& mod) override {
        if (!mEnabled) return;

        // リスナーを解除
        if (mChatListenerId != 0) {
            auto& world = api::World::instance();
            world.beforeEvents().chatSend.unsubscribe(mChatListenerId);
            mChatListenerId = 0;
        }

        mPlayer.reset();
        mLastGgTime = {};
        mEnabled = false;

        mod.getSelf().getLogger().info("AutoGG disabled");
    }

private:
    bool mEnabled{false};
    uint64_t mChatListenerId{0};
    std::unique_ptr<api::Player> mPlayer;
    std::chrono::steady_clock::time_point mLastGgTime{};

    void onChatReceive(origin_mod::OriginMod& mod, const std::string& message) {
        if (!mEnabled || !mPlayer) return;

        // MessageConfigから設定を取得
        auto& messageConfig = config::MessageConfig::instance();
        const auto& autoggConfig = messageConfig.getAutoGGConfig();

        if (!autoggConfig.enabled) return;

        // World APIでゲーム終了メッセージを検出
        auto& world = api::World::instance();
        if (!world.isGameEndMessage(message)) return;

        mod.getSelf().getLogger().info("AutoGG: Game end detected in message: '{}'", message);

        // クールダウンチェック（3秒）
        auto now = std::chrono::steady_clock::now();
        if (now - mLastGgTime < std::chrono::seconds(3)) {
            mod.getSelf().getLogger().debug("AutoGG: Still in cooldown, skipping");
            return;
        }

        // MessageConfigからランダムメッセージを取得して送信
        auto ggMessage = messageConfig.getRandomAutoGGMessage();
        if (ggMessage.has_value()) {
            try {
                // 設定可能な遅延を使用
                std::mt19937 rng{std::random_device{}()};
                std::uniform_int_distribution<int> delayDist(autoggConfig.minDelay, autoggConfig.maxDelay);
                int delayMs = delayDist(rng);

                // TODO: 将来的に遅延送信を実装（現在は即座に送信）
                mPlayer->sendMessage(ggMessage.value());
                mod.getSelf().getLogger().info("AutoGG: Sent message: '{}' (planned delay: {}ms)",
                    ggMessage.value(), delayMs);
                mLastGgTime = now;
            } catch (...) {
                mod.getSelf().getLogger().debug("AutoGG: Failed to send message");
            }
        }
    }
};

} // namespace origin_mod::features

namespace {
void registerAutoGg(origin_mod::features::FeatureRegistry& reg) {
    (void)reg.registerFeature<origin_mod::features::AutoGgFeature>("autogg");
}
} // namespace

ORIGINMOD_REGISTER_FEATURE(registerAutoGg);