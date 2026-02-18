#pragma once
#include "types.h"

namespace Config {
    void init();

    struct Settings {
        GameMode last_mode = MODE_1V1;
        int      tick_ms   = 55;
        Slot     slots[4];
    };

    Settings& get();
    void save();
    void load();

    ScoreData& scores();
    void save_scores();
    void load_scores();
}
