/* Column raycaster with portal clipping (render_*.asm / editor previewView.js)
 * + billboard items/enemies (render_items.asm). */
#include "defs.h"
#include <string.h>
#define ddx_tab ((const u16 *)CAST_DDX)
#define ddy_tab ((const u16 *)CAST_DDY)
#define projT1 ((const u16 *)PROJT1_ADDR)
#define projT2 ((const u16 *)PROJT2_ADDR)
#define rz_tab ((const u16 *)RZ_ADDR)
u16 mul8x16hi(u8 a, u16 b);

#define CLIP_MAX 8
/* page-aligned (cast.s indexes them with H = page); kept clear of _DATA by mkrom's check */
__at(0xD900) u8 clip_top[256];
__at(0xDA00) u8 clip_bot[256];
__at(0xDB00) u8 clip_z[256];
u8 clip_n[VIEW_W];
s8 col_stepx[VIEW_W], col_stepy[VIEW_W];
static u8 col_ang[VIEW_W];
static u8 sky_base;
u8 last_playera;
u8 col_aim_slot[VIEW_W], col_aim_z[VIEW_W];

/* per-column cast state (globals: SDCC addresses them directly) */
extern u16 g_sdx, g_sdy, g_ddxp, g_ddyp, g_P, g_projT;
extern const u8 *g_mp;
extern u8 g_mx, g_my, g_cur, g_nxt, g_side, g_steps, g_eye;
extern s8 g_stepx, g_stepy;
extern s16 g_stepyp, g_stepxp;
extern u16 *g_colp;
extern u16 g_fillv;
void dda_run(void);
u8 projn_fast(u8 dh);
s8 proj_row(u8 h);
void fill_span(u8 y0, u8 y1);
u16 col_ddxp[VIEW_W], col_ddyp[VIEW_W];
const u8 *g_mp0;
void cast_columns(void);
u8 col, cn;
extern u8 ytop, ybot;
extern s16 g_eyeneg;
u8 tfx, tfy;


#define fill(y0, y1, e) do { g_fillv = (e); fill_span((y0), (y1)); } while (0)
static u8 clampspan(s8 y) {
  if (y < (s8)ytop) return ytop;
  if (y > (s8)ybot) return ybot;
  return (u8)y;
}
static u8 cp_i;
static void clip_push(u8 t, u8 b) {
  cp_i = clip_n[col];
  if (cp_i >= CLIP_MAX) cp_i = CLIP_MAX - 1;    /* keep the farthest closing entry */
  else clip_n[col] = cp_i + 1;
  cp_i += cn;
  clip_top[cp_i] = t; clip_bot[cp_i] = b;
  clip_z[cp_i] = (g_P >> 8) >= 16 ? 255 : (u8)(g_P >> 4);
}
static u16 wall_entry_for(u8 c, u8 pat) { return pat >= 15 ? 0 : wall_entry[c] + pat; }
static u16 floor_entry_for(u8 c, u8 pat) { return pat >= 15 ? 0 : floor_entry[c] + pat; }
void sky_fill(u8 y0, u8 y1) {
  const u8 *s;
  u16 *p;
  u8 sc = col + 4 + sky_base;
  while (sc >= 40) sc -= 40;
  s = ((const u8 *)CAST_SKY) + sc * 12;
  p = g_colp + ((u16)y0 << 5);
  for (; y0 < y1; y0++, p += VIEW_W) {
    u8 c = s[y0 < 12 ? y0 : 11];
    *p = c ? floor_entry[c] : 0;
  }
}
/* fill with a colour: cyan (3) is the sky strip, as fill_span */
static void fill_col(u8 y0, u8 y1, u8 c, u16 e) {
  if (c == 3) sky_fill(y0, y1); else fill(y0, y1, e);
}

