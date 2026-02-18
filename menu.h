#pragma once
#include "types.h"

namespace Menu {
    void init_colors();

    // Returns false if user wants to quit
    // Fills mode + slots for the game to use
    bool run(GameMode &mode, Slot slots[4]);

    void show_scores();
    void show_settings();
}
