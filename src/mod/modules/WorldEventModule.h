#pragma once

#include "ll/api/event/ListenerBase.h"

#include "mod/modules/IModule.h"

namespace origin_mod {
class OriginMod;
}

namespace origin_mod::modules {

class WorldEventModule final : public IModule {
public:
    [[nodiscard]] std::string_view id() const noexcept override { return "world-events"; }

    [[nodiscard]] bool load(OriginMod& mod) override;
    [[nodiscard]] bool enable(OriginMod& mod) override;
    [[nodiscard]] bool disable(OriginMod& mod) override;

private:
    ll::event::ListenerPtr mTickListener;
    uint64_t               mTickCounter{0};
};

} // namespace origin_mod::modules
