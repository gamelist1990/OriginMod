#pragma once

namespace origin_mod::commands {
class OriginCommandRegistry;

// Registers built-in chat subcommands that must always be available
// (even if static-initializer based registration is not executed).
void registerBuiltinCommands(OriginCommandRegistry& registry);
} // namespace origin_mod::commands
