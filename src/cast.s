;; SquareDoom SMS - column caster inner loop, hand-written Z80.
;; cast_rest(): walk one column's ray (DDA: castasm.c dda_run) and handle each
;; sector crossing: near ceiling / floor flats, solid wall, upper and lower
;; ledges, clip-stack push.  Same rules as the C64 render_*.asm.
;; Runs with the level's cast bank in slot 2: map, per-crossing info and a full
;; projection table (row for every row set x height difference), so projecting
;; a height is one indexed load.  ytop/ybot live in C/B during a crossing.
        .module cast
        .globl _cast_rest, _ytop, _ybot, _g_eyeneg
        .globl _g_P, _g_cur, _g_nxt, _g_side, _g_fillv, _g_steps, _g_mp
        .globl _col, _cn, _clip_n
        .globl _sec_bright, _sec_wdark, _sec_ceil, _sec_floor, _sec_ccol, _sec_fcol, _sec_type
        .globl _sec_seen, _sec_visited, _seen_gen, _sw_n, _sw_mask
        .globl _wall_entry, _flat_tab, _flat_row
        .globl _dda_run, _fill_span, _sky_fill, _switch_face, _paint_switch, _door_colour
        .include "gen/rinfo.inc"
        CLIP_MAX = 8
        HORIZ = 12
        CLIPT = 0xD9            ; page-aligned clip stacks (render.c: __at)

        .area _DATA
_ytop:: .ds 1                   ; ytop then ybot: ld bc,(_ytop) -> C = ytop, B = ybot
_ybot:: .ds 1
_g_eyeneg:: .ds 2               ; -eye height (set per frame)
oc_rowp: .ds 2                  ; row table for this crossing, already offset by -eye
oc_fall: .ds 1
oc_z:   .ds 1                   ; clip-stack z of this crossing (P>>4)
oc_wpat: .ds 1
oc_ncy: .ds 1                   ; near ceiling row (0..24)
oc_nfy: .ds 1                   ; near floor row
oc_e:   .ds 1
oc_cc:  .ds 1                   ; ceil / floor of g_cur (cached)
oc_cf:  .ds 1
oc_a:   .ds 1

        .area _CODE

;; ---------------------------------------------------------------- helpers
;; flat fill [H, L) with colour A, sector light of DE (D = 0); sky = colour 3
flat_fill::
        cp a,#3
        jr nz,1$
        ld a,h
        jp _sky_fill
1$:     push hl
        add a,a
        ld l,a
        ld h,#0
        ld bc,#_flat_row
        add hl,bc
        ld a,(hl)
        inc hl
        ld h,(hl)
        ld l,a                  ; &flat_tab[colour * 32]
        ex de,hl
        ld bc,#_sec_bright
        add hl,bc
        ld a,(hl)
        cp a,#17
        jr c,2$
        ld a,#16
2$:     add a,a
        ld l,a
        ld h,#0
        add hl,de
        ld a,(hl)
        inc hl
        ld h,(hl)
        ld l,a
        ld (_g_fillv),hl
        pop hl
        ld a,h
        jp _fill_span

;; wall fill [H, L) with colour A (cyan 3 = sky), wall pattern of g_cur + falloff
wall_fill::
        cp a,#3
        jr nz,1$
        ld a,h
        jp _sky_fill
1$:     push hl
        add a,a
        ld c,a                  ; colour * 2
        ld a,(_g_cur)
        ld e,a
        ld d,#0
        ld hl,#_sec_wdark
        add hl,de
        ld a,(hl)
        inc a
        jr nz,2$
        xor a,a
        jr 3$
2$:     dec a
3$:     ld hl,#oc_fall
        add a,(hl)
        cp a,#15
        jr c,4$
        ld hl,#0                ; pattern 15: black
        jr 5$
4$:     ld (oc_wpat),a
        ld e,c
        ld hl,#_wall_entry
        add hl,de
        ld e,(hl)
        inc hl
        ld d,(hl)
        ld l,a
        ld h,#0
        add hl,de
5$:     ld (_g_fillv),hl
        pop hl
        ld a,h
        jp _fill_span

;; push (H = top, L = bot, z = oc_z) on this column's clip stack
clip_push::
        ex de,hl
        ld a,(_col)
        ld c,a
        ld b,#0
        ld hl,#_clip_n
        add hl,bc
        ld a,(hl)
        cp a,#CLIP_MAX
        jr c,1$
        ld a,#CLIP_MAX-1
        jr 2$
