/* Player input, movement/collision, pickups, damage, HUD, info line
 * (input.asm, player.asm, pickup.asm, hud.asm, levelstats.asm). */
#include "defs.h"

u16 playerx, playery;
u8 playera, eyeheight, player_prev_sec, player_sector, plr_id;
u8 health, armor, combat_armor, keys, owned_weapons, cur_weapon, has_backpack;
u8 ammo[3];
u8 god_mode, hurt_flash, hud_dirty;
u16 radsuit_ms;
u8 dt_ms;
u8 num_kills, num_items_got, num_secrets_got;
u16 map_time_sec, map_time_ms;
u8 info_kind, info_len; const char *info_name; u16 info_ms;
u8 old_floor, old_ceil;
static s16 wish_x, wish_y;
static u16 turn_acc;

const u8 wpn_ammo_idx[6] = {0, 0, 0, 1, 0, 2};
const u8 ammo_max_base[3] = {200, 50, 50};
const u8 ammo_max_pack[3] = {250, 100, 100};
const u8 wpn_own_bit[6] = {0, 1, 2, 4, 8, 16};

/* scale_vel: (|trig| * vel_ms * 8) >> 8 world 8.8 */
static void wish_add(u8 vel, s8 sx, s8 sy) {
  s16 vx = (s16)(((u16)(sx < 0 ? -sx : sx) * vel) >> 5);
  s16 vy = (s16)(((u16)(sy < 0 ? -sy : sy) * vel) >> 5);
  wish_x += sx < 0 ? -vx : vx;
  wish_y += sy < 0 ? -vy : vy;
}
/* 64 angle units per second of hold */
static u8 turn_deliver(u8 vel) {
  u8 d;
  turn_acc += (u16)vel * 64;
  d = turn_acc >> 10;
  turn_acc &= 0x3ff;
  return d;
}
static void cycle_weapon(s8 dir) {
  u8 w = cur_weapon, n;
  for (n = 0; n < 6; n++) {
    w = (u8)(w + 6 + dir) % 6;
    if (w == 0 || (owned_weapons & wpn_own_bit[w])) { switch_weapon(w); return; }
  }
}

void read_input(void) {
  u8 tl, tr, fw, bk, sl, sr, wn, wp;
  s8 turn = 0;
  wish_x = 0; wish_y = 0;
  __asm__("di");
  key_use = in_use; tl = in_turn_l; tr = in_turn_r; fw = in_fwd; bk = in_back;
  sl = in_strafel; sr = in_strafer; wn = in_wnext; wp = in_wprev;
  in_use = in_turn_l = in_turn_r = in_fwd = in_back = in_strafel = in_strafer = in_wnext = in_wprev = 0;
  __asm__("ei");
  if (!health) return;
  if (wn) cycle_weapon(1);
  if (wp) cycle_weapon(-1);
  if (tl > tr) turn = -(s8)turn_deliver(tl - tr);
  else if (tr > tl) turn = turn_deliver(tr - tl);
  if (sr) wish_add(sr, costab[playera], sintab[playera]);
  if (sl) wish_add(sl, -costab[playera], -sintab[playera]);
  if (bk) wish_add(bk, -sintab[playera], costab[playera]);
  if (fw) wish_add(fw, sintab[playera], -costab[playera]);
  playera += turn;
}

u8 tile_blocked(u8 id) {
  u8 mf, mc;
  if (!id) return 1;
  if (sec_action(id) == ACT_WINDOW) return 1;
  if (sec_ceil[id] < sec_floor[id] || sec_ceil[id] - sec_floor[id] < 4) return 1;
  if (sec_floor[id] > old_floor && sec_floor[id] - old_floor >= 3) return 1;
  mf = old_floor > sec_floor[id] ? old_floor : sec_floor[id];
  mc = old_ceil < sec_ceil[id] ? old_ceil : sec_ceil[id];
  if (mc < mf || mc - mf < 4) return 1;
  return 0;
}
static u8 standing_blocked(void) { return tile_blocked(sector_at(playerx >> 11, playery >> 11)); }

