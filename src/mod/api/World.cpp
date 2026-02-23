#include "mod/api/World.h"

namespace origin_mod::api {

World& World::instance() {
    static World g;
    return g;
}

} // namespace origin_mod::api
