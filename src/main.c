/* SquareDoom SMS - entry point, interrupts, game loop (gameloop.asm, titleflow.asm). */
#include "defs.h"

volatile u8 frame_ms;
volatile u16 vbl_count;
u8 pal_mode;
volatile u8 in_turn_l, in_turn_r, in_fwd, in_back, in_strafel, in_strafer;
volatile u8 in_use, in_fire, in_wnext, in_wprev, in_map;
volatile u8 pad_now, input_paused, pad_pressed;
static u8 pad_prev, ms_phase, wtick_ms;
u8 level_num, difficulty, end_level, clev, game_complete, episode;
static u8 border_idx, random8 = 0x5a;
static u16 random16 = 0xa3b7;
u8 weapon_hl_colour(void);

__sfr __at 0xDC port_dc;

u8 rnd8(void) { random8 = random8 * 9 + 193; return random8; }   /* Deathchase GetRandom8 */
u16 rnd16(void) { random16 = random16 * 5 + 0x3611; return random16; }

static u8 sat_add(u8 a, u8 b) { u16 v = a + b; return v > 255 ? 255 : (u8)v; }

/* frame interrupt: timing, pad sampling (C64 CIA Timer A input_irq), weapons, sound */
static void frame_isr(void) {
  u8 ms, p, dir;
  vdp_isr_flip();
  if (pal_mode) ms = 20;
  else { ms = ms_phase == 2 ? 16 : 17; if (++ms_phase == 3) ms_phase = 0; }
  vbl_count++;
  frame_ms = sat_add(frame_ms, ms);
  p = ~port_dc & 0x3f;
  pad_now = p;
  pad_pressed |= p & ~pad_prev;
  if (!input_paused) {
    dir = p & 15;
    if (p & PAD_2) {
      if (p & PAD_LEFT) in_strafel = sat_add(in_strafel, ms);
      if (p & PAD_RIGHT) in_strafer = sat_add(in_strafer, ms);
      if ((p & PAD_UP) && !(pad_prev & PAD_UP)) in_wnext = 1;
      if ((p & PAD_DOWN) && !(pad_prev & PAD_DOWN)) in_wprev = 1;
      if (!dir) in_use = 1;
    } else {
      if (p & PAD_LEFT) in_turn_l = sat_add(in_turn_l, ms);
      if (p & PAD_RIGHT) in_turn_r = sat_add(in_turn_r, ms);
      if (p & PAD_UP) in_fwd = sat_add(in_fwd, ms);
      if (p & PAD_DOWN) in_back = sat_add(in_back, ms);
    }
    if (p & PAD_1) in_fire = 1;
    key_fire_held = p & PAD_1;
    wtick_ms += ms;
    while (wtick_ms >= 20) { wtick_ms -= 20; weapon_tick_isr(); }
  }
  pad_prev = p;
  sound_isr();
}

void wait_frames(u8 n) { while (n--) SMS_waitForVBlank(); vdp_sync(); }
void set_border(u8 i) { border_idx = i; }

void present_frame(u8 with_sprites) {
  vdp_sync();
  vdp_upload_fb();
  flip_sat = with_sprites;
  flip_border = c64_sms[border_idx];
  flip_pending = 1;
}

static void detect_region(void) {
  u8 v, mx = 0;
  u16 n;
  SMS_waitForVBlank();
  for (n = 0; n < 4000; n++) { v = SMS_getVCount(); if (v > mx) mx = v; }
  pal_mode = mx > 0xDA;
}

/* ---------------------------------------------------------------- game */
u16 game_frames, frame_vbl, frame_t0;   /* frames rendered; vblanks per frame */
static void game_frame(void) {
  game_frames++; frame_vbl = vbl_count - frame_t0; frame_t0 = vbl_count;
  u8 t;
  /* dt from the frame interrupt accumulator */
  __asm__("di"); t = frame_ms; frame_ms = 0; __asm__("ei");
  dt_ms = t ? t : 20;
  SMS_mapROMBank(LOGIC_BANK);
  if (health) {
    player_frame();
    update_map_time();
    read_input();
    if (SMS_queryPauseRequested()) { SMS_resetPauseRequest(); mapscreen(); if (end_level == 3) return; }
    apply_move();
    try_walk_into();
    try_pickups();
    try_use();
    try_switch();
  } else {
    read_input();
    update_map_time();
    player_death_frame();
  }
  proc_update();
  sector_specials_update();
  update_info_msg();
  update_eye();
  SMS_mapROMBank(RENDER_BANK);
  render_world();
  draw_info_msg();
  draw_hud();
  weapon_overlay();
  present_frame(1);
  SMS_mapROMBank(LOGIC_BANK);
  if (health) { update_muzzle_flash(); hitscan_process(); }
  enemy_think();
  barrel_update();
}

static void game(void) {
  level_num = 1; health = 0; god_mode = 0;
  for (;;) {
    hide_weapon();
    SMS_mapROMBank(BANK_FRONT);
    show_entering();
    SMS_mapROMBank(LOGIC_BANK);
    enemy_reset();
    load_level();
    end_level = 0;
    start_level();
    vdp_init_game();
    wpn_visible = 1;
    for (;;) {
      game_frame();
      if (end_level == 3) return;       /* quit to menu */
      if (end_level == 2) break;                          /* died: restart map */
      if (end_level == 1) {
        hide_weapon();
        if (clev) { clev = 0; break; }
        SMS_mapROMBank(BANK_FRONT);
        after_level_end();
        if (level_num == 8) { game_complete = 1; return; }
        level_num++;
        break;
      }
    }
  }
}

void main(void) {
  SMS_displayOff();
  SMS_useFirstHalfTilesforSprites(0);
  SMS_setSpriteMode(SPRITEMODE_NORMAL);
  SMS_VDPturnOffFeature(VDPFEATURE_LEFTCOLBLANK);
  detect_region();
  sound_init();
  effects_vol = 15; music_vol = 10;
  SMS_setFrameInterruptHandler(frame_isr);
  music_start();
  for (;;) {
    SMS_mapROMBank(BANK_FRONT);
    frontend();            /* title + menus; returns on New Game with difficulty set */
    game();
  }
}

SMS_EMBED_SEGA_ROM_HEADER(9999, 0);
SMS_EMBED_SDSC_HEADER_AUTO_DATE(1, 0, "Steve McCrea", "SquareDoom",
  "Sega Master System conversion of the C64 SquareDoom");