/* keep the player off blocked neighbours (local 1.0 .. 7.0 in the tile) */
static void push_walls(void) {
  u8 lx = (playerx >> 8) & 7, ly = (playery >> 8) & 7, mx, my;
  if (!(lx == 0 || (lx == 7 && (playerx & 255)) || ly == 0 || (ly == 7 && (playery & 255)))) return;
  mx = playerx >> 11; my = playery >> 11;
  if (tile_blocked(sector_at(mx - 1, my)) && ((playerx >> 8) & 7) == 0) playerx = (u16)((mx << 3) | 1) << 8;
  if (tile_blocked(sector_at(mx + 1, my)) && ((playerx >> 8) & 7) == 7 && (playerx & 255)) playerx = (u16)((mx << 3) | 7) << 8;
  if (tile_blocked(sector_at(mx, my - 1)) && ((playery >> 8) & 7) == 0) playery = (u16)((my << 3) | 1) << 8;
  if (tile_blocked(sector_at(mx, my + 1)) && ((playery >> 8) & 7) == 7 && (playery & 255)) playery = (u16)((my << 3) | 7) << 8;
}

void apply_move(void) {
  u16 sx, sy, nx, ny;
  u8 id;
  if (!wish_x && !wish_y) return;
  sx = playerx; sy = playery;
  id = sector_at(playerx >> 11, playery >> 11);
  old_floor = id ? sec_floor[id] : 0; old_ceil = id ? sec_ceil[id] : 0;
  nx = playerx + wish_x; ny = playery + wish_y;
  playerx = nx; playery = ny;
  push_walls(); if (!standing_blocked()) return;
  playerx = nx; playery = sy;
  push_walls(); if (!standing_blocked()) return;
  playerx = sx; playery = ny;
  push_walls(); if (!standing_blocked()) return;
  playerx = sx; playery = sy;
}

/* ---------------------------------------------------------------- info line */
static char info_buf[48];
void info_show(const char *s) {
  char *d = info_buf;
  if (s != info_buf) { u8 n = 0; while (*s && n < 47) { *d++ = *s++; n++; } *d = 0; }
  info_name = info_buf; info_len = 1; info_ms = 4000;
}
void update_info_msg(void) {
  if (!info_ms) return;
  if (info_ms > dt_ms) { info_ms -= dt_ms; return; }
  info_ms = 0; info_len = 0; info_kind = 0;
}
/* row 0 (and row 1 when the line is wider than the 32-column screen) */
void draw_info_msg(void) {
  const char *s;
  u8 n, x, y;
  if (!info_len) return;
  s = info_name;
  n = 0; while (s[n]) n++;
  x = 0; y = 0;
  if (n > VIEW_W) {
    u8 cut = VIEW_W;
    while (cut && s[cut] != ' ') cut--;
    while (x < cut) { fb[x] = char_entry(s[x]); x++; }
    s += cut + 1; x = 0; y = 1;
  }
  while (*s) { fb[y * VIEW_W + x] = char_entry(*s++); x++; }
}

static const char * const pickup_names[] = {
  "health", "shells", "shotgun", "chaingun", "chainsaw", "rocket launcher", "green armor",
  "blue armor", "backpack", "red keycard", "blue keycard", "yellow keycard", "soulsphere",
  "rad suit", "health bonus", "armor bonus", "clip", "shell box", "ammo box", "health crate",
  "rocket", "box of rockets" };

static void pickup_message(const char *name) {
  char *d = info_buf; const char *p = "picked up the ";
  play_sound(SND_ITEMUP);
  while (*p) *d++ = *p++;
  while (*name) *d++ = *name++;
  *d++ = '.'; *d = 0;
  info_kind = 0; hud_dirty = 1;
  info_show(info_buf);
}

