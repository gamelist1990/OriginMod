#include "mod/features/FeatureRegistrar.h"

#include "mod/features/FeatureRegistry.h"

namespace origin_mod::features {

FeatureRegistrar& FeatureRegistrar::instance() {
    static FeatureRegistrar g;
    return g;
}

bool FeatureRegistrar::add(RegisterFn fn) {
    if (!fn) {
        return false;
    }
    mFns.emplace_back(std::move(fn));
    return true;
}

void FeatureRegistrar::applyAll(FeatureRegistry& registry) const {
    for (auto const& fn : mFns) {
        fn(registry);
    }
}

} // namespace origin_mod::features
