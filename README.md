# Tron

Terminal Tron C++ / ncurses.

## Setup

```
sudo pacman -S ncurses   # arch linux users
make
./tron
```

## Quick Start

```
./tron          # open the menu
./tron auto | ./tron a    # jump straight into autotron (screensaver mode)
```

## Modes

- **1v1** — 1v1 with a friend or an AI
- **FFA** — 4 players, up to 2 human players
- **2v2** — teams of 2, teammates pass through each other's trails (can play solo with an AI or with a friend against AIs)
- **Endless** — 1 human vs 7 AI. AI respawn when killed, you don't. Survive as long as you can.
- **AutoTron** — Automated Tron in your terminal for visual pleasure

## Controls

Up to 2 human players. Pick a scheme in the lobby:

| Scheme | Up | Down | Left | Right |
|--------|----|------|------|-------|
| WASD   | W  | S    | A    | D     |
| IJKL   | I  | K    | J    | L     |
| Arrows | ↑  | ↓    | ←    | →     |
| Numpad | 8  | 5    | 4    | 6     |

In-game: **Q** quit, **R** restart.

## Colors

8 player colors: Cyan, Magenta, Green, Yellow, Red, Blue, White, Orange.

Orange works best on terminals with 256-color support (most modern terminals).

## AI

Three difficulty levels per CPU slot: Easy, Medium, Hard.

## Config

Settings and scores save to `~/.config/tron/`. Delete that folder to reset.

## Files

```
main.cpp     entry point + CLI arg handling
menu.cpp/h   menus, lobby, scores, settings
game.cpp/h   game loop, grid, ai, rendering, respawn logic
config.cpp/h persistence
types.h      shared types
Makefile     build
```
