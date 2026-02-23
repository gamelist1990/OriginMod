#include <string>
#include <vector>
#include <functional>
#include <fmt/format.h>

#include "mod/OriginMod.h"
#include "mod/commands/OriginCommandRegistry.h"
#include "mod/commands/help/HelpCommand.h"

namespace {

void printHeader(origin_mod::OriginMod& mod, std::function<void(std::string const&)> reply) {
    auto const& modName = mod.getSelf().getName();
    std::string version;
    if (auto const& v = mod.getSelf().getManifest().version; v.has_value()) {
        version = v->to_string();
    }

    reply("§7========================================");
    if (!version.empty()) {
        reply(fmt::format("§b {} §fv{}", modName, version));
    } else {
        reply(fmt::format("§b {}", modName));
    }
    reply("§7----------------------------------------");
}

} // namespace

namespace origin_mod::commands::builtin {

bool registerHelp(origin_mod::commands::OriginCommandRegistry& command) {
    origin_mod::commands::OriginCommandRegistry::Descriptor d;
    d.name = "help";
    d.description = "ヘルプを表示します";
    d.minArgs = 0;
    d.maxArgs = 1;
    d.executor = [&command](origin_mod::commands::OriginCommandRegistry::Descriptor::Context const& ctx, std::vector<std::string> const& /*args*/) {
        auto reply = [&ctx](std::string const& s) { ctx.player.localSendMessage(s); };
        printHeader(ctx.mod, reply);

        ctx.player.localSendMessage("  §e-help §7- §fこのヘルプ");
        for (auto const& meta : command.list()) {
            ctx.player.localSendMessage("  §e-{} §7- §f{}", meta.name, meta.description);
        }
        ctx.player.localSendMessage("§7========================================");
    };
    return command.registerCommand(std::move(d));
}

} // namespace origin_mod::commands::builtin
