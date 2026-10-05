;; SquareDoom SMS - column caster inner loop, hand-written Z80.
;; cast_rest(): walk one column's ray (DDA in castasm.c: dda_run) and handle
;; every sector crossing: near ceiling / floor flats, solid wall, upper and
;; lower ledges, clip-stack push.  Same rules as render.c / C64 render_*.asm.
;; Per-sector attributes are derived once when a sector is entered (S_CUR/S_NXT).
        .module cast
        .globl _cast_rest
        .globl _g_P, _g_cur, _g_nxt, _g_side, _g_eye, _g_fillv, _g_steps
        .globl _ytop, _ybot, _col, _cn, _clip_n, _clip_top, _clip_bot, _clip_z
        .globl _sec_bright, _sec_wdark, _sec_ceil, _sec_floor, _sec_ccol, _sec_fcol, _sec_type
        .globl _sec_seen, _sec_visited, _seen_gen, _sw_n
        .globl _lightfall
        .globl _wall_entry, _floor_entry, _rtab1, _rtab2, _rset_off, _rset_len, _rset_data
        .globl _dda_run, _fill_span, _sky_fill, _switch_face, _paint_switch, _door_colour
        CLIP_MAX = 8
        HORIZ = 12
        ;; sector attribute block
        A_DHC = 0       ; ceil - eye (signed)
        A_DHF = 1       ; floor - eye (signed)
        A_FPAT = 2
        A_WDARK = 3
        A_CFILL = 4     ; u16 ceiling flat entry, 0xFFFF = sky
        A_FFILL = 6     ; u16 floor flat entry, 0xFFFF = sky
        A_SIZE = 2

        .area _DATA
s_cur:  .ds A_SIZE
s_nxt:  .ds A_SIZE
oc_rowp: .ds 2
oc_rlen: .ds 1
oc_wpat: .ds 1
oc_wc:  .ds 1
oc_ncy: .ds 1
oc_nfy: .ds 1
oc_a:   .ds 1
oc_t:   .ds 1
oc_bo:  .ds 1

        .area _CODE

;; ---------------------------------------------------------------- helpers
;; A = sector, HL = block -> block[0] = ceil - eye, block[1] = floor - eye
sect_attrs::
        ex de,hl
        ld c,a
        ld b,#0
        ld hl,#_sec_ceil
        add hl,bc
        ld a,(hl)
        ld hl,#_g_eye
        sub a,(hl)
        ld (de),a
        inc de
        ld hl,#_sec_floor
        add hl,bc
        ld a,(hl)
        ld hl,#_g_eye
        sub a,(hl)
        ld (de),a
        ret

;; flat fill [B, C) of sector oc_cur with colour from table HL (ccol/fcol)
fill_flat_sec::
        ld a,(_g_cur)
        ld e,a
        ld d,#0
        add hl,de
        ld a,(hl)
        cp a,#3
        jr nz,1$
        ld a,b
        ld l,c
        jp _sky_fill
1$:     push bc
        add a,a
        ld l,a
        ld h,#0
        push de
        ld de,#_floor_entry
        add hl,de
        ld a,(hl)
        inc hl
        ld h,(hl)
        ld l,a                  ; hl = floor_entry[colour]
        pop de
        ex de,hl                ; de = entry, hl = sector
        ld bc,#_sec_bright
        add hl,bc
        ld a,(hl)
        cp a,#16
        jr nc,3$                ; full bright: pattern 0
        cpl
        and a,#15
        cp a,#15
        jr nz,2$
        ld de,#0                ; pattern 15: black
        jr 3$
2$:     ld l,a
        ld h,#0
        add hl,de
        ex de,hl
3$:     ld (_g_fillv),de
        pop bc
        ld a,b
        ld l,c
        jp _fill_span

