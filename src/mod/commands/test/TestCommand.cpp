#include "mod/commands/test/TestCommand.h"
#include "mod/api/World.h"
#include "mod/api/Player.h"
#include "mod/OriginMod.h"

#include <fmt/format.h>

namespace origin_mod::commands::test {

void executeTest(OriginMod& mod, const std::vector<std::string>& args) {
    origin_mod::api::Player player{mod};
    auto& world = origin_mod::api::World::instance();

    if (args.size() >= 1 && args[0] == "attack") {
        // 攻撃イベントのテスト用リスナーを登録
        static uint64_t listenerId = 0;

        if (listenerId == 0) {
            listenerId = world.afterEvents().playerAttack.subscribe(
                [&player](origin_mod::api::PlayerAttackEntityEventData& event) {
                    player.localSendMessage(fmt::format(
                        "§6[Attack Event] §f{} attacked {} ({}) - Distance: {:.1f}m {}",
                        event.attackerName,
                        event.targetName,
                        event.targetType,
                        event.distance,
                        event.targetIsPlayer ? "§b[Player]" : ""
                    ));
                }
            );
            player.localSendMessage("§a攻撃イベントリスナーを登録しました");
        } else {
            world.afterEvents().playerAttack.unsubscribe(listenerId);
            listenerId = 0;
            player.localSendMessage("§c攻撃イベントリスナーを解除しました");
        }

    } else {
        player.localSendMessage("=== テストコマンド ===");
        player.localSendMessage("§7使用法:");
        player.localSendMessage("§7  test attack - 攻撃イベントのテスト (ON/OFF)");
    }
}

} // namespace origin_mod::commands::test