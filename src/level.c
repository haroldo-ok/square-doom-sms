/* [logic bank] Level start, sector specials, door/lift/stairs processes, triggers
 * (level.asm, process.asm, util.asm). */
#include "defs.h"

u8 key_use, key_use_was;
u8 elev_mode, elev_dest, tmp_sec;
static u8 elev_reclose, elev_fcol, elev_faction, elev_chain, elev_found, trig_sec, boss_floors_done;

#define PROC_NUM 8
enum { PROC_FREE, PROC_TIMER, PROC_RAISE_CEIL, PROC_LOWER_CEIL, PROC_RAISE_FLOOR, PROC_LOWER_FLOOR };
static u8 proc_kind[PROC_NUM], proc_a[PROC_NUM], proc_b[PROC_NUM], proc_e[PROC_NUM];
static u16 proc_c[PROC_NUM];
#define DOOR_OPEN_GAP 5
#define MOTION_STEP_MS 128
#define CLOSET_RAISE 6

static const u8 tu_dx[4] = {0, 1, 0, 0xff};
static const u8 tu_dy[4] = {0xff, 0, 1, 0};
static const u8 bright_to_wdark[17] = {15,14,13,12,11,10,9,8,7,6,5,4,3,2,1,0,0xff};

static u8 sec_trigger(u8 s) { return (sec_type[s] & TRIG_MASK) >> 5; }
static void sec_oneshot_if(u8 s) { if (sec_type[s] & SHOT_BIT) sec_type[s] &= 0x9f; }

void update_eye(void) {
  eyeheight = player_prev_sec ? sec_floor[player_prev_sec] + 3 : 11;
  if (!health) eyeheight -= 2;
}

static u8 flash_sec[2], flash_base[2], flash_lit;
static u16 flash_ms, dmg_ms;

void start_level(void) {
  u8 i, s;
  for (i = 0; i < PROC_NUM; i++) proc_kind[i] = PROC_FREE;
  boss_floors_done = 0;
  playerx = spawn_xya[0] << 8; playery = spawn_xya[1] << 8; playera = spawn_xya[2];
  num_kills = num_items_got = num_secrets_got = 0;
  map_time_ms = 0; map_time_sec = 0;
  init_hud_state();
  last_playera = playera ^ 0x80;
  clear_sector_seen();
  seen_gen = 0;
  flash_sec[0] = flash_sec[1] = 0; flash_lit = 0; flash_ms = 0; dmg_ms = 0; radsuit_ms = 0;
  s = 0;
  for (i = 0; i < SEC_N; i++) {
    sec_wdark[i] = bright_to_wdark[sec_bright[i] > 16 ? 16 : sec_bright[i]];
    sec_visited[i] = 0;
    if (i && i <= sector_max && sec_action(i) == ACT_FLASH_LIGHTS && s < 2) {
      flash_sec[s] = i; flash_base[s] = sec_bright[i]; s++;
    }
  }
  sec_wdark[0] = 0;
  player_prev_sec = sector_at(playerx >> 11, playery >> 11);
  plr_id = player_sector = player_prev_sec;
  update_eye();
}

