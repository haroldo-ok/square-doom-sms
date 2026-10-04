/* Title splash (C64 Koala cover, converted). Fixed area: maps its own bank. */
#include "defs.h"
#include "menu_data.h"
static void wait_button(void) {
  u8 old = pad_now, k;
  for (;;) {
    SMS_waitForVBlank();
    k = pad_now;
    if (SMS_queryPauseRequested()) { SMS_resetPauseRequest(); break; }
    if ((k & ~old) & (PAD_1 | PAD_2)) break;
    old = k;
  }
}
/* ---------------------------------------------------------------- splash */
void show_splash(void) {
  u8 i, bank = SMS_getROMBank();
  SMS_displayOff();
  vdp_set_nt_front();
  SMS_initSprites(); SMS_finalizeSprites(); SMS_copySpritestoSAT();
  SMS_mapROMBank(BANK_SPLASH);
  SMS_loadTiles((const void *)SPLASH_TILES_ADDR, 0, SPLASH_TILES_SIZE);
  SMS_loadTileMap(0, 0, (const void *)SPLASH_MAP_ADDR, SPLASH_MAP_SIZE);
  for (i = 0; i < 16; i++) { SMS_setBGPaletteColor(i, c64_sms[i]); SMS_setSpritePaletteColor(i, c64_sms[i]); }
  SMS_mapROMBank(bank);
  SMS_displayOn();
  wait_button();
}

