#pragma once
#include "types.h"

namespace Game {
    // returns index of winner (0-3), or -1 for draw
    int run(GameMode mode, Slot slots[4]);
}
