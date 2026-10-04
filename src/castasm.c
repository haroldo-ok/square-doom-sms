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

/* advance the DDA until the map id changes (or OOB / step budget) -> g_nxt, g_P, g_side */
void dda_run(void) __naked {
  __asm
09301$:
    ld hl,(_g_sdx)
    ld de,(_g_sdy)
    ld a,l
    sub a,e
    ld a,h
    sbc a,d
    jr nc,09310$
    ; ---- x step: P = sdx; sdx += ddxp (saturating)
    ld (_g_P),hl
    ld de,(_g_ddxp)
    add hl,de
    jr nc,09302$
    ld hl,#0xffff
09302$:
    ld (_g_sdx),hl
    xor a,a
    ld (_g_side),a
    ld a,(_g_stepx)
    ld c,a
    ld a,(_g_mx)
    add a,c
    ld (_g_mx),a
    ld hl,(_g_mp)
    ld a,c
    rla
    sbc a,a
    ld b,a
    add hl,bc
    ld (_g_mp),hl
    jr 09320$
09310$:
    ; ---- y step
    ex de,hl
    ld (_g_P),hl
    ld de,(_g_ddyp)
    add hl,de
    jr nc,09311$
    ld hl,#0xffff
09311$:
    ld (_g_sdy),hl
    ld a,#1
    ld (_g_side),a
    ld a,(_g_stepy)
    ld c,a
    ld a,(_g_my)
    add a,c
    ld (_g_my),a
    ld hl,(_g_mp)
    ld bc,(_g_stepyp)
    add hl,bc
    ld (_g_mp),hl
09320$:
    ld a,(_g_mx)
    ld c,a
    ld a,(_g_my)
    or a,c
    and a,#0xe0
    jr nz,09330$
    ld a,(_g_cur)
    cp a,(hl)
    jr nz,09331$
    or a,a
    jr z,09331$
    ld hl,#_g_steps
    dec (hl)
    jr nz,09301$
09330$:
    xor a,a
    ld (_g_nxt),a
    ret
09331$:
    ld a,(hl)
    ld (_g_nxt),a
    ; portal into a sector that looks identical (same heights, colours and
    ; light): nothing to draw, just mark it seen and keep walking
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
    jp nz,09301$
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
    ; hl = colp + y0*64
    ld l,b
    ld h,#0
    add hl,hl
    add hl,hl
    add hl,hl
    add hl,hl
    add hl,hl
    add hl,hl
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
