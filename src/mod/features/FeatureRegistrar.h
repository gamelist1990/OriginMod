#pragma once

#include <functional>
#include <vector>

namespace origin_mod::features {

class FeatureRegistry;

class FeatureRegistrar {
public:
    using RegisterFn = std::function<void(FeatureRegistry&)>;

    static FeatureRegistrar& instance();
    [[nodiscard]] bool add(RegisterFn fn);
    void applyAll(FeatureRegistry& registry) const;

private:
    FeatureRegistrar() = default;
    std::vector<RegisterFn> mFns;
};

} // namespace origin_mod::features

#ifndef ORIGINMOD_CONCAT_INNER
#define ORIGINMOD_CONCAT_INNER(a, b) a##b
#endif
#ifndef ORIGINMOD_CONCAT
#define ORIGINMOD_CONCAT(a, b) ORIGINMOD_CONCAT_INNER(a, b)
#endif

// Usage: ORIGINMOD_REGISTER_FEATURE(registerFn);
#define ORIGINMOD_REGISTER_FEATURE(RegisterFn)                                                                    \
    namespace {                                                                                                \
    [[maybe_unused]] const bool ORIGINMOD_CONCAT(kOriginModFeatureRegistered_, __COUNTER__) =                          \
        ::origin_mod::features::FeatureRegistrar::instance().add(RegisterFn);                                      \
    } // namespace
