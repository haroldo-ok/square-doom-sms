/* [logic bank] Monsters (VicDoom p_enemy AI), projectiles, enemy hitscan LOS, barrels
 * and explosions (enemy.asm, missile.asm, hitscan.asm). */
#include "defs.h"
u16 mul8x16hi(u8 a, u16 b);

u8 mobj_alloc[MAX_MOBJ], mobj_x[MAX_MOBJ], mobj_y[MAX_MOBJ], mobj_xfrac[MAX_MOBJ], mobj_yfrac[MAX_MOBJ];
u8 mobj_info[MAX_MOBJ], mobj_state[MAX_MOBJ], mobj_health[MAX_MOBJ], mobj_movedir[MAX_MOBJ];
u8 mobj_movecnt[MAX_MOBJ], mobj_react[MAX_MOBJ], mobj_flags[MAX_MOBJ], mobj_corpse[MAX_MOBJ];
u8 missile_z, procket_z, anim_frame;
u8 fx_kind[FX_MAX], fx_tx[FX_MAX], fx_ty[FX_MAX], fx_time[FX_MAX];
u8 pain_boost, react_dt_units;
static u8 react_dt_rem, anim_dt_acc, new_chase_dir_frame, barrel_events;
static u8 ea, einfo, esec, edist;
static s16 wish_x, wish_y;
static u8 missile_zfrac, procket_zfrac;
static s16 missile_momx, missile_momy, missile_momz, procket_momx, procket_momy;
static u16 missile_life, procket_life;

#define MELEERANGE 4
#define STANDOFF 12
#define DI_NODIR 8
#define PAIN_MOVECNT 16
#define FALL_MOVECNT 8
#define ATK_WINDUP 18
#define ATK_MOVECNT 14
#define ATK_REACT 16
#define NEWDIR_RETRY 8
enum { A_CHASE, A_FLINCH, A_MELEE, A_SHOOT, A_MISSILE, A_FALL, A_FLY };

static const u8 pain_chance[4] = {2, 3, 4, 0xff};
static const u8 chase_state[4] = {0, 4, 9, 13};
static const u8 pain_state[4] = {1, 5, 10, 14};
static const u8 melee_state[4] = {0xff, 6, 11, 15};
static const u8 shoot_state[4] = {2, 7, 0xff, 16};
static const u8 death_state[4] = {3, 8, 12, 17};
static const u8 death_sound[4] = {SND_SGTDTH, SND_PLPAIN, SND_DMPAIN, SND_DMPAIN};
static const u8 opposite_dir[9] = {4, 5, 6, 7, 0, 1, 2, 3, DI_NODIR};
static const u8 diags_dir[4] = {3, 1, 5, 7};
static const s8 tryd_x[2][8] = {{96, 66, 0, -66, -96, -66, 0, 66}, {127, 88, 0, -88, -127, -88, 0, 88}};
static const s8 tryd_y[2][8] = {{0, 66, 96, 66, 0, -66, -96, -66}, {0, 88, 127, 88, 0, -88, -127, -88}};
/* speed 4 is 128 per 64 ms in the C64 table; 127 + extra carry keeps it in a byte */

static u8 rnd(void) { return rnd8(); }
static u8 approx_dist(s8 dx, s8 dy) {
  u8 ax = dx < 0 ? -dx : dx, ay = dy < 0 ? -dy : dy;
  return ay >= ax ? (ax >> 1) + ay : (ay >> 1) + ax;
}

/* ---------------------------------------------------------------- hitscan */
#define HS_IDLE 0
#define HS_PENDING 1
#define HS_CLEAR 2
#define HS_BLOCKED 3
static u8 hs_status, hs_claimed, hs_actor;
static u8 hs_x0, hs_y0, hs_x1, hs_y1, hs_z0, hs_z1;

static void hitscan_reset(void) { hs_status = HS_IDLE; hs_claimed = 0; hs_actor = 0xff; }
static void hitscan_release(void) { hs_status = HS_IDLE; hs_actor = 0xff; }
static u8 hitscan_request(u8 m) {
  u8 s;
  if (hs_claimed) return 1;
  hs_claimed = 1; hs_actor = m;
  s = sector_at(mobj_x[m] >> 3, mobj_y[m] >> 3);
  if (s == player_sector) { hs_status = HS_CLEAR; return 0; }
  hs_z0 = sec_floor[s] + 2; hs_z1 = sec_floor[player_sector] + 3;
  hs_x0 = mobj_x[m]; hs_y0 = mobj_y[m]; hs_x1 = playerx >> 8; hs_y1 = playery >> 8;
  hs_status = HS_PENDING;
  return 0;
}

