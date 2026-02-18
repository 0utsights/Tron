#pragma once
#include "types.h"

namespace Config {
    // paths: ~/.config/tron/settings, ~/.config/tron/scores
    void init();  // ensure dirs exist

    // ── Settings ───────────────────────────────────────
    struct Settings {
        GameMode last_mode    = MODE_1V1;
        int      tick_ms      = 55;
        Slot     slots[4];    // last-used lobby config
    };
    Settings& get();
    void save();
    void load();

    // ── Scores ─────────────────────────────────────────
    ScoreData& scores();
    void save_scores();
    void load_scores();
}
