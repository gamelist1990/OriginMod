#pragma once

#include <string>
#include "mod/api/Player.h"

namespace origin_mod {
class OriginMod;
}

namespace origin_mod::events {

/**
 * プレイヤー攻撃イベントデータ
 */
struct PlayerAttackEventData {
    std::string attackerName;      // 攻撃者名
    std::string targetName;        // 対象名
    std::string targetType;        // 対象のタイプ名
    double damage;                 // ダメージ値
    bool wasCritical;             // クリティカルヒットか
    double distance;              // 攻撃距離
    bool targetIsPlayer;          // 対象がプレイヤーか

    // 位置情報
    double attackerX, attackerY, attackerZ;
    double targetX, targetY, targetZ;
};

/**
 * プレイヤー攻撃イベントヘルパー
 * AttackInfoFeatureから移動してきた処理ロジック
 */
class PlayerAttackEventHelper {
public:
    /**
     * LeviLamina PlayerAttackEventから我々のイベントデータを作成
     */
    static PlayerAttackEventData createFromLeviLamina(
        const class Player& attacker,
        const class Actor& target
    );

    /**
     * 攻撃距離を計算
     */
    static double calculateDistance(
        double x1, double y1, double z1,
        double x2, double y2, double z2
    );

    /**
     * ターゲット名を取得（優先順位: NameTag -> FormattedNameTag -> TypeName）
     */
    static std::string getTargetName(const class Actor& target);

private:
    PlayerAttackEventHelper() = default;
};

} // namespace origin_mod::events