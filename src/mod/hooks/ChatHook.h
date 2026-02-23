#pragma once

namespace origin_mod {
class OriginMod;
}

namespace origin_mod::hooks {

// チャットフックを初期化
void initializeChatHook(OriginMod& mod);

// チャットフックを終了
void shutdownChatHook();

} // namespace origin_mod::hooks