#include "menu.h"
#include "config.h"
#include <cstring>

void Menu::init_colors() {
    start_color();
    use_default_colors();
    // trail colors
    init_pair(CP_TRAIL(PC_CYAN),    COLOR_CYAN,    -1);
    init_pair(CP_TRAIL(PC_MAGENTA), COLOR_MAGENTA, -1);
    init_pair(CP_TRAIL(PC_GREEN),   COLOR_GREEN,   -1);
    init_pair(CP_TRAIL(PC_YELLOW),  COLOR_YELLOW,  -1);
    // head colors (inverted)
    init_pair(CP_HEAD(PC_CYAN),    COLOR_WHITE, COLOR_CYAN);
    init_pair(CP_HEAD(PC_MAGENTA), COLOR_WHITE, COLOR_MAGENTA);
    init_pair(CP_HEAD(PC_GREEN),   COLOR_WHITE, COLOR_GREEN);
    init_pair(CP_HEAD(PC_YELLOW),  COLOR_WHITE, COLOR_YELLOW);
    // UI
    init_pair(CP_WALL,  COLOR_WHITE,  -1);
    init_pair(CP_HUD,   COLOR_YELLOW, -1);
    init_pair(CP_TITLE, COLOR_CYAN,   -1);
    init_pair(CP_SEL,   COLOR_BLACK,  COLOR_CYAN);
    init_pair(CP_DIM,   COLOR_WHITE,  -1);
}

// ── Helpers ────────────────────────────────────────────────

static void center(int y, const char* s, int pair=CP_HUD, bool bold=false) {
    int x = (COLS - (int)strlen(s)) / 2;
    if (bold) attron(A_BOLD);
    attron(COLOR_PAIR(pair));
    mvaddstr(y, x, s);
    attroff(COLOR_PAIR(pair));
    if (bold) attroff(A_BOLD);
}

static void draw_box(int y, int x, int h, int w, int pair=CP_WALL) {
    attron(COLOR_PAIR(pair) | A_DIM);
    mvaddstr(y,     x,     "╭"); mvaddstr(y,     x+w-1, "╮");
    mvaddstr(y+h-1, x,     "╰"); mvaddstr(y+h-1, x+w-1, "╯");
    for (int i=x+1; i<x+w-1; i++) { mvaddstr(y,i,"─"); mvaddstr(y+h-1,i,"─"); }
    for (int i=y+1; i<y+h-1; i++) { mvaddstr(i,x,"│"); mvaddstr(i,x+w-1,"│"); }
    attroff(COLOR_PAIR(pair) | A_DIM);
}

static const char* title_art[] = {
    " ▀▀█▀▀ █▀▀█ █▀▀█ █▀▀▄ ",
    "   █   █▄▄▀ █  █ █  █ ",
    "   █   █  █ █▄▄█ █  █ ",
    nullptr
};

static void draw_title() {
    int sy = 2;
    for (int i=0; title_art[i]; i++)
        center(sy+i, title_art[i], CP_TITLE, true);
}

// ── Generic vertical menu selector ────────────────────────

static int vmenu(int y, const std::vector<const char*>& items, int sel=0) {
    keypad(stdscr, TRUE);
    timeout(-1);
    while (true) {
        for (int i=0; i<(int)items.size(); i++) {
            if (i==sel) {
                center(y+i, items[i], CP_SEL, true);
            } else {
                center(y+i, items[i], CP_HUD, false);
            }
        }
        refresh();
        int ch = getch();
        if (ch=='q'||ch=='Q') return -1;
        if (ch==KEY_UP   ||ch=='w'||ch=='W') sel = (sel-1+(int)items.size())%(int)items.size();
        if (ch==KEY_DOWN ||ch=='s'||ch=='S') sel = (sel+1)%(int)items.size();
        if (ch=='\n'||ch==' ') return sel;
    }
}

// ── Title screen ──────────────────────────────────────────

enum TitleOpt { T_QUICK, T_CUSTOM, T_SCORES, T_SETTINGS, T_QUIT };

