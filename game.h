#pragma once
#include "types.h"

namespace Game {
    // Returns: index of winning player (0-3), or -1 for draw
    // Updates scores internally
    int run(GameMode mode, Slot slots[4]);
}