1$:     inc (hl)
2$:     ld hl,#_cn
        add a,(hl)
        ld l,a
        ld h,#CLIPT
        ld (hl),d
        inc h
        ld (hl),e
        inc h
        ld a,(oc_z)
        ld (hl),a
        ret

;; ---------------------------------------------------------------- the column
_cast_rest:
        push ix
        call 800$
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
        ;; walking through void (player outside the sectors)
        ld a,(_g_nxt)
        or a,a
        jr nz,102$
        ld hl,(_g_mp)
        ld de,#-0x8000
        add hl,de
        ld a,h
        cp a,#4
        jp nc,199$
        ld a,(_g_steps)
        or a,a
        jr nz,100$
        jp 199$
102$:   ld (_g_cur),a
        call 800$
        ld a,(_g_cur)
        ld e,a
        ld d,#0
        ld hl,#_sec_seen
        add hl,de
        ld a,(_seen_gen)
        ld (hl),a
        ld hl,#_sec_visited
        add hl,de
        ld (hl),#0xff
        ld hl,(_ytop)
        ld a,l
        ld l,h
        ld h,a
        call clip_push
        jr 100$

110$:   ;; ======== a crossing at distance P ========
        ld hl,(_g_P)            ; rinfo index: P>>3 (near) or 256 + min(255, P>>6)
        ld a,h
        cp a,#8
        jr nc,112$
        ld a,l
        srl h
        rra
        srl h
        rra
        srl h
        rra
        ld l,a
        ld h,#0
        jr 114$
112$:   cp a,#64
        ld a,#255
        jr nc,113$
        add hl,hl
        add hl,hl
        ld a,h
113$:   ld l,a
        ld h,#1
114$:   add hl,hl
        add hl,hl
        ld de,#RINFO_ADDR
        add hl,de
        ld e,(hl)
        inc hl
        ld d,(hl)
        inc hl
        ld a,(hl)
        ld (oc_z),a
        inc hl
        ld a,(hl)
        ld (oc_fall),a
        ld hl,(_g_eyeneg)
        add hl,de
        ld (oc_rowp),hl         ; row of height h = (oc_rowp)[h]
        ;; ---- near ceiling: fill [ytop, row) with the ceiling flat
        ld a,(_g_cur)
        ld e,a
        ld d,#0
        ld a,(oc_cc)
        ld hl,(oc_rowp)
        add a,l
        ld l,a
        adc a,h
        sub a,l
        ld h,a
        ld a,(hl)
        ld (oc_ncy),a
        ld bc,(_ytop)           ; C = ytop, B = ybot
        cp a,c
        jr c,120$
        jr z,120$
        cp a,b
        jr c,115$
        ld a,b
115$:   ld (_ytop),a            ; new ytop
        ld l,a
        ld h,c                  ; [old ytop, new ytop)
        ld bc,#_sec_ccol
        ex de,hl
        add hl,bc
        ex de,hl
        ld a,(de)
        push af
        ld a,(_g_cur)
        ld e,a
        ld d,#0
        pop af
        call flat_fill
        ld a,(_g_cur)
        ld e,a
        ld d,#0
120$:   ;; ---- near floor: fill [row, ybot) if the row is at/below the horizon
        ld a,(oc_cf)
        ld hl,(oc_rowp)
        add a,l
        ld l,a
        adc a,h
        sub a,l
        ld h,a
        ld a,(hl)
        ld (oc_nfy),a
        cp a,#HORIZ
        jr c,130$
        ld bc,(_ytop)
        cp a,b
        jr nc,130$
        cp a,c
        jr nc,125$
        ld a,c
125$:   ld (_ybot),a
        ld h,a
        ld l,b                  ; [row, old ybot)
        ld bc,#_sec_fcol
        ex de,hl
        add hl,bc
        ex de,hl
        ld a,(de)
        push af
        ld a,(_g_cur)
        ld e,a
        ld d,#0
        pop af
        call flat_fill
130$:   ld bc,(_ytop)
        ld a,b
        cp a,c
        jr z,190$
        jr c,190$
        ld a,(_g_nxt)
        or a,a
        jr nz,140$
        ;; ---- solid wall (or switch)
        ld a,(_g_cur)           ; any switch face in this sector?
        ld e,a
        srl e
        srl e
        srl e
        ld d,#0
        ld hl,#_sw_mask
        add hl,de
        and a,#7
        ld b,a
        ld a,(hl)
        inc b
