/* [front-end bank] Doom-style front end: menus (m_menu.c layout with the Doom
 * menu graphics), text pages in the STCFN font, intermission (wi_stuff.c graphics),
 * "entering", map complete + melt. */
#include "defs.h"
#include "menu_data.h"

static u8 ui_keys, ui_old, ui_pressed;
static void ui_read(void) {
  u8 e;
  __asm__("di"); e = pad_pressed; pad_pressed = 0; __asm__("ei");
  ui_old = ui_keys; ui_keys = pad_now;
  if (SMS_queryPauseRequested()) { SMS_resetPauseRequest(); e |= 0x40; }
  ui_pressed = e;
}
static void ui_wait_release(void) { do { SMS_waitForVBlank(); ui_read(); } while (ui_keys & 0x3f); }
static void wait_any_button(void) {
  ui_wait_release();
  do { SMS_waitForVBlank(); ui_read(); } while (!(ui_pressed & (PAD_1 | PAD_2 | 0x40)));
}
/* display off, clear the 0x3800 name table and the sprites */
static void ui_begin(void) {
  vdp_sync();
  SMS_displayOff();
  vdp_set_nt_front();
  SMS_VRAMmemsetW(0x3800, 0, 0x700);
  SMS_VRAMmemset(0, 0, 32);
  SMS_initSprites(); SMS_finalizeSprites(); SMS_copySpritestoSAT();
  SMS_setBackdropColor(0);
}

/* ---------------------------------------------------------------- text pages (STCFN) */
static void text_font(void) {
  SMS_loadTiles(txt_font, 0, 64 * 32);       /* ASCII 32..95 -> tiles 0..63 */
  SMS_loadBGPalette(txt_pal);
}
static void printt(u8 x, u8 y, const char *s) {
  SMS_setNextTileatXY(x, y);
  while (*s && *s != '\n' && x++ < 32) {
    char c = *s++;
    if (c >= 'a' && c <= 'z') c -= 32;
    if (c < 32 || c > 95) c = 32;
    SMS_setTile(c - 32);
  }
}
static void show_pages(const u8 *addr, u8 npages) {
  const char *p = (const char *)addr;
  u8 pg, y;
  for (pg = 0; pg < npages; pg++) {
    ui_begin();
    text_font();
    y = 2;
    while (*p) {
      if (y < 22) printt(1, y, p);
      while (*p && *p != '\n') p++;
      if (*p == '\n') p++;
      y++;
    }
    p++;
    SMS_displayOn();
    wait_any_button();
    play_sound(SND_PISTOL);
  }
}

/* ---------------------------------------------------------------- menus */
#define NM_BACK 0xf0
#define NM_START 0xf1
#define NM_ORDER 0xf2
#define NM_CTRL 0xf3
#define NM_HELP 0xf4
#define NM_CREDITS 0xf5
#define NM_QUIT 0xf6
static const u8 menu_img[4] = {IMG_MENU_MAIN, IMG_MENU_EPISODE, IMG_MENU_SKILL, IMG_MENU_SOUND};
static const u8 next_menu[4][6] = {
  {1, 3, NM_CTRL, NM_HELP, NM_CREDITS, NM_QUIT},
  {2, NM_ORDER, NM_ORDER},
  {NM_START, NM_START, NM_START},
  {3, 3} };
#define MSPR 182            /* skull + knob sprite tiles at VRAM 438..447, clear of the screen tiles */
static u8 menu_id, menu_item, stack_d, stk_m[3], stk_i[3], cursor_frame, cursor_tick;

