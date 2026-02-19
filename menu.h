#pragma once
#include "types.h"

namespace Menu {
    void init_colors();
    // returns false = quit. fills mode + slots.
    bool run(GameMode &mode, Slot slots[8]);
    void show_scores();
    void show_settings();
}
