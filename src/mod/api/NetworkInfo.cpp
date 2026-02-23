#include "mod/api/NetworkInfo.h"

#include <mutex>

namespace origin_mod::api {

namespace {
std::mutex gMutex;
std::string gServerIpPort;
}

void NetworkInfo::setServerIpPort(std::string_view ipPort) {
    std::scoped_lock lk(gMutex);
    gServerIpPort.assign(ipPort.begin(), ipPort.end());
}

std::string NetworkInfo::getServerIpPort() {
    std::scoped_lock lk(gMutex);
    return gServerIpPort;
}

} // namespace origin_mod::api