;; A = dh (signed height - eye) -> A = row (signed). Clobbers BC DE HL
proj_dh::
        or a,a
        jr z,8$
        jp m,5$
        call rows_n             ; above the eye: row = 12 - n
        ld b,a
        ld a,#HORIZ
        sub a,b
        ret
5$:     neg                     ; below: row = 12 + n
        call rows_n
        add a,#HORIZ
        ret
8$:     ld a,#HORIZ
        ret
;; A = dh (1..) -> min(13, rows) from this crossing's row set
rows_n::
        cp a,#32
        jr c,0$
        ld a,#31
0$:     ld hl,#oc_rlen
        cp a,(hl)
        jr nc,9$
        ld hl,(oc_rowp)
        add a,l
        ld l,a
        jr nc,1$
        inc h
1$:     ld a,(hl)
        ret
9$:     ld a,#13
        ret

;; A = signed row -> clamped to [ytop, ybot]. Clobbers C
clamp_a::
        bit 7,a
        jr z,1$
        xor a,a
1$:     ld c,a
        ld a,(_ytop)
        cp a,c
        ret nc
        ld a,(_ybot)
        cp a,c
        ret c
        ld a,c
        ret

;; flat fill [B, C) with entry HL (0xFFFF = sky)
fill_flat::
        ld a,h
        and a,l
        inc a
        jr nz,1$
        ld a,b
        ld l,c
        jp _sky_fill
1$:     ld (_g_fillv),hl
        ld a,b
        ld l,c
        jp _fill_span
;; wall fill [B, C) with colour A and oc_wpat (cyan 3 = sky)
fill_wall::
        cp a,#3
        jr nz,1$
        ld a,b
        ld l,c
        jp _sky_fill
1$:     ld e,a
        ld a,(oc_wpat)
        cp a,#15
        jr c,2$
        ld hl,#0
        jr 3$
2$:     ld l,e
        ld h,#0
        add hl,hl
        ld de,#_wall_entry
        add hl,de
        ld e,(hl)
        inc hl
        ld d,(hl)
        ld l,a
        ld h,#0
        add hl,de
3$:     ld (_g_fillv),hl
        ld a,b
        ld l,c
        jp _fill_span

;; push (B = top, C = bot, z = P>>4)
clip_push::
        ld a,(_col)
        ld e,a
        ld d,#0
        ld hl,#_clip_n
        add hl,de
        ld a,(hl)
        cp a,#CLIP_MAX
        jr c,1$
        ld a,#CLIP_MAX-1
        jr 2$
1$:     inc (hl)
2$:     ld e,a
        ld a,(_cn)
        add a,e
        ld e,a
        ld hl,#_clip_top
        add hl,de
        ld (hl),b
        ld hl,#_clip_bot
        add hl,de
        ld (hl),c
        ld hl,(_g_P)
        ld a,h
        cp a,#16
        ld a,#255
        jr nc,3$
        ld a,l
        srl h
        rra
        srl h
        rra
        srl h
        rra
        srl h
        rra
3$:     ld hl,#_clip_z
        add hl,de
        ld (hl),a
        ret

;; ---------------------------------------------------------------- the column
;; entry: g_cur = start sector, ytop/ybot/clip state initialised, g_steps set
_cast_rest:
        push ix
        ld a,(_g_cur)
        or a,a
        jr z,100$
        ld hl,#s_cur
        call sect_attrs
100$:   call _dda_run
        ld hl,(_g_P)
        ld a,h
        or a,l
        jr nz,101$
        inc hl
        ld (_g_P),hl
101$:   ld a,(_g_cur)
        or a,a
        jr nz,110$
        ;; leaving the void
        ld a,(_g_nxt)
        or a,a
        jr nz,102$
        ld a,(_g_steps)
        or a,a
        jr nz,100$
        jp 199$
