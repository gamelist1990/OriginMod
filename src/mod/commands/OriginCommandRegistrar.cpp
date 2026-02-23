#include "mod/commands/OriginCommandRegistrar.h"

#include "mod/commands/OriginCommandRegistry.h"

namespace origin_mod::commands {

OriginCommandRegistrar& OriginCommandRegistrar::instance() {
    static OriginCommandRegistrar g;
    return g;
}

bool OriginCommandRegistrar::add(RegisterFn fn) {
    if (!fn) {
        return false;
    }
    mFns.emplace_back(std::move(fn));
    return true;
}

void OriginCommandRegistrar::applyAll(OriginCommandRegistry& registry) const {
    for (auto const& fn : mFns) {
        fn(registry);
    }
}

} // namespace origin_mod::commands
