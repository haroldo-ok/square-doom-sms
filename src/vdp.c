/* VDP: double-buffered name tables (0x3800 / 0x3000), framebuffer upload. */
#include "defs.h"
__sfr __at 0xBF vdp_ctrl;
__sfr __at 0xBE vdp_data;

u16 fb[VIEW_H * VIEW_W];
extern u8 sat_n, sat_y[64], sat_xn[128];
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

/* Deferred flip: present_frame() queues the swap and returns at once, so the
 * next frame is computed while the display waits for VBlank. The frame
 * interrupt performs the swap (name table, sprite table, border colour); the
 * main thread calls vdp_sync() before touching the VDP again. */
volatile u8 flip_pending;
u8 flip_sat, flip_border;
void vdp_sync(void) { while (flip_pending) ; }
void vdp_isr_flip(void) {
  u8 n, i;
  const u8 *s;
  if (!flip_pending) return;
  vdp_ctrl = nt_back ? 0xFD : 0xFF; vdp_ctrl = 0x82;
  nt_back ^= 1;
  if (flip_sat) {
    n = sat_n;
    vdp_ctrl = 0x00; vdp_ctrl = 0x7F;
    for (s = sat_y, i = n; i; i--) vdp_data = *s++;
    vdp_data = 0xD0;
    vdp_ctrl = 0x80; vdp_ctrl = 0x7F;
    for (s = sat_xn, i = n << 1; i; i--) vdp_data = *s++;
  }
  vdp_ctrl = 0x0E; vdp_ctrl = 0xC0;            /* CRAM 0x1E: sprite palette entry 14 */
  vdp_data = flip_border;
  vdp_ctrl = 0x0E; vdp_ctrl = 0x87;            /* backdrop = sprite entry 14 */
  flip_pending = 0;
}
/* UI screens draw with SMSlib at 0x3800: make it the front table */
void vdp_set_nt_front(void) { vdp_sync(); vdp_reg(2, 0xFF); nt_back = 1; }

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
  SMS_loadSpritePalette(game_sprpal_doom);
}


/* in-game font: yellow text (sprite palette half of pair 4) */
u16 char_entry(char c) {
  u8 t;
  if (c >= 'a' && c <= 'z') c -= 32;
  if ((u8)c < 32 || (u8)c > 95) return 0;
  t = char_tiles[c - 32];
  return t ? t | TILE_USE_SPRITE_PALETTE : 0;
}

void text_at(u8 x, u8 y, const char *s) {
  u16 *d = fb + y * VIEW_W + x;
  while (*s && x < VIEW_W) { *d++ = char_entry(*s++); x++; }
}

/* full-screen 3D mode: game tiles + both name tables cleared, display on */
void vdp_init_game(void) {
  vdp_sync();
  SMS_displayOff();
  load_game_tiles();
  SMS_VRAMmemsetW(0x3000, 0, 0x700);
  SMS_VRAMmemsetW(0x3800, 0, 0x700);
  SMS_initSprites(); SMS_finalizeSprites(); SMS_copySpritestoSAT();
  nt_back = 1; vdp_flip();          /* front = 0x3000, back = 0x3800 */
  fb_clear();
  { extern u8 wpn_shown_pose; wpn_shown_pose = 0xff; }
  last_playera = playera ^ 0x80;
  SMS_displayOn();
}

/* copy sat_* to the sprite attribute table (call in VBlank) */
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
