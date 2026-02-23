#pragma once

#include <vector>
#include <string>

namespace origin_mod {
class OriginMod;
}

namespace origin_mod::commands::toggle {

// トグルコマンドを実行
void executeToggle(OriginMod& mod, const std::vector<std::string>& args);

} // namespace origin_mod::commands::toggle