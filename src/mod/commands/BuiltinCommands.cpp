#include "mod/commands/BuiltinCommands.h"

#include "mod/commands/help/HelpCommand.h"
#include "mod/commands/toggle/ToggleCommand.h"
#include "mod/commands/OriginCommandRegistry.h"

namespace origin_mod::commands {

void registerBuiltinCommands(OriginCommandRegistry& registry) {
    // Keep this list explicit so registration does not rely on global/static initialization.
    ::origin_mod::commands::builtin::registerHelp(registry);
    ::origin_mod::commands::builtin::registerToggle(registry);
}

} // namespace origin_mod::commands
