/* Automap (mapscreen.asm) + cheat codes (cheats.asm, entered as pad sequences). */
#include "defs.h"
static u8 ui_keys, ui_old, ui_pressed;
static void ui_read(void) {
  u8 e;
  __asm__("di"); e = pad_pressed; pad_pressed = 0; __asm__("ei");
  ui_old = ui_keys; ui_keys = pad_now;
  if (SMS_queryPauseRequested()) { SMS_resetPauseRequest(); e |= 0x40; }
  ui_pressed = e;
}
/* ---------------------------------------------------------------- automap + cheats */
static u8 cheat_seq[8], cheat_n, clev_sel;
static const u8 c_dqd[7] = {PAD_LEFT, PAD_RIGHT, PAD_LEFT, PAD_RIGHT, PAD_1, PAD_1, PAD_2};
static const u8 c_kfa[7] = {PAD_LEFT, PAD_RIGHT, PAD_LEFT, PAD_RIGHT, PAD_2, PAD_2, PAD_1};
static const u8 c_clev[7] = {PAD_LEFT, PAD_RIGHT, PAD_LEFT, PAD_RIGHT, PAD_1, PAD_2, PAD_2};
static char clev_msg[] = "change level e1m1";
static u8 cheat_match(const u8 *c) {
  u8 i;
  if (cheat_n < 7) return 0;
  for (i = 0; i < 7; i++) if (cheat_seq[(cheat_n - 7 + i) & 7] != c[i]) return 0;
  return 1;
}
static void cheat_input(u8 b) {
  u8 i;
  cheat_seq[cheat_n & 7] = b; cheat_n++;
  if (cheat_match(c_dqd)) {
    god_mode ^= 1; health = 100; hud_dirty = 1; cheat_n = 0;
    info_kind = 1; info_show("degreelessness mode"); play_sound(SND_ITEMUP);
  } else if (cheat_match(c_kfa)) {
    keys = 7; owned_weapons = 0x1f; armor = 200; combat_armor = 1;
    for (i = 0; i < 3; i++) ammo[i] = has_backpack ? ammo_max_pack[i] : ammo_max_base[i];
    hud_dirty = 1; cheat_n = 0;
    info_kind = 1; info_show("keys, full ammo"); play_sound(SND_ITEMUP);
  } else if (cheat_match(c_clev)) {
    cheat_n = 0; clev_sel = level_num;
  }
}

static void map_draw(u8 scroll) {
  u8 x, y;
  u16 *d = fb;
  const u8 *m = LV_MAP + ((u16)scroll << 5);
  for (y = 0; y < VIEW_H; y++) for (x = 0; x < VIEW_W; x++) {
    u8 id = *m++;
    *d++ = (id && sec_visited[id]) ? solid_entry[sec_fcol[id]] : 0;
  }
  {
    u8 px = playerx >> 11, py = playery >> 11;
    static const u8 arrow_dir[4] = {3, 0, 1, 2};
    if (py >= scroll && py < scroll + VIEW_H)
      fb[(py - scroll) * VIEW_W + px] = (T_ARROW + arrow_dir[(u8)(playera + 32) >> 6]) | TILE_USE_SPRITE_PALETTE;
  }
  if (clev_sel) {
    clev_msg[16] = '0' + clev_sel;
    text_at(0, VIEW_H - 1, clev_msg);
  } else if (info_len && info_kind) text_at(0, VIEW_H - 1, info_name);
}

void mapscreen(void) {
  s8 scroll;
  u8 hold = 0;
  input_paused = 1;
  hide_weapon();
  scroll = (s8)(playery >> 11) - 12;
  if (scroll < 0) scroll = 0; if (scroll > 8) scroll = 8;
  clev_sel = 0; cheat_n = 0;
  sat_n = 0;
  ui_keys = pad_now;
  for (;;) {
    map_draw(scroll);
    present_frame(1);
    ui_read();
    if (ui_pressed & 0x40) break;
    if (clev_sel) {
      if ((ui_pressed & PAD_UP) && clev_sel < 9) clev_sel++;
      if ((ui_pressed & PAD_DOWN) && clev_sel > 1) clev_sel--;
      if (ui_pressed & PAD_1) {
        level_num = clev_sel; clev = 1; end_level = 1; clev_sel = 0;
        info_kind = 1; info_show("change level");
        break;
      }
      if (ui_pressed & PAD_2) clev_sel = 0;
      continue;
    }
    if ((ui_keys & PAD_UP) && scroll > 0) scroll--;
    if ((ui_keys & PAD_DOWN) && scroll < 8) scroll++;
    if (ui_pressed & PAD_LEFT) cheat_input(PAD_LEFT);
    if (ui_pressed & PAD_RIGHT) cheat_input(PAD_RIGHT);
    if (ui_pressed & PAD_1) cheat_input(PAD_1);
    if (ui_pressed & PAD_2) cheat_input(PAD_2);
    if ((ui_keys & (PAD_1 | PAD_2)) == (PAD_1 | PAD_2)) { if (++hold >= 60) { end_level = 3; break; } }
    else hold = 0;
  }
  /* drop anything latched while the map was up */
  __asm__("di");
  in_use = in_turn_l = in_turn_r = in_fwd = in_back = in_strafel = in_strafer = in_wnext = in_wprev = 0;
  __asm__("ei");
  input_paused = 0;
  if (health) wpn_visible = 1;
  frame_ms = 0;
}
