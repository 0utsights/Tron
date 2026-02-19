#include "menu.h"
#include "game.h"
#include "config.h"
#include <clocale>
#include <cstring>
#include <ncurses.h>

int main(int argc, char* argv[]) {
    setlocale(LC_ALL, "");
    initscr(); cbreak(); noecho();
    curs_set(0);
    keypad(stdscr, TRUE);
    Menu::init_colors();
    Config::init();

    // ./tron auto  or  ./tron a  — jump straight into autotron
    if (argc > 1 && (strcmp(argv[1],"auto")==0 || strcmp(argv[1],"a")==0)) {
        GameMode mode = MODE_AUTO;
        Slot slots[8];
        PColor cols[] = {PC_CYAN,PC_MAGENTA,PC_GREEN,PC_YELLOW,PC_RED,PC_BLUE,PC_WHITE,PC_ORANGE};
        for (int i=0; i<6; i++)
            slots[i] = {false, cols[i], 0, AI_HARD, 1}; // team=1 = camera mode
        Game::run(mode, slots);
        endwin();
        return 0;
    }

    GameMode mode;
    Slot slots[8];
    while (Menu::run(mode, slots))
        Game::run(mode, slots);

    endwin();
    return 0;
}
