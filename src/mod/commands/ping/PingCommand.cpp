#include "mod/commands/ping/PingCommand.h"

#include "mod/OriginMod.h"
#include "mod/api/NetworkInfo.h"
#include "mod/api/Player.h"

#include "ll/api/service/Bedrock.h"

#include "mc/client/game/ClientInstance.h"

#include <fmt/format.h>

namespace origin_mod::commands::ping {

void executePing(origin_mod::OriginMod& mod, const std::vector<std::string>& /*args*/) {
    auto player = origin_mod::api::Player{mod};

    int pingMs = -1;
    double connSec = 0.0;

    try {
        auto ciOpt = ll::service::bedrock::getClientInstance();
        if (ciOpt) {
            pingMs = ciOpt->getServerPingTime();
            connSec = ciOpt->getServerConnectionTime();
        }
    } catch (...) {
    }

    const auto ipPort = origin_mod::api::NetworkInfo::getServerIpPort();

    if (!ipPort.empty()) {
        player.localSendMessage("§e[Ping] §fServer: §b{} §7| §fPing: §a{}ms §7| §fConn: §a{:.1f}s", ipPort, pingMs, connSec);
    } else {
        player.localSendMessage("§e[Ping] §fPing: §a{}ms §7| §fConn: §a{:.1f}s §7| §fServer: §7(unknown)", pingMs, connSec);
    }
}

} // namespace origin_mod::commands::ping
