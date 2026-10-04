/* VDP: double-buffered name tables (0x3800 / 0x3000), framebuffer upload. */
#include "defs.h"
__sfr __at 0xBF vdp_ctrl;
__sfr __at 0xBE vdp_data;

u16 fb[VIEW_H * VIEW_W];
u8 nt_back;            /* 0: back buffer = 0x3800, 1: back buffer = 0x3000 */

/* Copy fb (1536 bytes) to the back name table. outi+jr = 28 cycles/byte:
 * safe while the display is on. Interrupts only around the address write. */
void vdp_upload_fb(void) __naked {
  __asm
    ld a,(_nt_back)
    or a,a
    ld de,#0x7800
    jr z,09101$
    ld de,#0x7000
09101$:
    di
    ld a,e
    out (0xbf),a
    ld a,d
    out (0xbf),a
    ei
    ld hl,#_fb
    ld c,#0xbe
    ld a,#6
09102$:
    ld b,#0
09103$:
    outi
    jr nz,09103$
    dec a
    jr nz,09102$
    ret
  __endasm;
}

static void vdp_reg(u8 r, u8 v) { u8 rr = 0x80 | r; __asm__("di"); vdp_ctrl = v; vdp_ctrl = rr; __asm__("ei"); }

void vdp_flip(void) {
  vdp_reg(2, nt_back ? 0xFD : 0xFF);
  nt_back ^= 1;
}
/* UI screens draw with SMSlib at 0x3800: make it the front table */
void vdp_set_nt_front(void) { vdp_reg(2, 0xFF); nt_back = 1; }

void fb_clear(void) {
  u16 *p = fb; u16 n = VIEW_H * VIEW_W;
  while (n--) *p++ = 0;
}

void load_game_tiles(void) {
  u8 b = SMS_getROMBank();
  SMS_mapROMBank(GAMETILES_BANK);
  SMS_loadTiles((const void *)GAMETILES_ADDR, 0, 256 * 32);
  SMS_mapROMBank(b);
  SMS_loadBGPalette(game_bgpal);
  SMS_loadSpritePalette(game_sprpal);
}

void load_weapon_tiles(u8 w) {
  u8 b = SMS_getROMBank();
  const u8 *t = wtile_tab + w * 4;
  u16 addr = t[1] | (t[2] << 8);
  u8 n = t[3];
  SMS_mapROMBank(t[0]);
  if (n > 128) {
    SMS_loadTiles((const void *)addr, 256, 128 * 32);
    SMS_loadTiles((const void *)(addr + 128 * 32), 440, (n - 128) * 32);
  } else SMS_loadTiles((const void *)addr, 256, n * 32);
  SMS_mapROMBank(b);
}

/* in-game font: yellow text (sprite palette half of pair 4) */
u16 char_entry(char c) {
  const char *p;
  if (c >= 'a' && c <= 'z') return (T_FONT_AZ + c - 'a') | TILE_USE_SPRITE_PALETTE;
  if (c >= 'A' && c <= 'Z') return (T_FONT_AZ + c - 'A') | TILE_USE_SPRITE_PALETTE;
  if (c >= '0' && c <= '9') return (T_FONT_09Y + c - '0') | TILE_USE_SPRITE_PALETTE;
  for (p = FONT_PUNCT_CHARS; *p; p++)
    if (*p == c) return (T_FONT_PUNCT + (p - FONT_PUNCT_CHARS)) | TILE_USE_SPRITE_PALETTE;
  return 0;
}

void text_at(u8 x, u8 y, const char *s) {
  u16 *d = fb + y * VIEW_W + x;
  while (*s && x < VIEW_W) { *d++ = char_entry(*s++); x++; }
}

/* full-screen 3D mode: game tiles + both name tables cleared, display on */
void vdp_init_game(void) {
  SMS_displayOff();
  load_game_tiles();
  SMS_VRAMmemsetW(0x3000, 0, 0x700);
  SMS_VRAMmemsetW(0x3800, 0, 0x700);
  SMS_initSprites(); SMS_finalizeSprites(); SMS_copySpritestoSAT();
  nt_back = 1; vdp_flip();          /* front = 0x3000, back = 0x3800 */
  fb_clear();
  wpn_tiles_loaded = 0xff;
  last_playera = playera ^ 0x80;
  SMS_displayOn();
}

/* copy sat_* to the sprite attribute table (call in VBlank) */
extern u8 sat_n, sat_y[64], sat_xn[128];
void vdp_upload_sat(void) {
  u8 n = sat_n, i;
  const u8 *s;
  __asm__("di");
  vdp_ctrl = 0x00; vdp_ctrl = 0x7F;            /* 0x3F00 | write */
  for (s = sat_y, i = n; i; i--) vdp_data = *s++;
  vdp_data = 0xD0;
  vdp_ctrl = 0x80; vdp_ctrl = 0x7F;            /* 0x3F80 */
  for (s = sat_xn, i = n << 1; i; i--) vdp_data = *s++;
  __asm__("ei");
}
