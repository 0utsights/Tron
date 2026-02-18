#include "game.h"
#include "config.h"
#include <cstring>
#include <cstdlib>
#include <ctime>
#include <unistd.h>

// ── Grid ───────────────────────────────────────────────────

static int GW, GH;
static Cell* grid;
static Dir*  prev_dir;

static inline int idx(int x, int y) { return y*GW+x; }
static inline bool blocked(int x, int y) {
    return x<=0||x>=GW-1||y<=0||y>=GH-1||grid[idx(x,y)]!=C_EMPTY;
}
static inline bool blocked_for(int x, int y, int team, GameMode mode, Cell* g) {
    if (x<=0||x>=GW-1||y<=0||y>=GH-1) return true;
    Cell c = g[idx(x,y)];
    if (c==C_EMPTY) return false;
    if (c==C_WALL) return true;
    // in 2v2, teammate trails don't kill
    if (mode==MODE_2V2) {
        // cells C_P1..C_P4 map to players 0..3
        int cell_player = (int)c - (int)C_P1;
        // teams: 0,1 = team 0; 2,3 = team 1
        int cell_team = cell_player / 2;
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
    mvaddstr(0,0,"╭");     mvaddstr(0,GW-1,"╮");
    mvaddstr(GH-1,0,"╰"); mvaddstr(GH-1,GW-1,"╯");
    attroff(COLOR_PAIR(CP_WALL) | A_DIM);
}

// ── Player state ───────────────────────────────────────────

struct Player {
    int x, y;
    Dir dir;
    bool alive;
    Slot* slot;
    Cell cell;    // C_P1..C_P4
    int index;    // 0..3
};

static Player players[4];
static int num_players;

static void spawn_players(GameMode mode, Slot slots[4]) {
    num_players = mode_players[mode];
    // spawn positions depending on player count
    struct { int x, y; Dir d; } spawns2[] = {
        {GW/4,   GH/2, D_RIGHT},
        {3*GW/4, GH/2, D_LEFT},
    };
    struct { int x, y; Dir d; } spawns4[] = {
        {GW/4,   GH/4,     D_RIGHT},
        {3*GW/4, GH/4,     D_LEFT},
        {GW/4,   3*GH/4,   D_RIGHT},
        {3*GW/4, 3*GH/4,   D_LEFT},
    };

    for (int i=0; i<num_players; i++) {
        Player& p = players[i];
        p.slot  = &slots[i];
        p.cell  = (Cell)(C_P1 + i);
        p.index = i;
        p.alive = true;
        if (num_players == 2) {
            p.x=spawns2[i].x; p.y=spawns2[i].y; p.dir=spawns2[i].d;
        } else {
            p.x=spawns4[i].x; p.y=spawns4[i].y; p.dir=spawns4[i].d;
        }
        grid[idx(p.x, p.y)] = p.cell;
        prev_dir[idx(p.x, p.y)] = p.dir;
    }
}

// ── Drawing ────────────────────────────────────────────────

static void draw_trail(Player& p, Dir old_dir) {
    int cp_t = CP_TRAIL(p.slot->color);
    int cp_h = CP_HEAD(p.slot->color);

    // previous position: replace head with trail segment
    int ox = p.x - dir_dx(p.dir);
    int oy = p.y - dir_dy(p.dir);
    if (ox>0 && ox<GW-1 && oy>0 && oy<GH-1) {
        attron(COLOR_PAIR(cp_t) | A_BOLD);
        mvaddstr(oy, ox, Trail::corner(old_dir, p.dir));
        attroff(COLOR_PAIR(cp_t) | A_BOLD);
    }
    // new head
    attron(COLOR_PAIR(cp_h) | A_BOLD);
    mvaddstr(p.y, p.x, Trail::HD);
    attroff(COLOR_PAIR(cp_h) | A_BOLD);
}

static void draw_head(Player& p) {
    attron(COLOR_PAIR(CP_HEAD(p.slot->color)) | A_BOLD);
    mvaddstr(p.y, p.x, Trail::HD);
    attroff(COLOR_PAIR(CP_HEAD(p.slot->color)) | A_BOLD);
}

static void draw_hud(GameMode mode) {
    attron(COLOR_PAIR(CP_HUD));
    move(GH, 0);
    clrtoeol();
    std::string hud = " ";
    for (int i=0; i<num_players; i++) {
        char buf[32];
        const char* type = players[i].slot->human ? "P" : "CPU";
        const char* status = players[i].alive ? "●" : "✕";
        snprintf(buf, 32, "%s%d%s ", type, i+1, status);
        hud += buf;
        if (mode==MODE_2V2 && i==1) hud += " vs ";
        else if (i < num_players-1) hud += " ";
    }
    hud += "  [Q]uit [R]estart";
    mvaddstr(GH, 0, hud.c_str());
    attroff(COLOR_PAIR(CP_HUD));
}

// ── AI ─────────────────────────────────────────────────────

static void ai_think(Player& p, GameMode mode) {
    if (!p.alive || p.slot->human) return;

    int team = p.slot->team;
    // lookahead distance based on difficulty
    int look = (p.slot->diff==AI_EASY) ? 2 : (p.slot->diff==AI_MED) ? 5 : 12;
    // chance to keep current direction (lower = smarter / more reactive)
    int inertia = (p.slot->diff==AI_EASY) ? 85 : (p.slot->diff==AI_MED) ? 70 : 50;

    // current direction safe?
    int nx = p.x+dir_dx(p.dir), ny = p.y+dir_dy(p.dir);
    if (!blocked_for(nx,ny,team,mode,grid) && (rand()%100 < inertia))
        return;

    // evaluate each direction
    Dir best = p.dir;
    int best_space = -1;

    for (int d=0; d<4; d++) {
        Dir dd = (Dir)d;
        if (dd == dir_opposite(p.dir)) continue;

        int cx=p.x, cy=p.y, space=0;
        for (int s=0; s<look; s++) {
            cx += dir_dx(dd); cy += dir_dy(dd);
            if (blocked_for(cx,cy,team,mode,grid)) break;
            space++;
        }

        // hard difficulty: also count perpendicular space
        if (p.slot->diff == AI_HARD && space > 0) {
            int cx2=p.x+dir_dx(dd), cy2=p.y+dir_dy(dd);
            for (int sd=0; sd<4; sd++) {
                Dir perp = (Dir)sd;
                if (perp==dd || perp==dir_opposite(dd)) continue;
                int px=cx2,py=cy2;
                for (int s=0;s<look/2;s++) {
                    px+=dir_dx(perp); py+=dir_dy(perp);
                    if (blocked_for(px,py,team,mode,grid)) break;
                    space++;
                }
            }
        }

        if (space > best_space) { best_space=space; best=dd; }
    }
    p.dir = best;
}

// ── Movement ───────────────────────────────────────────────

static void move_player(Player& p, GameMode mode) {
    if (!p.alive) return;
    int nx = p.x + dir_dx(p.dir);
    int ny = p.y + dir_dy(p.dir);

    if (blocked_for(nx, ny, p.slot->team, mode, grid)) {
        p.alive = false;
        return;
    }

    Dir old_dir = prev_dir[idx(p.x, p.y)];
    p.x = nx; p.y = ny;
    grid[idx(nx,ny)] = p.cell;
    prev_dir[idx(nx,ny)] = p.dir;
    draw_trail(p, old_dir);
}

// ── Input ──────────────────────────────────────────────────

static int handle_input() {
    int ch = getch();
    if (ch == ERR) return 0;
    if (ch=='q'||ch=='Q') return -1;
    if (ch=='r'||ch=='R') return 1;

    for (int i=0; i<num_players; i++) {
        if (!players[i].slot->human || !players[i].alive) continue;
        const KeySet& ks = keysets()[players[i].slot->keyset];
        Dir nd = D_NONE;
        if (ch==ks.up)    nd=D_UP;
        if (ch==ks.down)  nd=D_DOWN;
        if (ch==ks.left)  nd=D_LEFT;
        if (ch==ks.right) nd=D_RIGHT;
        if (nd!=D_NONE && nd!=dir_opposite(players[i].dir))
            players[i].dir = nd;
    }
    return 0;
}

// ── Countdown ──────────────────────────────────────────────

static void countdown() {
    for (int i=3; i>0; i--) {
        char buf[4];
        snprintf(buf, 4, " %d ", i);
        attron(COLOR_PAIR(CP_HUD) | A_BOLD);
        mvaddstr(GH/2, GW/2-1, buf);
        attroff(COLOR_PAIR(CP_HUD) | A_BOLD);
        refresh();
        napms(600);
    }
    attron(COLOR_PAIR(CP_HUD) | A_BOLD);
    mvaddstr(GH/2, GW/2-2, " GO! ");
    attroff(COLOR_PAIR(CP_HUD) | A_BOLD);
    refresh();
    napms(300);
    mvaddstr(GH/2, GW/2-2, "     ");
}

// ── Result screen ──────────────────────────────────────────

static void show_result(const char* msg) {
    int len = strlen(msg);
    attron(COLOR_PAIR(CP_HUD) | A_BOLD);
    mvaddstr(GH/2, (GW-len)/2, msg);
    attroff(COLOR_PAIR(CP_HUD) | A_BOLD);
    attron(COLOR_PAIR(CP_HUD));
    const char* sub = "[ R to replay | Q for menu ]";
    mvaddstr(GH/2+2, (GW-(int)strlen(sub))/2, sub);
    attroff(COLOR_PAIR(CP_HUD));
    refresh();
}

// ── Main game function ─────────────────────────────────────

int Game::run(GameMode mode, Slot slots[4]) {
    srand(time(nullptr));

    GW = COLS;
    GH = LINES - 1;  // reserve bottom for HUD
    if (GW < 30 || GH < 16) return -1;

    grid = new Cell[GW*GH];
    prev_dir = new Dir[GW*GH];

    int result = -1;
    bool keep_playing = true;

    while (keep_playing) {
        // setup round
        grid_init();
        spawn_players(mode, slots);
        erase();
        draw_border();
        for (int i=0;i<num_players;i++) draw_head(players[i]);
        draw_hud(mode);
        refresh();

        countdown();

        timeout(0);
        struct timespec start;
        clock_gettime(CLOCK_MONOTONIC, &start);

        int tick_us = Config::get().tick_ms * 1000;
        bool round_over = false;

        // game loop
        while (!round_over) {
            int inp = handle_input();
            if (inp == -1) { keep_playing=false; break; }
            if (inp == 1)  break;  // restart

            for (int i=0;i<num_players;i++) ai_think(players[i], mode);
            for (int i=0;i<num_players;i++) move_player(players[i], mode);

            // check alive count
            int alive_count = 0;
            int last_alive = -1;
            for (int i=0;i<num_players;i++) {
                if (players[i].alive) { alive_count++; last_alive=i; }
            }

            // 2v2: check if one team is entirely dead
            if (mode == MODE_2V2) {
                bool team0_alive = false, team1_alive = false;
                for (int i=0;i<num_players;i++) {
                    if (players[i].alive) {
                        if (slots[i].team==0) team0_alive=true;
                        else team1_alive=true;
                    }
                }
                if (!team0_alive || !team1_alive) {
                    round_over = true;
                    // determine result
                    if (!team0_alive && !team1_alive) {
                        show_result("  DRAW!  ");
                        result = -1;
                    } else if (team0_alive) {
                        show_result("  Team 1 wins!  ");
                        result = 0;
                    } else {
                        show_result("  Team 2 wins!  ");
                        result = 2;
                    }
                }
            } else {
                // FFA / 1v1
                if (alive_count <= 1) {
                    round_over = true;
                    if (alive_count == 0) {
                        show_result("  DRAW!  ");
                        result = -1;
                    } else {
                        char buf[32];
                        const char* type = players[last_alive].slot->human ? "Player" : "CPU";
                        snprintf(buf, 32, "  %s %d wins!  ", type, last_alive+1);
                        show_result(buf);
                        result = last_alive;
                    }
                }
            }

            draw_hud(mode);
            refresh();
            usleep(tick_us);
        }

        if (!keep_playing) break;

        // update scores if round ended (human player won)
        if (round_over) {
            struct timespec end;
            clock_gettime(CLOCK_MONOTONIC, &end);
            double elapsed = (end.tv_sec-start.tv_sec) + (end.tv_nsec-start.tv_nsec)/1e9;

            auto& sc = Config::scores();
            sc.rounds_played++;

            // check if any human won
            bool human_won = (result>=0 && slots[result].human);
            if (mode==MODE_2V2 && result>=0) {
                int winning_team = slots[result].team;
                human_won = false;
                for (int i=0;i<num_players;i++)
                    if (slots[i].team==winning_team && slots[i].human) human_won=true;
            }

            if (human_won) {
                sc.total_wins++;
                sc.current_streak++;
                if (sc.current_streak > sc.best_streak)
                    sc.best_streak = sc.current_streak;
            } else {
                sc.current_streak = 0;
            }
            if (elapsed > sc.best_time) sc.best_time = elapsed;
            Config::save_scores();

            // wait for R or Q
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
