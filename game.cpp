#include "game.h"
#include "config.h"
#include <cstring>
#include <cstdlib>
#include <ctime>
#include <unistd.h>
#include <vector>

static int GW, GH;
static Cell* grid;
static Dir*  prev_dir;

static inline int idx(int x, int y) { return y*GW+x; }

static inline bool blocked_for(int x, int y, int team, GameMode mode) {
    if (x<=0||x>=GW-1||y<=0||y>=GH-1) return true;
    Cell c = grid[idx(x,y)];
    if (c==C_EMPTY) return false;
    if (c==C_WALL) return true;
    if (mode==MODE_2V2) {
        int cell_team = ((int)c - (int)C_P1) / 2;
        if (cell_team == team) return false;
    }
    return true;
}

static void grid_init() {
    std::memset(grid, C_EMPTY, GW*GH*sizeof(Cell));
    for (int i=0; i<GW*GH; i++) prev_dir[i] = D_NONE;
    for (int x=0;x<GW;x++) { grid[idx(x,0)]=C_WALL; grid[idx(x,GH-1)]=C_WALL; }
    for (int y=0;y<GH;y++) { grid[idx(0,y)]=C_WALL; grid[idx(GW-1,y)]=C_WALL; }
}

static void draw_border() {
    attron(COLOR_PAIR(CP_WALL) | A_DIM);
    for (int x=0;x<GW;x++) { mvaddstr(0,x,"─"); mvaddstr(GH-1,x,"─"); }
    for (int y=0;y<GH;y++) { mvaddstr(y,0,"│"); mvaddstr(y,GW-1,"│"); }
    mvaddstr(0,0,"╭"); mvaddstr(0,GW-1,"╮");
    mvaddstr(GH-1,0,"╰"); mvaddstr(GH-1,GW-1,"╯");
    attroff(COLOR_PAIR(CP_WALL) | A_DIM);
}

// player states:
//   alive=true,  active=true  -> moving
//   alive=false, active=true  -> dead, trail flashing
//   alive=false, active=false -> trail erased, waiting to respawn
struct Player {
    int x, y;
    Dir dir;
    bool alive;
    bool active;
    Slot* slot;
    Cell cell;
    int index;
    int death_tick; // -1 = not dead
    int label_tick; // tick when label was set, -1 = no label
    std::vector<std::pair<int,int>> trail_cells;
};

static Player players[8];
static int num_players;

static void find_spawn(int &sx, int &sy, Dir &sd) {
    for (int attempts=0; attempts<500; attempts++) {
        sx = 4 + rand() % (GW-8);
        sy = 4 + rand() % (GH-8);
        if (grid[idx(sx,sy)] != C_EMPTY) continue;
        Dir dirs[] = {D_UP, D_DOWN, D_LEFT, D_RIGHT};
        for (int d=0; d<4; d++) {
            int nx = sx+dir_dx(dirs[d]), ny = sy+dir_dy(dirs[d]);
            if (nx>0 && nx<GW-1 && ny>0 && ny<GH-1 && grid[idx(nx,ny)]==C_EMPTY) {
                sd = dirs[d];
                return;
            }
        }
    }
    sx = GW/2; sy = GH/2; sd = D_RIGHT;
}

static void spawn_player(Player& p) {
    int sx, sy; Dir sd;
    find_spawn(sx, sy, sd);
    p.x = sx; p.y = sy; p.dir = sd;
    p.alive = true; p.active = true;
    p.death_tick = -1;
    p.label_tick = -1; // set by game loop after spawn
    p.trail_cells.clear();
    grid[idx(sx,sy)] = p.cell;
    prev_dir[idx(sx,sy)] = sd;
    p.trail_cells.push_back({sx,sy});
}

