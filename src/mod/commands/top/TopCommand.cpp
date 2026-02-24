#include "mod/commands/top/TopCommand.h"

#include "mod/OriginMod.h"
#include "mod/api/Player.h"

#include "ll/api/service/Bedrock.h"

#include "mc/client/game/ClientInstance.h"
#include "mc/client/player/LocalPlayer.h"
#include "mc/world/level/BlockSource.h"
#include "mc/world/level/block/Block.h"
#include "mc/deps/core/math/Vec3.h"
#include "mc/world/level/BlockPos.h"
#include <fmt/format.h>

namespace origin_mod::commands::top {

void executeTop(origin_mod::OriginMod& mod, const std::vector<std::string>& /*args*/) {
    auto player = origin_mod::api::Player{mod};

    try {
        auto ciOpt = ll::service::bedrock::getClientInstance();
        if (!ciOpt) {
            player.localSendMessage("§c[!] クライアントインスタンスが取得できません。");
            return;
        }

        auto* localPlayer = ciOpt->getLocalPlayer();
        if (!localPlayer) {
            player.localSendMessage("§c[!] ローカルプレイヤーが取得できません。");
            return;
        }

        // プレイヤーの現在位置を取得
        Vec3 playerPos = localPlayer->getPosition();
        bool groundAbove = false;  // 上にブロックがあるかチェック
        BlockPos blockPos;

        auto& blockSource = localPlayer->getDimensionBlockSource();

        // プレイヤーより上のブロックを探索（最大で316まで）
        for (int y = 0; y < 316 - (int)playerPos.y; ++y) {
            BlockPos checkPos((int)playerPos.x, (int)playerPos.y + y, (int)playerPos.z);
            const Block& block = blockSource.getBlock(checkPos);

            if (!block.isAir()) {  // 空気ブロック以外
                groundAbove = true;
                blockPos = checkPos;
                break;
            }
        }

        if (groundAbove) {
            // 上にブロックが見つかった場合、その上の空気スペースを探す
            for (int y = 0; y < 316 - blockPos.y; ++y) {
                BlockPos checkPos1(blockPos.x, blockPos.y + y, blockPos.z);
                BlockPos checkPos2(blockPos.x, blockPos.y + y + 1, blockPos.z);

                const Block& block1 = blockSource.getBlock(checkPos1);
                const Block& block2 = blockSource.getBlock(checkPos2);

                // 2ブロック分の空間が見つかった
                if (block1.isAir() && block2.isAir()) {
                    Vec3 newPos((float)blockPos.x + 0.5f, (float)(blockPos.y + y + 1), (float)blockPos.z + 0.5f);
                    localPlayer->teleportTo(newPos, false, 0, 1, false);
                    player.localSendMessage("§a[Top] 上空の安全な場所にテレポートしました！");
                    return;
                }
            }
            player.localSendMessage("§c[Top] 上に十分な空間がありません！");
        } else {
            player.localSendMessage("§c[Top] 上にブロックがありません！");
        }

    } catch (const std::exception& e) {
        mod.getSelf().getLogger().error("Top command execution error: {}", e.what());
        player.localSendMessage("§c[!] コマンドの実行中にエラーが発生しました。");
    }
}

} // namespace origin_mod::commands::top