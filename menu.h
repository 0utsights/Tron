#pragma once
#include "types.h"

namespace Menu {
    void init_colors();
    bool run(GameMode &mode, Slot slots[4]);
    void show_scores();
    void show_settings();
}
