#pragma once

#include <vector>
#include <string>

namespace origin_mod {
class OriginMod;
}

namespace origin_mod::commands::config {

// 設定管理コマンドを実行
void executeConfig(OriginMod& mod, const std::vector<std::string>& args);

} // namespace origin_mod::commands::config