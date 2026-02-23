#include "mod/commands/players/PlayersCommand.h"
#include "mod/api/World.h"
#include "mod/api/Player.h"
#include "mod/OriginMod.h"

#include <fmt/format.h>

namespace origin_mod::commands::players {

void executePlayers(OriginMod& mod, const std::vector<std::string>& args) {
    origin_mod::api::Player player{mod};
    auto& world = origin_mod::api::World::instance();

    if (args.size() >= 1 && args[0] == "location") {
        // プレイヤーの位置情報を表示
        auto players = world.getPlayers(mod);

        player.localSendMessage("=== プレイヤー位置情報 ===");
        for (const auto& p : players) {
            auto loc = p.location();
            auto health = p.getHealth();
            player.localSendMessage(fmt::format("§a{}: §f位置({:.1f}, {:.1f}, {:.1f}) 体力: {}",
                p.name(), loc.x, loc.y, loc.z, health));
        }

    } else if (args.size() >= 1 && args[0] == "entities") {
        // 全エンティティ情報を表示
        auto entities = world.getEntities(mod);

        player.localSendMessage(fmt::format("=== エンティティ一覧 ({} 個) ===", entities.size()));

        int count = 0;
        int playerCount = 0;
        for (const auto& entity : entities) {
            if (count >= 10) {
                player.localSendMessage("§e... (最初の10個のみ表示)");
                break;
            }

            auto loc = entity.location();
            auto health = entity.getHealth();
            std::string name = entity.getName();
            std::string typeName = entity.getTypeName();

            if (name.empty()) {
                name = "無名";
            }

            bool isPlayerEntity = entity.isPlayer();
            if (isPlayerEntity) {
                playerCount++;
            }

            player.localSendMessage(fmt::format("§e{}§f [{}]: 位置({:.1f}, {:.1f}, {:.1f}) 体力: {} {}",
                name, typeName, loc.x, loc.y, loc.z, health,
                isPlayerEntity ? "§b[プレイヤー]" : ""));
            count++;
        }

        player.localSendMessage(fmt::format("§7プレイヤーエンティティ: {} / {} 個", playerCount, entities.size()));

    } else if (args.size() >= 1 && args[0] == "debug") {
        // デバッグ情報を表示
        auto playerNames = world.getPlayerNames();
        auto players = world.getPlayers(mod);
        auto entities = world.getEntities(mod);

        player.localSendMessage("=== デバッグ情報 ===");
        player.localSendMessage(fmt::format("§7PlayerNames: {} 個", playerNames.size()));
        player.localSendMessage(fmt::format("§7Player Objects: {} 個", players.size()));
        player.localSendMessage(fmt::format("§7All Entities: {} 個", entities.size()));

        // Actorからプレイヤーを探索
        int actorPlayerCount = 0;
        for (const auto& entity : entities) {
            if (entity.isPlayer()) {
                actorPlayerCount++;
            }
        }
        player.localSendMessage(fmt::format("§7Actor isPlayer(): {} 個", actorPlayerCount));

        // Level情報を表示
        auto level = ll::service::getLevel();
        if (level) {
            player.localSendMessage(fmt::format("§7Level: 有効, ClientSide: {}",
                level->isClientSide() ? "Yes" : "No"));

            // getRuntimeActorListのサイズを直接確認
            try {
                auto actors = level->getRuntimeActorList();
                player.localSendMessage(fmt::format("§7RuntimeActorList: {} 個", actors.size()));
            } catch (...) {
                player.localSendMessage("§7RuntimeActorList: アクセスエラー");
            }
        } else {
            player.localSendMessage("§cLevel: 無効");
        }

    } else {
        // プレイヤー一覧を表示（引数なし、または不明な引数）
        auto playerNames = world.getPlayerNames();

        player.localSendMessage(fmt::format("=== オンラインプレイヤー ({}) ===", playerNames.size()));
        for (const auto& name : playerNames) {
            player.localSendMessage(fmt::format("§a- {}", name));
        }

        player.localSendMessage("§7使用法:");
        player.localSendMessage("§7  players location - プレイヤーの位置と体力を表示");
        player.localSendMessage("§7  players entities - 全エンティティを表示");
        player.localSendMessage("§7  players debug - プレイヤー検出のデバッグ情報");
    }
}

} // namespace origin_mod::commands::players