static void draw_sprites(void) {
  u8 i = menu_first[menu_id] + menu_item, x = menu_skull_x[i], y = menu_skull_y[i], t = MSPR + (cursor_frame ? 4 : 0);
  SMS_initSprites();
  SMS_addSprite(x, y, t); SMS_addSprite(x + 8, y, t + 1);
  SMS_addSprite(x, y + 8, t + 2); SMS_addSprite(x + 8, y + 8, t + 3);
  if (menu_id == 3) {                          /* thermometer knobs (M_THERMO) */
    u8 k;
    for (k = 0; k < 2; k++) {
      u8 v = k ? music_vol : effects_vol;
      u8 kx = therm_x[k] + (u8)(((u16)v * 32) / 5);
      SMS_addSprite(kx, therm_y[k], MSPR + 8); SMS_addSprite(kx, therm_y[k] + 8, MSPR + 9);
    }
  }
  SMS_finalizeSprites();
}
static void draw_menu(void) {
  ui_begin();
  img_load(menu_img[menu_id], 0, 0, 0, IMG_PAL);
  img_load(IMG_MENU_SPRITES, 256 + MSPR, 0, 0, IMG_SPRPAL | IMG_NOMAP);
  draw_sprites();
  SMS_copySpritestoSAT();
  SMS_displayOn();
}
static void menu_back(void) {
  if (!stack_d) return;
  play_sound(SND_PISTOL);
  stack_d--; menu_id = stk_m[stack_d]; menu_item = stk_i[stack_d];
  draw_menu();
}
/* returns 1 when a new game was started */
static u8 menu_select(void) {
  u8 n = next_menu[menu_id][menu_item];
  if (menu_id == 1) episode = menu_item;
  if (n == NM_START) { play_sound(SND_PISTOL); difficulty = menu_item; return 1; }
  if (n < 4) {
    if (n == menu_id) return 0;                 /* volume rows */
    play_sound(SND_PISTOL);
    stk_m[stack_d] = menu_id; stk_i[stack_d] = menu_item; stack_d++;
    menu_id = n; menu_item = 0; draw_menu();
    return 0;
  }
  play_sound(SND_PISTOL);
  switch (n) {
  case NM_ORDER: show_pages(txt_order, TXT_ORDER_PAGES); break;
  case NM_CTRL: show_pages(txt_control, TXT_CONTROL_PAGES); break;
  case NM_HELP: show_pages(txt_readthis, TXT_READTHIS_PAGES); break;
  case NM_CREDITS: show_pages(txt_credits, TXT_CREDITS_PAGES); break;
  case NM_QUIT: show_splash(); stack_d = 0; menu_id = 0; menu_item = 0; break;
  }
  draw_menu();
  return 0;
}

u8 menu_active;
void frontend(void) {
  input_paused = 1;
  hide_weapon();
  set_border(0);
  if (game_complete) {
    game_complete = 0;
    show_pages(txt_ending, TXT_ENDING_PAGES);
  } else show_splash();
  menu_id = 0; menu_item = 0; stack_d = 0;
  draw_menu();
  ui_wait_release();
  menu_active = 1;
  for (;;) {
    u8 old = menu_item, redraw = 0;
    SMS_waitForVBlank();
    SMS_copySpritestoSAT();
    ui_read();
    if (++cursor_tick >= 8) { cursor_tick = 0; cursor_frame ^= 1; redraw = 1; }   /* Doom: every 8 tics */
    if ((ui_pressed & PAD_UP) && menu_item) menu_item--;
    if ((ui_pressed & PAD_DOWN) && menu_item + 1 < menu_items[menu_id]) menu_item++;
    if (menu_item != old) { play_sound(SND_PISTOL); redraw = 1; }
    if (menu_id == 3 && (ui_pressed & (PAD_LEFT | PAD_RIGHT))) {
      u8 *v = menu_item ? &music_vol : &effects_vol;
      if ((ui_pressed & PAD_LEFT) && *v) (*v)--;
      if ((ui_pressed & PAD_RIGHT) && *v < 15) (*v)++;
      play_sound(SND_STNMOV);
      redraw = 1;
    }
    if (redraw) draw_sprites();
    if (ui_pressed & PAD_2) menu_back();
    if (ui_pressed & PAD_1) { if (menu_select()) break; }
  }
  menu_active = 0;
  SMS_displayOff();
  wait_frames(20);
  input_paused = 0;
}

/* ---------------------------------------------------------------- level transitions */
/* intermission pieces: loaded one after another into VRAM */
static u16 wi_next;
static u16 wi_digits;
static u8 wi_piece(u8 id, u8 w, u8 x, u8 y) {
  if (x == 0xff) x = (32 - w) >> 1;
  wi_next += img_load(id, wi_next, x, y, wi_next == 1 ? IMG_PAL : 0);
  return x;
}
static void wi_begin(void) {
  ui_begin();
  wi_next = 1;                 /* tile 0 stays blank (background) */
  wi_digits = 0;
}
static const u8 wi_lv_img[9] = {IMG_WI_LV0, IMG_WI_LV1, IMG_WI_LV2, IMG_WI_LV3, IMG_WI_LV4,
                                IMG_WI_LV5, IMG_WI_LV6, IMG_WI_LV7, IMG_WI_LV8};
static const u8 wi_lv_w[9] = {WI_LV0_W, WI_LV1_W, WI_LV2_W, WI_LV3_W, WI_LV4_W, WI_LV5_W, WI_LV6_W, WI_LV7_W, WI_LV8_W};
void show_entering(void) {
  input_paused = 1;
  wi_begin();
  wi_piece(IMG_WI_ENTERING, WI_ENTERING_W, 0xff, 8);
  wi_piece(wi_lv_img[level_num - 1], wi_lv_w[level_num - 1], 0xff, 11);
  SMS_displayOn();
  wait_frames(60);
  input_paused = 0;
}