/* tile march from monster to player; portal heights checked at the ray z */
void hitscan_process(void) {
  u8 mx, my, ex, ey, cur, nxt, steps, z, zd, zerr, count;
  s8 xs, ys, zs;
  u8 adx, ady;
  u32 bx, by;
  if (hs_status != HS_PENDING) return;
  mx = hs_x0 >> 3; my = hs_y0 >> 3; ex = hs_x1 >> 3; ey = hs_y1 >> 3;
  if (mx == ex && my == ey) { hs_status = HS_CLEAR; return; }
  xs = hs_x1 > hs_x0 ? 1 : hs_x1 < hs_x0 ? -1 : 0;
  ys = hs_y1 > hs_y0 ? 1 : hs_y1 < hs_y0 ? -1 : 0;
  adx = xs > 0 ? hs_x1 - hs_x0 : hs_x0 - hs_x1;
  ady = ys > 0 ? hs_y1 - hs_y0 : hs_y0 - hs_y1;
  /* distance to the first boundary in 1/256 tile */
  bx = xs > 0 ? 256 - ((hs_x0 & 7) << 5) : ((hs_x0 & 7) << 5);
  by = ys > 0 ? 256 - ((hs_y0 & 7) << 5) : ((hs_y0 & 7) << 5);
  steps = (ex > mx ? ex - mx : mx - ex) + (ey > my ? ey - my : my - ey);
  if (!steps) steps = 1;
  z = hs_z0;
  if (hs_z1 >= hs_z0) { zd = hs_z1 - hs_z0; zs = 1; } else { zd = hs_z0 - hs_z1; zs = -1; }
  zerr = 0; count = 32;
  cur = map_id(mx, my);
  while (!(mx == ex && my == ey)) {
    if (!count--) { hs_status = HS_BLOCKED; return; }
    /* nearer crossing: bx/|dx| vs by/|dy| (tie -> Y) */
    if (xs && (!ys || bx * ady < by * adx)) {
      mx += xs; bx += 256;
      if (mx >= 32) { hs_status = HS_BLOCKED; return; }
    } else {
      my += ys; by += 256;
      if (my >= 32) { hs_status = HS_BLOCKED; return; }
    }
    zerr += zd;
    while (zerr >= steps) {
      zerr -= steps; z += zs;
      if (zs > 0 ? z > hs_z1 : z < hs_z1) z = hs_z1;
    }
    nxt = map_id(mx, my);
    if (!nxt) { hs_status = HS_BLOCKED; return; }
    if (nxt != cur) {
      u8 mf = sec_floor[cur] > sec_floor[nxt] ? sec_floor[cur] : sec_floor[nxt];
      u8 mc = sec_ceil[cur] < sec_ceil[nxt] ? sec_ceil[cur] : sec_ceil[nxt];
      if (mf >= z || mc <= z) { hs_status = HS_BLOCKED; return; }
      cur = nxt;
    } else if (sec_floor[cur] >= z || sec_ceil[cur] <= z) { hs_status = HS_BLOCKED; return; }
  }
  hs_status = HS_CLEAR;
}

/* ---------------------------------------------------------------- setup */
void enemy_reset(void) {
  u8 i;
  for (i = 0; i < MAX_MOBJ; i++) { mobj_alloc[i] = 0; mobj_corpse[i] = 0xff; }
  for (i = 0; i < FX_MAX; i++) fx_kind[i] = 0;
  anim_frame = anim_dt_acc = react_dt_rem = new_chase_dir_frame = pain_boost = barrel_events = 0;
  hitscan_reset();
}