1320$:  rrca
        djnz 1320$
        rlca                    ; bit (cur & 7) -> bit 0 ... re-align
        bit 0,a
        jr z,132$
        call _switch_face
        or a,a
        jr z,132$
        call 900$               ; wall pattern -> oc_wpat
        ld a,(oc_wpat)
        ld l,a
        ld a,(_g_side)
        add a,#8
        call _paint_switch
        jr 133$
132$:   ld bc,(_ytop)
        ld h,c
        ld l,b
        ld a,(_g_side)
        add a,#8
        call wall_fill
133$:   ld a,(_ybot)
        ld h,a
        ld l,a
        call clip_push
        pop ix
        ret
190$:   ld h,c
        ld l,b
        call clip_push
199$:   pop ix
        ret
900$:   ld a,(_g_cur)           ; wall pattern for paint_switch
        ld e,a
        ld d,#0
        ld hl,#_sec_wdark
        add hl,de
        ld a,(hl)
        inc a
        jr nz,901$
        xor a,a
        jr 902$
901$:   dec a
902$:   ld hl,#oc_fall
        add a,(hl)
        cp a,#15
        jr c,903$
        ld a,#15
903$:   ld (oc_wpat),a
        ret

140$:   ;; ---- portal: mark nxt seen
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
        ;; ---- upper ledge: ceil[nxt] < ceil[cur]
        ld hl,#_sec_ceil
        add hl,de
        ld a,(hl)               ; a = ceil[nxt]
        ld hl,#oc_cc
        cp a,(hl)
        jr nc,150$
        ld hl,(oc_rowp)         ; bo = clamp(row(ceil[nxt]))
        add a,l
        ld l,a
        adc a,h
        sub a,l
        ld h,a
        ld a,(hl)
        ld bc,(_ytop)
        cp a,c
        jr nc,141$
        ld a,c
141$:   cp a,b
        jr c,142$
        ld a,b
142$:   ld (oc_e),a             ; bo
        ld a,(oc_ncy)           ; t = clamp(near ceiling row)
        cp a,c
        jr nc,143$
        ld a,c
143$:   cp a,b
        jr c,144$
        ld a,b
144$:   ld h,a
        ld a,(oc_e)
        ld l,a
        cp a,h
        jr z,147$
        jr c,147$
        ld a,(oc_a)             ; door jambs / door face colours
        cp a,#2
        jr z,145$
        cp a,#3
        jr z,145$
        cp a,#4
        jr z,145$
        cp a,#11
        jr z,145$
        ld a,(_g_side)
        add a,#8
        jr 146$
145$:   push hl
        call _door_colour
        pop hl
146$:   call wall_fill
147$:   ld a,(oc_e)             ; ytop = max(ytop, bo)
        ld bc,(_ytop)
        cp a,c
        jr c,148$
        ld (_ytop),a
        ld c,a
148$:   ld a,b
        cp a,c
        jr z,149$
        jr nc,1491$
149$:   ld h,c
        ld l,b
        call clip_push
        pop ix
        ret
1491$:  ld a,(_g_nxt)
        ld e,a
        ld d,#0
150$:   ;; ---- lower ledge: floor[nxt] > floor[cur]
        ld hl,#_sec_floor
        add hl,de
        ld b,(hl)               ; b = floor[nxt]
        ld a,(oc_cf)
        cp a,b
        jr nc,170$
        ld a,b
        ld hl,(oc_rowp)         ; t = clamp(row(floor[nxt]))
        add a,l
        ld l,a
        adc a,h
        sub a,l
        ld h,a
        ld a,(hl)
        ld bc,(_ytop)
        cp a,c
        jr nc,151$
        ld a,c
151$:   cp a,b
        jr c,152$
        ld a,b
152$:   ld (oc_e),a             ; t
        ld a,(oc_nfy)           ; bo = clamp(near floor row)
        cp a,c
        jr nc,153$
        ld a,c
153$:   cp a,b
        jr c,154$
        ld a,b