/* wall U (0..255) at the current crossing; euclidean distance = P / fish */
static u8 wall_u(void) {
  u8 a = col_ang[col];
  s8 d; u8 m, f, u;
  u16 dist = (u16)(((u32)g_P << 8) / fishtab[col]);
  if (g_side == 0) { d = -costab[a]; f = tfy; } else { d = sintab[a]; f = tfx; }
  m = d < 0 ? -d : d; m = m >= 64 ? 255 : m << 2;
  m = (u8)mul8x16hi(m, dist);
  u = d < 0 ? f - m : f + m;
  if (g_side == 0 && sintab[a] > 0) u = ~u;
  if (g_side == 1 && costab[a] > 0) u = ~u;
  return u;
}
static s16 proj_full(u8 h) {
  s16 dh = (s16)h - g_eye;
  s32 n = ((s32)(dh < 0 ? -dh : dh) * 1024 + g_P - 1) / g_P;
  if (n > 200) n = 200;
  return dh > 0 ? HORIZON - (s16)n : HORIZON + (s16)n;
}
void paint_switch(u8 wc, u8 wpat) {
  s16 wt = proj_full(sec_ceil[g_cur]), wb = proj_full(sec_floor[g_cur]);
  s16 wh = wb - wt;
  u8 u = wall_u() >> 4, y;
  u16 we = wall_entry_for(wc, wpat);
  const u8 *t = (const u8 *)CAST_SWITCH + u * 16;
  if (wh <= 0) { fill(ytop, ybot, we); return; }
  for (y = ytop; y < ybot; y++) {
    s16 v = ((wb - 1 - y) * 16) / wh;
    u8 c;
    if (v < 0) v = 0; if (v > 15) v = 15;
    c = t[v];
    g_colp[(u16)y << 5] = c == 0xff ? we : solid_entry[c];
  }
}
u8 switch_face(void) {
  u8 f, i;
  if (g_side == 0) f = g_stepx < 0 ? 1 : 3; else f = g_stepy < 0 ? 2 : 0;
  for (i = 0; i < sw_n; i++) if (sw_sec[i] == g_cur && sw_dir[i] == f) return 1;
  return 0;
}

/* the sector-crossing core lives in cast.s */
void cast_rest(void);
extern u8 g_ix0, g_ix1, g_iy0, g_iy1, item_cand[];
u8 items_in_box(void);
s16 smul8(s8 a, s8 b);
u8 door_colour(void) {
  u8 u = wall_u();
  return (u < 0x20 || u >= 0xe0) ? sec_ccol[g_nxt] : sec_fcol[g_nxt];
}

static void cast_column(void) {
  u16 ddx = col_ddxp[col], ddy = col_ddyp[col];
  g_mx = playerx >> 11; g_my = playery >> 11;
  g_stepx = col_stepx[col]; g_stepy = col_stepy[col];
  g_stepyp = g_stepy < 0 ? -32 : 32;
  g_stepxp = g_stepx;
  g_ddxp = ddx; g_ddyp = ddy;
  g_mp = LV_MAP + ((u16)g_my << 5) + g_mx;
  g_sdx = mul8x16hi(g_stepx > 0 ? ~tfx : tfx, ddx);
  g_sdy = mul8x16hi(g_stepy > 0 ? ~tfy : tfy, ddy);
  ytop = 0; ybot = VIEW_H;
  cn = col * CLIP_MAX; clip_n[col] = 0;
  g_cur = plr_id;
  g_P = 0; clip_push(0, VIEW_H);
  g_steps = 64;
  cast_rest();
}

/* column rays for a new view angle: the DDA steps come precomputed from the
 * ray banks (128 bytes per angle), only the step signs are worked out here */
static void rebuild_col_rays(void) {
  u8 c, b = SMS_getROMBank();
  const u8 *p;
  sky_base = (u8)(playera * 5) >> 3;
  SMS_mapROMBank(BANK_RAYS0 + (playera >> 6));
  p = (const u8 *)0x8000 + (u16)(playera & 63) * 256;
  memcpy(col_ddxp, p, 64);
  memcpy(col_ddyp, p + 64, 64);
  memcpy(col_stepx, p + 128, 32);
  memcpy(col_stepy, p + 160, 32);
  SMS_mapROMBank(b);
  for (c = 0; c < VIEW_W; c++) col_ang[c] = playera + angtab[c];
}

/* ---------------------------------------------------------------- items */
#define MAX_VIS 48
u8 vis_slot[MAX_VIS], vis_sec[MAX_VIS];
static u8 vis_type[MAX_VIS], vis_depth[MAX_VIS], vis_zc[MAX_VIS], vis_order[MAX_VIS];
static s8 vis_dx[MAX_VIS], vis_dy[MAX_VIS], vis_lat[MAX_VIS];
u8 nvis;
static s8 isin, icos;

static s8 dir_tiles(s8 a) {
  u8 m = a < 0 ? -a : a;
  m = (u8)((u16)m * 3 >> 6);
  return a < 0 ? -(s8)m : (s8)m;
}
static u8 clamp31(s8 v) { return v < 0 ? 0 : v > 31 ? 31 : (u8)v; }