static void goto_chase(void) { mobj_state[ea] = chase_state[einfo]; }
static void calc_dist(void) {
  edist = approx_dist((s8)((playerx >> 8) - mobj_x[ea]), (s8)((playery >> 8) - mobj_y[ea]));
}
static u8 check_sight(void) {
  if (esec == player_sector) return 1;
  if (sec_seen[esec] == seen_gen) return 1;
  return edist < 3;
}
static u8 check_melee_range(void) { return edist < MELEERANGE && check_sight(); }
static u8 check_missile_range(void) {
  s16 d;
  if (mobj_react[ea] || !check_sight()) return 0;
  d = (edist < 13 ? edist : 12) * 16 - 64;
  if (d < 0) return 1;
  if (melee_state[einfo] == 0xff) { d -= 128; if (d < 0) return 1; }
  if (d > 200) d = 200;
  return rnd() >= (u8)d;
}

/* drop >= 3 also blocks monsters */
static u8 enemy_tile_blocked(u8 id) {
  if (tile_blocked(id)) return 1;
  if (old_floor > sec_floor[id] && old_floor - sec_floor[id] >= 3) return 1;
  return 0;
}
static void set_pos(u8 m, u16 x, u16 y) { mobj_x[m] = x >> 8; mobj_xfrac[m] = x; mobj_y[m] = y >> 8; mobj_yfrac[m] = y; }

static void enemy_push_walls(void) {
  u8 m = ea, lx = mobj_x[m] & 7, ly = mobj_y[m] & 7, mx, my, s;
  u16 x = (mobj_x[m] << 8) | mobj_xfrac[m], y = (mobj_y[m] << 8) | mobj_yfrac[m];
  if (!(lx < 2 || lx >= 6 || (lx == 5 && mobj_xfrac[m] >= 0x81) || ly < 2 || ly >= 6 || (ly == 5 && mobj_yfrac[m] >= 0x81))) return;
  mx = mobj_x[m] >> 3; my = mobj_y[m] >> 3;
  s = map_id(mx, my); old_floor = s ? sec_floor[s] : 0; old_ceil = s ? sec_ceil[s] : 0;
  if (enemy_tile_blocked(sector_at(mx - 1, my)) && ((x >> 8) & 7) < 2) x = (((mx << 3) | 2) << 8) | 0x80;
  if (enemy_tile_blocked(sector_at(mx + 1, my))) {
    u8 l = (x >> 8) & 7;
    if (l >= 6 || (l == 5 && (x & 255) >= 0x81)) x = (((mx << 3) | 5) << 8) | 0x80;
  }
  if (enemy_tile_blocked(sector_at(mx, my - 1)) && ((y >> 8) & 7) < 2) y = (((my << 3) | 2) << 8) | 0x80;
  if (enemy_tile_blocked(sector_at(mx, my + 1))) {
    u8 l = (y >> 8) & 7;
    if (l >= 6 || (l == 5 && (y & 255) >= 0x81)) y = (((my << 3) | 5) << 8) | 0x80;
  }
  set_pos(m, x, y);
}

/* returns 1 if moved */
static u8 try_move(void) {
  u8 m = ea, ox = mobj_x[m] >> 3, oy = mobj_y[m] >> 3, nx, ny, s;
  u16 sx = (mobj_x[m] << 8) | mobj_xfrac[m], sy = (mobj_y[m] << 8) | mobj_yfrac[m];
  s = map_id(ox, oy); old_floor = s ? sec_floor[s] : 0; old_ceil = s ? sec_ceil[s] : 0;
  set_pos(m, sx + wish_x, sy + wish_y);
  nx = mobj_x[m] >> 3; ny = mobj_y[m] >> 3;
  if ((nx != ox && ny != oy &&
       (enemy_tile_blocked(sector_at(nx, oy)) || enemy_tile_blocked(sector_at(ox, ny)))) ||
      enemy_tile_blocked(sector_at(nx, ny))) {
    set_pos(m, sx, sy); enemy_push_walls(); return 0;
  }
  enemy_push_walls(); return 1;
}

static s16 scale_tryd(s8 v) {
  u16 a = (u16)(v < 0 ? -v : v);
  if (a == 127) a = 128;
  a = (a * dt_ms) >> 6;
  return v < 0 ? -(s16)a : (s16)a;
}

