/* Weapons: 50 Hz pose/fire state machine (weapon.asm update_weapon_irq),
 * shot resolution, and the HUD weapon overlay (BG tiles + edge sprites). */
#include "defs.h"

u8 wpn_visible, wpn_shot_req, wpn_pose, wpn_flash_ticks, wpn_tiles_loaded = 0xff;
volatile u8 key_fire_held;
static u8 wpn_pose_ticks, wpn_fire_ticks, recoil_step;
static u8 muzzle_flash_var, muzzle_hi_cycle, muzzle_hi_col = 1, mg_frame;
static u8 saw_running, saw_blade_frame, saw_blade_div;
static u8 pose_flash;                 /* flash variant latched at shot time */
#define POSE_IDLE 0
#define POSE_FIRE 1
#define POSE_RECOIL 2
#define POSE_COCK 3
#define FLASH_TICKS 15
#define PUNCH_TICKS 18
#define PISTOL_FIRE_TICKS 15
#define PISTOL_RECOIL_TICKS 8
#define SG_COCK_TICKS 30
static const u8 fire_ticks_tab[6] = {40, 5, 30, 45, 5, 60};
static const u8 recoil_ticks[3] = {1, 2, 2};
static const u8 wpn_sound[6] = {SND_PUNCH, SND_SAWFUL, SND_PISTOL, SND_SHOTGN, SND_PISTOL, SND_SHOTGN};
static const u8 muzzle_hi_cols[4] = {1, 7, 1, 10};
static const u8 bright_to_wpn_hi[17] = {11,11,11,11,11,12,12,12,12,12,15,15,15,15,15,15,1};

void switch_weapon(u8 w) {
  if (w && !(owned_weapons & wpn_own_bit[w])) return;
  if (w == cur_weapon) return;
  __asm__("di");
  cur_weapon = w;
  wpn_pose = wpn_pose_ticks = wpn_flash_ticks = wpn_fire_ticks = recoil_step = wpn_shot_req = 0;
  saw_running = saw_blade_frame = saw_blade_div = mg_frame = 0;
  __asm__("ei");
  hud_dirty = 1;
}
void hide_weapon(void) { wpn_visible = 0; }

/* runs every 20 ms of elapsed time from the frame interrupt */
void weapon_tick_isr(void) {
  u8 w = cur_weapon;
  if (!wpn_visible || !health || w > 5) return;
  if (wpn_fire_ticks) wpn_fire_ticks--;
  if (key_fire_held) {
    if (w == 1) {                                    /* chainsaw runs while held */
      saw_running = 1;
      if (!(++saw_blade_div & 3)) saw_blade_frame ^= 1;
    }
  } else {
    mg_frame = 0;
    saw_running = 0; saw_blade_frame = 0; saw_blade_div = 0;
  }
  /* tick flash (pistol pose owns its flash) */
  if (w != 2 && wpn_flash_ticks) {
    if (!--wpn_flash_ticks && w == 3) {
      wpn_pose = POSE_COCK; wpn_pose_ticks = SG_COCK_TICKS; play_sound(SND_SGCOCK);
    }
  }
  /* tick pose */
  if (wpn_pose_ticks && !--wpn_pose_ticks) {
    switch (w) {
    case 0: wpn_pose = POSE_IDLE; break;
    case 2:
      if (wpn_pose == POSE_FIRE) { wpn_pose = POSE_RECOIL; wpn_pose_ticks = PISTOL_RECOIL_TICKS; wpn_flash_ticks = 0; }
      else wpn_pose = POSE_IDLE;
      break;
    case 3: wpn_pose = POSE_IDLE; break;
    case 5:
      if (++recoil_step < 3) wpn_pose_ticks = recoil_ticks[recoil_step];
      else { wpn_pose = POSE_IDLE; recoil_step = 0; }
      break;
    }
  }
  if (!key_fire_held || wpn_fire_ticks) return;
  /* try fire */
  if (w >= 2) {
    if (w == 5 && (mobj_alloc[MOBJ_PLAYER_ROCKET] || wpn_shot_req)) goto empty;
    if (ammo[wpn_ammo_idx[w]] <= wpn_shot_req) goto empty;
  }
  switch (w) {
  case 0: wpn_pose = POSE_FIRE; wpn_pose_ticks = PUNCH_TICKS; break;
  case 2: wpn_pose = POSE_FIRE; wpn_pose_ticks = PISTOL_FIRE_TICKS; wpn_flash_ticks = FLASH_TICKS;
          pose_flash = muzzle_flash_var & 1; muzzle_flash_var++; break;
  case 3: wpn_pose = POSE_FIRE; wpn_flash_ticks = FLASH_TICKS; wpn_pose_ticks = 0;
          pose_flash = muzzle_flash_var & 1; muzzle_flash_var++; break;
  case 4: wpn_pose = POSE_FIRE; wpn_flash_ticks = FLASH_TICKS; mg_frame ^= 1; muzzle_flash_var++; break;
  case 5: wpn_pose = POSE_RECOIL; recoil_step = 0; wpn_pose_ticks = recoil_ticks[0]; wpn_flash_ticks = FLASH_TICKS; break;
  }
  if (w >= 2) { muzzle_hi_cycle++; muzzle_hi_col = muzzle_hi_cols[muzzle_hi_cycle & 3]; }
  play_sound(wpn_sound[w]);
  wpn_shot_req++;
  wpn_fire_ticks = fire_ticks_tab[w];
  return;
empty:
  play_sound(SND_OOF);
  wpn_fire_ticks = fire_ticks_tab[w];
}

