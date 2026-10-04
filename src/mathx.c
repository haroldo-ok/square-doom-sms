/* small Z80 helpers */
#include "defs.h"
/* (a * b) >> 8 : A = a, DE = b, result in DE */
u16 mul8x16hi(u8 a, u16 b) __naked {
  (void)a; (void)b;
  __asm
    ld hl,#0
    ld c,#0
    ld b,#8
09201$:
    add hl,hl
    rl c
    rla
    jr nc,09202$
    add hl,de
    jr nc,09202$
    inc c
09202$:
    djnz 09201$
    ld e,h
    ld d,c
    ret
  __endasm;
}
u16 *fill_ptr; u16 fill_val; u8 fill_n;
/* write fill_val into fill_n cells down a column of the 32-wide framebuffer */
void fill_asm(void) __naked {
  __asm
    ld a,(_fill_n)
    or a,a
    ret z
    ld b,a
    ld hl,(_fill_ptr)
    ld de,(_fill_val)
09201$:
    ld (hl),e
    inc hl
    ld (hl),d
    ld a,l
    add a,#63
    ld l,a
    jr nc,09202$
    inc h
09202$:
    djnz 09201$
    ret
  __endasm;
}
