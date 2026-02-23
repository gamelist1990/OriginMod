#pragma once

#include <vector>
#include <functional>

namespace origin_mod::features {

class FeatureRegistry; // forward declaration

class FeatureRegistrar {
public:
    using Callback = void (*)(FeatureRegistry&);

    static FeatureRegistrar& instance() {
        static FeatureRegistrar inst;
        return inst;
    }

    void add(Callback cb) {
        mCallbacks.push_back(cb);
    }

    void applyAll(FeatureRegistry& reg) {
        for (auto cb : mCallbacks) {
            cb(reg);
        }
    }

private:
    FeatureRegistrar() = default;
    std::vector<Callback> mCallbacks;
};

} // namespace origin_mod::features

// Macro for registering feature initializer functions.  The argument should be a
// function with signature `void(FeatureRegistry&)` (see the bottom of
// `AimJitterFeature.cpp` for an example).  The generated static object will
// run at startup and call `FeatureRegistrar::instance().add(fn)`.
#define ORIGINMOD_REGISTER_FEATURE(fn)                                    \
    namespace {                                                           \
        struct _originmod_feature_registrar_##fn {                         \
            _originmod_feature_registrar_##fn() {                          \
                origin_mod::features::FeatureRegistrar::instance().add(fn); \
            }                                                              \
        };                                                                 \
        static _originmod_feature_registrar_##fn                           \
            _originmod_feature_registrar_instance_##fn; \
                    \
    }
