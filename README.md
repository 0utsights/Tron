# tron

Terminal Tron Light Cycles. C++ / ncurses.

## Setup

Needs `g++` and `ncurses` (both likely already on your system).

```
sudo pacman -S ncurses   # if not installed
make
./tron
```

Optional global install:

```
sudo make install   # copies to /usr/local/bin
```

## Modes

- **1v1** — 1-2 humans, rest AI
- **FFA** — 4 players, up to 2 human, last standing wins
- **2v2** — teams of 2, teammates can cross each other's trails

## Controls

Up to 2 human players. Pick a scheme per player in the lobby:

| Scheme  | Up | Down | Left | Right |
|---------|----|------|------|-------|
| WASD    | W  | S    | A    | D     |
| IJKL    | I  | K    | J    | L     |
| Arrows  | ↑  | ↓    | ←    | →     |
| Numpad  | 8  | 5    | 4    | 6     |

In-game: **Q** quit, **R** restart.

## AI

Three levels: Easy, Medium, Hard. Set per CPU slot in the lobby. Hard AI checks perpendicular space when choosing a direction.

## Config

Settings and high scores save to `~/.config/tron/`. Delete that folder to reset everything.

## Files

```
main.cpp     entry point
menu.cpp/h   menus, lobby, scores, settings
game.cpp/h   game loop, grid, ai, rendering
config.cpp/h persistence
types.h      shared types
Makefile     build
```
