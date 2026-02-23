#include "mod/commands/toggle/ToggleCommand.h"
#include "mod/OriginMod.h"
#include "mod/api/Player.h"
#include "mod/features/FeatureManager.h"
#include <fmt/format.h>
#include <optional>

namespace origin_mod::commands::toggle {

void executeToggle(OriginMod& mod, const std::vector<std::string>& args) {
    origin_mod::api::Player player{mod};
    auto& fm = features::FeatureManager::instance();
    fm.initialize(mod);

    if (args.empty() || (args.size() == 1 && args[0] == "list")) {
        // 機能一覧を表示
        auto list = fm.list();
        player.localSendMessage("§7========================================");
        player.localSendMessage(fmt::format("§b Features §7| §f{} 件", list.size()));
        player.localSendMessage("§7----------------------------------------");
        for (const auto& f : list) {
            auto status = f.enabled ? "§a● ON " : "§c● OFF";
            player.localSendMessage(fmt::format("  {} §e{} §7- §f{}", status, f.id, f.description));
        }
        player.localSendMessage("§7========================================");
        return;
    }

    if (args.size() >= 2 && args[0] == "set") {
        // 機能の有効/無効を切り替え
        std::string feature = args[1];
        std::optional<bool> desired;
        if (args.size() >= 3) {
            auto s = args[2];
            if (s == "on" || s == "ON" || s == "true") {
                desired = true;
            } else if (s == "off" || s == "OFF" || s == "false") {
                desired = false;
            }
        }

        auto res = fm.setEnabled(mod, feature, desired);
        if (!res.has_value()) {
            player.localSendMessage(fmt::format("§c[!] 不明な feature: §f{}", feature));
            return;
        }
        auto statusColor = *res ? "§a" : "§c";
        auto statusText  = *res ? "ON" : "OFF";
        player.localSendMessage(fmt::format("§7[§b{}§7] §e{} §7= {}{}",
            mod.getSelf().getName(), feature, statusColor, statusText));
        return;
    }

    player.localSendMessage("§c[!] コマンドが不正です。");
    player.localSendMessage("§7使用法: -toggle [list|set <feature> [on|off]]");
}

} // namespace origin_mod::commands::toggle