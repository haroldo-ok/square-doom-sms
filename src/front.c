/* [front-end bank] Front end: title splash, menus (menu.asm), text/story pages, "entering",
 * map complete + melt + intermission tally (titleflow.asm, levelstats.asm),
 * in-game automap (mapscreen.asm) and cheats (cheats.asm, pad codes). */
#include "defs.h"
#include "menu_data.h"

#define T_LOGO 1
#define T_SETA 129
#define T_SETB 225
#define T_SKULL 417
#define C_A 14            /* set A ink: white / dark grey (BG), yellow (sprite pal) */
#define C_B 13            /* set B ink: red (BG), yellow (sprite pal) */
#define HI TILE_USE_SPRITE_PALETTE
static u8 ui_keys, ui_old, ui_pressed;
static const char * const level_titles[9] = {
  "hangar", "nuclear plant", "toxin refinery", "command control", "phobos lab",
  "central processing", "computer station", "phobos anomaly", "military base" };

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

static void ui_palette(u8 a_colour) {
  u8 i;
  for (i = 0; i < 16; i++) { SMS_setBGPaletteColor(i, c64_sms[i]); SMS_setSpritePaletteColor(i, c64_sms[i]); }
  SMS_setBGPaletteColor(C_B, c64_sms[2]);
  SMS_setBGPaletteColor(C_A, c64_sms[a_colour]);
  SMS_setSpritePaletteColor(C_B, c64_sms[7]);
  SMS_setSpritePaletteColor(C_A, c64_sms[7]);
}
/* display off, clear the 0x3800 name table, load the font sets */
static void ui_begin(u8 a_colour, u8 seta_fg, u8 seta_bg) {
  SMS_displayOff();
  vdp_set_nt_front();
  SMS_VRAMmemsetW(0x3800, 0, 0x700);
  SMS_initSprites(); SMS_finalizeSprites(); SMS_copySpritestoSAT();
  SMS_load1bppTiles(mfont, T_SETA, 96 * 8, seta_bg, seta_fg);
  SMS_load1bppTiles(mfont2, T_SETB, 192 * 8, 0, C_B);
  SMS_VRAMmemset(0, 0, 32);
  ui_palette(a_colour);
  SMS_setBackdropColor(0);
}
static void ui_logo(void) {
  u8 x, y;
  SMS_loadTiles(logo_tiles, T_LOGO, 128 * 32);
  for (y = 0; y < 8; y++) {
    SMS_setNextTileatXY(8, y);
    for (x = 0; x < 16; x++) SMS_setTile(T_LOGO + y * 16 + x);
  }
}
static u8 slen(const char *s) { u8 n = 0; while (s[n] && s[n] != '\n') n++; return n; }
static u8 ascii_glyph(char c) { if (c < 32 || c > 127) c = 32; return c - 32; }
/* set A, one row */
static void printa(u8 x, u8 y, const char *s, u16 flags) {
  SMS_setNextTileatXY(x, y);
  while (*s && *s != '\n' && x++ < 32) SMS_setTile((T_SETA + ascii_glyph(*s++)) | flags);
}
/* set B, double height */
static void printb(u8 x, u8 y, const char *s, u16 flags) {
  const char *p; u8 xx;
  SMS_setNextTileatXY(x, y);
  for (p = s, xx = x; *p && xx < 32; p++, xx++) SMS_setTile((T_SETB + 2 * ascii_glyph(*p)) | flags);
  SMS_setNextTileatXY(x, y + 1);
  for (p = s, xx = x; *p && xx < 32; p++, xx++) SMS_setTile((T_SETB + 2 * ascii_glyph(*p) + 1) | flags);
}
static u8 centre(const char *s) { u8 n = slen(s); return n >= 32 ? 0 : (32 - n) >> 1; }