static void spawn_players_fixed(GameMode mode, Slot slots[8]) {
    num_players = mode_players(mode);
    struct { float fx, fy; Dir d; } pos[] = {
        {0.25f, 0.50f, D_RIGHT}, {0.75f, 0.50f, D_LEFT},
        {0.25f, 0.25f, D_RIGHT}, {0.75f, 0.25f, D_LEFT},
        {0.25f, 0.75f, D_RIGHT}, {0.75f, 0.75f, D_LEFT},
        {0.50f, 0.25f, D_DOWN},  {0.50f, 0.75f, D_UP},
    };
    for (int i=0; i<num_players; i++) {
        Player& p = players[i];
        p.slot = &slots[i]; p.cell = (Cell)(C_P1+i); p.index = i;
        p.alive = true; p.active = true;
        p.death_tick = -1;
        p.label_tick = -1;
        p.trail_cells.clear();
        p.x = (int)(pos[i].fx * GW);
        p.y = (int)(pos[i].fy * GH);
        if (p.x<=1) p.x=2;
        if (p.x>=GW-2) p.x=GW-3;
        if (p.y<=1) p.y=2;
        if (p.y>=GH-2) p.y=GH-3;
        p.dir = pos[i].d;
        grid[idx(p.x,p.y)] = p.cell;
        prev_dir[idx(p.x,p.y)] = p.dir;
        p.trail_cells.push_back({p.x,p.y});
    }
}

static void draw_trail_seg(Player& p, Dir old_dir) {
    int ox = p.x - dir_dx(p.dir);
    int oy = p.y - dir_dy(p.dir);
    if (ox>0 && ox<GW-1 && oy>0 && oy<GH-1) {
        attron(COLOR_PAIR(CP_TRAIL(p.slot->color)) | A_BOLD);
        mvaddstr(oy, ox, Trail::corner(old_dir, p.dir));
        attroff(COLOR_PAIR(CP_TRAIL(p.slot->color)) | A_BOLD);
    }
    attron(COLOR_PAIR(CP_HEAD(p.slot->color)) | A_BOLD);
    mvaddstr(p.y, p.x, Trail::HD);
    attroff(COLOR_PAIR(CP_HEAD(p.slot->color)) | A_BOLD);
}

static void draw_head(Player& p) {
    attron(COLOR_PAIR(CP_HEAD(p.slot->color)) | A_BOLD);
    mvaddstr(p.y, p.x, Trail::HD);
    attroff(COLOR_PAIR(CP_HEAD(p.slot->color)) | A_BOLD);
}

// should this player get a label?
static bool wants_label(Player& p, GameMode mode) {
    if (mode == MODE_AUTO) return false;
    if (p.slot->human) return true;
    if (mode == MODE_1V1) return true; // label both in 1v1
    return false;
}

static void draw_label(Player& p) {
    char buf[8];
    if (p.slot->human)
        snprintf(buf, sizeof(buf), "YOU");
    else
        snprintf(buf, sizeof(buf), "CPU");

    int lx = p.x - (int)strlen(buf)/2;
    int ly = p.y - 1;
    if (ly < 1) ly = p.y + 1; // below if too close to top
    if (lx < 1) lx = 1;
    if (lx + (int)strlen(buf) >= GW-1) lx = GW - 2 - (int)strlen(buf);

    attron(COLOR_PAIR(CP_TRAIL(p.slot->color)) | A_BOLD);
    mvaddstr(ly, lx, buf);
    attroff(COLOR_PAIR(CP_TRAIL(p.slot->color)) | A_BOLD);
}

static void erase_label(Player& p) {
    // overwrite with spaces where label was
    int lx = p.x - 1; // "YOU" or "CPU" = 3 chars
    int ly = p.y - 1;
    if (ly < 1) ly = p.y + 1;
    if (lx < 1) lx = 1;
    // erase a generous area since player may have moved
    for (int dx = -2; dx <= 4; dx++) {
        int ex = p.x + dx, ey = ly;
        if (ex > 0 && ex < GW-1 && ey > 0 && ey < GH-1) {
            if (grid[idx(ex,ey)] == C_EMPTY)
                mvaddch(ey, ex, ' ');
        }
    }
}

static void erase_trail(Player& p) {
    for (auto& [cx,cy] : p.trail_cells) {
        if (cx>0 && cx<GW-1 && cy>0 && cy<GH-1) {
            grid[idx(cx,cy)] = C_EMPTY;
            prev_dir[idx(cx,cy)] = D_NONE;
            mvaddch(cy, cx, ' ');
        }
    }
    p.trail_cells.clear();
}

