#pragma once

#include <string_view>

namespace origin_mod::util {

// Append a line to debug.txt when the mod is built in debug mode.
// In debug builds the file is placed in the mod's config directory (same
// location as features.json); this avoids relying on the working directory.
// In release builds this function does nothing.
void debugLog(std::string_view msg);

} // namespace origin_mod::util
