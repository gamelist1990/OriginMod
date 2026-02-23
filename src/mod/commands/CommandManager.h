#pragma once

#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

namespace origin_mod {
class OriginMod;
}

namespace origin_mod::commands {

// シンプルなコマンドハンドラーの型定義
using CommandHandler = std::function<void(OriginMod& mod, const std::vector<std::string>& args)>;

struct Command {
    std::string name;
    std::string description;
    CommandHandler handler;
    int minArgs = 0;
    int maxArgs = 100;
};

class CommandManager {
public:
    static CommandManager& getInstance();

    // コマンドを登録
    void registerCommand(const Command& command);

    // コマンドを実行
    bool executeCommand(OriginMod& mod, const std::string& name, const std::vector<std::string>& args);

    // 登録されたコマンドの一覧を取得
    std::vector<Command> getCommands() const;

    // 初期化（ビルトインコマンドを登録）
    void initialize();

private:
    CommandManager() = default;

    std::unordered_map<std::string, Command> commands_;
    bool initialized_ = false;
};

} // namespace origin_mod::commands