void add_ammo(u8 pool, u8 amount) {
  u8 mx = has_backpack ? ammo_max_pack[pool] : ammo_max_base[pool];
  u16 v = ammo[pool] + amount;
  ammo[pool] = v > mx ? mx : (u8)v;
  hud_dirty = 1;
}
static u8 ammo_full(u8 pool) { return ammo[pool] >= (has_backpack ? ammo_max_pack[pool] : ammo_max_base[pool]); }

static u8 pickup_apply(u8 t) {
  switch (t) {
  case IT_HEALTH: if (health >= 100) return 0; health = health + 25 > 100 ? 100 : health + 25; break;
  case IT_SHELLS: if (ammo_full(1)) return 0; add_ammo(1, 10); break;
  case IT_GREENARMOR: if (armor >= 100) return 0; armor = 100; combat_armor = 0; break;
  case IT_BLUEARMOR: if (armor >= 200) return 0; armor = 200; combat_armor = 1; break;
  case IT_BACKPACK: has_backpack = 1; add_ammo(0, 10); add_ammo(1, 4); add_ammo(2, 1); break;
  case IT_REDCARD: keys |= 1; break;
  case IT_BLUECARD: keys |= 4; break;
  case IT_YELLOWCARD: keys |= 2; break;
  case IT_SOULSPHERE: if (health >= 200) return 0; health = health + 100 > 200 ? 200 : health + 100; break;
  case IT_RADSUIT: radsuit_ms = 60000; break;
  case IT_CHAINSAW: owned_weapons |= 1; switch_weapon(1); break;
  case IT_SHOTGUN: owned_weapons |= 4; add_ammo(1, 8); switch_weapon(3); break;
  case IT_CHAINGUN: owned_weapons |= 8; add_ammo(0, 20); switch_weapon(4); break;
  case IT_ROCKETLAUNCHER: owned_weapons |= 16; add_ammo(2, 2); switch_weapon(5); break;
  case IT_HEALTHBONUS: if (health >= 200) return 0; health++; break;
  case IT_ARMORBONUS: if (armor >= 200) return 0; armor++; break;
  case IT_CLIP: if (ammo_full(0)) return 0; add_ammo(0, 10); break;
  case IT_SHELLBOX: if (ammo_full(1)) return 0; add_ammo(1, 20); break;
  case IT_AMMOBOX: if (ammo_full(0)) return 0; add_ammo(0, 50); break;
  case IT_HEALTHCRATE: if (health >= 100) return 0; health = health + 10 > 100 ? 100 : health + 10; break;
  case IT_ONEROCKET: if (ammo_full(2)) return 0; add_ammo(2, 1); break;
  case IT_BOXOFROCKETS: if (ammo_full(2)) return 0; add_ammo(2, 5); break;
  default: return 0;
  }
  pickup_message(pickup_names[t - IT_HEALTH]);
  return 1;
}

void try_pickups(void) {
  u8 px = playerx >> 11, py = playery >> 11, m;
  s8 i = item_find(px, py);
  if (i >= 0 && item_t[i] >= IT_HEALTH && item_t[i] <= IT_BOXOFROCKETS) {
    if (pickup_apply(item_t[i])) { num_items_got++; item_remove(i); }
    return;
  }
  for (m = 0; m < MOBJ_PLAYER_ROCKET; m++) {   /* possessed corpses drop ammo once */
    u8 dx, dy;
    if (!mobj_alloc[m] || mobj_info[m] != MOBJINFO_POS || mobj_health[m] || (mobj_flags[m] & MF_GUTS_TAKEN)) continue;
    dx = mobj_x[m] - (playerx >> 8); if (dx & 0x80) dx = -dx;
    dy = mobj_y[m] - (playery >> 8); if (dy & 0x80) dy = -dy;
    if (dx >= 4 || dy >= 4) continue;
    mobj_flags[m] |= MF_GUTS_TAKEN;
    add_ammo(1, 4); add_ammo(0, 10);
    pickup_message("ammo");
    return;
  }
}