static u8 p_move(void) {
  u8 d = mobj_movedir[ea], sp;
  if (d == DI_NODIR) return 0;
  if (edist < MELEERANGE) return 1;
  if (melee_state[mobj_info[ea]] == 0xff && edist < STANDOFF) return 1;
  sp = mobj_info[ea] ? 1 : 0;
  wish_x = scale_tryd(tryd_x[sp][d]); wish_y = scale_tryd(tryd_y[sp][d]);
  return try_move();
}

static u8 mobj_countdown(void) {
  u8 c = mobj_movecnt[ea];
  if (!c) return 0;
  if (c <= react_dt_units) { mobj_movecnt[ea] = 0; return 0; }
  mobj_movecnt[ea] = c - react_dt_units;
  return 1;
}
static u8 roll_movecnt(void) { u8 v = (rnd() & 15) << 3; return v ? v : NEWDIR_RETRY; }
static u8 try_walk(void) { if (!p_move()) return 0; mobj_movecnt[ea] = roll_movecnt(); return 1; }

static void new_chase_dir(void) {
  u8 olddir, turnaround, d1, d2, nd;
  s8 dx, dy;
  if (new_chase_dir_frame) { if (!mobj_movecnt[ea]) mobj_movecnt[ea] = NEWDIR_RETRY; return; }
  new_chase_dir_frame = 1;
  olddir = mobj_movedir[ea]; turnaround = opposite_dir[olddir];
  dx = (playerx >> 8) - mobj_x[ea]; dy = (playery >> 8) - mobj_y[ea];
  d1 = dx > 0 ? 0 : (dx < -1 ? 4 : DI_NODIR);
  d2 = dy > 0 ? 2 : (dy < -1 ? 6 : DI_NODIR);
  nd = DI_NODIR;
  if (d1 != DI_NODIR && d2 != DI_NODIR) {
    u8 i = (dy < 0 ? 2 : 0) + (dx >= 0 ? 1 : 0);
    if (diags_dir[i] != turnaround) nd = diags_dir[i];
  }
  if (nd == DI_NODIR) {
    if (d1 == turnaround) d1 = DI_NODIR;
    if (d2 == turnaround) d2 = DI_NODIR;
    if (d1 != DI_NODIR) nd = d1; else if (d2 != DI_NODIR) nd = d2; else nd = rnd() & 7;
  }
  mobj_movedir[ea] = nd;
  if (try_walk()) return;
  mobj_movedir[ea] = rnd() & 7;
  mobj_movecnt[ea] = roll_movecnt();
}

static void start_attack(u8 st) {
  mobj_state[ea] = st; mobj_movecnt[ea] = ATK_WINDUP;
  mobj_flags[ea] |= MF_JUSTATTACKED | MF_ATK_WINDUP;
}

static void a_chase(void) {
  calc_dist();
  if (mobj_react[ea]) mobj_react[ea] = mobj_react[ea] > react_dt_units ? mobj_react[ea] - react_dt_units : 0;
  if (mobj_flags[ea] & MF_JUSTATTACKED) { mobj_flags[ea] &= ~MF_JUSTATTACKED; new_chase_dir(); return; }
  if (mobj_react[ea]) goto move;
  if (melee_state[einfo] != 0xff && check_melee_range()) { start_attack(melee_state[einfo]); return; }
  if (shoot_state[einfo] == 0xff || mobj_movecnt[ea]) goto move;
  if (einfo == MOBJINFO_POS) {
    if (hs_actor == ea) {
      if (hs_status == HS_PENDING) goto walk;
      if (hs_status == HS_CLEAR) { start_attack(shoot_state[einfo]); return; }
      if (hs_status == HS_BLOCKED) { hitscan_release(); goto move; }
    }
    if (!check_missile_range()) goto move;
    if (hitscan_request(ea)) goto walk;
    if (hs_status == HS_CLEAR) { start_attack(shoot_state[einfo]); return; }
    goto walk;
  }
  if (!check_missile_range()) goto move;
  start_attack(shoot_state[einfo]);
  return;
move:
  if (!mobj_movecnt[ea]) goto newdir;
  mobj_countdown();
walk:
  if (p_move() || mobj_movecnt[ea]) goto snd;
newdir:
  new_chase_dir();
snd:
  if (rnd() < 3) play_sound(SND_GURGLE);
}

