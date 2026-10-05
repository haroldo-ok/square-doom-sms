/* Fixed-area loader for the front-end image blobs (title, menus, intermission). */
#include "defs.h"
#include "menu_data.h"

void img_load_ba(u8 bank, u16 addr, u16 tbase, u8 x, u8 y, u8 flags) {
  u8 ob = SMS_getROMBank(), w, h, np, r, c;
  const u8 *p;
  u16 nt;
  SMS_mapROMBank(bank);
  p = (const u8 *)addr;
  nt = p[0] | (p[1] << 8); w = p[2]; h = p[3]; np = p[4]; p += 5;
  if (flags & IMG_PAL) { if (np) SMS_loadBGPalette(p); if (np > 1) SMS_loadSpritePalette(p + 16); }
  if (flags & IMG_SPRPAL) SMS_loadSpritePalette(p);
  p += np * 16;
  SMS_loadTiles(p, tbase, nt * 32);
  p += nt * 32;
  if (!(flags & IMG_NOMAP)) {
    for (r = 0; r < h; r++) {
      SMS_setNextTileatXY(x, y + r);
      for (c = 0; c < w; c++, p += 2) SMS_setTile((p[0] | (p[1] << 8)) + tbase);
    }
  }
  SMS_mapROMBank(ob);
}

/* -> number of tiles the blob uses (to allocate the next piece after it) */
u16 img_load(u8 id, u16 tbase, u8 x, u8 y, u8 flags) {
  u8 ob = SMS_getROMBank();
  u16 nt;
  SMS_mapROMBank(img_bank[id]);
  nt = *(const u16 *)img_addr[id];
  SMS_mapROMBank(ob);
  img_load_ba(img_bank[id], img_addr[id], tbase, x, y, flags);
  return nt;
}