/* ---------------------------------------------------------------- damage */
void damage_player(u8 dmg) {
  if (!health || god_mode) return;
  if (difficulty == 0) { dmg >>= 1; if (!dmg) dmg = 1; }
  else if (difficulty == 2) { u16 v = dmg + (dmg >> 1); dmg = v > 255 ? 255 : (u8)v; }
  play_sound(SND_OOF);
  hurt_flash = 1;
  set_border(2);
  if (armor) {
    u8 saved = combat_armor ? (dmg >> 1) : (dmg >> 2) + (dmg >> 4);
    if (saved) {
      dmg -= saved;
      armor -= saved;
      if (armor >= 201) { armor = 0; combat_armor = 0; }
    }
  }
  if (health <= dmg) { health = 0; hide_weapon(); } else health -= dmg;
  hud_dirty = 1;
}

void player_frame(void) {
  if (radsuit_ms) radsuit_ms = radsuit_ms > dt_ms ? radsuit_ms - dt_ms : 0;
  if (hurt_flash) { hurt_flash--; set_border(2); return; }
  if (!radsuit_ms) set_border(0);
  else if (radsuit_ms >= 5000 || ((radsuit_ms >> 8) & 2)) set_border(5);
  else set_border(0);
}

void player_death_frame(void) {
  set_border(2);
  if (!key_use) { key_use_was = 0; return; }
  if (key_use_was) return;
  key_use_was = 1;
  end_level = 2;
}

void init_hud_state(void) {
  if (!health) {
    ammo[0] = 50; ammo[1] = 0; ammo[2] = 0;
    owned_weapons = 2; health = 100; armor = 0; combat_armor = 0; has_backpack = 0;
    cur_weapon = 0xff; switch_weapon(2);
  }
  keys = 0; info_ms = 0; info_len = 0; info_kind = 0; key_use_was = 0;
  radsuit_ms = 0; hurt_flash = 0; hud_dirty = 1;
}

void update_map_time(void) {
  map_time_ms += dt_ms;
  while (map_time_ms >= 1000) { map_time_ms -= 1000; map_time_sec++; }
}

/* ---------------------------------------------------------------- HUD (row 23) */
static void hud_num(u16 *d, u8 v, u16 base, u16 flag) {
  u8 h = 0, t = 0;
  while (v >= 100) { v -= 100; h++; }
  while (v >= 10) { v -= 10; t++; }
  d[0] = (base + h) | flag; d[1] = (base + t) | flag; d[2] = (base + v) | flag;
}
void draw_hud(void) {
  u16 *r = fb + HUD_ROW * VIEW_W;
  r[0] = cur_weapon >= 2 ? (T_AMMOICON + cur_weapon - 2) | TILE_USE_SPRITE_PALETTE : 0;
  hud_num(r + 1, ammo[wpn_ammo_idx[cur_weapon]], T_FONT_09, TILE_USE_SPRITE_PALETTE);
  r[4] = T_HEALTHICON | TILE_USE_SPRITE_PALETTE;
  hud_num(r + 5, health, T_FONT_09, TILE_USE_SPRITE_PALETTE);
  r[24] = T_ARMORICON | TILE_USE_SPRITE_PALETTE;
  hud_num(r + 25, armor, T_FONT_09, TILE_USE_SPRITE_PALETTE);
  r[28] = 0;
  r[29] = (keys & 1) ? T_KEY_RED | TILE_USE_SPRITE_PALETTE : 0;
  r[30] = (keys & 2) ? T_KEY_YELLOW | TILE_USE_SPRITE_PALETTE : 0;
  r[31] = (keys & 4) ? T_KEY_BLUE | TILE_USE_SPRITE_PALETTE : 0;
  hud_dirty = 0;
}
