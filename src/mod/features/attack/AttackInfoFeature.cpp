#include <chrono>
#include <string>
#include <cmath>

#include <fmt/format.h>

#include "mod/OriginMod.h"
#include "mod/api/Player.h"
#include "mod/api/World.h"
#include "mod/features/FeatureRegistrar.h"
#include "mod/features/FeatureRegistry.h"
#include "mod/features/IFeature.h"

// LeviLamina includes for attack detection
#include "ll/api/event/EventBus.h"
#include "ll/api/event/Listener.h"
#include "ll/api/event/player/PlayerAttackEvent.h"

// MineCraft includes for entity access
#include "mc/world/actor/Actor.h"
#include "mc/world/actor/player/Player.h"

namespace origin_mod::features {

class AttackInfoFeature final : public IFeature {
public:
    [[nodiscard]] std::string_view id() const noexcept override { return "attackinfo"; }
    [[nodiscard]] std::string_view name() const noexcept override { return "Attack Info"; }
    [[nodiscard]] std::string_view description() const noexcept override {
        return "攻撃時に敵の名前とHP情報を表示します（World Event連携版）";
    }

    void onEnable(origin_mod::OriginMod& mod) override {
        if (mEnabled) return;

        // LeviLamina PlayerAttackEventにリスナーを登録（World経由で発行するため）
        auto& eventBus = ll::event::EventBus::getInstance();

        mAttackListenerId = eventBus.emplaceListener<ll::event::PlayerAttackEvent>(
            [this, &mod](ll::event::PlayerAttackEvent& event) {
                onPlayerAttack(mod, event);
            }
        );

        // World afterEvents().playerAttack にもリスナーを登録（UI表示用）
        auto& world = api::World::instance();
        mPlayerAttackListenerId = world.afterEvents().playerAttack.subscribe(
            [this](api::PlayerAttackEventData& attackData) {
                onPlayerAttackEvent(attackData);
            }
        );

        mEnabled = true;
        mPlayer = std::make_unique<api::Player>(mod);

        mod.getSelf().getLogger().info("AttackInfo enabled - LeviLamina + World Event integration");
    }

    void onDisable(origin_mod::OriginMod& mod) override {
        if (!mEnabled) return;

        // LeviLaminaイベントリスナーを削除
        if (mAttackListenerId) {
            auto& eventBus = ll::event::EventBus::getInstance();
            eventBus.removeListener<ll::event::PlayerAttackEvent>(mAttackListenerId);
            mAttackListenerId = nullptr;
        }

        // EventManagerリスナーを削除
        if (mPlayerAttackListenerId != 0) {
            auto& world = api::World::instance();
            world.afterEvents().playerAttack.unsubscribe(mPlayerAttackListenerId);
            mPlayerAttackListenerId = 0;
        }

        mPlayer.reset();
        mEnabled = false;

        mod.getSelf().getLogger().info("AttackInfo disabled");
    }

private:
    bool mEnabled{false};
    ll::event::ListenerPtr mAttackListenerId{nullptr};
    uint64_t mPlayerAttackListenerId{0};
    std::unique_ptr<api::Player> mPlayer;

    // LeviLamina PlayerAttackEventのハンドラー（World経由でイベントを発行）
    void onPlayerAttack(origin_mod::OriginMod& mod, ll::event::PlayerAttackEvent& event) {
        if (!mEnabled || !mPlayer) return;

        try {
            auto& target = event.target();
            auto& player = event.self();

            // 攻撃データを作成
            api::PlayerAttackEventData attackData{};
            attackData.attackerName = player.getRealName();
            attackData.targetName = target.getNameTag().empty() ?
                (target.getFormattedNameTag().empty() ? target.getTypeName() : target.getFormattedNameTag())
                : target.getNameTag();
            attackData.targetType = target.getTypeName();
            attackData.targetIsPlayer = target.isPlayer();

            // 位置情報
            auto attackerPos = player.getPosition();
            auto targetPos = target.getPosition();

            attackData.attackerX = attackerPos.x;
            attackData.attackerY = attackerPos.y;
            attackData.attackerZ = attackerPos.z;
            attackData.targetX = targetPos.x;
            attackData.targetY = targetPos.y;
            attackData.targetZ = targetPos.z;

            // 距離計算
            double dx = attackData.targetX - attackData.attackerX;
            double dy = attackData.targetY - attackData.attackerY;
            double dz = attackData.targetZ - attackData.attackerZ;
            attackData.distance = std::sqrt(dx*dx + dy*dy + dz*dz);

            // ダメージ情報（LeviLamina PlayerAttackEventには直接の情報がない）
            attackData.damage = 0.0;
            attackData.wasCritical = false;

            // Worldにイベントを発行
            auto& world = api::World::instance();
            world.emitPlayerAttack(attackData);

        } catch (const std::exception& e) {
            mod.getSelf().getLogger().debug("AttackInfo: Error processing LeviLamina attack event: {}", e.what());
        } catch (...) {
            mod.getSelf().getLogger().debug("AttackInfo: Unknown error processing LeviLamina attack event");
        }
    }

    // WorldからのPlayerAttackEventのハンドラー（UI表示）
    void onPlayerAttackEvent(api::PlayerAttackEventData& attackData) {
        if (!mEnabled || !mPlayer) return;

        try {
            // シンプルなUI表示
            std::string message = fmt::format(
                "§e[Attack] §fTarget: {} §c♥HP情報取得中... §f({:.0f}m) {}",
                attackData.targetName,
                attackData.distance,
                attackData.targetIsPlayer ? "§b[Player]" : ""
            );

            mPlayer->localSendMessage(message);

        } catch (const std::exception& e) {
            // エラー時はログ出力のみ
        } catch (...) {
            // 無視
        }
    }
};

} // namespace origin_mod::features

namespace {
void registerAttackInfo(origin_mod::features::FeatureRegistry& reg) {
    (void)reg.registerFeature<origin_mod::features::AttackInfoFeature>("attackinfo");
}
} // namespace

ORIGINMOD_REGISTER_FEATURE(registerAttackInfo);