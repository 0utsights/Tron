#pragma once
#include <ncurses.h>
#include <string>
#include <vector>

enum Dir { D_UP=0, D_DOWN, D_LEFT, D_RIGHT, D_NONE };
inline Dir dir_opposite(Dir d) {
    constexpr Dir opp[] = {D_DOWN, D_UP, D_RIGHT, D_LEFT, D_NONE};
    return opp[d];
}
inline int dir_dx(Dir d) { return d==D_LEFT?-1:d==D_RIGHT?1:0; }
inline int dir_dy(Dir d) { return d==D_UP?-1:d==D_DOWN?1:0; }

enum Cell : uint8_t {
    C_EMPTY=0, C_WALL,
    C_P1, C_P2, C_P3, C_P4, C_P5, C_P6, C_P7, C_P8
};

// 8 player colors
enum PColor {
    PC_CYAN=0, PC_MAGENTA, PC_GREEN, PC_YELLOW,
    PC_RED, PC_BLUE, PC_WHITE, PC_ORANGE,
    PC_COUNT
};
constexpr const char* pcolor_name[] = {
    "Cyan","Magenta","Green","Yellow","Red","Blue","White","Orange"
};

// ncurses pairs: trail 1-8, head 9-16, ui 17+
constexpr int CP_TRAIL(int c) { return 1+c; }
constexpr int CP_HEAD(int c)  { return 9+c; }
constexpr int CP_WALL  = 17;
constexpr int CP_HUD   = 18;
constexpr int CP_TITLE = 19;
constexpr int CP_SEL   = 20;
constexpr int CP_DIM   = 21;
constexpr int CP_FLASH = 22;

struct KeySet {
    int up, down, left, right;
    const char* name;
};

inline const std::vector<KeySet>& keysets() {
    static const std::vector<KeySet> k = {
        {'w','s','a','d',                          "WASD"},
        {'i','k','j','l',                          "IJKL"},
        {KEY_UP, KEY_DOWN, KEY_LEFT, KEY_RIGHT,    "Arrows"},
        {'8','5','4','6',                          "Numpad"},
    };
    return k;
}

enum AIDiff { AI_EASY=0, AI_MED, AI_HARD };
constexpr const char* diff_name[] = {"Easy","Medium","Hard"};

enum GameMode { MODE_1V1=0, MODE_FFA, MODE_2V2, MODE_ENDLESS, MODE_AUTO };
constexpr const char* mode_name[] = {"1v1","FFA (4p)","2v2 Teams","Endless","AutoTron"};
inline int mode_players(GameMode m) {
    switch(m) {
        case MODE_1V1: return 2;
        case MODE_FFA: return 4;
        case MODE_2V2: return 4;
        case MODE_ENDLESS: return 8;
        case MODE_AUTO: return 6;
    }
    return 2;
}

struct Slot {
    bool human    = false;
    PColor color  = PC_CYAN;
    int keyset    = 0;
    AIDiff diff   = AI_MED;
    int team      = 0;
};

namespace Trail {
    constexpr const char* V  = "│";
    constexpr const char* H  = "─";
    constexpr const char* UL = "╭";
    constexpr const char* UR = "╮";
    constexpr const char* DL = "╰";
    constexpr const char* DR = "╯";
    constexpr const char* HD = "●";

    inline const char* corner(Dir from, Dir to) {
        if (from==to || from==D_NONE) return (to==D_UP||to==D_DOWN)?V:H;
        if ((from==D_UP   &&to==D_RIGHT)||(from==D_LEFT &&to==D_DOWN))  return UL;
        if ((from==D_UP   &&to==D_LEFT) ||(from==D_RIGHT&&to==D_DOWN))  return UR;
        if ((from==D_DOWN &&to==D_RIGHT)||(from==D_LEFT &&to==D_UP))    return DL;
        if ((from==D_DOWN &&to==D_LEFT) ||(from==D_RIGHT&&to==D_UP))    return DR;
        return (to==D_UP||to==D_DOWN)?V:H;
    }
}

struct ScoreData {
    int total_wins     = 0;
    int best_streak    = 0;
    int current_streak = 0;
    int rounds_played  = 0;
    double best_time   = 0.0;
    double best_endless = 0.0; // longest endless survival
};
