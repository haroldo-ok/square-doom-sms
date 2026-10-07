/* Hand-written Z80 for the column caster's hot paths. All state in globals. */
#include "defs.h"
u16 g_sdx, g_sdy, g_ddxp, g_ddyp, g_P, g_projT;
u16 *g_mp_dummy;
const u8 *g_mp;
u8 g_mx, g_my, g_cur, g_nxt, g_side, g_steps, g_eye;
s8 g_stepx, g_stepy;
s16 g_stepyp;
u16 *g_colp;
u16 g_fillv;

/* advance the DDA until the map id changes (or the step budget runs out)
 * -> g_nxt, g_P (distance of that crossing), g_side.  Register-resident loop:
 * HL = map pointer, E = current sector, D = steps left; HL' = sdx, DE' = sdy.
 * Every level has a void border, so a ray from inside the map always stops at a
 * map cell; the caller bounds-checks the pointer while walking through void. */
s16 g_stepxp;
void dda_run(void) __naked {
  __asm
    ld hl,(_g_sdx)
    ld de,(_g_sdy)
    exx
    ld hl,(_g_mp)
    ld a,(_g_cur)
    ld e,a
    ld a,(_g_steps)
    ld d,a
09301$:
    exx
    ld a,l
    sub a,e
    ld a,h
    sbc a,d
    jr nc,09310$
    ; ---- x step
    ld bc,(_g_ddxp)
    add hl,bc
    jr nc,09302$
    ld hl,#0xffff
09302$:
    exx
    ld bc,(_g_stepxp)
    add hl,bc
    ld a,(hl)
    cp a,e
    jr nz,09305$
    dec d
    jr nz,09301$
    xor a,a
09305$:
    ld b,#0                 ; side 0
    jr 09330$
09310$:
    ; ---- y step
    ex de,hl
    ld bc,(_g_ddyp)
    add hl,bc
    jr nc,09311$
    ld hl,#0xffff
09311$:
    ex de,hl
    exx
    ld bc,(_g_stepyp)
    add hl,bc
    ld a,(hl)
    cp a,e
    jr nz,09315$
    dec d
    jr nz,09301$
    xor a,a
09315$:
    ld b,#1                 ; side 1
09330$:
    ; A = next id (0 when out of steps), B = side
    ld (_g_nxt),a
    ld (_g_mp),hl
    ld a,d
    ld (_g_steps),a
    ld a,b
    ld (_g_side),a
    exx
    ld (_g_sdx),hl
    ld (_g_sdy),de
    ; P = (side ? sdy - ddy : sdx - ddx)
    or a,a
    jr nz,09332$
    ld bc,(_g_ddxp)
    jr 09333$
09332$:
    ex de,hl
    ld bc,(_g_ddyp)
09333$:
    or a,a
    sbc hl,bc
    ld (_g_P),hl
    ; portal into a sector that looks identical (same heights, colours and
    ; light): nothing to draw, just mark it seen and keep walking
    ld a,(_g_nxt)
    or a,a
    ret z
    ld e,a
    ld d,#0
    ld a,(_g_cur)
    or a,a
    ret z
    ld c,a
    ld b,#0
    ld hl,#_sec_floor
    add hl,de
    ld a,(hl)
    ld hl,#_sec_floor
    add hl,bc
    cp a,(hl)
    ret nz
    ld hl,#_sec_ceil
    add hl,de
    ld a,(hl)
    ld hl,#_sec_ceil
    add hl,bc
    cp a,(hl)
    ret nz
    ld hl,#_sec_fcol
    add hl,de
    ld a,(hl)
    ld hl,#_sec_fcol
    add hl,bc
    cp a,(hl)
    ret nz
    ld hl,#_sec_ccol
    add hl,de
    ld a,(hl)
    ld hl,#_sec_ccol
    add hl,bc
    cp a,(hl)
    ret nz
    ld hl,#_sec_bright
    add hl,de
    ld a,(hl)
    ld hl,#_sec_bright
    add hl,bc
    cp a,(hl)
    ret nz
    ld hl,#_sec_seen
    add hl,de
    ld a,(_seen_gen)
    ld (hl),a
    ld hl,#_sec_visited
    add hl,de
    ld (hl),#0xff
    ld a,e
    ld (_g_cur),a
    ld hl,#_g_steps
    dec (hl)
    jp nz,_dda_run
    xor a,a
    ld (_g_nxt),a
    ret
  __endasm;
}