/* ---------------------------------------------------------------- text pages */
static void show_pages(const u8 *addr, u8 npages, u8 story) {
  const char *p = (const char *)addr;
  u8 pg;
  for (pg = 0; pg < npages; pg++) {
    u8 y, x;
    if (story) ui_begin(1, 0, 1); else ui_begin(1, C_B, 0);
    if (story) {                     /* white paper panel, black text */
      for (y = 1; y < 23; y++) { SMS_setNextTileatXY(1, y); for (x = 1; x < 31; x++) SMS_setTile(T_SETA); }
    }
    y = 2;
    while (*p) {
      if (y < 22) printa(1 + (story ? 0 : 0) + (story ? 0 : 0) + 0, y, p, 0);
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
static const char * const menu_items[4][6] = {
  {"New game", "Options", "Controls", "Read this!", "Credits", "Quit"},
  {"Knee deep in the dead", "The shores of hell", "Inferno", "Back"},
  {"I'm too young to die", "Hurt me plenty", "Ultra-violence", "Back"},
  {"Effects volume 15", "Music volume 10", "Back"} };
static const u8 menu_sizes[4] = {6, 4, 4, 3};
static const char * const section[4] = {"SquareDoom", "Which episode?", "Choose skill level", "Options"};
static const u8 next_menu[4][6] = {
  {1, 3, NM_CTRL, NM_HELP, NM_CREDITS, NM_QUIT},
  {2, NM_ORDER, NM_ORDER, NM_BACK},
  {NM_START, NM_START, NM_START, NM_BACK},
  {3, 3, NM_BACK} };
static u8 menu_id, menu_item, stack_d, stk_m[3], stk_i[3], cursor_frame, cursor_tick;
static char volbuf[2][20];

static const char *item_text(u8 i) {
  if (menu_id == 3 && i < 2) {
    const char *b = menu_items[3][i]; char *d = volbuf[i]; u8 v = i ? music_vol : effects_vol;
    while (*b) *d++ = *b++;
    d[-2] = v >= 10 ? '1' : ' '; d[-1] = '0' + (v >= 10 ? v - 10 : v); *d = 0;
    return volbuf[i];
  }
  return menu_items[menu_id][i];
}
#define ITEM_Y(i) (11 + (i) * 2)
#define ITEM_X 6
static void draw_item(u8 i) { printb(ITEM_X, ITEM_Y(i), item_text(i), i == menu_item ? HI : 0); }
static void draw_cursor(void) {
  u8 t = (cursor_frame ? 9 : 0) + (T_SKULL - 256), x, y;
  SMS_initSprites();
  for (y = 0; y < 3; y++) for (x = 0; x < 3; x++)
    SMS_addSprite(ITEM_X * 8 - 28 + x * 8, ITEM_Y(menu_item) * 8 - 4 + y * 8, t + y * 3 + x);
  SMS_finalizeSprites();
}
static void draw_menu(void) {
  u8 i;
  ui_begin(11, C_A, 0);
  ui_logo();
  SMS_loadTiles(skull_tiles, T_SKULL, SKULL_TILES_SIZE);
  printb(centre(section[menu_id]), 8, section[menu_id], HI);
  for (i = 0; i < menu_sizes[menu_id]; i++) draw_item(i);
  printa(1, 23, menu_id == 3 ? "d-pad move/adjust 1 select" : "d-pad move  1 select  2 back", 0);
  draw_cursor();
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
  if (n == NM_BACK) { menu_back(); return 0; }
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
  case NM_ORDER: show_pages(txt_order, TXT_ORDER_PAGES, 0); break;
  case NM_CTRL: show_pages(txt_control, TXT_CONTROL_PAGES, 0); break;
  case NM_HELP: show_pages(txt_readthis, TXT_READTHIS_PAGES, 1); break;
  case NM_CREDITS: show_pages(txt_credits, TXT_CREDITS_PAGES, 0); break;
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
    show_pages(txt_ending, TXT_ENDING_PAGES, 1);
  } else show_splash();
  menu_id = 0; menu_item = 0; stack_d = 0;
  draw_menu();
  ui_wait_release();
  menu_active = 1;
  for (;;) {
    u8 old = menu_item;
    SMS_waitForVBlank();
    SMS_copySpritestoSAT();
    ui_read();
    if (++cursor_tick >= 20) { cursor_tick = 0; cursor_frame ^= 1; draw_cursor(); }
    if ((ui_pressed & PAD_UP) && menu_item) menu_item--;
    if ((ui_pressed & PAD_DOWN) && menu_item + 1 < menu_sizes[menu_id]) menu_item++;
    if (menu_item != old) {
      play_sound(SND_PISTOL);
      draw_item(old); draw_item(menu_item); draw_cursor();
    }
    if (menu_id == 3 && menu_item < 2 && (ui_pressed & (PAD_LEFT | PAD_RIGHT))) {
      u8 *v = menu_item ? &music_vol : &effects_vol;
      if ((ui_pressed & PAD_LEFT) && *v) (*v)--;
      if ((ui_pressed & PAD_RIGHT) && *v < 15) (*v)++;
      draw_item(menu_item);
      play_sound(SND_PISTOL);
    }
    if (ui_pressed & PAD_2) menu_back();
    if (ui_pressed & PAD_1) { if (menu_select()) break; }
  }
  menu_active = 0;
  SMS_displayOff();
  wait_frames(20);
  input_paused = 0;
}

/* ---------------------------------------------------------------- level transitions */
void show_entering(void) {
  const char *t = level_titles[level_num - 1];
  input_paused = 1;
  ui_begin(1, C_A, 0);
  printb(12, 8, "entering", 0);
  printa(centre(t), 11, t, 0);
  SMS_displayOn();
  wait_frames(60);
  input_paused = 0;
}

static void summary_tick(void) { play_sound(SND_PISTOL); wait_frames(2); }
static u8 pct(u8 got, u8 total) { return total ? (u8)(((u16)got * 100) / total) : 0; }
static void put3(u8 x, u8 y, u8 v, char suffix) {
  char b[5];
  b[0] = '0' + v / 100; b[1] = '0' + (v / 10) % 10; b[2] = '0' + v % 10; b[3] = suffix; b[4] = 0;
  printb(x, y, b, 0);
}
static void put_time(u8 y, u16 s) {
  char b[6]; u8 m = 0;
  while (s >= 60) { s -= 60; m++; }
  b[0] = '0' + m / 10; b[1] = '0' + m % 10; b[2] = ':'; b[3] = '0' + s / 10; b[4] = '0' + s % 10; b[5] = 0;
  printb(18, y, b, 0);
}
static void roll_pct(u8 target, u8 y) {
  u8 cur = 0;
  for (;;) {
    put3(19, y, cur, '%'); summary_tick();
    if (cur >= target) return;
    cur = cur + 4 > target ? target : cur + 4;
  }
}
static void roll_time(u16 target, u8 y) {
  u16 cur = 0;
  for (;;) {
    put_time(y, cur); summary_tick();
    if (cur >= target) return;
    cur = cur + 5 > target ? target : cur + 5;
  }
}
static void summary_screen(void) {
  const char *t = level_titles[level_num - 1];
  ui_begin(1, C_A, 0);
  printa(centre(t), 1, t, 0);
  printb(12, 2, "finished", 0);
  printb(9, 5, "kills", 0); printb(9, 8, "items", 0); printb(9, 11, "secret", 0);
  printb(9, 14, "time", 0); printb(9, 17, "par", 0);
  SMS_displayOn();
  roll_pct(pct(num_kills, level_num_enemies), 5); wait_frames(60);
  roll_pct(pct(num_items_got, level_num_items), 8); wait_frames(60);
  roll_pct(pct(num_secrets_got, level_num_secrets), 11); wait_frames(60);
  if (map_time_sec >= 600) { printb(18, 14, "sucks", 0); summary_tick(); }
  else roll_time(map_time_sec, 14);
  wait_frames(60);
  roll_time(level_par_time, 17);
  wait_frames(60);
  printa(9, 21, "press a button", HI);
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