/* ---------------------------------------------------------------- processes */
static u8 proc_alloc(u8 kind, u8 a, u8 b, u16 c) {
  u8 i;
  for (i = 0; i < PROC_NUM; i++) if (proc_kind[i] == PROC_FREE) {
    proc_kind[i] = kind; proc_a[i] = a; proc_b[i] = b; proc_c[i] = c; proc_e[i] = 0;
    return i;
  }
  return 0xff;
}
static u8 proc_sector_busy(u8 s) {
  u8 i;
  for (i = 0; i < PROC_NUM; i++) if (proc_kind[i] && proc_a[i] == s) return 1;
  return 0;
}
static u8 proc_count_free(void) {
  u8 i, n = 0;
  for (i = 0; i < PROC_NUM; i++) if (!proc_kind[i]) n++;
  return n;
}
static u8 accum(u8 i) {
  proc_c[i] += dt_ms;
  if (proc_c[i] >= MOTION_STEP_MS) { proc_c[i] -= MOTION_STEP_MS; return 1; }
  return 0;
}
void proc_update(void) {
  u8 i, s;
  for (i = 0; i < PROC_NUM; i++) {
    s = proc_a[i];
    switch (proc_kind[i]) {
    case PROC_TIMER:
      if (proc_c[i] > dt_ms) { proc_c[i] -= dt_ms; break; }
      {
        u8 k = proc_b[i], b = 0;
        if (k == PROC_LOWER_CEIL) play_sound(SND_DORCLS);
        if (k == PROC_RAISE_FLOOR || k == PROC_LOWER_FLOOR) b = proc_e[i];
        proc_kind[i] = PROC_FREE;
        proc_alloc(k, s, b, 0);
      }
      break;
    case PROC_RAISE_CEIL:
      if (!accum(i)) break;
      if (sec_ceil[s] >= proc_b[i]) { proc_kind[i] = 0; break; }
      if (++sec_ceil[s] >= proc_b[i]) proc_kind[i] = 0;
      break;
    case PROC_LOWER_CEIL:
      if (sector_at(playerx >> 11, playery >> 11) == s) break;   /* never crush the player */
      if (!accum(i)) break;
      if (sec_ceil[s] <= sec_floor[s]) { proc_kind[i] = 0; break; }
      if (--sec_ceil[s] == sec_floor[s]) proc_kind[i] = 0;
      break;
    case PROC_RAISE_FLOOR:
      if (!accum(i)) break;
      if (sec_floor[s] >= proc_b[i]) { proc_kind[i] = 0; break; }
      if (++sec_floor[s] >= proc_b[i]) proc_kind[i] = 0;
      break;
    case PROC_LOWER_FLOOR:
      if (!accum(i)) break;
      if (sec_floor[s] <= proc_b[i]) { proc_kind[i] = 0; break; }
      if (--sec_floor[s] == proc_b[i]) proc_kind[i] = 0;
      break;
    }
  }
}

/* ---------------------------------------------------------------- doors */
static const u8 door_key_cols[3] = {2, 7, 6};
static const u16 door_reclose[4] = {0, 5000, 10000, 30000};
static const char * const door_msgs[3] = {
  "you need a red key to open this door",
  "you need a yellow key to open this door",
  "you need a blue key to open this door" };

static void door_open_activate(u8 s) {
  u8 k, dest;
  for (k = 0; k < 3; k++) if (sec_ccol[s] == door_key_cols[k]) {
    if (!(keys & (1 << k))) { info_kind = 1; info_show(door_msgs[k]); play_sound(SND_OOF); return; }
    break;
  }
  if (proc_sector_busy(s)) return;
  if (proc_count_free() < (elev_mode ? 2 : 1)) return;
  dest = sec_floor[s] + DOOR_OPEN_GAP;
  if (sec_ceil[s] >= dest) return;
  if (proc_alloc(PROC_RAISE_CEIL, s, dest, 0) == 0xff) return;
  play_sound(SND_DOROPN);
  if (elev_mode) proc_alloc(PROC_TIMER, s, PROC_LOWER_CEIL, door_reclose[elev_mode]);
}

static void closet_activate(u8 s) {
  if (proc_sector_busy(s) || proc_count_free() < 2) return;
  if (proc_alloc(PROC_RAISE_FLOOR, s, sec_floor[s] + CLOSET_RAISE, 0) == 0xff) return;
  if (proc_alloc(PROC_RAISE_CEIL, s, sec_ceil[s] + CLOSET_RAISE, 0) == 0xff) return;
  play_sound(SND_STNMOV);
}

static u8 elev_in_chain(u8 id) {
  u8 c = elev_chain;
  while (c) { if (c == id) return 1; c = sec_target[c]; }
  return 0;
}

/* min/max neighbouring floor across the same-tag chain (tmp_sec = head) -> elev_dest */
void elevator_find_dest(void) {
  u8 cur = tmp_sec, best, mx, my, d;
  elev_chain = tmp_sec;
  elev_fcol = sec_fcol[tmp_sec];
  elev_faction = sec_action(tmp_sec);
  elev_found = 0;
  best = elev_mode ? 0 : 0x1f;
  while (cur) {
    for (my = 0; my < MAP_SIZE; my++) for (mx = 0; mx < MAP_SIZE; mx++) {
      if (map_id(mx, my) != cur) continue;
      for (d = 0; d < 4; d++) {
        u8 nx = mx + tu_dx[d], ny = my + tu_dy[d], nb, f;
        if (nx >= MAP_SIZE || ny >= MAP_SIZE) continue;
        nb = map_id(nx, ny);
        if (!nb || elev_in_chain(nb)) continue;
        f = sec_floor[nb];
        if (elev_mode ? (f > best) : (f < best)) {
          best = f; elev_fcol = sec_fcol[nb]; elev_faction = sec_action(nb); elev_found = 1;
        }
      }
    }
    cur = sec_target[cur];
  }
  elev_dest = elev_found ? best : sec_floor[elev_chain];
}