154$:   ld l,a
        ld a,(oc_e)
        ld h,a
        cp a,l
        jr nc,157$              ; nothing to draw
        ld a,(oc_a)             ; lifts / closets show the next floor colour
        cp a,#5
        jr z,155$
        cp a,#6
        jr z,155$
        cp a,#10
        jr z,155$
        cp a,#12
        jr z,155$
        cp a,#15
        jr z,155$
        ld a,(_g_side)
        add a,#8
        jr 156$
155$:   push hl
        ld a,(_g_nxt)
        ld e,a
        ld d,#0
        ld hl,#_sec_fcol
        add hl,de
        ld a,(hl)
        pop hl
156$:   call wall_fill
157$:   ld a,(oc_e)             ; ybot = min(ybot, t)
        ld bc,(_ytop)
        cp a,b
        jr nc,158$
        ld (_ybot),a
        ld b,a
158$:   ld a,b
        cp a,c
        jr z,159$
        jr nc,170$
159$:   ld h,c
        ld l,b
        call clip_push
        pop ix
        ret
170$:   ;; ---- open: push, nxt becomes cur
        ld bc,(_ytop)
        ld h,c
        ld l,b
        call clip_push
        ld a,(_g_nxt)
        ld (_g_cur),a
        call 800$
        ld a,(_g_steps)
        or a,a
        jp nz,100$
        pop ix
        ret
800$:   ld a,(_g_cur)           ; cache ceil/floor of the current sector
        ld e,a
        ld d,#0
        ld hl,#_sec_ceil
        add hl,de
        ld a,(hl)
        ld (oc_cc),a
        ld hl,#_sec_floor
        add hl,de
        ld a,(hl)
        ld (oc_cf),a
        ret

;; ---------------------------------------------------------------- all 32 columns
        .globl _cast_columns, _col_stepx, _col_stepy, _col_ddxp, _col_ddyp
        .globl _tfx, _tfy, _g_mp0, _plr_id, _fb, _g_colp, _g_stepx, _g_stepy
        .globl _g_stepxp, _g_stepyp, _g_ddxp, _g_ddyp, _g_sdx, _g_sdy, _mul8x16hi
_cast_columns:
        xor a,a
        ld (_col),a
        ld (_cn),a
        ld hl,#_fb
        ld (_g_colp),hl
300$:   ld a,(_col)
        ld e,a
        ld d,#0
        ld hl,#_col_stepx
        add hl,de
        ld a,(hl)
        ld (_g_stepx),a
        ld c,a
        rla
        sbc a,a
        ld b,a
        ld (_g_stepxp),bc
        ld hl,#_col_stepy
        add hl,de
        ld a,(hl)
        ld (_g_stepy),a
        ld bc,#32
        or a,a
        jp p,301$
        ld bc,#-32
301$:   ld (_g_stepyp),bc
        ld hl,#_col_ddyp
        add hl,de
        add hl,de
        ld c,(hl)
        inc hl
        ld b,(hl)
        ld (_g_ddyp),bc
        ld hl,#_col_ddxp
        add hl,de
        add hl,de
        ld e,(hl)
        inc hl
        ld d,(hl)
        ld (_g_ddxp),de
        ;; sdx = (stepx > 0 ? ~fracx : fracx) * ddx >> 8
        ld a,(_g_stepx)
        or a,a
        ld a,(_tfx)
        jp m,302$
        cpl
302$:   call _mul8x16hi
        ld (_g_sdx),de
        ld de,(_g_ddyp)
        ld a,(_g_stepy)
        or a,a
        ld a,(_tfy)
        jp m,303$
        cpl
303$:   call _mul8x16hi
        ld (_g_sdy),de
        ld hl,(_g_mp0)
        ld (_g_mp),hl
        ld hl,#0x1800           ; ytop = 0, ybot = 24
        ld (_ytop),hl
        ld a,(_col)
        ld e,a
        ld d,#0
        ld hl,#_clip_n
        add hl,de
        ld (hl),#0
        ld a,(_plr_id)
        ld (_g_cur),a
        ld hl,#0
        ld (_g_P),hl
        xor a,a
        ld (oc_z),a
        ld hl,#0x0018           ; push (0, 24, z = 0)
        call clip_push
        ld a,#64
        ld (_g_steps),a
        call _cast_rest
        ld hl,(_g_colp)
        inc hl
        inc hl
        ld (_g_colp),hl
        ld a,(_cn)
        add a,#CLIP_MAX
        ld (_cn),a
        ld a,(_col)
        inc a
        ld (_col),a
        cp a,#32
        jp nz,300$
        ret