static int title_screen() {
    erase();
    draw_title();
    center(7, "── Light Cycles ──", CP_DIM);
    return vmenu(10, {"  Quick Play  ","  Custom Game  ","  High Scores  ","  Settings  ","  Quit  "});
}

// ── Mode select ───────────────────────────────────────────

static int mode_select() {
    erase();
    draw_title();
    center(7, "── Select Mode ──", CP_DIM);
    return vmenu(10, {"  1v1  ","  FFA (4 players)  ","  2v2 Teams  "});
}

// ── Lobby ─────────────────────────────────────────────────
// Two-column layout: left = humans, right = AI
// Navigate with arrows, enter to toggle/cycle options

static void draw_slot_card(int y, int x, int w, int idx, Slot& sl, int cursor, int field) {
    bool is_sel = (cursor == idx);
    int pair = is_sel ? CP_SEL : CP_DIM;

    // card border in player color
    draw_box(y, x, 8, w, CP_TRAIL(sl.color));

    // header
    char buf[64];
    snprintf(buf, sizeof(buf), " Player %d ", idx+1);
    attron(COLOR_PAIR(CP_TRAIL(sl.color)) | A_BOLD);
    mvaddstr(y, x+2, buf);
    attroff(COLOR_PAIR(CP_TRAIL(sl.color)) | A_BOLD);

    // type
    snprintf(buf, sizeof(buf), "Type: %-8s", sl.human ? "Human" : "CPU");
    int row = y+2;
    attron(COLOR_PAIR(is_sel && field==0 ? CP_SEL : CP_HUD));
    mvaddstr(row, x+2, buf);
    attroff(COLOR_PAIR(is_sel && field==0 ? CP_SEL : CP_HUD));

    // color
    snprintf(buf, sizeof(buf), "Color: %-8s", pcolor_name[sl.color]);
    row++;
    attron(COLOR_PAIR(is_sel && field==1 ? CP_SEL : CP_HUD));
    mvaddstr(row, x+2, buf);
    attroff(COLOR_PAIR(is_sel && field==1 ? CP_SEL : CP_HUD));

    if (sl.human) {
        // controls
        snprintf(buf, sizeof(buf), "Keys: %-8s", keysets()[sl.keyset].name);
        row++;
        attron(COLOR_PAIR(is_sel && field==2 ? CP_SEL : CP_HUD));
        mvaddstr(row, x+2, buf);
        attroff(COLOR_PAIR(is_sel && field==2 ? CP_SEL : CP_HUD));
    } else {
        // difficulty
        snprintf(buf, sizeof(buf), "AI: %-8s", diff_name[sl.diff]);
        row++;
        attron(COLOR_PAIR(is_sel && field==2 ? CP_SEL : CP_HUD));
        mvaddstr(row, x+2, buf);
        attroff(COLOR_PAIR(is_sel && field==2 ? CP_SEL : CP_HUD));
    }
}

