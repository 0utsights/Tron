#include "config.h"
#include <fstream>
#include <cstdlib>
#include <sys/stat.h>

static std::string config_dir() {
    const char* home = getenv("HOME");
    return std::string(home ? home : "/tmp") + "/.config/tron";
}

void Config::init() {
    std::string dir = config_dir();
    mkdir(dir.substr(0, dir.rfind('/')).c_str(), 0755);
    mkdir(dir.c_str(), 0755);
    load();
    load_scores();
}

// ── Settings ───────────────────────────────────────────────

static Config::Settings settings;

Config::Settings& Config::get() { return settings; }

void Config::save() {
    std::ofstream f(config_dir() + "/settings");
    if (!f) return;
    auto& s = settings;
    f << (int)s.last_mode << ' ' << s.tick_ms << '\n';
    for (int i = 0; i < 4; i++) {
        auto& sl = s.slots[i];
        f << sl.human << ' ' << (int)sl.color << ' ' << sl.keyset
          << ' ' << (int)sl.diff << ' ' << sl.team << '\n';
    }
}

void Config::load() {
    std::ifstream f(config_dir() + "/settings");
    if (!f) {
        // defaults
        settings.slots[0] = {true, PC_CYAN, 0, AI_MED, 0};
        settings.slots[1] = {false, PC_MAGENTA, 1, AI_MED, 1};
        settings.slots[2] = {false, PC_GREEN, 2, AI_MED, 0};
        settings.slots[3] = {false, PC_YELLOW, 3, AI_HARD, 1};
        return;
    }
    int m; f >> m >> settings.tick_ms;
    settings.last_mode = (GameMode)m;
    for (int i = 0; i < 4; i++) {
        int h, c, k, d, t;
        f >> h >> c >> k >> d >> t;
        settings.slots[i] = {(bool)h, (PColor)c, k, (AIDiff)d, t};
    }
}

// ── Scores ─────────────────────────────────────────────────

static ScoreData score_data;

ScoreData& Config::scores() { return score_data; }

void Config::save_scores() {
    std::ofstream f(config_dir() + "/scores");
    if (!f) return;
    auto& s = score_data;
    f << s.total_wins << ' ' << s.best_streak << ' ' << s.current_streak
      << ' ' << s.rounds_played << ' ' << s.best_time << '\n';
}

void Config::load_scores() {
    std::ifstream f(config_dir() + "/scores");
    if (!f) return;
    auto& s = score_data;
    f >> s.total_wins >> s.best_streak >> s.current_streak
      >> s.rounds_played >> s.best_time;
}