/* rows from the horizon for a height difference dh: ceil(dh * T / 1024), max 13 */
u8 projn_fast(u8 dh) __naked {
  (void)dh;
  __asm
    ; A = dh (0..31). 24-bit acc = dh * T in C:H:L, shift-add over 5 bits
    ld de,(_g_projT)
    ld hl,#0
    ld c,#0
    and a,#0x1f
    rlca
    rlca
    rlca            ; dh in bits 7..3
    ld b,#5
09401$:
    add hl,hl
    rl c
    rla
    jr nc,09402$
    add hl,de
    jr nc,09402$
    inc c
09402$:
    djnz 09401$
    ; x = (C:H) = acc >> 8 ; n = (x + 3) >> 2 ; saturate 13
    ld a,c
    or a,a
    jr nz,09410$
    ld a,h
    cp a,#52
    jr nc,09410$
    add a,#3
    srl a
    srl a
    ret
09410$:
    ld a,#13
    ret
  __endasm;
}

/* projected row of height h (signed, horizon 12) */
s8 proj_row(u8 h) {
  u8 e = g_eye;
  if (h > e) return HORIZON - projn_fast(h - e);
  if (h < e) return HORIZON + projn_fast(e - h);
  return HORIZON;
}

/* g_colp[y0..y1) = g_fillv  (A = y0, L = y1): computed jump into 24 unrolled stores */
void fill_span(u8 y0, u8 y1) __naked {
  (void)y0; (void)y1;
  __asm
    ld b,a
    ld a,l
    sub a,b
    ret z
    ret c
    ; target = 09599$ - count*4
    add a,a
    add a,a
    ld c,a
    ld a,#<(09599$)
    sub a,c
    ld e,a
    ld a,#>(09599$)
    sbc a,#0
    ld d,a
    push de
    ; hl = colp + y0*64  (y0 < 32: (y0 >> 2) is the high byte, (y0 & 3) << 6 the low)
    ld a,b
    rrca
    rrca
    ld l,a
    and a,#0x3f
    ld h,a
    ld a,l
    and a,#0xc0
    ld l,a
    ld de,(_g_colp)
    add hl,de
    ld de,(_g_fillv)
    ld bc,#63
    ret
    ld (hl),e
    inc hl
    ld (hl),d
    add hl,bc
    ld (hl),e
    inc hl
    ld (hl),d
    add hl,bc
    ld (hl),e
    inc hl
    ld (hl),d
    add hl,bc
    ld (hl),e
    inc hl
    ld (hl),d
    add hl,bc
    ld (hl),e
    inc hl
    ld (hl),d
    add hl,bc
    ld (hl),e
    inc hl
    ld (hl),d
    add hl,bc
    ld (hl),e
    inc hl
    ld (hl),d
    add hl,bc
    ld (hl),e
    inc hl
    ld (hl),d
    add hl,bc
    ld (hl),e
    inc hl
    ld (hl),d
    add hl,bc
    ld (hl),e
    inc hl
    ld (hl),d
    add hl,bc
    ld (hl),e
    inc hl
    ld (hl),d
    add hl,bc
    ld (hl),e
    inc hl
    ld (hl),d
    add hl,bc
    ld (hl),e
    inc hl
    ld (hl),d
    add hl,bc
    ld (hl),e
    inc hl
    ld (hl),d
    add hl,bc
    ld (hl),e
    inc hl
    ld (hl),d
    add hl,bc
    ld (hl),e
    inc hl
    ld (hl),d
    add hl,bc
    ld (hl),e
    inc hl
    ld (hl),d
    add hl,bc
    ld (hl),e
    inc hl
    ld (hl),d
    add hl,bc
    ld (hl),e
    inc hl
    ld (hl),d
    add hl,bc
    ld (hl),e
    inc hl
    ld (hl),d
    add hl,bc
    ld (hl),e
    inc hl
    ld (hl),d
    add hl,bc
    ld (hl),e
    inc hl
    ld (hl),d
    add hl,bc
    ld (hl),e
    inc hl
    ld (hl),d
    add hl,bc
    ld (hl),e
    inc hl
    ld (hl),d
    add hl,bc
09599$:
    ret
  __endasm;
}