static void a_flinch(void) {
  if (!mobj_health[ea]) { mobj_movecnt[ea] = FALL_MOVECNT; mobj_state[ea] = death_state[einfo]; return; }
  if (!mobj_movecnt[ea]) { goto_chase(); return; }
  mobj_countdown();
}

static u8 attack_fire_now(void) {
  if (!mobj_movecnt[ea]) { goto_chase(); return 0; }
  if (mobj_countdown()) return 0;
  if (!(mobj_flags[ea] & MF_ATK_WINDUP)) { goto_chase(); return 0; }
  mobj_flags[ea] &= ~MF_ATK_WINDUP;
  return 1;
}
static void attack_recover(void) { mobj_movecnt[ea] = ATK_MOVECNT; mobj_react[ea] = ATK_REACT; }

static void a_shoot(void) {
  u8 d;
  if (!attack_fire_now()) return;
  play_sound(SND_PISTOL);
  calc_dist();
  d = edist < 29 ? edist : 28;
  if ((rnd() & 31) > d) { u8 v = (rnd() & 3) + 2; damage_player(v * 3); }
  hitscan_release();
  attack_recover();
}
static void a_melee(void) {
  u8 v;
  if (!attack_fire_now()) return;
  play_sound(SND_CLAW);
  v = (rnd() & 7) + 1; damage_player(v * 3);
  attack_recover();
}

/* ---------------------------------------------------------------- projectiles */
static s16 scale_mom(s8 a, u16 span) {
  u16 q = ((u16)(a < 0 ? -a : a) << 6) / span;
  if (q > 255) q = 255;
  return a < 0 ? -(s16)(q << 8) : (s16)(q << 8);
}
static void spawn_enemy_missile(void) {
  u8 m = MOBJ_MISSILE, span, adz;
  s8 dx, dy, dz;
  missile_z = sec_floor[sector_at(mobj_x[ea] >> 3, mobj_y[ea] >> 3)] + 2; missile_zfrac = 0;
  dx = (playerx >> 8) - mobj_x[ea]; dy = (playery >> 8) - mobj_y[ea];
  span = approx_dist(dx, dy); if (!span) span = 1;
  dz = eyeheight - missile_z; adz = dz < 0 ? -dz : dz;
  if (adz >= span) span = adz;
  missile_momx = scale_mom(dx, span); missile_momy = scale_mom(dy, span); missile_momz = scale_mom(dz, span);
  mobj_x[m] = mobj_x[ea]; mobj_y[m] = mobj_y[ea]; mobj_alloc[m] = 1;
  mobj_info[m] = MOBJINFO_IMPSHOT; mobj_state[m] = 18; mobj_xfrac[m] = 0; mobj_yfrac[m] = 0;
  mobj_flags[m] = einfo == MOBJINFO_BARON ? MF_PLASMA : 0;
  mobj_corpse[m] = 0xff;
  missile_life = 16000;
  mobj_health[m] = einfo == MOBJINFO_IMP ? 10 : 30;
}
static void a_missile(void) {
  if (!attack_fire_now()) return;
  if (!mobj_alloc[MOBJ_MISSILE]) spawn_enemy_missile();
  attack_recover();
}

/* (mom_h * vel_ms) / 16 as 8.8 world delta (proj_scale_vel) */
static s16 proj_vel(s16 mom, u8 vel) {
  s8 h = mom >> 8;
  u16 a = (u16)(h < 0 ? -h : h) * vel;
  a = (a >> 5) << 1;
  return h < 0 ? -(s16)a : (s16)a;
}

u8 spawn_player_rocket(void) {
  u8 m = MOBJ_PLAYER_ROCKET;
  u16 x, y;
  if (mobj_alloc[m]) return 1;
  procket_z = eyeheight - 1; procket_zfrac = 0;
  procket_momx = (s16)sintab[playera] << 8; procket_momy = (s16)(-costab[playera]) << 8;
  mobj_alloc[m] = 1; mobj_info[m] = MOBJINFO_IMPSHOT; mobj_state[m] = 18; mobj_flags[m] = 0;
  mobj_corpse[m] = 0xff; procket_life = 16000;
  x = playerx + proj_vel(procket_momx, 25); y = playery + proj_vel(procket_momy, 25);
  set_pos(m, x, y);
  return 0;
}