void forever_adopt_nb(u8 s) {
  sec_fcol[s] = elev_fcol;
  sec_type[s] = (sec_type[s] & (TRIG_MASK | SHOT_BIT)) | elev_faction;
}

void floor_forever_activate(void) {
  u8 s = tmp_sec;
  if (proc_sector_busy(s) || sec_floor[s] == elev_dest) return;
  if (proc_alloc(elev_mode ? PROC_RAISE_FLOOR : PROC_LOWER_FLOOR, s, elev_dest, 0) == 0xff) return;
  play_sound(SND_STNMOV);
}

static void elevator_activate(u8 s) {
  u8 i, kind, ret, home;
  if (proc_sector_busy(s) || proc_count_free() < 2) return;
  if (sec_floor[s] == elev_dest) return;
  home = sec_floor[s];
  if (elev_mode) { kind = PROC_RAISE_FLOOR; ret = PROC_LOWER_FLOOR; }
  else { kind = PROC_LOWER_FLOOR; ret = PROC_RAISE_FLOOR; }
  if (proc_alloc(kind, s, elev_dest, 0) == 0xff) return;
  play_sound(SND_STNMOV);
  i = proc_alloc(PROC_TIMER, s, ret, elev_reclose ? 15000 : 5000);
  if (i != 0xff) proc_e[i] = home;
}

static void stairs_activate(u8 start) {
  u8 s, hops = 0, amount;
  s = sec_target[start];
  if (!s) return;
  while (s) {
    if (proc_sector_busy(s)) return;
    hops++;
    if (sec_action(s) != ACT_CONTINUE_STAIRS) break;
    s = sec_target[s];
  }
  if (proc_count_free() < hops) return;
  sec_oneshot_if(start);
  amount = 1;
  s = sec_target[start];
  for (;;) {
    u8 dest = sec_floor[s] + amount;
    if (dest > 31) dest = 31;
    if (sec_floor[s] < dest) proc_alloc(PROC_RAISE_FLOOR, s, dest, 0);
    if (sec_action(s) != ACT_CONTINUE_STAIRS || !sec_target[s]) break;
    amount++; s = sec_target[s];
  }
  play_sound(SND_STNMOV);
}

static void trigger_action(void) {
  u8 a = sec_action(trig_sec), s;
  switch (a) {
  case ACT_END_LEVEL: end_level = 1; play_sound(SND_STNMOV); break;
  case ACT_OPEN_DOOR: case ACT_OPEN_DOOR_FOREVER: case ACT_OPEN_DOOR_10S: case ACT_OPEN_DOOR_30S:
    elev_mode = a == ACT_OPEN_DOOR ? 1 : a == ACT_OPEN_DOOR_FOREVER ? 0 : a == ACT_OPEN_DOOR_10S ? 2 : 3;
    s = sec_target[trig_sec];
    if (!s) door_open_activate(trig_sec);
    else while (s) { door_open_activate(s); s = sec_target[s]; }
    break;
  case ACT_OPEN_MONSTER_CLOSET:
    s = sec_target[trig_sec];
    if (!s) closet_activate(trig_sec);
    else while (s) { closet_activate(s); s = sec_target[s]; }
    break;
  case ACT_LOWER_FLOOR: case ACT_LOWER_FLOOR_15S:
    elev_reclose = a == ACT_LOWER_FLOOR_15S;
    s = sec_target[trig_sec]; if (!s) s = trig_sec;
    elev_mode = 0; tmp_sec = s; elevator_find_dest();
    while (s) { elevator_activate(s); s = sec_target[s]; }
    break;
  case ACT_LOWER_FLOOR_FOREVER: case ACT_RAISE_FLOOR:
    s = sec_target[trig_sec]; if (!s) s = trig_sec;
    elev_mode = a == ACT_RAISE_FLOOR; tmp_sec = s; elevator_find_dest();
    while (s) { forever_adopt_nb(s); tmp_sec = s; floor_forever_activate(); s = sec_target[s]; }
    break;
  case ACT_RAISE_STAIRS: stairs_activate(trig_sec); return;
  case ACT_SECRET: num_secrets_got++; break;
  default: return;
  }
  sec_oneshot_if(trig_sec);
}