/* ix/iy world hi byte, frac = world lo byte */
/* (s16) x >> 6 without a shift loop */
static s16 asr6(s16 x) { u8 lo = (u8)x; s8 hi = (s8)(x >> 8); return ((s16)hi << 2) | (lo >> 6); }
static void vis_push(u8 slot, u8 type, u8 ix, u8 ixf, u8 iy, u8 iyf, u8 sec) {
  s16 dx16, dy16, d;
  s8 dx, dy;
  if (nvis >= MAX_VIS) return;
  dx16 = (s16)(((u16)ix << 8) | ixf) - (s16)playerx;
  dy16 = (s16)(((u16)iy << 8) | iyf) - (s16)playery;
  dx16 >>= 8; dy16 >>= 8;
  if (dx16 > 120 || dx16 < -120 || dy16 > 120 || dy16 < -120) return;
  dx = (s8)dx16; dy = (s8)dy16;
  d = smul8(dx, isin) - smul8(dy, icos);
  if (d <= 0) return;
  if ((d >> 6) == 0) return;
  {
    /* outside the ~68 degree view (plus a sprite's width)? skip the projection */
    s16 lat = smul8(dx, icos) + smul8(dy, isin);
    s16 al = lat < 0 ? -lat : lat;
    if (al > d + (8 << 6)) return;
    lat = asr6(lat);
    vis_lat[nvis] = lat > 127 ? 127 : lat < -127 ? -127 : (s8)lat;
  }
  vis_depth[nvis] = (d >> 6) > 255 ? 255 : (u8)(d >> 6);
  vis_zc[nvis] = (d >> 5) > 255 ? 255 : (u8)(d >> 5);
  vis_slot[nvis] = slot; vis_type[nvis] = type; vis_sec[nvis] = sec;
  vis_dx[nvis] = dx; vis_dy[nvis] = dy;
  nvis++;
}

static u8 mobj_type(u8 m) {
  if (mobj_info[m] >= MOBJINFO_IMPSHOT) {
    if (m == MOBJ_PLAYER_ROCKET) return IT_ROCKET;
    return (mobj_flags[m] & MF_PLASMA) ? IT_PLASMABALL : IT_FIREBALL;
  }
  if (mobj_corpse[m] != 0xff && mobj_corpse[m] >= IT_POSCORPSE) return mobj_corpse[m];
  return mobj_info[m] + 1;
}

/* -> 0: open aperture found (ap_t, ap_b), 1: occluded */
static u8 ap_t, ap_b;
static u8 clip_find(u8 c, u8 want) {
  u8 n = clip_n[c], base = c * CLIP_MAX, i, best = 0xff, equal = 0;
  for (i = 0; i < n; i++) {
    u8 z = clip_z[base + i];
    if (z < want) { best = i; equal = 0; }
    else if (z == want) { best = i; equal = 1; }
    else break;
  }
  if (best == 0xff) return 1;
  for (;;) {
    u8 t = clip_top[base + best], b = clip_bot[base + best];
    if (t < b) { ap_t = t; ap_b = b; return 0; }
    if (!equal || best == 0) return 1;
    best--; equal = 0;
  }
}