static u8 missile_tile_blocked(u8 id) { return !id || (u8)(sec_ceil[id] - sec_floor[id]) < 4 || sec_ceil[id] < sec_floor[id]; }

static void missile_push_walls(void) {
  u8 m = ea, mx = mobj_x[m] >> 3, my = mobj_y[m] >> 3;
  u16 x = (mobj_x[m] << 8) | mobj_xfrac[m], y = (mobj_y[m] << 8) | mobj_yfrac[m];
  if (missile_tile_blocked(sector_at(mx - 1, my)) && ((x >> 8) & 7) < 2) x = (((mx << 3) | 2) << 8) | 0x80;
  if (missile_tile_blocked(sector_at(mx + 1, my))) { u8 l = (x >> 8) & 7; if (l >= 6 || (l == 5 && (x & 255) >= 0x81)) x = (((mx << 3) | 5) << 8) | 0x80; }
  if (missile_tile_blocked(sector_at(mx, my - 1)) && ((y >> 8) & 7) < 2) y = (((my << 3) | 2) << 8) | 0x80;
  if (missile_tile_blocked(sector_at(mx, my + 1))) { u8 l = (y >> 8) & 7; if (l >= 6 || (l == 5 && (y & 255) >= 0x81)) y = (((my << 3) | 5) << 8) | 0x80; }
  set_pos(m, x, y);
}
static u8 missile_try_move(void) {
  u8 m = ea, ox = mobj_x[m] >> 3, oy = mobj_y[m] >> 3, nx, ny;
  u16 sx = (mobj_x[m] << 8) | mobj_xfrac[m], sy = (mobj_y[m] << 8) | mobj_yfrac[m];
  set_pos(m, sx + wish_x, sy + wish_y);
  nx = mobj_x[m] >> 3; ny = mobj_y[m] >> 3;
  if ((nx != ox && ny != oy && (missile_tile_blocked(sector_at(nx, oy)) || missile_tile_blocked(sector_at(ox, ny)))) ||
      missile_tile_blocked(sector_at(nx, ny))) { set_pos(m, sx, sy); return 0; }
  return 1;
}

static u8 procket_try_hit(void) {
  u8 i, rx = mobj_x[MOBJ_PLAYER_ROCKET], ry = mobj_y[MOBJ_PLAYER_ROCKET];
  for (i = 0; i < MOBJ_PLAYER_ROCKET; i++) {
    u8 s; s8 dz;
    if (!mobj_alloc[i] || mobj_info[i] >= MOBJINFO_IMPSHOT) continue;
    if (approx_dist(mobj_x[i] - rx, mobj_y[i] - ry) >= 3) continue;
    s = map_id(mobj_x[i] >> 3, mobj_y[i] >> 3);
    if (!s) continue;
    dz = sec_floor[s] + 2 - procket_z;
    if (dz < 0) dz = -dz;
    if (dz < 3) return 1;
  }
  return 0;
}

static void a_fly(void) {
  u8 rocket = ea == MOBJ_PLAYER_ROCKET, rem, step, s;
  u16 *life = rocket ? &procket_life : &missile_life;
  if (*life < dt_ms) goto impact;
  *life -= dt_ms;
  rem = dt_ms ? dt_ms : 1;
  while (rem) {
    step = rem > 188 ? 188 : rem;
    if (!rocket) {
      s8 dz;
      calc_dist();
      dz = missile_z - eyeheight; if (dz < 0) dz = -dz;
      if (edist < 3 && dz < 3) {
        damage_player(mobj_health[ea] + (rnd() & 15));
        mobj_alloc[MOBJ_MISSILE] = 0; return;
      }
    } else if (procket_try_hit()) goto impact;
    wish_x = proj_vel(rocket ? procket_momx : missile_momx, step);
    wish_y = proj_vel(rocket ? procket_momy : missile_momy, step);
    if (!missile_try_move()) goto impact;
    {
      u8 *z = rocket ? &procket_z : &missile_z, *zf = rocket ? &procket_zfrac : &missile_zfrac;
      if (!rocket) {
        u16 zz = ((u16)*z << 8 | *zf) + proj_vel(missile_momz, step);
        *z = zz >> 8; *zf = zz;
      }
      s = sector_at(mobj_x[ea] >> 3, mobj_y[ea] >> 3);
      if (!s || sec_floor[s] >= *z || sec_ceil[s] <= *z) goto impact;
    }
    rem -= step;
  }
  return;
impact:
  missile_push_walls();
  if (!rocket) {
    calc_dist();
    if (edist < 4) damage_player(mobj_health[ea] + (rnd() & 15));
    mobj_alloc[MOBJ_MISSILE] = 0;
    return;
  }
  mobj_alloc[MOBJ_PLAYER_ROCKET] = 0;
  explode_tile(mobj_x[MOBJ_PLAYER_ROCKET] >> 3, mobj_y[MOBJ_PLAYER_ROCKET] >> 3);
}

