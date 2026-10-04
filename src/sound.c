/* Sound: Doom PC-speaker effects on PSG tone 2 (C64 used SID voice 3),
 * converted SID tune on PSG tone 0/1 + noise (C64 music voices 1-2). */
#include "defs.h"
__sfr __at 0x7F psg_port;

static const u8 *sfx_ptr;
static u8 sfx_count, sfx_max;
static s8 sfx_index;
static u8 sfx_prio;
static const u8 *mus_ptr;
static u8 mus_on, mus_wait;
static u16 mus_acc;
static u8 mus_att[3];
u8 effects_vol, music_vol;

static void psg_tone(u8 ch, u16 p) {
  psg_port = 0x80 | (ch << 5) | (p & 15);
  psg_port = (p >> 4) & 0x3f;
}
static void psg_att(u8 ch, u8 a) { psg_port = 0x90 | (ch << 5) | (a & 15); }

void sound_init(void) {
  u8 c;
  for (c = 0; c < 4; c++) psg_att(c, 15);
  sfx_index = -1; sfx_prio = 0; mus_on = 0;
  mus_att[0] = mus_att[1] = mus_att[2] = 15;
}

/* higher-or-equal priority preempts (playsound.asm) */
void play_sound(u8 s) {
  u8 pr = sfx_priority[s];
  const u8 *p = sfx_data + sfx_off[s];
  if (pr < sfx_prio) return;
  __asm__("di");
  sfx_prio = pr;
  sfx_ptr = p;
  sfx_max = p[0];
  sfx_count = 0;
  sfx_index = s;
  __asm__("ei");
}

static u8 mus_out(u8 a) {
  u16 v = a + (15 - music_vol);
  return v > 15 ? 15 : (u8)v;
}
void music_start(void) {
  __asm__("di"); mus_ptr = music_data; mus_on = 1; mus_wait = 0; mus_acc = 0;
  __asm__("ei");
}
void music_stop(void) { mus_on = 0; psg_att(0, 15); psg_att(1, 15); psg_att(3, 15); }

static void music_tick(void) {
  u8 b, fl;
  if (mus_wait) { mus_wait--; return; }
  for (;;) {
    b = *mus_ptr++;
    if (b == 0) { mus_ptr = music_data + MUSIC_LOOP; continue; }
    break;
  }
  if (b & 0x80) { mus_wait = (b & 0x7f) - 1; return; }
  fl = b;
  if (fl & 1) { psg_tone(0, mus_ptr[0] | (mus_ptr[1] << 8)); mus_ptr += 2; }
  if (fl & 2) { mus_att[0] = *mus_ptr++; }
  if (fl & 4) { psg_tone(1, mus_ptr[0] | (mus_ptr[1] << 8)); mus_ptr += 2; }
  if (fl & 8) { mus_att[1] = *mus_ptr++; }
  if (fl & 16) { mus_att[2] = *mus_ptr++; }
  if (fl & 32) { psg_port = 0xE4 | (*mus_ptr++ & 3); }
  psg_att(0, mus_out(mus_att[0]));
  psg_att(1, mus_out(mus_att[1]));
  psg_att(3, mus_out(mus_att[2]));
}

/* once per frame from the frame interrupt */
void sound_isr(void) {
  if (sfx_index >= 0) {
    sfx_count++;
    if (sfx_count >= sfx_max) {
      psg_att(2, 15); sfx_index = -1; sfx_prio = 0;
    } else {
      u8 p = sfx_ptr[sfx_count];
      if (p == 0) psg_att(2, 15);
      else {
        u8 v = effects_vol;
        if (sfx_index == SND_SAWFUL) v >>= 2;
        psg_tone(2, pcs_period[p]);
        psg_att(2, 15 - v);
      }
    }
  }
  if (mus_on) {
    u16 lim = pal_mode ? 5000 : 6000;
    mus_acc += MUSIC_TICK_RATE_X100;
    while (mus_acc >= lim) { mus_acc -= lim; music_tick(); }
  }
}
