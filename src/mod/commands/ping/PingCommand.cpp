#include "mod/commands/ping/PingCommand.h"

#include "mod/OriginMod.h"
#include "mod/api/Player.h"

#include "ll/api/service/Bedrock.h"

#include "mc/client/game/ClientInstance.h"
#include "mc/network/GameConnectionInfo.h"
#include "mc/world/Minecraft.h"
#include <fmt/format.h>

namespace origin_mod::commands::ping {

void executePing(origin_mod::OriginMod& mod, const std::vector<std::string>& /*args*/) {
    auto        player     = origin_mod::api::Player{mod};
    int         pingMs     = -1;
    double      connSec    = -1.0;
    std::string serverText = "(unknown)";

    try {
        auto ciOpt = ll::service::bedrock::getClientInstance();
        if (ciOpt) {
            try {
                auto gciOpt = ciOpt->getGameConnectionInfo();
                if (gciOpt) {
                    const auto&       gci  = *gciOpt;
                    const std::string host = gci.mHostIpAddress;
                    const int         port = gci.mPort;

                    if (!host.empty() && port > 0) {
                        serverText = fmt::format("{}:{}", host, port);
                    } else {
                        const std::string unresolved = gci.mUnresolvedUrl;
                        if (!unresolved.empty()) {
                            serverText = unresolved;
                        }
                    }
                }
            } catch (...) {}

            pingMs = ciOpt->getRemoteServerNetworkTimeMs();
            mod.getSelf().getLogger().info(
                " | getRemoteServerTimeMs: {}ms"
                " | getRemoteServerNetworkTimeMs: {}ms"
                " | getServerPingTime: {}ms"
                " | hasGameConnectionInfo: {}",
                ciOpt->getRemoteServerTimeMs(),
                ciOpt->getRemoteServerNetworkTimeMs(),
                ciOpt->getServerPingTime(),
                ciOpt->getGameConnectionInfo().has_value()
            );
            connSec = ciOpt->getServerConnectionTime();
        }
    } catch (...) {}

    const std::string pingText = (pingMs) ? fmt::format("{}ms", pingMs) : "(measuring...)";
    const std::string connText = (connSec >= 0.0) ? fmt::format("{:.1f}s", connSec) : "(unknown)";

    player.localSendMessage(
        fmt::format("§e[Ping] §fServer: §b{} §7| §fPing: §a{} §7| §fConn: §a{}", serverText, pingText, connText)
    );
}

} // namespace origin_mod::commands::ping
