#pragma once
#include "types.h"

namespace Game {
    // standard modes: returns winner index or -1
    int run(GameMode mode, Slot slots[8]);
}