/* signed 8x8 -> 16 multiply via quarter squares: a*b = q(|a|+|b|) - q(||a|-|b||) */
s16 smul8(s8 a, s8 b) __naked {
  (void)a; (void)b;
  __asm
    ; A = a, L = b
    ld c,a
    xor a,l
    ld b,a          ; sign of the product in bit 7
    ld a,c
    bit 7,a
    jr z,09601$
    neg
09601$:
    ld c,a          ; |a|
    ld a,l
    bit 7,a
    jr z,09602$
    neg
09602$:
    ld e,a          ; |b|
    push bc
    add a,c
    ld l,a
    ld a,#0
    adc a,a
    ld h,a
    add hl,hl
    ld bc,#_qsq
    add hl,bc
    ld a,(hl)
    inc hl
    ld h,(hl)
    ld l,a          ; hl = q(|a|+|b|)
    pop bc
    ld a,c
    sub a,e
    jr nc,09603$
    neg
09603$:
    push hl
    ld l,a
    ld h,#0
    add hl,hl
    ld de,#_qsq
    add hl,de
    ld e,(hl)
    inc hl
    ld d,(hl)
    pop hl
    or a,a
    sbc hl,de
    bit 7,b
    jr z,09604$
    xor a,a
    sub a,l
    ld l,a
    sbc a,a
    sub a,h
    ld h,a
09604$:
    ex de,hl
    ret
  __endasm;
}

/* indices of items inside [ri_x0..ri_x1] x [ri_y0..ri_y1] -> cand[], returns count */
u8 g_ix0, g_ix1, g_iy0, g_iy1;
u8 item_cand[MAX_ITEMS];
u8 items_in_box(void) __naked {
  __asm
    ld a,(_n_items)
    or a,a
    ret z
    push ix
    ld b,a
    ld c,#0            ; index
    ld e,#0            ; count
    ld hl,#_item_x
    ld ix,#_item_y
09801$:
    ld a,(_g_ix0)
    ld d,a
    ld a,(hl)
    cp a,d
    jr c,09809$
    ld a,(_g_ix1)
    cp a,(hl)
    jr c,09809$
    ld a,(_g_iy0)
    ld d,a
    ld a,0 (ix)
    cp a,d
    jr c,09809$
    ld d,a
    ld a,(_g_iy1)
    cp a,d
    jr c,09809$
    push hl
    ld hl,#_item_cand
    ld d,#0
    add hl,de
    ld (hl),c
    pop hl
    inc e
09809$:
    inc hl
    inc ix
    inc c
    djnz 09801$
    ld a,e
    pop ix
    ret
  __endasm;
}

/* weapon overlay: wov_n records of (fb byte offset, entry) -> fb; then
 * wov_ns sprite y bytes -> sat_y and 2*wov_ns x/tile bytes -> sat_xn */
const u8 *wov_src; u8 wov_n, wov_ns;
extern u8 sat_y[64], sat_xn[128];
void wov_copy(void) __naked {
  __asm
    ld hl,(_wov_src)
    ld a,(_wov_n)
    or a,a
    jr z,09902$
    ld b,a
09901$:
    ld e,(hl)
    inc hl
    ld d,(hl)
    inc hl
    push hl
    ld hl,#_fb
    add hl,de
    ex de,hl
    pop hl
    ld a,(hl)
    ld (de),a
    inc hl
    inc de
    ld a,(hl)
    ld (de),a
    inc hl
    djnz 09901$
09902$:
    ld a,(_wov_ns)
    or a,a
    ret z
    ld c,a
    ld b,#0
    ld de,#_sat_y
    ldir
    ld a,(_wov_ns)
    add a,a
    ld c,a
    ld de,#_sat_xn
    ldir
    ret
  __endasm;
}

/* &flat_tab[colour * 32] for the caster */
const u16 * const flat_row[16] = {
  flat_tab + 0, flat_tab + 32, flat_tab + 64, flat_tab + 96, flat_tab + 128, flat_tab + 160,
  flat_tab + 192, flat_tab + 224, flat_tab + 256, flat_tab + 288, flat_tab + 320, flat_tab + 352,
  flat_tab + 384, flat_tab + 416, flat_tab + 448, flat_tab + 480 };

/* ---------------------------------------------------------------- sprite columns
 * Draw columns [sc_l, sc_r) of one billboard: per column find the clip-stack
 * aperture at depth sc_want, stamp the aim slot, and copy texels
 * tex[(u << sc_msh) + sc_vrow[y]] (0xff = transparent) as solid colour cells. */