static bool lobby(GameMode mode, Slot slots[4]) {
    int n = mode_players[mode];
    // assign teams for 2v2
    if (mode == MODE_2V2) {
        slots[0].team = 0; slots[1].team = 0;
        slots[2].team = 1; slots[3].team = 1;
    }

    int cursor = 0;  // which slot
    int field  = 0;  // which field within slot (0=type, 1=color, 2=keys/diff)
    int max_fields = 3;
    timeout(-1);

    while (true) {
        erase();
        center(1, "── Player Setup ──", CP_TITLE, true);
        center(2, mode_name[mode], CP_DIM);

        if (mode == MODE_2V2) {
            center(3, "Team 1               Team 2", CP_DIM);
        }

        // draw slot cards
        int card_w = 22;
        int gap = 2;
        int total_w = n * card_w + (n-1) * gap;
        int start_x = (COLS - total_w) / 2;

        for (int i = 0; i < n; i++) {
            int cx = start_x + i * (card_w + gap);
            draw_slot_card(5, cx, card_w, i, slots[i], cursor, field);
        }

        // help text
        center(14, "[←→] Switch player  [↑↓] Switch field  [Enter] Cycle value", CP_DIM);
        center(15, "[Space] Start game  [Q] Back", CP_DIM);

        if (mode == MODE_2V2) {
            center(16, "Team 1: P1+P2  |  Team 2: P3+P4", CP_HUD);
        }

        refresh();

        int ch = getch();
        if (ch == 'q' || ch == 'Q') return false;
        if (ch == ' ') {
            // validate: at least 1 human
            bool has_human = false;
            for (int i=0;i<n;i++) if (slots[i].human) has_human=true;
            if (!has_human) {
                center(18, "Need at least 1 human player!", CP_HUD, true);
                refresh();
                napms(1000);
                continue;
            }
            // validate: max 2 humans
            int humans = 0;
            for (int i=0;i<n;i++) if (slots[i].human) humans++;
            if (humans > 2) {
                center(18, "Max 2 human players!", CP_HUD, true);
                refresh();
                napms(1000);
                continue;
            }
            // validate: no duplicate colors
            bool dup = false;
            for (int i=0;i<n&&!dup;i++)
                for (int j=i+1;j<n;j++)
                    if (slots[i].color==slots[j].color) dup=true;
            if (dup) {
                center(18, "Each player needs a unique color!", CP_HUD, true);
                refresh();
                napms(1000);
                continue;
            }
            // validate: no duplicate keysets for humans
            bool dup_keys = false;
            for (int i=0;i<n&&!dup_keys;i++)
                for (int j=i+1;j<n;j++)
                    if (slots[i].human && slots[j].human && slots[i].keyset==slots[j].keyset)
                        dup_keys=true;
            if (dup_keys) {
                center(18, "Human players need different controls!", CP_HUD, true);
                refresh();
                napms(1000);
                continue;
            }
            return true;
        }

        if (ch==KEY_LEFT  || ch=='a' || ch=='A') cursor = (cursor-1+n)%n;
        if (ch==KEY_RIGHT || ch=='d' || ch=='D') cursor = (cursor+1)%n;
        if (ch==KEY_UP    || ch=='w' || ch=='W') field  = (field-1+max_fields)%max_fields;
        if (ch==KEY_DOWN  || ch=='s' || ch=='S') field  = (field+1)%max_fields;

        if (ch=='\n') {
            Slot& s = slots[cursor];
            switch (field) {
                case 0: s.human = !s.human; break;
                case 1: s.color = (PColor)(((int)s.color+1) % PC_COUNT); break;
                case 2:
                    if (s.human) s.keyset = (s.keyset+1) % (int)keysets().size();
                    else s.diff = (AIDiff)(((int)s.diff+1) % 3);
                    break;
            }
        }
    }
}

// ── High scores screen ────────────────────────────────────