static u8 dv_type, dv_slot, dv_z, dv_feet, dv_mip, dv_mipw, dv_miph, dv_enemy, dv_mirror, dv_aim, dv_c, dv_l, dv_r;
static s8 dv_dx, dv_dy, dv_v;
static u16 dv_rz, dv_ustep, dv_vstep, dv_uacc;
static s16 dv_lat;
static const u8 *dv_tex;
static void draw_vis(u8 vi) {
  static s16 centre, left, right, top, bot, H, Wd;
  #define type dv_type
  #define slot dv_slot
  #define z dv_z
  #define feet dv_feet
  #define mip dv_mip
  #define mipw dv_mipw
  #define miph dv_miph
  #define is_enemy dv_enemy
  #define mirror dv_mirror
  #define aim dv_aim
  #define c dv_c
  #define l dv_l
  #define r dv_r
  #define rz dv_rz
  #define ustep dv_ustep
  #define vstep dv_vstep
  #define uacc dv_uacc
  #define tex dv_tex
  type = vis_type[vi]; slot = vis_slot[vi]; z = vis_depth[vi];
  is_enemy = 0; mirror = 0; aim = 0xff;
  rz = rz_tab[z];
  dv_v = vis_lat[vi];
  {
    /* v*32/z = |v| * rz >> 8 */
    u16 pv = mul8x16hi(dv_v < 0 ? -dv_v : dv_v, rz);
    if (pv > 127) pv = 127;
    centre = sprcol[dv_v < 0 ? (u8)(-(s8)pv) : (u8)pv];
  }
  if (centre >= 128) centre -= 256;
  if (!(slot & 0xC0) && type >= IT_SOLDIER && type <= IT_BARON) is_enemy = 1;
  if (is_enemy) { H = rz >> 6; } else { H = (rz + 64) >> 7; }
  if (H < 1) H = 1; if (H > 127) H = 127;
  if (is_enemy) {
    u8 st = mobj_state[slot], t = state_texture[st];
    Wd = H >> 1; if (!Wd) Wd = 1;
    if ((t & TEX_ANIMATE) && state_action[st] == ACTION_CHASE && (anim_frame & 2)) mirror = 1;
    tex = (const u8 *)0; mip = t & 0x3f;   /* frame */
  } else {
    Wd = H;
    if (type == IT_EXPLOSION) { H = H * 2 > 16 ? 16 : H * 2; Wd = H; }
    else if (type == IT_CANDELABRA || type == IT_TECHCOLUMN) { H = Wd * 2 > 127 ? 127 : Wd * 2; }
  }
  /* feet */
  if (type == IT_FIREBALL || type == IT_PLASMABALL) feet = missile_z;
  else if (type == IT_ROCKET) feet = procket_z;
  else feet = sec_floor[vis_sec[vi]];
  {
    s16 e = (s16)g_eye - feet;
    u16 m = mul8x16hi(e < 0 ? (u8)(-e) : (u8)e, rz);
    bot = (e < 0 ? -(s16)m : (s16)m) + HORIZON + 1;
  }
  top = bot - H;
  if (centre < -16 || centre > 47) return;
  Wd = ((u16)Wd * sprwscale[centre + 16] + 128) >> 8; if (!Wd) Wd = 1;
  left = centre - (Wd >> 1); right = left + Wd;
  if (right <= 0 || left >= VIEW_W) return;
  l = left < 0 ? 0 : (u8)left; r = right > VIEW_W ? VIEW_W : (u8)right;
  {
    s16 y0 = top < 0 ? 0 : top, y1 = bot > VIEW_H ? VIEW_H : bot;
    if (bot < 0 || top >= VIEW_H || y0 >= y1) return;
    if (!is_enemy && y1 - y0 > H) return;
  }
  /* mip + texture */
  if (is_enemy) {
    u8 fr = mip;
    mip = Wd >= 13 ? 0 : Wd >= 7 ? 1 : Wd >= 4 ? 2 : Wd >= 2 ? 3 : 4;
    tex = (const u8 *)enemy_mip_addr[fr * 5 + mip];
    mipw = enemy_mip_w[mip]; miph = enemy_mip_h[mip];
    if (mobj_health[slot] && mobj_info[slot] < MOBJINFO_IMPSHOT) aim = vi;
  } else {
    mip = Wd >= 8 ? 0 : Wd >= 4 ? 1 : Wd >= 2 ? 2 : 3;
    tex = (const u8 *)item_mip_addr[type * 4 + mip];
    mipw = item_mip_w[mip]; miph = mipw;
    if (type == IT_BARREL) aim = vi;
  }
  /* (mip << 8) / size via the 65536/size table: mip sizes are powers of two */
  {
    u8 ush = mipw == 16 ? 4 : mipw == 8 ? 5 : mipw == 4 ? 6 : mipw == 2 ? 7 : 8;
    u8 vsh = miph == 32 ? 3 : miph == 16 ? 4 : miph == 8 ? 5 : miph == 4 ? 6 : miph == 2 ? 7 : 8;
    ustep = recip_tab[Wd] >> ush;
    vstep = recip_tab[H] >> vsh;
  }
  {
    /* texel row per screen row, computed once per sprite */
    static u8 vrow[VIEW_H], msh;
    u8 y, ys = top < 0 ? 0 : (u8)top, ye = bot > VIEW_H ? VIEW_H : (u8)bot;
    u16 vacc = (u16)(ys - top) * vstep;
    for (y = ys; y < ye; y++, vacc += vstep) { u8 vv = vacc >> 8; vrow[y] = vv >= miph ? miph - 1 : vv; }
    msh = miph == 32 ? 5 : miph == 16 ? 4 : miph == 8 ? 3 : miph == 4 ? 2 : miph == 2 ? 1 : 0;
    {
      extern u8 sc_l, sc_r, sc_want, sc_aim, sc_z, sc_mipw, sc_mirror, sc_msh, sc_ys, sc_ye, sc_vrow[];
      extern u16 sc_uacc, sc_ustep;
      extern const u8 *sc_tex;
      void sprite_cols(void);
      for (y = ys; y < ye; y++) sc_vrow[y] = vrow[y];
      sc_l = l; sc_r = r; sc_want = vis_zc[vi]; sc_aim = aim; sc_z = z;
      sc_mipw = mipw; sc_mirror = mirror; sc_msh = msh; sc_ys = ys; sc_ye = ye;
      sc_uacc = (u16)(l - left) * ustep; sc_ustep = ustep; sc_tex = tex;
      sprite_cols();
    }
  }
  #undef type
  #undef slot
  #undef z
  #undef feet
  #undef mip
  #undef mipw
  #undef miph
  #undef is_enemy
  #undef mirror
  #undef aim
  #undef c
  #undef l
  #undef r
  #undef rz
  #undef ustep
  #undef vstep
  #undef uacc
  #undef tex
}