u8 sc_l, sc_r, sc_want, sc_aim, sc_z, sc_mipw, sc_mirror, sc_msh, sc_ys, sc_ye;
u16 sc_uacc, sc_ustep;
const u8 *sc_tex;
u8 sc_vrow[24];
static u8 sc_c, sc_y0, sc_y1;
extern u8 clip_n[];
void sprite_cols(void) __naked {
  __asm
    ld a,(_sc_l)
    ld (_sc_c),a
09701$:
    ld a,(_sc_c)
    ld hl,#_sc_r
    cp a,(hl)
    ret nc
    ; ---- clip_find: last entry with z <= want (ties -> the entry before an equal closed one)
    ld e,a
    ld d,#0
    ld hl,#_clip_n
    add hl,de
    ld b,(hl)               ; n
    add a,a
    add a,a
    add a,a
    ld l,a                  ; base = c * 8
    ld h,#0xDB              ; clip_z page
    ld c,#0xff              ; best
    ld d,#0                 ; equal flag
    ld a,b
    or a,a
    jp z,09790$
    ld a,(_sc_want)
    ld e,a
09702$:
    ld a,(hl)
    cp a,e
    jr z,09703$
    jr nc,09705$            ; z > want: stop
    ld c,l
    ld d,#0
    jr 09704$
09703$:
    ld c,l
    ld d,#1
09704$:
    inc l
    djnz 09702$
09705$:
    ld a,c
    inc a
    jp z,09790$             ; nothing at or in front: occluded
09706$:
    ld l,c
    ld h,#0xD9              ; clip_top
    ld a,(hl)
    inc h
    cp a,(hl)               ; top < bot ?
    jr c,09708$
    ld a,d
    or a,a
    jp z,09790$
    ld a,(_sc_c)
    add a,a
    add a,a
    add a,a
    cp a,c
    jp z,09790$             ; best == 0
    dec c
    ld d,#0
    jr 09706$
09708$:
    ; aperture [top=a, bot=(hl)] -> y0 = max(ys, top), y1 = min(ye, bot)
    ld b,a
    ld a,(_sc_ys)
    cp a,b
    jr nc,09709$
    ld a,b
09709$:
    ld (_sc_y0),a
    ld b,(hl)
    ld a,(_sc_ye)
    cp a,b
    jr c,09710$
    ld a,b
09710$:
    ld (_sc_y1),a
    ld hl,#_sc_y0
    sub a,(hl)
    jp z,09790$
    jp c,09790$
    ld b,a                  ; rows to draw
    ; ---- aim stamp
    ld a,(_sc_aim)
    inc a
    jr z,09711$
    dec a
    ld e,(hl)               ; (keep) - reload column
    ld a,(_sc_c)
    ld e,a
    ld d,#0
    ld hl,#_col_aim_slot
    add hl,de
    ld a,(_sc_aim)
    ld (hl),a
    ld hl,#_col_aim_z
    add hl,de
    ld a,(_sc_z)
    ld (hl),a
09711$:
    ; ---- texture column: u = min(uacc >> 8, mipw - 1), mirrored if asked
    ld a,(_sc_uacc+1)
    ld hl,#_sc_mipw
    cp a,(hl)
    jr c,09712$
    ld a,(hl)
    dec a
09712$:
    ld c,a
    ld a,(_sc_mirror)
    or a,a
    jr z,09713$
    ld a,(hl)
    dec a
    sub a,c
    ld c,a
09713$:
    ld l,c
    ld h,#0
    ld a,(_sc_msh)
    or a,a
    jr z,09715$
09714$:
    add hl,hl
    dec a
    jr nz,09714$
09715$:
    ld de,(_sc_tex)
    add hl,de
    push hl
    pop iy                  ; iy = texture column
    ; fb pointer: fb + y0*64 + c*2
    ld a,(_sc_y0)
    rrca
    rrca
    ld l,a
    and a,#0x3f
    ld h,a
    ld a,l
    and a,#0xc0
    ld l,a
    ld a,(_sc_c)
    add a,a
    add a,l
    ld l,a
    ld de,#_fb
    add hl,de
    ; vrow pointer
    ld a,(_sc_y0)
    ld e,a
    ld d,#0
    push hl
    ld hl,#_sc_vrow
    add hl,de
    ex de,hl
    pop hl
    ; B = rows, DE = &vrow[y0], HL = fb cell, IY = texture column
09720$:
    ld a,(de)
    inc de
    push de
    push iy
    ex (sp),hl              ; hl = tex column, (sp) = fb cell
    add a,l
    ld l,a
    adc a,h
    sub a,l
    ld h,a
    ld a,(hl)               ; texel colour
    pop hl                  ; fb cell
    inc a
    jr z,09722$             ; transparent
    dec a
    add a,a
    ld e,a
    ld d,#0
    push hl
    ld hl,#_solid_entry
    add hl,de
    ld e,(hl)
    inc hl
    ld d,(hl)
    pop hl
    ld (hl),e
    inc hl
    ld (hl),d
    dec hl
09722$:
    pop de
    ld a,l
    add a,#64
    ld l,a
    jr nc,09723$
    inc h
09723$:
    djnz 09720$
09790$:
    ; next column
    ld hl,(_sc_uacc)
    ld de,(_sc_ustep)
    add hl,de
    ld (_sc_uacc),hl
    ld hl,#_sc_c
    inc (hl)
    jp 09701$
  __endasm;
}