static void a_fall(void) {
  if (mobj_movecnt[ea]) { mobj_countdown(); return; }
  if (mobj_info[ea] == MOBJINFO_BARON) mobj_corpse[ea] = IT_BARONCORPSE;
  else if (mobj_info[ea] < MOBJINFO_BARON) mobj_corpse[ea] = IT_POSCORPSE + mobj_info[ea];
  else mobj_corpse[ea] = state_texture[mobj_state[ea]] & 0xbf;
}

/* ---------------------------------------------------------------- think */
void enemy_think(void) {
  u8 m;
  u16 t;
  new_chase_dir_frame = 0;
  t = react_dt_rem + dt_ms;
  react_dt_units = t >> 4; react_dt_rem = t & 15;
  t = anim_dt_acc + dt_ms;
  if (t >= 250) { t -= 250; anim_frame++; }
  anim_dt_acc = t;
  hs_claimed = 0;
  player_sector = plr_id;
  for (m = 0; m < MAX_MOBJ; m++) {
    u8 act;
    if (!mobj_alloc[m]) continue;
    ea = m; einfo = mobj_info[m];
    act = state_action[mobj_state[m]];
    if (einfo != MOBJINFO_IMPSHOT) {
      if (!mobj_health[m]) {
        if (mobj_corpse[m] != 0xff || act != A_FALL) continue;
      } else if (act == A_CHASE) {
        esec = sector_at(mobj_x[m] >> 3, mobj_y[m] >> 3);
        if (!esec || sec_seen[esec] != seen_gen) continue;
      }
    }
    esec = sector_at(mobj_x[m] >> 3, mobj_y[m] >> 3);
    switch (act) {
    case A_CHASE: a_chase(); break;
    case A_FLINCH: a_flinch(); break;
    case A_MELEE: a_melee(); break;
    case A_SHOOT: a_shoot(); break;
    case A_MISSILE: a_missile(); break;
    case A_FALL: a_fall(); break;
    default: a_fly(); break;
    }
  }
}

/* ---------------------------------------------------------------- damage */
void enemy_damage(u8 m, u8 dmg) {
  if (difficulty == 0) { u16 v = dmg + (dmg >> 1); dmg = v > 255 ? 255 : (u8)v; }
  if (!mobj_alloc[m] || !mobj_health[m] || mobj_info[m] >= MOBJINFO_IMPSHOT) return;
  ea = m; einfo = mobj_info[m];
  if (mobj_health[m] > dmg) {
    u16 p;
    mobj_health[m] -= dmg;
    mobj_flags[m] |= MF_JUSTATTACKED;
    p = dmg + pain_boost;
    if (p <= 255 && (u8)p <= pain_chance[einfo]) return;
    if (pain_chance[einfo] == 0xff) return;
    mobj_state[m] = pain_state[einfo]; mobj_movecnt[m] = PAIN_MOVECNT;
    return;
  }
  mobj_health[m] = 0; mobj_movecnt[m] = FALL_MOVECNT;
  num_kills++;
  play_sound(death_sound[einfo]);
  mobj_state[m] = death_state[einfo];
  if (einfo == MOBJINFO_BARON && level_num == 8) {
    u8 i;
    for (i = 0; i < MAX_MOBJ; i++)
      if (mobj_alloc[i] && mobj_info[i] == MOBJINFO_BARON && mobj_health[i]) return;
    boss_lower_forever_floors();
  }
}

