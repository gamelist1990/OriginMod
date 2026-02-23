#pragma once

#include <string>
#include <string_view>

namespace origin_mod::api {

// Simple shared cache for current server endpoint info.
// Best-effort: updated from packet hooks when available.
class NetworkInfo {
public:
    static void setServerIpPort(std::string_view ipPort);
    [[nodiscard]] static std::string getServerIpPort();

private:
    NetworkInfo() = default;
};

} // namespace origin_mod::api