static void flash_trail(Player& p, bool bright) {
    int pair = CP_TRAIL(p.slot->color);
    if (bright) {
        attron(COLOR_PAIR(pair) | A_BOLD);
        for (auto& [cx,cy] : p.trail_cells)
            if (cx>0 && cx<GW-1 && cy>0 && cy<GH-1)
                mvaddstr(cy, cx, "█");
        attroff(COLOR_PAIR(pair) | A_BOLD);
    } else {
        for (auto& [cx,cy] : p.trail_cells)
            if (cx>0 && cx<GW-1 && cy>0 && cy<GH-1)
                mvaddch(cy, cx, ' ');
    }
}

static void draw_hud(GameMode mode) {
    attron(COLOR_PAIR(CP_HUD));
    move(GH, 0); clrtoeol();
    std::string hud = " ";
    for (int i=0; i<num_players; i++) {
        char buf[32];
        const char* type = players[i].slot->human ? "P" : "AI";
        const char* status = players[i].alive ? "●" :
                             players[i].active ? "~" : "✕";
        snprintf(buf, 32, "%s%d%s ", type, i+1, status);
        hud += buf;
        if (mode==MODE_2V2 && i==1) hud += " vs ";
    }
    if (mode==MODE_AUTO) hud += " [Q]uit";
    else hud += " [Q]uit [R]estart";
    mvaddstr(GH, 0, hud.c_str());
    attroff(COLOR_PAIR(CP_HUD));
}

static void ai_think(Player& p, GameMode mode) {
    if (!p.alive || !p.active || p.slot->human) return;
    int team = p.slot->team;
    int look    = (p.slot->diff==AI_EASY) ? 2 : (p.slot->diff==AI_MED) ? 5 : 12;
    int inertia = (p.slot->diff==AI_EASY) ? 85 : (p.slot->diff==AI_MED) ? 70 : 50;

    int nx = p.x+dir_dx(p.dir), ny = p.y+dir_dy(p.dir);
    if (!blocked_for(nx,ny,team,mode) && (rand()%100 < inertia)) return;

    Dir best = p.dir;
    int best_space = -1;
    for (int d=0; d<4; d++) {
        Dir dd = (Dir)d;
        if (dd == dir_opposite(p.dir)) continue;
        int cx=p.x, cy=p.y, space=0;
        for (int s=0; s<look; s++) {
            cx+=dir_dx(dd); cy+=dir_dy(dd);
            if (blocked_for(cx,cy,team,mode)) break;
            space++;
        }
        if (p.slot->diff == AI_HARD && space > 0) {
            int cx2=p.x+dir_dx(dd), cy2=p.y+dir_dy(dd);
            for (int sd=0; sd<4; sd++) {
                Dir perp=(Dir)sd;
                if (perp==dd||perp==dir_opposite(dd)) continue;
                int px=cx2, py=cy2;
                for (int s=0;s<look/2;s++) {
                    px+=dir_dx(perp); py+=dir_dy(perp);
                    if (blocked_for(px,py,team,mode)) break;
                    space++;
                }
            }
        }
        if (space > best_space) { best_space=space; best=dd; }
    }
    p.dir = best;
}

static void move_player(Player& p, GameMode mode) {
    if (!p.alive || !p.active) return;
    int nx = p.x + dir_dx(p.dir);
    int ny = p.y + dir_dy(p.dir);
    if (blocked_for(nx, ny, p.slot->team, mode)) { p.alive=false; return; }

    Dir old_dir = prev_dir[idx(p.x, p.y)];
    p.x = nx; p.y = ny;
    grid[idx(nx,ny)] = p.cell;
    prev_dir[idx(nx,ny)] = p.dir;
    p.trail_cells.push_back({nx,ny});
    draw_trail_seg(p, old_dir);
}

static int handle_input(GameMode mode) {
    int ch = getch();
    if (ch == ERR) return 0;
    if (ch=='q'||ch=='Q') return -1;
    if ((ch=='r'||ch=='R') && mode!=MODE_AUTO) return 1;
    for (int i=0; i<num_players; i++) {
        if (!players[i].slot->human || !players[i].alive || !players[i].active) continue;
        const KeySet& ks = keysets()[players[i].slot->keyset];
        Dir nd = D_NONE;
        if (ch==ks.up) nd=D_UP;
        if (ch==ks.down) nd=D_DOWN;
        if (ch==ks.left) nd=D_LEFT;
        if (ch==ks.right) nd=D_RIGHT;
        if (nd!=D_NONE && nd!=dir_opposite(players[i].dir))
            players[i].dir = nd;
    }
    return 0;
}

