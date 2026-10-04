/* Fixed-area world state + helpers shared by every bank: sector tables,
 * item list, map access, level loading (level.asm item_skill_filter,
 * enemy_alloc_all), and monster state tables used by the renderer. */
#include "defs.h"

u8 sec_floor[SEC_N], sec_ceil[SEC_N], sec_type[SEC_N], sec_target[SEC_N];
u8 sec_fcol[SEC_N], sec_ccol[SEC_N], sec_bright[SEC_N];
u8 sec_wdark[SEC_N], sec_seen[SEC_N], sec_visited[SEC_N];
u8 sector_max, seen_gen;
u8 sw_n, sw_sec[8], sw_dir[8];
u8 level_num_enemies, level_num_items, level_num_secrets, level_par_time;
u8 item_x[MAX_ITEMS], item_y[MAX_ITEMS], item_t[MAX_ITEMS];
u8 n_items;
u8 spawn_xya[3];

const u8 state_texture[] = {
  TEX_ANIMATE + 0, 2, 1, 2,
  TEX_ANIMATE + 3, 5, 4, 4, 5,
  TEX_ANIMATE + 6, 8, 7, 8,
  TEX_ANIMATE + 9, 9, 10, 10, 9,
  0 };
const u8 state_action[] = {   /* chase 0 flinch 1 melee 2 shoot 3 missile 4 fall 5 fly 6 */
  0, 1, 3, 5,  0, 1, 2, 4, 5,  0, 1, 2, 5,  0, 1, 2, 4, 5,  6 };

u8 map_id(u8 mx, u8 my) { return LV_MAP[(my << 5) | mx]; }
u8 sector_at(u8 mx, u8 my) { if ((mx | my) & 0xE0) return 0; return LV_MAP[(my << 5) | mx]; }
u8 sec_action(u8 s) { return sec_type[s] & ACT_MASK; }
void clear_sector_seen(void) { u8 i; for (i = 0; i < SEC_N; i++) sec_seen[i] = 0; }
void mark_seen(u8 s) { if (s) { sec_seen[s] = seen_gen; sec_visited[s] = 0xff; } }
s8 item_find(u8 tx, u8 ty) {
  u8 i;
  for (i = 0; i < n_items; i++) if (item_x[i] == tx && item_y[i] == ty) return i;
  return -1;
}
void item_remove(u8 i) {
  n_items--;
  for (; i < n_items; i++) { item_x[i] = item_x[i + 1]; item_y[i] = item_y[i + 1]; item_t[i] = item_t[i + 1]; }
}

/* copy the level's mutable tables into RAM; item layer -> item list + monsters.
 * The monster pool must already be reset (enemy_reset, logic bank). */
void load_level(void) {
  static const u8 hp[4] = {20, 30, 60, 255};
  static const u8 chase[4] = {0, 4, 9, 13};
  const u8 *base, *s, *l;
  u16 i;
  u8 n = level_num - 1, bank = SMS_getROMBank();
  SMS_mapROMBank(BANK_INIT0 + n / LEVELS_PER_INIT_BANK);
  base = (const u8 *)0x8000 + (u16)(n % LEVELS_PER_INIT_BANK) * LEVEL_INIT_SIZE;
  l = base; s = base + 1024;
  for (i = 0; i < SEC_N; i++) {
    sec_floor[i] = s[i]; sec_ceil[i] = s[200 + i]; sec_type[i] = s[400 + i]; sec_target[i] = s[600 + i];
    sec_fcol[i] = s[800 + i]; sec_ccol[i] = s[1000 + i]; sec_bright[i] = s[1200 + i];
  }
  s += 1400;
  spawn_xya[0] = s[0]; spawn_xya[1] = s[1]; spawn_xya[2] = s[2];
  sw_n = s[3];
  for (i = 0; i < 8; i++) { sw_sec[i] = s[4 + i]; sw_dir[i] = s[12 + i]; }
  sector_max = s[20];
  level_num_enemies = s[21]; level_num_items = s[22]; level_num_secrets = s[23]; level_par_time = s[24];
  n_items = 0;
  for (i = 0; i < 1024; i++) {
    u8 v = l[i], code, t;
    if (!v) continue;
    code = v & 0xc0; t = v & 0x3f;
    if (code == 0x40 && difficulty != 0) continue;
    if (code == 0x80 && difficulty == 0) continue;
    if (code == 0xc0 && difficulty != 2) continue;
    if (t >= IT_SOLDIER && t <= IT_BARON) {
      u8 m, info = t - 1;
      for (m = 0; m < MOBJ_PLAYER_ROCKET; m++) if (!mobj_alloc[m]) break;
      if (m == MOBJ_PLAYER_ROCKET) continue;
      mobj_alloc[m] = 1; mobj_movedir[m] = 0; mobj_flags[m] = 0; mobj_movecnt[m] = 0;
      mobj_xfrac[m] = 0x80; mobj_yfrac[m] = 0x80;
      mobj_x[m] = ((i & 31) << 3) + 4; mobj_y[m] = ((i >> 5) << 3) + 4;
      mobj_react[m] = 4; mobj_info[m] = info; mobj_health[m] = hp[info];
      mobj_state[m] = chase[info]; mobj_corpse[m] = 0xff;
      continue;
    }
    if (n_items < MAX_ITEMS) { item_x[n_items] = i & 31; item_y[n_items] = i >> 5; item_t[n_items] = t; n_items++; }
  }
  SMS_mapROMBank(bank);
}
