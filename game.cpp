#include "game.h"
#include "config.h"
#include <cstring>
#include <cstdlib>
#include <ctime>
#include <cmath>
#include <unistd.h>
#include <vector>
#include <algorithm>

// world grid (may be larger than screen)
static int GW, GH;
// screen/viewport size
static int SW, SH;
// camera top-left corner in world coords
static int cam_x, cam_y;
static bool use_camera;

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

// world -> screen conversion
static inline int scr_x(int wx) { return wx - cam_x; }
static inline int scr_y(int wy) { return wy - cam_y; }
static inline bool on_screen(int wx, int wy) {
    int sx = scr_x(wx), sy = scr_y(wy);
    return sx>=0 && sx<SW && sy>=0 && sy<SH;
}

// center camera on a world position
static void center_cam(int wx, int wy) {
    cam_x = wx - SW/2;
    cam_y = wy - SH/2;
    if (cam_x < 0) cam_x = 0;
    if (cam_y < 0) cam_y = 0;
    if (cam_x + SW > GW) cam_x = GW - SW;
    if (cam_y + SH > GH) cam_y = GH - SH;
}

// full viewport redraw from grid state
static void render_viewport() {
    for (int sy=0; sy<SH; sy++) {
        for (int sx=0; sx<SW; sx++) {
            int wx = cam_x + sx;
            int wy = cam_y + sy;
            if (wx<0||wx>=GW||wy<0||wy>=GH) {
                mvaddch(sy, sx, ' ');
                continue;
            }
            Cell c = grid[idx(wx,wy)];
            if (c == C_EMPTY) {
                mvaddch(sy, sx, ' ');
            } else if (c == C_WALL) {
                bool top = (wy==0), bot = (wy==GH-1);
                bool lft = (wx==0), rgt = (wx==GW-1);
                attron(COLOR_PAIR(CP_WALL) | A_DIM);
                if (top && lft)      mvaddstr(sy,sx,"╭");
                else if (top && rgt) mvaddstr(sy,sx,"╮");
                else if (bot && lft) mvaddstr(sy,sx,"╰");
                else if (bot && rgt) mvaddstr(sy,sx,"╯");
                else if (top||bot)   mvaddstr(sy,sx,"─");
                else                 mvaddstr(sy,sx,"│");
                attroff(COLOR_PAIR(CP_WALL) | A_DIM);
            } else {
                int pi = (int)c - (int)C_P1;
                Dir cur_dir = prev_dir[idx(wx,wy)];

                // figure out the "incoming" direction by checking which
                // neighbor with the same cell color points toward us
                Dir in_dir = cur_dir;
                // check all 4 neighbors for one that has the same cell
                // and whose direction leads into this cell
                for (int d=0; d<4; d++) {
                    Dir dd = (Dir)d;
                    int nx = wx - dir_dx(dd);
                    int ny = wy - dir_dy(dd);
                    if (nx>=0 && nx<GW && ny>=0 && ny<GH
                        && grid[idx(nx,ny)] == c
                        && prev_dir[idx(nx,ny)] == dd) {
                        in_dir = dd;
                        break;
                    }
                }

                const char* ch = Trail::corner(in_dir, cur_dir);
                attron(COLOR_PAIR(CP_TRAIL(pi)) | A_BOLD);
                mvaddstr(sy, sx, ch);
                attroff(COLOR_PAIR(CP_TRAIL(pi)) | A_BOLD);
            }
        }
    }
}

// fixed-camera border draw (for non-camera modes)
static void draw_border() {
    attron(COLOR_PAIR(CP_WALL) | A_DIM);
    for (int x=0;x<GW;x++) { mvaddstr(0,x,"─"); mvaddstr(GH-1,x,"─"); }
    for (int y=0;y<GH;y++) { mvaddstr(y,0,"│"); mvaddstr(y,GW-1,"│"); }
    mvaddstr(0,0,"╭"); mvaddstr(0,GW-1,"╮");
    mvaddstr(GH-1,0,"╰"); mvaddstr(GH-1,GW-1,"╯");
    attroff(COLOR_PAIR(CP_WALL) | A_DIM);
}

