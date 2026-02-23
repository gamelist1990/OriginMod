#pragma once

#include <vector>
#include <string>

namespace origin_mod {
class OriginMod;
}

namespace origin_mod::commands::help {

// ヘルプコマンドを実行
void executeHelp(OriginMod& mod, const std::vector<std::string>& args);

} // namespace origin_mod::commands::help