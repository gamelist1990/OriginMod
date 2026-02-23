#include "mod/commands/CommandManager.h"
#include "mod/commands/help/HelpCommand.h"
#include "mod/commands/toggle/ToggleCommand.h"
#include "mod/commands/players/PlayersCommand.h"
#include "mod/commands/config/ConfigCommand.h"
#include "mod/OriginMod.h"
#include "mod/api/Player.h"
#include <algorithm>
#include <fmt/format.h>

namespace origin_mod::commands {

CommandManager& CommandManager::getInstance() {
    static CommandManager instance;
    return instance;
}

void CommandManager::registerCommand(const Command& command) {
    std::string lowerName = command.name;
    std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::tolower);
    commands_[lowerName] = command;
}

bool CommandManager::executeCommand(OriginMod& mod, const std::string& name, const std::vector<std::string>& args) {
    std::string lowerName = name;
    std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::tolower);

    auto it = commands_.find(lowerName);
    if (it == commands_.end()) {
        return false;
    }

    const Command& cmd = it->second;

    // 引数の数をチェック
    if ((int)args.size() < cmd.minArgs || (int)args.size() > cmd.maxArgs) {
        origin_mod::api::Player{mod}.localSendMessage("§c[!] 引数の数が正しくありません。");
        return true; // コマンドは見つかったが、引数エラー
    }

    try {
        cmd.handler(mod, args);
        return true;
    } catch (const std::exception& e) {
        mod.getSelf().getLogger().error("Command execution error: {}", e.what());
        origin_mod::api::Player{mod}.localSendMessage("§c[!] コマンドの実行中にエラーが発生しました。");
        return true;
    }
}

std::vector<Command> CommandManager::getCommands() const {
    std::vector<Command> result;
    result.reserve(commands_.size());
    for (const auto& [name, cmd] : commands_) {
        result.push_back(cmd);
    }
    return result;
}

void CommandManager::initialize() {
    if (initialized_) {
        return;
    }

    // ヘルプコマンドを登録
    Command helpCmd;
    helpCmd.name = "help";
    helpCmd.description = "ヘルプを表示します";
    helpCmd.minArgs = 0;
    helpCmd.maxArgs = 0;
    helpCmd.handler = help::executeHelp;
    registerCommand(helpCmd);

    // トグルコマンドを登録
    Command toggleCmd;
    toggleCmd.name = "toggle";
    toggleCmd.description = "Feature の有効/無効を切り替えます";
    toggleCmd.minArgs = 0;
    toggleCmd.maxArgs = 3;
    toggleCmd.handler = toggle::executeToggle;
    registerCommand(toggleCmd);

    // プレイヤーコマンドを登録
    Command playersCmd;
    playersCmd.name = "players";
    playersCmd.description = "プレイヤー・エンティティ情報を表示します";
    playersCmd.minArgs = 0;
    playersCmd.maxArgs = 3;  // players location, players entities, players debug に対応
    playersCmd.handler = players::executePlayers;
    registerCommand(playersCmd);

    // 設定コマンドを登録
    Command configCmd;
    configCmd.name = "config";
    configCmd.description = "設定ファイルの管理とリロード";
    configCmd.minArgs = 0;
    configCmd.maxArgs = 3;
    configCmd.handler = config::executeConfig;
    registerCommand(configCmd);

    initialized_ = true;
}

} // namespace origin_mod::commands