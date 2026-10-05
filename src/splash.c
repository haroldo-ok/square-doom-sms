/* Title screen: Doom 1.9 TITLEPIC (two palettes). Fixed area: maps its own bank. */
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
void show_splash(void) {
  SMS_displayOff();
  vdp_set_nt_front();
  SMS_initSprites(); SMS_finalizeSprites(); SMS_copySpritestoSAT();
  img_load_ba(TITLE_BANK, TITLE_ADDR, 0, 0, 0, IMG_PAL);
  SMS_setBackdropColor(0);
  SMS_displayOn();
  wait_button();
}
