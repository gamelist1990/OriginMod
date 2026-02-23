#pragma once

#include "ll/api/command/Optional.h"
#include "ll/api/command/SoftEnum.h"

namespace origin_mod::commands {

// SoftEnum tag for feature id auto-completion.
enum class FeatureNameTag {};
using FeatureName = ll::command::SoftEnum<FeatureNameTag>;

// SoftEnum tag for /origin subcommand auto-completion.
enum class SubcommandNameTag {};
using SubcommandName = ll::command::SoftEnum<SubcommandNameTag>;

// on/off values with autocomplete.
enum class ToggleState : int {
    on  = 0,
    off = 1,
};

} // namespace origin_mod::commands
