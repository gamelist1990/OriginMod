#include <optional>
#include <string>
#include <vector>
#include <fmt/format.h>

#include "mod/OriginMod.h"
#include "mod/commands/OriginCommandRegistry.h"
#include "mod/commands/OriginCommandTypes.h"
#include "mod/features/FeatureManager.h"
#include "mod/commands/toggle/ToggleCommand.h"

namespace {

} // namespace

namespace origin_mod::commands::builtin {

bool registerToggle(origin_mod::commands::OriginCommandRegistry& command) {
    origin_mod::commands::OriginCommandRegistry::Descriptor d;
    d.name = "toggle";
    d.description = "Feature の有効/無効を切り替えます";
    d.minArgs = 0;
    d.maxArgs = 3;
    d.executor = [](origin_mod::commands::OriginCommandRegistry::Descriptor::Context const& ctx, std::vector<std::string> const& args) {
            auto& fm = origin_mod::features::FeatureManager::instance();
            fm.initialize(ctx.mod);

            // args: tokens after subcommand name. Examples:
            // -toggle list                       -> args = {"list"}
            // -toggle set <feature> [on|off]     -> args = {"set","feature","on"}

            if (args.empty() || (args.size() == 1 && args[0] == "list")) {
                auto list = fm.list();
                ctx.player.localSendMessage("§7========================================");
                ctx.player.localSendMessage("§b Features §7| §f{} 件", list.size());
                ctx.player.localSendMessage("§7----------------------------------------");
                for (auto const& f : list) {
                    auto status = f.enabled ? "§a● ON " : "§c● OFF";
                    ctx.player.localSendMessage("  {} §e{} §7- §f{}", status, f.id, f.description);
                }
                ctx.player.localSendMessage("§7========================================");
                return;
            }

            if (args.size() >= 2 && args[0] == "set") {
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

                auto res = fm.setEnabled(ctx.mod, feature, desired);
                if (!res.has_value()) {
                    ctx.player.localSendMessage("§c[!] 不明な feature: §f{}", feature);
                    return;
                }
                auto statusColor = *res ? "§a" : "§c";
                auto statusText  = *res ? "ON" : "OFF";
                ctx.player.localSendMessage("§7[§b{}§7] §e{} §7= {}{}", ctx.mod.getSelf().getName(), feature, statusColor, statusText);
                return;
            }

            ctx.player.localSendMessage("§c[!] コマンドが不正です。");
    };
    return command.registerCommand(std::move(d));
}

} // namespace origin_mod::commands::builtin