/* game loop side: consume shots after render (COL_AIM valid) */
void update_muzzle_flash(void) {
  u8 n;
  __asm__("di");
  n = wpn_shot_req; wpn_shot_req = 0;
  __asm__("ei");
  while (n--) {
    u8 w = cur_weapon;
    if (w >= 2) {
      u8 *a = &ammo[wpn_ammo_idx[w]];
      if (!*a) continue;
      (*a)--; hud_dirty = 1;
    }
    switch (w) {
    case 0: case 1:
      pain_boost = w ? 10 : 0;
      try_damage_enemy((rnd8() >> 4) + 1, 4);
      pain_boost = 0;
      break;
    case 2: case 4: try_damage_enemy((rnd8() >> 4) + 1, 0xff); break;
    case 3: {
      u8 p;
      for (p = 0; p < 5; p++) {
        u8 r = rnd8() & 15;
        while (r >= 5) r -= 5;
        try_damage_at_col((rnd8() >> 4) + 1, MUZZLE_COL - AIM_COL_SLACK + r);
      }
      break; }
    case 5: spawn_player_rocket(); break;
    }
  }
}

static u8 pose_index(void) {
  u8 w = cur_weapon;
  switch (w) {
  case 0: return wpn_pose == POSE_FIRE;
  case 1: return (saw_running ? 2 : 0) + saw_blade_frame;
  case 2: return wpn_pose == POSE_FIRE ? 1 + pose_flash : wpn_pose == POSE_RECOIL ? 3 : 0;
  case 3: return wpn_pose == POSE_COCK ? 3 : wpn_flash_ticks ? 1 + pose_flash : 0;
  case 4: return wpn_flash_ticks ? (mg_frame ? 2 : 3) : mg_frame;
  default:
    if (wpn_pose == POSE_RECOIL) return 1 + recoil_step;
    return wpn_flash_ticks ? 4 : 0;
  }
}

/* highlight colour (sprite palette entry 12) */
u8 weapon_hl_colour(void) {
  u8 b;
  if (wpn_flash_ticks && cur_weapon >= 2) return muzzle_hi_col;
  b = sec_bright[player_sector]; if (b > 16) b = 16;
  return bright_to_wpn_hi[b];
}

/* stamp interior BG tiles into fb + build the sprite table (sat_*) */
u8 sat_n, sat_y[64], sat_xn[128];
void weapon_overlay(void) {
  const u8 *d;
  u8 nb, ns, bank = SMS_getROMBank();
  sat_n = 0;
  if (!(wpn_visible && health && cur_weapon == wpn_tiles_loaded)) return;
  SMS_mapROMBank(WPOSE_BANK);
  d = (const u8 *)WPOSE_ADDR + wpose_off[wpose_first[cur_weapon] + pose_index()];
  nb = d[0]; ns = d[1]; d += 2;
  while (nb--) {
    u16 cell = d[0] | (d[1] << 8);
    u8 t = d[2];
    fb[cell] = (t < 128 ? 256 + t : 440 + t - 128) | TILE_USE_SPRITE_PALETTE;
    d += 3;
  }
  {
    u8 *py = sat_y, *pxn = sat_xn;
    sat_n = ns;
    while (ns--) { *pxn++ = d[0]; *py++ = d[1] - 1; *pxn++ = d[2]; d += 3; }
  }
  SMS_mapROMBank(bank);
}
