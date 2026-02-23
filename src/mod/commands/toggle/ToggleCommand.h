#pragma once

namespace origin_mod::commands {
class OriginCommandRegistry;
}

namespace origin_mod::commands::builtin {
// Register built-in "toggle" subcommand into registry.
bool registerToggle(origin_mod::commands::OriginCommandRegistry& registry);
} // namespace origin_mod::commands::builtin
