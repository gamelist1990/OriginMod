#pragma once

namespace origin_mod::commands {
class OriginCommandRegistry;
}

namespace origin_mod::commands::builtin {
// Register built-in "help" subcommand into registry.
bool registerHelp(origin_mod::commands::OriginCommandRegistry& registry);
} // namespace origin_mod::commands::builtin