struct Player {
    int x, y;
    Dir dir;
    bool alive;
    bool active;
    Slot* slot;
    Cell cell;
    int index;
    int death_tick;
    int label_tick;
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
    p.label_tick = -1;
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
        p.death_tick = -1; p.label_tick = -1;
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

// incremental trail draw (fixed camera only)
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

// draw head at screen position (works for both modes)
static void draw_head_at(Player& p) {
    int sx = use_camera ? scr_x(p.x) : p.x;
    int sy = use_camera ? scr_y(p.y) : p.y;
    if (sx>=0 && sx<SW && sy>=0 && sy<SH) {
        attron(COLOR_PAIR(CP_HEAD(p.slot->color)) | A_BOLD);
        mvaddstr(sy, sx, Trail::HD);
        attroff(COLOR_PAIR(CP_HEAD(p.slot->color)) | A_BOLD);
    }
}

static bool wants_label(Player& p, GameMode mode) {
    if (mode == MODE_AUTO) return false;
    if (p.slot->human) return true;
    if (mode == MODE_1V1) return true;
    return false;
}

static void draw_label(Player& p) {
    char buf[8];
    snprintf(buf, sizeof(buf), p.slot->human ? "YOU" : "CPU");
    int wx = p.x, wy = p.y;
    int sx = (use_camera ? scr_x(wx) : wx) - (int)strlen(buf)/2;
    int sy = (use_camera ? scr_y(wy) : wy) - 1;
    if (sy < 0) sy += 2;
    if (sx < 0) sx = 0;
    if (sx + (int)strlen(buf) >= SW) sx = SW - 1 - (int)strlen(buf);
    attron(COLOR_PAIR(CP_TRAIL(p.slot->color)) | A_BOLD);
    mvaddstr(sy, sx, buf);
    attroff(COLOR_PAIR(CP_TRAIL(p.slot->color)) | A_BOLD);
}

static void erase_label(Player& p) {
    int wx = p.x, wy = p.y;
    int sy = (use_camera ? scr_y(wy) : wy) - 1;
    if (sy < 0) sy += 2;
    for (int dx = -2; dx <= 4; dx++) {
        int ex = (use_camera ? scr_x(wx) : wx) + dx;
        if (ex >= 0 && ex < SW && sy >= 0 && sy < SH) {
            // only clear if the underlying grid cell is empty
            int gwx = (use_camera ? cam_x : 0) + ex;
            int gwy = (use_camera ? cam_y : 0) + sy;
            if (gwx>=0 && gwx<GW && gwy>=0 && gwy<GH && grid[idx(gwx,gwy)]==C_EMPTY)
                mvaddch(sy, ex, ' ');
        }
    }
}

static void erase_trail(Player& p) {
    for (auto& [cx,cy] : p.trail_cells) {
        if (cx>0 && cx<GW-1 && cy>0 && cy<GH-1) {
            grid[idx(cx,cy)] = C_EMPTY;
            prev_dir[idx(cx,cy)] = D_NONE;
            if (!use_camera) mvaddch(cy, cx, ' ');
        }
    }
    p.trail_cells.clear();
}

static void flash_trail(Player& p, bool bright) {
    int pair = CP_TRAIL(p.slot->color);
    for (auto& [cx,cy] : p.trail_cells) {
        if (cx<=0 || cx>=GW-1 || cy<=0 || cy>=GH-1) continue;
        int sx = use_camera ? scr_x(cx) : cx;
        int sy = use_camera ? scr_y(cy) : cy;
        if (sx<0||sx>=SW||sy<0||sy>=SH) continue;
        if (bright) {
            attron(COLOR_PAIR(pair) | A_BOLD);
            mvaddstr(sy, sx, "█");
            attroff(COLOR_PAIR(pair) | A_BOLD);
        } else {
            mvaddch(sy, sx, ' ');
        }
    }
}

// find the camera follow target for autotron
static int find_follow_target() {
    int best = -1, best_len = -1;
    for (int i=0; i<num_players; i++) {
        if (!players[i].alive || !players[i].active) continue;
        int len = (int)players[i].trail_cells.size();
        if (len > best_len) { best_len = len; best = i; }
    }
    // nobody alive? find first one that will respawn soonest (lowest death_tick)
    if (best < 0) {
        int earliest = 999999999;
        for (int i=0; i<num_players; i++) {
            if (players[i].death_tick >= 0 && players[i].death_tick < earliest) {
                earliest = players[i].death_tick;
                best = i;
            }
        }
    }
    // absolute fallback
    if (best < 0) best = 0;
    return best;
}

// draw proximity arrow to nearest alive enemy (endless only)
static void draw_nearest_arrow(int follow_idx) {
    Player& me = players[follow_idx];
    if (!me.alive) return;

    int nearest = -1;
    double nearest_dist = 1e9;
    for (int i=0; i<num_players; i++) {
        if (i == follow_idx || !players[i].alive || !players[i].active) continue;
        double dx = players[i].x - me.x;
        double dy = players[i].y - me.y;
        double d = sqrt(dx*dx + dy*dy);
        if (d < nearest_dist) { nearest_dist = d; nearest = i; }
    }
    if (nearest < 0) return;

    // direction from me to them
    double dx = players[nearest].x - me.x;
    double dy = players[nearest].y - me.y;

    // normalize and place arrow near viewport edge
    double len = sqrt(dx*dx + dy*dy);
    if (len < 1) return;
    dx /= len; dy /= len;

    // arrow position: push toward edge of screen
    int margin = 3;
    int ax = SW/2 + (int)(dx * (SW/2 - margin));
    int ay = SH/2 + (int)(dy * (SH/2 - margin));
    // clamp
    if (ax < margin) ax = margin;
    if (ax >= SW-margin) ax = SW-margin-1;
    if (ay < 1) ay = 1;
    if (ay >= SH-1) ay = SH-1;

    // pick arrow character
    const char* arrow;
    double angle = atan2(dy, dx);
    if      (angle > -0.39 && angle <= 0.39)  arrow = "→";
    else if (angle > 0.39  && angle <= 1.18)  arrow = "↘";
    else if (angle > 1.18  && angle <= 1.96)  arrow = "↓";
    else if (angle > 1.96  && angle <= 2.75)  arrow = "↙";
    else if (angle > 2.75  || angle <= -2.75) arrow = "←";
    else if (angle > -2.75 && angle <= -1.96) arrow = "↖";
    else if (angle > -1.96 && angle <= -1.18) arrow = "↑";
    else                                       arrow = "↗";

    attron(COLOR_PAIR(CP_TRAIL(players[nearest].slot->color)) | A_BOLD);
    mvaddstr(ay, ax, arrow);
    attroff(COLOR_PAIR(CP_TRAIL(players[nearest].slot->color)) | A_BOLD);
}

static void draw_hud(GameMode mode, int follow_idx) {
    int hud_y = use_camera ? SH : GH;
    move(hud_y, 0); clrtoeol();
    int x = 1;

    // in camera auto mode, show who we're following
    if (use_camera && mode == MODE_AUTO && follow_idx >= 0) {
        char fbuf[32];
        snprintf(fbuf, 32, "[watching AI%d] ", follow_idx+1);
        attron(COLOR_PAIR(CP_TRAIL(players[follow_idx].slot->color)) | A_DIM);
        mvaddstr(hud_y, x, fbuf);
        attroff(COLOR_PAIR(CP_TRAIL(players[follow_idx].slot->color)) | A_DIM);
        x += strlen(fbuf);
    }

    for (int i=0; i<num_players; i++) {
        char buf[16];
        const char* type = players[i].slot->human ? "P" : "AI";
        const char* status = players[i].alive ? "●" :
                             players[i].active ? "~" : "✕";
        snprintf(buf, 16, "%s%d%s", type, i+1, status);
        int pair = CP_TRAIL(players[i].slot->color);
        attron(COLOR_PAIR(pair) | (players[i].alive ? A_BOLD : A_DIM));
        mvaddstr(hud_y, x, buf);
        attroff(COLOR_PAIR(pair) | (players[i].alive ? A_BOLD : A_DIM));
        x += strlen(buf) + 1;
        if (mode==MODE_2V2 && i==1) {
            attron(COLOR_PAIR(CP_DIM));
            mvaddstr(hud_y, x, "vs ");
            attroff(COLOR_PAIR(CP_DIM));
            x += 3;
        }
    }
    attron(COLOR_PAIR(CP_DIM));
    if (mode==MODE_AUTO) mvaddstr(hud_y, x+1, "[Q]uit");
    else                 mvaddstr(hud_y, x+1, "[Q]uit [R]estart");
    attroff(COLOR_PAIR(CP_DIM));
}

static void ai_think(Player& p, GameMode mode) {
    if (!p.alive || !p.active || p.slot->human) return;
    int team = p.slot->team;
    int look, inertia, aggression;
    if (mode == MODE_AUTO) {
        look = 20; inertia = 30; aggression = 40;
    } else {
        look    = (p.slot->diff==AI_EASY) ? 2 : (p.slot->diff==AI_MED) ? 5 : 12;
        inertia = (p.slot->diff==AI_EASY) ? 85 : (p.slot->diff==AI_MED) ? 70 : 50;
        aggression = (p.slot->diff==AI_EASY) ? 5 : (p.slot->diff==AI_MED) ? 15 : 30;
    }
    bool do_perp = (p.slot->diff == AI_HARD || mode == MODE_AUTO);

    // current direction safe?
    int nx = p.x+dir_dx(p.dir), ny = p.y+dir_dy(p.dir);
    if (!blocked_for(nx,ny,team,mode) && (rand()%100 < inertia)) return;

    // find nearest other alive player
    int target_x = -1, target_y = -1;
    double nearest_dist = 1e9;
    for (int i=0; i<num_players; i++) {
        if (&players[i] == &p || !players[i].alive || !players[i].active) continue;
        double dx = players[i].x - p.x;
        double dy = players[i].y - p.y;
        double d = dx*dx + dy*dy;
        if (d < nearest_dist) { nearest_dist = d; target_x = players[i].x; target_y = players[i].y; }
    }

    // evaluate each direction
    Dir best = p.dir;
    int best_score = -99999;
    for (int d=0; d<4; d++) {
        Dir dd = (Dir)d;
        if (dd == dir_opposite(p.dir)) continue;

        // space check (survival)
        int cx=p.x, cy=p.y, space=0;
        for (int s=0; s<look; s++) {
            cx+=dir_dx(dd); cy+=dir_dy(dd);
            if (blocked_for(cx,cy,team,mode)) break;
            space++;
        }
        if (do_perp && space > 0) {
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

        // dead end = never go there
        if (space == 0) continue;

        // aggression bonus: prefer directions that move toward target
        int seek_bonus = 0;
        if (target_x >= 0 && rand()%100 < aggression) {
            int step_x = p.x + dir_dx(dd);
            int step_y = p.y + dir_dy(dd);
            double old_dist = (p.x-target_x)*(p.x-target_x) + (p.y-target_y)*(p.y-target_y);
            double new_dist = (step_x-target_x)*(step_x-target_x) + (step_y-target_y)*(step_y-target_y);
            if (new_dist < old_dist) seek_bonus = 8;
        }

        int score = space + seek_bonus;
        if (score > best_score) { best_score = score; best = dd; }
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
    // incremental draw only for fixed camera
    if (!use_camera) draw_trail_seg(p, old_dir);
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

static void countdown_cam(int follow_idx) {
    // camera mode countdown: render viewport centered on follow target
    for (int i=3; i>0; i--) {
        if (follow_idx >= 0) center_cam(players[follow_idx].x, players[follow_idx].y);
        render_viewport();
        // draw all heads
        for (int p=0;p<num_players;p++)
            if (players[p].active) draw_head_at(players[p]);
        char buf[4]; snprintf(buf,4," %d ",i);
        attron(COLOR_PAIR(CP_HUD)|A_BOLD);
        mvaddstr(SH/2, SW/2-1, buf);
        attroff(COLOR_PAIR(CP_HUD)|A_BOLD);
        draw_hud(MODE_ENDLESS, follow_idx);
        refresh(); napms(600);
    }
    attron(COLOR_PAIR(CP_HUD)|A_BOLD);
    mvaddstr(SH/2, SW/2-2, " GO! ");
    attroff(COLOR_PAIR(CP_HUD)|A_BOLD);
    refresh(); napms(300);
}

static void countdown_fixed() {
    int hh = use_camera ? SH : GH;
    int ww = use_camera ? SW : GW;
    for (int i=3; i>0; i--) {
        char buf[4]; snprintf(buf,4," %d ",i);
        attron(COLOR_PAIR(CP_HUD)|A_BOLD);
        mvaddstr(hh/2, ww/2-1, buf);
        attroff(COLOR_PAIR(CP_HUD)|A_BOLD);
        refresh(); napms(600);
    }
    attron(COLOR_PAIR(CP_HUD)|A_BOLD);
    mvaddstr(hh/2, ww/2-2, " GO! ");
    attroff(COLOR_PAIR(CP_HUD)|A_BOLD);
    refresh(); napms(300);
    mvaddstr(hh/2, ww/2-2, "     ");
}

static void show_result(const char* msg) {
    int hh = use_camera ? SH : GH;
    int ww = use_camera ? SW : GW;
    attron(COLOR_PAIR(CP_HUD)|A_BOLD);
    mvaddstr(hh/2, (ww-(int)strlen(msg))/2, msg);
    attroff(COLOR_PAIR(CP_HUD)|A_BOLD);
    const char* sub = "[ R to replay | Q for menu ]";
    attron(COLOR_PAIR(CP_HUD));
    mvaddstr(hh/2+2, (ww-(int)strlen(sub))/2, sub);
    attroff(COLOR_PAIR(CP_HUD));
    refresh();
}

int Game::run(GameMode mode, Slot slots[8]) {
    srand(time(nullptr));
    SW = COLS; SH = LINES - 1;
    if (SW<30 || SH<16) return -1;

    // decide grid size and camera mode
    use_camera = (mode == MODE_ENDLESS || (mode == MODE_AUTO && slots[0].team == 1));
    // slots[0].team is repurposed: 0=fixed, 1=camera for auto mode
    // endless always uses camera
    if (mode == MODE_ENDLESS) use_camera = true;

    if (use_camera) {
        GW = SW * 3; GH = SH * 3; // 3x terminal size
        if (GW < 150) GW = 150;
        if (GH < 80)  GH = 80;
    } else {
        GW = SW; GH = SH;
    }

    grid = new Cell[GW*GH];
    prev_dir = new Dir[GW*GH];
    int result = -1;
    bool keep_playing = true;

    bool respawning = (mode==MODE_ENDLESS || mode==MODE_AUTO);

    int tick_ms = Config::get().tick_ms;
    int tick_us = tick_ms * 1000;
    int flash_ticks   = 2000 / tick_ms;
    int respawn_ticks = (mode==MODE_AUTO ? 3000 : 10000) / tick_ms;
    int flash_toggle  = 250 / tick_ms;
    if (flash_toggle < 1) flash_toggle = 1;
    if (flash_ticks < 2) flash_ticks = 2;
    if (respawn_ticks < flash_ticks + 2) respawn_ticks = flash_ticks + 2;

    int follow_idx = 0;

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

        // find initial follow target
        if (use_camera) {
            if (mode == MODE_ENDLESS) {
                // follow the human
                for (int i=0;i<num_players;i++)
                    if (players[i].slot->human) { follow_idx=i; break; }
            } else {
                follow_idx = find_follow_target();
            }
            center_cam(players[follow_idx].x, players[follow_idx].y);
        }

        erase();
        if (use_camera) {
            render_viewport();
            for (int i=0;i<num_players;i++)
                if (players[i].active) draw_head_at(players[i]);
        } else {
            draw_border();
            for (int i=0;i<num_players;i++)
                if (players[i].active) draw_head_at(players[i]);
        }

        // pre-game labels
        for (int i=0;i<num_players;i++)
            if (wants_label(players[i], mode)) draw_label(players[i]);
        draw_hud(mode, follow_idx); refresh();

        if (mode != MODE_AUTO) {
            if (use_camera)
                countdown_cam(follow_idx);
            else
                countdown_fixed();
            // erase labels
            for (int i=0;i<num_players;i++)
                if (wants_label(players[i], mode)) erase_label(players[i]);
            for (int i=0;i<num_players;i++)
                if (players[i].active) draw_head_at(players[i]);
            refresh();
        }
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
            for (int i=0;i<num_players;i++) ai_think(players[i], mode);
            for (int i=0;i<num_players;i++) move_player(players[i], mode);

            // mark newly dead
            for (int i=0;i<num_players;i++) {
                Player& p = players[i];
                if (!p.alive && p.active && p.death_tick < 0)
                    p.death_tick = tick;
            }

            // respawn processing
            if (respawning) {
                for (int i=0;i<num_players;i++) {
                    Player& p = players[i];
                    if (p.alive) continue;
                    if (p.death_tick < 0) continue;
                    int since = tick - p.death_tick;

                    if (p.active) {
                        if (since <= flash_ticks) {
                            if (!use_camera) {
                                bool bright = ((since / flash_toggle) % 2) == 0;
                                flash_trail(p, bright);
                            }
                            // camera mode: flash handled in render_viewport below
                        } else {
                            erase_trail(p);
                            p.active = false;
                        }
                    } else {
                        if (since >= respawn_ticks) {
                            if (mode==MODE_ENDLESS && p.slot->human) {
                                // human stays dead
                            } else {
                                spawn_player(p);
                            }
                        }
                    }
                }
            }

            // update camera
            if (use_camera) {
                if (mode == MODE_AUTO) {
                    // follow longest trail, switch if current target died
                    if (!players[follow_idx].alive || !players[follow_idx].active)
                        follow_idx = find_follow_target();
                    else {
                        // check if someone else has a longer trail
                        int cur_len = (int)players[follow_idx].trail_cells.size();
                        for (int i=0;i<num_players;i++) {
                            if (!players[i].alive) continue;
                            if ((int)players[i].trail_cells.size() > cur_len + 20) {
                                follow_idx = i; break;
                            }
                        }
                    }
                }
                // endless always follows the human
                center_cam(players[follow_idx].x, players[follow_idx].y);
                render_viewport();

                // draw heads on top of viewport
                for (int i=0;i<num_players;i++)
                    if (players[i].alive && players[i].active)
                        draw_head_at(players[i]);

                // flash dead trails in camera mode
                if (respawning) {
                    for (int i=0;i<num_players;i++) {
                        Player& p = players[i];
                        if (!p.alive && p.active && p.death_tick >= 0) {
                            int since = tick - p.death_tick;
                            if (since <= flash_ticks) {
                                bool bright = ((since / flash_toggle) % 2) == 0;
                                flash_trail(p, bright);
                            }
                        }
                    }
                }

                // proximity arrow in endless
                if (mode == MODE_ENDLESS)
                    draw_nearest_arrow(follow_idx);
            }

            // win conditions
            if (mode==MODE_ENDLESS) {
                for (int i=0;i<num_players;i++) {
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

            tick++;
            draw_hud(mode, follow_idx); refresh();
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