102$:   ld (_g_cur),a
        ld e,a
        ld d,#0
        ld hl,#_sec_seen
        add hl,de
        ld a,(_seen_gen)
        ld (hl),a
        ld hl,#_sec_visited
        add hl,de
        ld (hl),#0xff
        ld a,e
        ld hl,#s_cur
        call sect_attrs
        ld a,(_ytop)
        ld b,a
        ld a,(_ybot)
        ld c,a
        call clip_push
        jr 100$

110$:   ;; ======== a crossing at distance P ========
        ;; R and table tier
        ld hl,(_g_P)
        ld a,h
        cp a,#8
        jr nc,112$
        srl h
        rr l
        srl h
        rr l
        srl h
        rr l
        ld e,l
        ld d,#0
        ld hl,#_rtab1
        add hl,de
        ld a,(hl)
        jr 1140$
112$:   cp a,#64
        ld a,#255
        jr nc,113$
        add hl,hl
        add hl,hl
        ld a,h
113$:   ld e,a
        ld d,#0
        ld hl,#_rtab2
        add hl,de
        ld a,(hl)
1140$:  ld e,a
        ld d,#0
        ld hl,#_rset_len
        add hl,de
        ld a,(hl)
        ld (oc_rlen),a
        ld hl,#_rset_off
        add hl,de
        add hl,de
        ld e,(hl)
        inc hl
        ld d,(hl)
        ld hl,#_rset_data
        add hl,de
        ld (oc_rowp),hl
;; wall pattern = sector darkness + distance falloff (lightfall[P>>4]),
        ;; applied to every sector, full-bright ones included
114$:   ld hl,(_g_P)
        ld a,h
        cp a,#16
        ld a,#255
        jr nc,1141$
        ld a,l
        srl h
        rra
        srl h
        rra
        srl h
        rra
        srl h
        rra
1141$:  ld e,a
        ld d,#0
        ld hl,#_lightfall
        add hl,de
        ld b,(hl)               ; b = distance falloff
        ld a,(_g_cur)
        ld e,a
        ld hl,#_sec_wdark
        add hl,de
        ld a,(hl)
        inc a
        jr nz,115$
        xor a,a                 ; full bright: no sector darkness
        jr 116$
115$:   dec a
116$:   add a,b
        cp a,#15
        jr c,117$
        ld a,#15
117$:   ld (oc_wpat),a
        ld a,(_g_side)
        or a,a
        ld a,#8
        jr z,118$
        inc a
118$:   ld (oc_wc),a
        ;; near ceiling row + flat
        ld a,(s_cur+A_DHC)
        call proj_dh
        ld (oc_ncy),a
        call clamp_a
        ld c,a
        ld a,(_ytop)
        cp a,c
        jr nc,120$
        ld b,a
        push bc
        ld hl,#_sec_ccol
        call fill_flat_sec
        pop bc
        ld a,c
        ld (_ytop),a
120$:   ;; near floor row + flat (only if at/below the horizon)
        ld a,(s_cur+A_DHF)
        call proj_dh
        ld (oc_nfy),a
        bit 7,a
        jr nz,130$
        cp a,#HORIZ
        jr c,130$
        call clamp_a
        ld b,a
        ld a,(_ybot)
        ld c,a
        ld a,b
        cp a,c
        jr nc,130$
        push bc
        ld hl,#_sec_fcol
        call fill_flat_sec
        pop bc
        ld a,b
        ld (_ybot),a
130$:   ld a,(_ytop)
        ld b,a
        ld a,(_ybot)
        ld c,a
        cp a,b
        jr z,190$
        jr c,190$
        ld a,(_g_nxt)
        or a,a
        jr nz,140$
        ;; ---- solid wall (or switch)
        ld a,(_sw_n)
        or a,a
        jr z,132$
        call _switch_face
        or a,a
        jr z,132$
        ld a,(oc_wpat)
        ld l,a
        ld a,(oc_wc)
        call _paint_switch
        jr 133$
132$:   ld a,(_ytop)
        ld b,a
        ld a,(_ybot)
        ld c,a
        ld a,(oc_wc)
        call fill_wall