static u8 ri_i, ri_j, ri_x0, ri_x1, ri_y0, ri_y1, ri_key, ri_kd;
static void render_items(void) {
  isin = sintab[playera]; icos = costab[playera];
  for (ri_i = 0; ri_i < VIEW_W; ri_i++) col_aim_slot[ri_i] = 0xff;
  nvis = 0;
  {
    u8 ptx = playerx >> 11, pty = playery >> 11;
    s8 cx = clamp31((s8)ptx + dir_tiles(isin)), cy = clamp31((s8)pty + dir_tiles(-icos));
    ri_x0 = clamp31(cx - 3); ri_x1 = clamp31(cx + 2); ri_y0 = clamp31(cy - 3); ri_y1 = clamp31(cy + 2);
  }
  {
    u8 n, k;
    g_ix0 = ri_x0; g_ix1 = ri_x1; g_iy0 = ri_y0; g_iy1 = ri_y1;
    n = items_in_box();
    for (k = 0; k < n; k++) {
      u8 i = item_cand[k], tx = item_x[i], ty = item_y[i], sc = map_id(tx, ty);
      /* like Doom, only things in sectors the rays reached this frame */
      if (sec_seen[sc] != seen_gen) continue;
      vis_push(VIS_LAYER | i, item_t[i], (tx << 3) + 4, 0x80, (ty << 3) + 4, 0x80, sc);
    }
  }
  for (ri_i = 0; ri_i < MAX_MOBJ; ri_i++) {
    u8 m = ri_i, mx, my, sc;
    s8 d;
    if (!mobj_alloc[m]) continue;
    mx = mobj_x[m]; my = mobj_y[m];
    d = (s8)(mx - (u8)(playerx >> 8)); if (d > 120 || d < -120) continue;
    d = (s8)(my - (u8)(playery >> 8)); if (d > 120 || d < -120) continue;
    sc = map_id(mx >> 3, my >> 3);
    if (sec_seen[sc] != seen_gen) continue;
    if (mobj_info[m] >= MOBJINFO_IMPSHOT)
      vis_push(m, mobj_type(m), mx, mobj_xfrac[m], my, mobj_yfrac[m], sc);
    else
      vis_push(m, mobj_type(m), mx, 0x80, my, 0x80, sc);
  }
  for (ri_i = 0; ri_i < FX_MAX; ri_i++) {
    if (fx_kind[ri_i] != FX_EXPL) continue;
    vis_push(VIS_FX | ri_i, IT_EXPLOSION, (fx_tx[ri_i] << 3) + 4, 0x80, (fx_ty[ri_i] << 3) + 4, 0x80, map_id(fx_tx[ri_i], fx_ty[ri_i]));
  }
  for (ri_i = 0; ri_i < nvis; ri_i++) {
    ri_key = ri_i; ri_kd = vis_depth[ri_i];
    ri_j = ri_i;
    while (ri_j && vis_depth[vis_order[ri_j - 1]] < ri_kd) { vis_order[ri_j] = vis_order[ri_j - 1]; ri_j--; }
    vis_order[ri_j] = ri_key;
  }
  for (ri_i = 0; ri_i < nvis; ri_i++) draw_vis(vis_order[ri_i]);
}

void render_world(void) {
  u16 tpx, tpy;
  if (++seen_gen == 0) { clear_sector_seen(); seen_gen = 1; }
  tpx = playerx >> 3; tpy = playery >> 3;
  tfx = tpx & 255; tfy = tpy & 255;
  plr_id = sector_at(playerx >> 11, playery >> 11);
  player_sector = plr_id;
  mark_seen(plr_id);
  g_eye = eyeheight;
  g_eyeneg = -(s16)eyeheight;
  SMS_mapROMBank(CAST_BANK);
  if (playera != last_playera) { last_playera = playera; rebuild_col_rays(); }
  g_mp0 = LV_MAP + ((u16)(playery >> 11) << 5) + (playerx >> 11);
  cast_columns();
  SMS_mapROMBank(RENDER_BANK);
  render_items();
}