static u8 face_nesw(void) { return (u8)(playera + 32) >> 6; }
/* within half a tile of the faced edge (tu_near_lo / tu_near_hi) */
static u8 near_face(u8 f) {
  u16 v = (f & 1) ? playerx : playery;
  u8 loc = (v >> 8) & 7, fr = v & 255;
  if (f == 0 || f == 3) return loc < 4 || (loc == 4 && fr == 0);
  return loc >= 4;
}

void try_use(void) {
  u8 f, mx, my, a;
  if (!key_use) { key_use_was = 0; return; }
  if (key_use_was) return;
  key_use_was = 1;
  f = face_nesw();
  if (!near_face(f)) return;
  mx = (playerx >> 11) + tu_dx[f]; my = (playery >> 11) + tu_dy[f];
  if (mx >= MAP_SIZE || my >= MAP_SIZE) return;
  trig_sec = map_id(mx, my);
  if (!trig_sec || sec_trigger(trig_sec) != TRIG_USE) return;
  a = sec_action(trig_sec);
  if ((a == ACT_LOWER_FLOOR || a == ACT_LOWER_FLOOR_15S || a == ACT_LOWER_FLOOR_FOREVER ||
       a == ACT_RAISE_FLOOR) && sec_target[trig_sec]) return;
  trigger_action();
}

void try_walk_into(void) {
  u8 s = sector_at(playerx >> 11, playery >> 11), a;
  if (s == player_prev_sec) return;
  player_prev_sec = s;
  if (!s || sec_trigger(s) != TRIG_WALK) return;
  trig_sec = s;
  a = sec_action(s);
  if ((a == ACT_LOWER_FLOOR || a == ACT_LOWER_FLOOR_15S || a == ACT_OPEN_DOOR_10S ||
       a == ACT_OPEN_DOOR_30S || a == ACT_OPEN_DOOR_FOREVER) && !sec_target[s]) return;
  trigger_action();
}

void try_switch(void) {
  static const u8 solid_face[4] = {2, 3, 0, 1};
  u8 f, mx, my, i;
  if (!key_use) return;
  trig_sec = sector_at(playerx >> 11, playery >> 11);
  if (!trig_sec || sec_trigger(trig_sec) != TRIG_SWITCH) return;
  f = face_nesw();
  if (!near_face(f)) return;
  mx = (playerx >> 11) + tu_dx[f]; my = (playery >> 11) + tu_dy[f];
  if (mx >= MAP_SIZE || my >= MAP_SIZE || map_id(mx, my)) return;
  for (i = 0; i < sw_n; i++)
    if (sw_sec[i] == trig_sec && sw_dir[i] == solid_face[f]) { trigger_action(); return; }
}

/* ---------------------------------------------------------------- passive specials */
void sector_specials_update(void) {
  u8 i;
  if (player_prev_sec && sec_action(player_prev_sec) == ACT_DAMAGE_FLOOR && !radsuit_ms) {
    dmg_ms += dt_ms;
    if (dmg_ms >= 1000) { dmg_ms -= 1000; damage_player(5); }
  } else dmg_ms = 0;
  if (!(flash_sec[0] | flash_sec[1])) return;
  flash_ms += dt_ms;
  if (flash_ms < 1000) return;
  flash_ms -= 1000;
  flash_lit ^= 1;
  for (i = 0; i < 2; i++) {
    u8 s = flash_sec[i];
    if (!s) continue;
    sec_bright[s] = flash_lit ? 16 : flash_base[i];
    sec_wdark[s] = bright_to_wdark[sec_bright[s]];
  }
}

/* E1M8: when the last baron dies, lower every forever floor with trigger none */
void boss_lower_forever_floors(void) {
  u8 s;
  if (boss_floors_done) return;
  boss_floors_done = 1;
  for (s = 1; s <= sector_max; s++) {
    if (sec_trigger(s) != TRIG_NONE || sec_action(s) != ACT_LOWER_FLOOR_FOREVER) continue;
    elev_mode = 0; tmp_sec = s; elevator_find_dest();
    forever_adopt_nb(s); tmp_sec = s; floor_forever_activate();
  }
}
