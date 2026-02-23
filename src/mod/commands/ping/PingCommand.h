#pragma once

#include <string>
#include <vector>

namespace origin_mod {
class OriginMod;
}

namespace origin_mod::commands::ping {

void executePing(origin_mod::OriginMod& mod, const std::vector<std::string>& args);

} // namespace origin_mod::commands::ping