static void countdown() {
    for (int i=3; i>0; i--) {
        char buf[4]; snprintf(buf,4," %d ",i);
        attron(COLOR_PAIR(CP_HUD)|A_BOLD);
        mvaddstr(GH/2, GW/2-1, buf);
        attroff(COLOR_PAIR(CP_HUD)|A_BOLD);
        refresh(); napms(600);
    }
    attron(COLOR_PAIR(CP_HUD)|A_BOLD);
    mvaddstr(GH/2, GW/2-2, " GO! ");
    attroff(COLOR_PAIR(CP_HUD)|A_BOLD);
    refresh(); napms(300);
    mvaddstr(GH/2, GW/2-2, "     ");
}

static void show_result(const char* msg) {
    attron(COLOR_PAIR(CP_HUD)|A_BOLD);
    mvaddstr(GH/2, (GW-(int)strlen(msg))/2, msg);
    attroff(COLOR_PAIR(CP_HUD)|A_BOLD);
    const char* sub = "[ R to replay | Q for menu ]";
    attron(COLOR_PAIR(CP_HUD));
    mvaddstr(GH/2+2, (GW-(int)strlen(sub))/2, sub);
    attroff(COLOR_PAIR(CP_HUD));
    refresh();
}

int Game::run(GameMode mode, Slot slots[8]) {
    srand(time(nullptr));
    GW = COLS; GH = LINES - 1;
    if (GW<30 || GH<16) return -1;

    grid = new Cell[GW*GH];
    prev_dir = new Dir[GW*GH];
    int result = -1;
    bool keep_playing = true;

    bool respawning = (mode==MODE_ENDLESS || mode==MODE_AUTO);

    int tick_ms = Config::get().tick_ms;
    int tick_us = tick_ms * 1000;
    // timing in ticks
    int flash_ticks   = (2000) / tick_ms;
    int respawn_ticks = (mode==MODE_AUTO ? 3000 : 10000) / tick_ms;
    int flash_toggle  = 250 / tick_ms;
    if (flash_toggle < 1) flash_toggle = 1;
    if (flash_ticks < 2)  flash_ticks = 2;
    if (respawn_ticks < flash_ticks + 2) respawn_ticks = flash_ticks + 2;
    int label_ticks = 2000 / tick_ms; // labels visible for 2 seconds

    while (keep_playing) {
        grid_init();
        if (respawning) {
            num_players = mode_players(mode);
            for (int i=0; i<num_players; i++) {
                players[i].slot = &slots[i];
                players[i].cell = (Cell)(C_P1+i);
                players[i].index = i;
                spawn_player(players[i]);
            }
        } else {
            spawn_players_fixed(mode, slots);
        }

        erase(); draw_border();
        for (int i=0; i<num_players; i++)
            if (players[i].active) draw_head(players[i]);
        // set labels for initial spawn
        for (int i=0; i<num_players; i++) {
            if (wants_label(players[i], mode)) {
                players[i].label_tick = 1;
                draw_label(players[i]);
            }
        }
        draw_hud(mode); refresh();

        if (mode != MODE_AUTO) countdown();
        timeout(0);

        struct timespec start;
        clock_gettime(CLOCK_MONOTONIC, &start);
        bool round_over = false;
        int tick = 1;

        while (!round_over) {
            int inp = handle_input(mode);
            if (inp == -1) { keep_playing=false; break; }
            if (inp == 1 && mode!=MODE_AUTO) break;

            // move
            for (int i=0; i<num_players; i++) ai_think(players[i], mode);
            for (int i=0; i<num_players; i++) move_player(players[i], mode);

            // mark newly dead
            for (int i=0; i<num_players; i++) {
                Player& p = players[i];
                if (!p.alive && p.active && p.death_tick < 0)
                    p.death_tick = tick;
            }

            // respawn processing
            if (respawning) {
                for (int i=0; i<num_players; i++) {
                    Player& p = players[i];
                    if (p.alive) continue;       // still kicking
                    if (p.death_tick < 0) continue; // shouldn't happen but safety

                    int since = tick - p.death_tick;

                    if (p.active) {
                        // trail still on screen, flash it
                        if (since <= flash_ticks) {
                            bool bright = ((since / flash_toggle) % 2) == 0;
                            flash_trail(p, bright);
                        } else {
                            // done flashing, erase
                            erase_trail(p);
                            p.active = false;
                        }
                    } else {
                        // trail gone, waiting to respawn
                        if (since >= respawn_ticks) {
                            if (mode==MODE_ENDLESS && p.slot->human) {
                                // human doesn't respawn in endless
                            } else {
                                spawn_player(p);
                                draw_head(p);
                                if (wants_label(p, mode)) p.label_tick = tick;
                            }
                        }
                    }
                }
            }

            // win conditions
            if (mode==MODE_ENDLESS) {
                for (int i=0; i<num_players; i++) {
                    if (players[i].slot->human && !players[i].alive && !round_over) {
                        round_over = true;
                        struct timespec now;
                        clock_gettime(CLOCK_MONOTONIC, &now);
                        double elapsed = (now.tv_sec-start.tv_sec)+(now.tv_nsec-start.tv_nsec)/1e9;
                        char buf[48];
                        snprintf(buf, 48, "  Survived %.1fs  ", elapsed);
                        show_result(buf);
                        result = -1;
                    }
                }
            } else if (!respawning) {
                int alive_count=0, last_alive=-1;
                for (int i=0;i<num_players;i++)
                    if (players[i].alive) { alive_count++; last_alive=i; }

                if (mode==MODE_2V2) {
                    bool t0=false, t1=false;
                    for (int i=0;i<num_players;i++)
                        if (players[i].alive) (slots[i].team==0?t0:t1)=true;
                    if (!t0||!t1) {
                        round_over = true;
                        if (!t0&&!t1) { show_result("  DRAW!  "); result=-1; }
                        else if (t0)  { show_result("  Team 1 wins!  "); result=0; }
                        else          { show_result("  Team 2 wins!  "); result=2; }
                    }
                } else if (alive_count <= 1) {
                    round_over = true;
                    if (alive_count==0) { show_result("  DRAW!  "); result=-1; }
                    else {
                        char buf[32];
                        snprintf(buf,32,"  %s %d wins!  ",
                                 players[last_alive].slot->human?"Player":"CPU", last_alive+1);
                        show_result(buf); result=last_alive;
                    }
                }
            }

            // label processing
            for (int i=0; i<num_players; i++) {
                Player& p = players[i];
                if (p.label_tick < 0) continue;
                int since_label = tick - p.label_tick;
                if (since_label >= label_ticks || !p.alive) {
                    erase_label(p);
                    p.label_tick = -1;
                } else {
                    draw_label(p);
                }
            }

            tick++;
            draw_hud(mode); refresh();
            usleep(tick_us);
        }

        if (!keep_playing) break;

        if (round_over) {
            struct timespec end;
            clock_gettime(CLOCK_MONOTONIC, &end);
            double elapsed = (end.tv_sec-start.tv_sec)+(end.tv_nsec-start.tv_nsec)/1e9;
            auto& sc = Config::scores();

            if (mode==MODE_ENDLESS) {
                sc.rounds_played++;
                if (elapsed > sc.best_endless) sc.best_endless = elapsed;
            } else if (mode!=MODE_AUTO) {
                sc.rounds_played++;
                bool human_won = (result>=0 && slots[result].human);
                if (mode==MODE_2V2 && result>=0) {
                    int wt = slots[result].team;
                    human_won = false;
                    for (int i=0;i<num_players;i++)
                        if (slots[i].team==wt && slots[i].human) human_won=true;
                }
                if (human_won) {
                    sc.total_wins++;
                    sc.current_streak++;
                    if (sc.current_streak > sc.best_streak) sc.best_streak = sc.current_streak;
                } else { sc.current_streak = 0; }
                if (elapsed > sc.best_time) sc.best_time = elapsed;
            }
            Config::save_scores();

            if (mode == MODE_AUTO) continue;

            timeout(-1);
            while (true) {
                int ch = getch();
                if (ch=='q'||ch=='Q') { keep_playing=false; break; }
                if (ch=='r'||ch=='R') break;
            }
        }
    }

    delete[] grid;
    delete[] prev_dir;
    return result;
}