void Menu::show_scores() {
    auto& s = Config::scores();
    int tab = 0;
    timeout(-1);
    while (true) {
        erase();
        center(1, "── High Scores ──", CP_TITLE, true);

        // tab bar
        const char* tabs[] = {"[1] Overview", "[2] Streaks", "[3] Time"};
        for (int i = 0; i < 3; i++) {
            int pair = (i==tab) ? CP_SEL : CP_DIM;
            int x = COLS/2 - 25 + i*18;
            attron(COLOR_PAIR(pair) | (i==tab ? A_BOLD : 0));
            mvaddstr(3, x, tabs[i]);
            attroff(COLOR_PAIR(pair) | (i==tab ? A_BOLD : 0));
        }

        char buf[64];
        int y = 6;
        attron(COLOR_PAIR(CP_HUD));
        switch (tab) {
            case 0:
                snprintf(buf, 64, "Total Wins:     %d", s.total_wins);    mvaddstr(y++, COLS/2-15, buf);
                snprintf(buf, 64, "Rounds Played:  %d", s.rounds_played); mvaddstr(y++, COLS/2-15, buf);
                if (s.rounds_played > 0) {
                    snprintf(buf, 64, "Win Rate:       %.0f%%",
                             100.0*s.total_wins/s.rounds_played);
                    mvaddstr(y++, COLS/2-15, buf);
                }
                break;
            case 1:
                snprintf(buf, 64, "Best Win Streak:    %d", s.best_streak);    mvaddstr(y++, COLS/2-15, buf);
                snprintf(buf, 64, "Current Streak:     %d", s.current_streak); mvaddstr(y++, COLS/2-15, buf);
                break;
            case 2:
                snprintf(buf, 64, "Best Survival Time: %.1fs", s.best_time); mvaddstr(y++, COLS/2-15, buf);
                break;
        }
        attroff(COLOR_PAIR(CP_HUD));

        center(LINES-3, "[1/2/3] Switch tab  [Q] Back", CP_DIM);
        refresh();

        int ch = getch();
        if (ch=='q'||ch=='Q'||ch==27) return;
        if (ch=='1') tab=0;
        if (ch=='2') tab=1;
        if (ch=='3') tab=2;
        if (ch==KEY_LEFT)  tab=(tab+2)%3;
        if (ch==KEY_RIGHT) tab=(tab+1)%3;
    }
}

// ── Settings screen ───────────────────────────────────────

void Menu::show_settings() {
    auto& cfg = Config::get();
    int sel = 0;
    timeout(-1);
    while (true) {
        erase();
        center(1, "── Settings ──", CP_TITLE, true);

        char buf[64];
        int y = 5;

        snprintf(buf, 64, "Game Speed (tick ms): %d", cfg.tick_ms);
        attron(COLOR_PAIR(sel==0 ? CP_SEL : CP_HUD));
        mvaddstr(y++, COLS/2-18, buf);
        attroff(COLOR_PAIR(sel==0 ? CP_SEL : CP_HUD));

        y++;
        center(y, "[↑↓] Select  [←→] Adjust  [Q] Save & Back", CP_DIM);
        refresh();

        int ch = getch();
        if (ch=='q'||ch=='Q'||ch==27) { Config::save(); return; }
        if (ch==KEY_UP)   sel = 0;
        if (ch==KEY_DOWN) sel = 0;
        if (sel == 0) {
            if (ch==KEY_RIGHT && cfg.tick_ms < 150) cfg.tick_ms += 5;
            if (ch==KEY_LEFT  && cfg.tick_ms > 20)  cfg.tick_ms -= 5;
        }
    }
}

// ── Main menu orchestrator ────────────────────────────────

bool Menu::run(GameMode &mode, Slot slots[4]) {
    while (true) {
        int opt = title_screen();
        if (opt < 0 || opt == T_QUIT) return false;

        if (opt == T_SCORES)   { show_scores(); continue; }
        if (opt == T_SETTINGS) { show_settings(); continue; }

        if (opt == T_QUICK) {
            // use last settings
            auto& cfg = Config::get();
            mode = cfg.last_mode;
            for (int i=0;i<4;i++) slots[i] = cfg.slots[i];
            return true;
        }

        if (opt == T_CUSTOM) {
            int m = mode_select();
            if (m < 0) continue;
            mode = (GameMode)m;

            // init slots with sensible defaults for this mode
            auto& cfg = Config::get();
            int n = mode_players[mode];
            PColor defaults[] = {PC_CYAN, PC_MAGENTA, PC_GREEN, PC_YELLOW};
            for (int i=0;i<4;i++) {
                slots[i] = cfg.slots[i];
                slots[i].color = defaults[i];
                if (i >= n) slots[i].human = false;
            }
            slots[0].human = true;
            if (n == 2) slots[1].human = false;

            if (!lobby(mode, slots)) continue;

            // save as last used
            cfg.last_mode = mode;
            for (int i=0;i<4;i++) cfg.slots[i] = slots[i];
            Config::save();
            return true;
        }
    }
}
