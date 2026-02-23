#pragma once

namespace origin_mod {
class OriginMod;

namespace modules {
class NativeCommandModule;
}

namespace hooks {

// Installs a hook on outgoing Text packets (chat). Used for chat-based command system.
void installOutgoingChatHook(modules::NativeCommandModule* module, OriginMod* mod);

// Clears pointers (hook remains installed but becomes no-op).
void uninstallOutgoingChatHook();

} // namespace hooks
} // namespace origin_mod
