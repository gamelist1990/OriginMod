#pragma once

#include <functional>
#include <vector>

namespace origin_mod::commands {

class OriginCommandRegistry;

class OriginCommandRegistrar {
public:
    using RegisterFn = std::function<void(OriginCommandRegistry&)>;

    static OriginCommandRegistrar& instance();
    [[nodiscard]] bool add(RegisterFn fn);
    void applyAll(OriginCommandRegistry& registry) const;

private:
    OriginCommandRegistrar() = default;
    std::vector<RegisterFn> mFns;
};

} // namespace origin_mod::commands

#ifndef ORIGINMOD_CONCAT_INNER
#define ORIGINMOD_CONCAT_INNER(a, b) a##b
#endif
#ifndef ORIGINMOD_CONCAT
#define ORIGINMOD_CONCAT(a, b) ORIGINMOD_CONCAT_INNER(a, b)
#endif

// Usage in a command .cpp:
//   static void registerX(origin_mod::commands::OriginCommandRegistry&);
//   ORIGINMOD_REGISTER_ORIGIN_COMMAND(registerX);
#define ORIGINMOD_REGISTER_ORIGIN_COMMAND(RegisterFn)                                                         \
    namespace {                                                                                            \
    [[maybe_unused]] const bool ORIGINMOD_CONCAT(kOriginModOriginCmdRegistered_, __COUNTER__) =                    \
        ::origin_mod::commands::OriginCommandRegistrar::instance().add(RegisterFn);                           \
    } // namespace