/* digit glyph g (0-9, 10 = %, 11 = :) as an 8x16 cell pair */
static void wi_glyph(u8 x, u8 y, u8 g) {
  SMS_setNextTileatXY(x, y); SMS_setTile(wi_digits + g);
  SMS_setNextTileatXY(x, y + 1); SMS_setTile(wi_digits + 12 + g);
}
static void wi_blank(u8 x, u8 y) {
  SMS_setNextTileatXY(x, y); SMS_setTile(0);
  SMS_setNextTileatXY(x, y + 1); SMS_setTile(0);
}
static void summary_tick(void) { play_sound(SND_PISTOL); wait_frames(2); }
static u8 pct(u8 got, u8 total) { return total ? (u8)(((u16)got * 100) / total) : 0; }
/* right-aligned percentage ending at column xr */
static void put_pct(u8 xr, u8 y, u8 v) {
  wi_glyph(xr, y, 10);
  wi_glyph(xr - 1, y, v % 10);
  if (v >= 10) wi_glyph(xr - 2, y, (v / 10) % 10); else wi_blank(xr - 2, y);
  if (v >= 100) wi_glyph(xr - 3, y, 1); else wi_blank(xr - 3, y);
}
static void put_time(u8 xr, u8 y, u16 s) {
  u8 m = 0;
  while (s >= 60) { s -= 60; m++; }
  wi_glyph(xr, y, s % 10); wi_glyph(xr - 1, y, s / 10); wi_glyph(xr - 2, y, 11);
  wi_glyph(xr - 3, y, m % 10);
  if (m >= 10) wi_glyph(xr - 4, y, m / 10); else wi_blank(xr - 4, y);
}
static void roll_pct(u8 target, u8 y) {
  u8 cur = 0;
  for (;;) {
    put_pct(27, y, cur); summary_tick();
    if (cur >= target) return;
    cur = cur + 2 > target ? target : cur + 2;
  }
}
static void roll_time(u16 target, u8 xr, u8 y) {
  u16 cur = 0;
  for (;;) {
    put_time(xr, y, cur); summary_tick();
    if (cur >= target) return;
    cur = cur + 3 > target ? target : cur + 3;
  }
}
static void summary_screen(void) {
  wi_begin();
  wi_piece(wi_lv_img[level_num - 1], wi_lv_w[level_num - 1], 0xff, 1);
  wi_piece(IMG_WI_FINISHED, WI_FINISHED_W, 0xff, 3);
  wi_piece(IMG_WI_KILLS, WI_KILLS_W, 4, 7);
  wi_piece(IMG_WI_ITEMS, WI_ITEMS_W, 4, 10);
  wi_piece(IMG_WI_SECRET, WI_SECRET_W, 4, 13);
  wi_piece(IMG_WI_TIME, WI_TIME_W, 1, 19);
  wi_piece(IMG_WI_PAR, WI_PAR_W, 17, 19);
  wi_digits = wi_next;
  wi_next += img_load(IMG_WI_DIGITS, wi_next, 0, 0, IMG_NOMAP);
  SMS_displayOn();
  roll_pct(pct(num_kills, level_num_enemies), 7); wait_frames(40);
  roll_pct(pct(num_items_got, level_num_items), 10); wait_frames(40);
  roll_pct(pct(num_secrets_got, level_num_secrets), 13); wait_frames(40);
  if (map_time_sec >= 600) wi_piece(IMG_WI_SUCKS, WI_SUCKS_W, 10, 19);
  else roll_time(map_time_sec, 14, 19);
  wait_frames(40);
  roll_time(level_par_time, 30, 19);
  wait_any_button();
  play_sound(SND_PISTOL);
}

/* the Doom screen melt on the framebuffer: 1000 single-cell column drips */
static void melt_screen(void) {
  u16 n = 1000;
  u8 k;
  while (n) {
    for (k = 0; k < 20 && n; k++, n--) {
      u8 c = rnd16() & 31, y;
      u16 *p = fb + (VIEW_H - 1) * VIEW_W + c;
      for (y = VIEW_H - 1; y; y--, p -= VIEW_W) *p = p[-VIEW_W];
      *p = 0;
    }
    present_frame(0);
  }
}

void after_level_end(void) {
  input_paused = 1;
  sat_n = 0;
  text_at(10, 12, "map complete");
  present_frame(1);
  wait_frames(60);
  melt_screen();
  wait_frames(120);
  summary_screen();
  input_paused = 0;
}

