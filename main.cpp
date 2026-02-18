#include "menu.h"
#include "game.h"
#include "config.h"
#include <clocale>
#include <ncurses.h>

int main() {
    setlocale(LC_ALL, "");
    initscr(); cbreak(); noecho();
    curs_set(0);
    keypad(stdscr, TRUE);
    Menu::init_colors();
    Config::init();

    GameMode mode;
    Slot slots[4];
    while (Menu::run(mode, slots))
        Game::run(mode, slots);

    endwin();
    return 0;
}