static void damage_vis(u8 vi, u8 dmg) {
  u8 slot = vis_slot[vi];
  if (!(slot & (VIS_LAYER | VIS_FX))) {
    if (mobj_health[slot] && mobj_info[slot] < MOBJINFO_IMPSHOT) enemy_damage(slot, dmg);
    return;
  }
  if (slot & VIS_FX) return;
  slot &= 0x7f;
  if (slot < n_items && item_t[slot] == IT_BARREL) explode_tile(item_x[slot], item_y[slot]);
}

/* nearest aimed target on MUZZLE +/- 2; maxz = 0xff any range */
void try_damage_enemy(u8 dmg, u8 maxz) {
  u8 c, bz = 0xff, bs = 0xff;
  for (c = MUZZLE_COL - AIM_COL_SLACK; c <= MUZZLE_COL + AIM_COL_SLACK; c++) {
    if (col_aim_slot[c] == 0xff) continue;
    if (col_aim_z[c] < bz) { bz = col_aim_z[c]; bs = col_aim_slot[c]; }
  }
  if (bs == 0xff) return;
  if (maxz != 0xff && bz >= maxz) return;
  damage_vis(bs, dmg);
}
void try_damage_at_col(u8 dmg, u8 c) {
  if (col_aim_slot[c] != 0xff) damage_vis(col_aim_slot[c], dmg);
}

/* ---------------------------------------------------------------- barrels / explosions */
static s8 fx_find(u8 tx, u8 ty) {
  u8 i;
  for (i = 0; i < FX_MAX; i++) if (fx_kind[i] && fx_tx[i] == tx && fx_ty[i] == ty) return i;
  return -1;
}
static s8 fx_alloc(void) { u8 i; for (i = 0; i < FX_MAX; i++) if (!fx_kind[i]) return i; return -1; }
static void fx_fuse_at(u8 tx, u8 ty) {
  s8 i;
  if (fx_find(tx, ty) >= 0) return;
  i = fx_alloc(); if (i < 0) return;
  fx_kind[i] = FX_FUSE; fx_tx[i] = tx; fx_ty[i] = ty; fx_time[i] = 12; barrel_events++;
}
void explode_tile(u8 tx, u8 ty) {
  s8 i = item_find(tx, ty), f;
  u8 cx = (tx << 3) + 4, cy = (ty << 3) + 4, m, x, y;
  if (i >= 0 && item_t[i] == IT_BARREL) item_remove(i);
  f = fx_find(tx, ty); if (f < 0) f = fx_alloc();
  if (f >= 0) { fx_kind[f] = FX_EXPL; fx_tx[f] = tx; fx_ty[f] = ty; fx_time[f] = 32; barrel_events++; }
  play_sound(SND_BAREXP);
  for (m = 0; m < MOBJ_PLAYER_ROCKET; m++) {
    u8 ax, ay;
    if (!mobj_alloc[m] || mobj_info[m] >= MOBJINFO_IMPSHOT) continue;
    ax = mobj_x[m] > cx ? mobj_x[m] - cx : cx - mobj_x[m];
    ay = mobj_y[m] > cy ? mobj_y[m] - cy : cy - mobj_y[m];
    if (ax > 8 || ay > 8) continue;
    enemy_damage(m, (rnd() & 31) + 16);
  }
  for (y = ty - 1; y != (u8)(ty + 2); y++) for (x = tx - 1; x != (u8)(tx + 2); x++) {
    s8 j;
    if (x >= 32 || y >= 32 || (x == tx && y == ty)) continue;
    j = item_find(x, y);
    if (j >= 0 && item_t[j] == IT_BARREL) fx_fuse_at(x, y);
  }
}
static u8 fx_countdown(u8 i) {
  if (!fx_time[i]) return 0;
  if (fx_time[i] <= react_dt_units) { fx_time[i] = 0; return 0; }
  fx_time[i] -= react_dt_units; return 1;
}
void barrel_update(void) {
  u8 i;
  if (!barrel_events) return;
  for (i = 0; i < FX_MAX; i++) {
    if (fx_kind[i] == FX_EXPL) {
      if (!fx_time[i]) { fx_kind[i] = 0; barrel_events--; }
      else fx_countdown(i);
    } else if (fx_kind[i] == FX_FUSE) {
      if (fx_countdown(i)) continue;
      fx_kind[i] = 0; barrel_events--;
      explode_tile(fx_tx[i], fx_ty[i]);
    }
  }
}
