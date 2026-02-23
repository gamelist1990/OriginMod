#include "mod/commands/help/HelpCommand.h"
#include "mod/OriginMod.h"
#include "mod/commands/CommandManager.h"
#include "mod/api/Player.h"
#include <fmt/format.h>
#include "ll/api/Versions.h"

namespace origin_mod::commands::help {

void executeHelp(OriginMod& mod, const std::vector<std::string>& args) {
    origin_mod::api::Player player{mod};

    player.localSendMessage("§7========================================");

    std::string version = "0.0.0";
    if (auto const& v = mod.getSelf().getManifest().version; v.has_value()) {
        version = v->to_string();
    }

    player.localSendMessage(fmt::format("§b {} §fv{}", mod.getSelf().getName(), version));
    player.localSendMessage("§7----------------------------------------");
    auto& cmdManager = CommandManager::getInstance();
    auto commands = cmdManager.getCommands();

    player.localSendMessage(fmt::format("§e利用可能なコマンド §7({}件):", commands.size()));
    for (const auto& cmd : commands) {
        player.localSendMessage(fmt::format("  §e-{} §7- §f{}", cmd.name, cmd.description));
    }

    player.localSendMessage("§7========================================");
}

} // namespace origin_mod::commands::help
