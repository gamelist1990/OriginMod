#pragma once

#include <vector>
#include <string>

namespace origin_mod {
class OriginMod;
}

namespace origin_mod::commands::players {

// プレイヤーコマンドを実行
void executePlayers(OriginMod& mod, const std::vector<std::string>& args);

} // namespace origin_mod::commands::players