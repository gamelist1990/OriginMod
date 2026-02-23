#pragma once

#include <vector>
#include <string>

namespace origin_mod {
class OriginMod;
}

namespace origin_mod::commands::test {

// テスト用コマンドを実行
void executeTest(OriginMod& mod, const std::vector<std::string>& args);

} // namespace origin_mod::commands::test