133$:   ld a,(_ybot)
        ld b,a
        ld c,a
190$:   call clip_push
199$:   pop ix
        ret

140$:   ;; ---- portal: mark nxt seen, derive its attributes
        ld e,a
        ld d,#0
        ld hl,#_sec_seen
        add hl,de
        ld a,(_seen_gen)
        ld (hl),a
        ld hl,#_sec_visited
        add hl,de
        ld (hl),#0xff
        ld hl,#_sec_type
        add hl,de
        ld a,(hl)
        and a,#0x1f
        ld (oc_a),a
        ld a,e
        ld hl,#s_nxt
        call sect_attrs
        ;; ---- upper ledge: ceil[nxt] < ceil[cur]
        ld a,(s_cur+A_DHC)
        ld b,a
        ld a,(s_nxt+A_DHC)
        cp a,b
        jp p,141$               ; (both small signed: plain compare via sign of diff)
141$:   sub a,b
        jp p,150$               ; nxt >= cur
        ld a,(s_nxt+A_DHC)
        call proj_dh
        call clamp_a
        ld (oc_bo),a
        ld a,(oc_ncy)
        call clamp_a
        ld (oc_t),a
        ld b,a
        ld a,(oc_bo)
        cp a,b
        jr z,146$
        jr c,146$
        ld c,a
        ld a,(oc_a)
        cp a,#2
        jr z,143$
        cp a,#3
        jr z,143$
        cp a,#4
        jr z,143$
        cp a,#11
        jr z,143$
        ld a,(oc_wc)
        call fill_wall
        jr 146$
143$:   push bc
        call _door_colour
        pop bc
        call fill_wall
146$:   ld a,(oc_bo)
        ld b,a
        ld a,(_ytop)
        cp a,b
        jr nc,147$
        ld a,b
        ld (_ytop),a
147$:   ld a,(_ytop)
        ld b,a
        ld a,(_ybot)
        ld c,a
        cp a,b
        jr z,148$
        jr nc,150$
148$:   call clip_push
        pop ix
        ret
150$:   ;; ---- lower ledge: floor[nxt] > floor[cur]
        ld a,(s_cur+A_DHF)
        ld b,a
        ld a,(s_nxt+A_DHF)
        sub a,b
        jp m,170$
        jr z,170$
        ld a,(s_nxt+A_DHF)
        call proj_dh
        call clamp_a
        ld (oc_t),a
        ld a,(oc_nfy)
        call clamp_a
        ld (oc_bo),a
        ld c,a
        ld a,(oc_t)
        ld b,a
        ld a,c
        cp a,b
        jr z,156$
        jr c,156$
        ld a,(oc_a)
        cp a,#5
        jr z,152$
        cp a,#6
        jr z,152$
        cp a,#10
        jr z,152$
        cp a,#12
        jr z,152$
        cp a,#15
        jr z,152$
        ld a,(oc_wc)
        jr 153$
152$:   ld a,(_g_nxt)
        ld e,a
        ld d,#0
        ld hl,#_sec_fcol
        add hl,de
        ld a,(hl)
153$:   call fill_wall
156$:   ld a,(oc_t)
        ld b,a
        ld a,(_ybot)
        cp a,b
        jr c,157$
        jr z,157$
        ld a,b
        ld (_ybot),a
157$:   ld a,(_ytop)
        ld b,a
        ld a,(_ybot)
        ld c,a
        cp a,b
        jr z,158$
        jr nc,170$
158$:   call clip_push
        pop ix
        ret
170$:   ;; ---- open: push, nxt becomes cur
        ld a,(_ytop)
        ld b,a
        ld a,(_ybot)
        ld c,a
        call clip_push
        ld a,(_g_nxt)
        ld (_g_cur),a
        ld hl,(s_nxt)
        ld (s_cur),hl
        ld a,(_g_steps)
        or a,a
        jp nz,100$
        pop ix
        ret
