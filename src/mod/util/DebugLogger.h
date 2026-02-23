#pragma once

#include <string_view>

namespace origin_mod::util {

// Append a line to debug.txt when the mod is built in debug mode.
// In debug builds the file is placed in the mod's config directory (same
// location as features.json); this avoids relying on the working directory.
// In release builds this function does nothing.

// Set whether debug logging is enabled at runtime.  By default it is `true`
// in debug builds and `false` in release builds, but callers can toggle it if
// needed (for example, to activate logging in a release build).
void setDebugEnabled(bool enabled);

// Query current enabled state.
bool isDebugEnabled();

// Append a line to debug.txt when the mod is built in debug mode.
// In debug builds the file is placed in the mod's config directory (same
// location as features.json); this avoids relying on the working directory.
// The call is a no-op when logging is disabled.
void debugLog(std::string_view msg);


} // namespace origin_mod::util
