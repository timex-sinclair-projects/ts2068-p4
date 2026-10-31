; q10probe: measure the SCLD's contention on a real TS2068 (PLAN.md §1.6,
; Q10), and the same here, so the two can be compared number for number.
;
; A code block at 32768, loaded from BASIC (q10probe.tap: CLEAR 32767,
; LOAD "" CODE, RANDOMIZE USR 32768). It takes the interrupt in IM 2 with
; its own handler, so the ROM's doesn't add time, and then for each test
; counts passes of a loop over two frames. A loop's one access under test
; is the first instruction; the rest runs from uncontended RAM:
;
;   ld a,(hl) / ld (hl),a / in a,(c)    the access
;   inc de                              the count
;   ld a,(FLAG) : or a                  the handler counts FLAG down
;   pad: 11 to 18 T-states, 3 bytes     the loop's period, P
;   jr nz,loop
;
; A loop whose period is a multiple of 8 T-states meets an 8-T-state
; contention pattern at the same place every pass; other periods walk
; through it. So the eight pads, P = 53..60 (65 for IN), give eight counts
; whose losses against an uncontended loop depend on the pattern's shape,
; not just its average (Q10a). Long counts average out the 0-3 T-states
; that the interrupt's timing varies by on a real machine.
;
; The tests (counts are of 2 frames, except FRM):
;
;   REF   read 8000h, uncontended: the reference for the next six
;   R40   read 4000h   R7F  read 7FFFh   (HOME RAM, both ends of 16K)
;   W5A   write 5AFFh  (the last attribute byte, put back afterwards)
;   X5A   the loop run from 5AE0h, reading 8000h: opcode fetches (Q10d)
;   EXR   read 4000h with the EXROM paged into chunk 2
;   DCK   read 4000h with the DOCK paged into chunk 2
;   INFB  IN from 80FBh: no SCLD port, high byte uncontended; the
;         reference for the ports (Q10c)
;   80FE 40FE 80FF 40FF 80F4 40F4 40FB    the SCLD's ports, and FB with a
;         contended high byte
;   FRM   read 8000h for 50 frames: the frame's length
;
; The screen shows REF, INFB and FRM as counts and the rest as passes lost
; against their reference (positive: slower). The counts are also left at
; RESULTS, 16 tests x 8 pads x 2 bytes, in this order, for
; tools/host/q10probe.py: SAVE "q10res" CODE 37376,256.
;
; Last, the snow test (Q10d): I points at 4000h for about 5 seconds while
; code runs. A Spectrum shows snow then; the TS2068 is said not to.
;
; sjasmplus q10probe.asm  ->  q10probe.bin, q10probe-code.tap

        DEVICE ZXSPECTRUM48

CHARS   equ 0x5c36              ; system variable: the font, less 256
HSR     equ 0xf4
DEC     equ 0xff
TABLE   equ 0x9000              ; IM 2 vectors: 257 bytes of 91h
ISR     equ 0x9191
RESULTS equ 0x9200              ; 37376
STACK   equ 0x9400
ATTR23  equ 0x5ae0              ; attribute row 23: X5A's loop, W5A's byte
NTESTS  equ 16

        org 0x8000
        jp main

; ---- the loops ---------------------------------------------------------

memloop:
ml_acc: ld a,(hl)               ; or ld (hl),a for W5A
        inc de
        ld a,(FLAG)
        or a
ml_pad: db 0,0,0
        jr nz,memloop
        ret
memloop_len equ $-memloop

inloop: in a,(c)
        inc de
        ld a,(FLAG)
        or a
il_pad: db 0,0,0
        jr nz,inloop
        ret

; The pads, 11 to 18 T-states. After `or a` the carry is clear, so
; `ret c` (D8h) is 5 T-states, not taken; `ld a,0` and `inc` keep the
; flags. The 6-T-state `inc` is of a pair the loop doesn't use.
padmem: db 0x3e,0,0, 0,0,0, 0,0,0xd8, 0,0xd8,0xd8
        db 0xd8,0xd8,0xd8, 0xd8,0xd8,0x03, 0xd8,0x03,0x03, 0x03,0x03,0x03
padin:  db 0x3e,0,0, 0,0,0, 0,0,0xd8, 0,0xd8,0xd8
        db 0xd8,0xd8,0xd8, 0xd8,0xd8,0x23, 0xd8,0x23,0x23, 0x23,0x23,0x23

; type (0 read, 1 write, 2 run from ATTR23, 3 IN), HSR, DEC, address or
; port, frames
tests:  db 0,0,0x00 : dw 0x8000 : db 2     ; REF
        db 0,0,0x00 : dw 0x4000 : db 2     ; R40
        db 0,0,0x00 : dw 0x7fff : db 2     ; R7F
        db 1,0,0x00 : dw 0x5aff : db 2     ; W5A
        db 2,0,0x00 : dw 0x8000 : db 2     ; X5A
        db 0,4,0x80 : dw 0x4000 : db 2     ; EXR
        db 0,4,0x00 : dw 0x4000 : db 2     ; DCK
        db 3,0,0x00 : dw 0x80fb : db 2     ; INFB
        db 3,0,0x00 : dw 0x80fe : db 2
        db 3,0,0x00 : dw 0x40fe : db 2
        db 3,0,0x00 : dw 0x80ff : db 2
        db 3,0,0x00 : dw 0x40ff : db 2
        db 3,0,0x00 : dw 0x80f4 : db 2
        db 3,0,0x00 : dw 0x40f4 : db 2
        db 3,0,0x00 : dw 0x40fb : db 2
        db 0,0,0x00 : dw 0x8000 : db 50    ; FRM

; ---- running the tests -------------------------------------------------

main:   di
        push af                 ; every register kept, on the caller's stack
        push bc
        push de
        push hl
        push ix
        push iy
        exx
        ex af,af'
        push af
        push bc
        push de
        push hl
        ld (savesp),sp
        ld sp,STACK
        ld a,i
        ld (savei),a
        ld hl,TABLE
        ld de,TABLE+1
        ld bc,256
        ld (hl),ISR/256
        ldir
        ld a,TABLE/256
        ld i,a
        im 2
        ld hl,0x4000            ; clear the picture (the colours stay)
        ld de,0x4001
        ld bc,0x17ff
        ld (hl),0
        ldir
        ld hl,ATTR23            ; keep attribute row 23
        ld de,saveattr
        ld bc,32
        ldir

        ld hl,tests
        ld (curt),hl
        ld hl,RESULTS
        ld (curres),hl
        ld b,NTESTS
tloop:  push bc
        xor a
rloop:  ld (curr),a
        call runone
        ld a,(curr)
        inc a
        cp 8
        jr c,rloop
        ld hl,(curt)
        ld de,6
        add hl,de
        ld (curt),hl
        pop bc
        djnz tloop

        ld hl,saveattr
        ld de,ATTR23
        ld bc,32
        ldir
        ei
        call show
        call snow

        di
        im 1
        ld a,(savei)
        ld i,a
        ld sp,(savesp)
        pop hl
        pop de
        pop bc
        pop af
        ex af,af'
        exx
        pop iy
        pop ix
        pop hl
        pop de
        pop bc
        pop af
        ei
        ret

; One test, (curt), with pad (curr): its count into (curres), advanced.
runone: ld ix,(curt)
        ld a,(curr)             ; the pad: 3 bytes at r * 3
        ld c,a
        add a,a
        add a,c
        ld l,a
        ld h,0
        ld a,(ix+0)
        cp 3
        jr z,ro_in
        ld de,padmem
        add hl,de
        ld de,ml_pad
        ldi
        ldi
        ldi
        ld a,0x7e               ; ld a,(hl)
        ld hl,memloop
        ld c,(ix+0)
        dec c
        jr nz,ro_nw
        ld a,0x77               ; ld (hl),a
ro_nw:  ld (ml_acc),a
        dec c
        jr nz,ro_go
        ld hl,memloop           ; X5A: the loop into attribute row 23
        ld de,ATTR23
        ld bc,memloop_len
        ldir
        ld hl,ATTR23
        jr ro_go
ro_in:  ld de,padin
        add hl,de
        ld de,il_pad
        ldi
        ldi
        ldi
        ld hl,inloop
ro_go:  ld (tgt+1),hl
        ld a,(ix+5)
        ld (nfr),a
        ld a,(ix+1)
        out (HSR),a
        ld a,(ix+2)
        out (DEC),a
        ld l,(ix+3)             ; HL for the memory loops, BC for IN
        ld h,(ix+4)
        ld b,h
        ld c,l
        ei
        halt                    ; sync to the frame
        ld a,(nfr)
        ld (FLAG),a
        ld de,0
tgt:    call 0
        xor a
        out (HSR),a
        out (DEC),a
        ld hl,(curres)
        ld (hl),e
        inc hl
        ld (hl),d
        inc hl
        ld (curres),hl
        ret

; ---- the results on screen ---------------------------------------------

; row, kind (0: counts, pads 11-14; 1: counts, 15-18; 2: losses), test,
; reference test (losses) or width (counts), label
rows:   db 2,0,0,5 : dw l_ref
        db 3,1,0,5 : dw l_none
        db 4,2,1,0 : dw l_r40
        db 5,2,2,0 : dw l_r7f
        db 6,2,3,0 : dw l_w5a
        db 7,2,4,0 : dw l_x5a
        db 8,2,5,0 : dw l_exr
        db 9,2,6,0 : dw l_dck
        db 10,0,7,5 : dw l_infb
        db 11,1,7,5 : dw l_none
        db 12,2,8,7 : dw l_80fe
        db 13,2,9,7 : dw l_40fe
        db 14,2,10,7 : dw l_80ff
        db 15,2,11,7 : dw l_40ff
        db 16,2,12,7 : dw l_80f4
        db 17,2,13,7 : dw l_40f4
        db 18,2,14,7 : dw l_40fb
        db 19,0,15,6 : dw l_frm
        db 20,1,15,6 : dw l_none
        db 0xff

show:   ld hl,0x5800+1*32   ; the loss columns, 3 wide, packed: shade
        call stripe             ; every other one (header, rows 4-9, 12-18)
        ld hl,0x5800+4*32
        ld b,6
sw1:    call stripe
        djnz sw1
        ld hl,0x5800+12*32
        ld b,7
sw2:    call stripe
        djnz sw2
        ld bc,0x0000
        call at
        ld hl,s_title
        call pstr
        ld bc,0x0100
        call at
        ld hl,s_head
        call pstr
        ld ix,rows
sh1:    ld b,(ix+0)
        ld a,b
        inc a
        ret z
        ld c,0
        call at
        ld l,(ix+4)
        ld h,(ix+5)
        call pstr
        ld a,(ix+1)
        cp 2
        jr z,sh_loss
        ld b,0                  ; counts: four of them
        or a
        jr z,sh2
        ld b,4
sh2:    ld c,4
sh3:    push bc
        ld a,(ix+2)
        call result
        ld b,(ix+3)
        ld c,0
        call pnum
        pop bc
        inc b
        dec c
        jr nz,sh3
        jr sh_next
sh_loss:
        ld b,0
sh4:    push bc
        ld a,(ix+3)             ; reference - test
        call result
        push hl
        ld a,(ix+2)
        call result
        ex de,hl
        pop hl
        or a
        sbc hl,de
        ld bc,0x0301            ; width 3, signed
        call pnum
        pop bc
        inc b
        ld a,b
        cp 8
        jr c,sh4
sh_next:
        ld de,6
        add ix,de
        jr sh1

; Attribute row HL: black on white, columns 4-27 in threes, white and
; cyan by turns; HL to the next row
stripe: push bc
        ld b,32
        ld c,0
st1:    ld a,c
        sub 4
        jr c,st_w
        cp 24
        jr nc,st_w
        ld d,0
st2:    sub 3                   ; A / 3, odd or even
        jr c,st3
        inc d
        jr st2
st3:    bit 0,d
        jr z,st_w
        ld (hl),0x28            ; cyan paper
        jr st4
st_w:   ld (hl),0x38            ; white paper
st4:    inc hl
        inc c
        djnz st1
        pop bc
        ret

; HL = the count of test A, pad B
result: push de
        ld l,a
        ld h,0
        add hl,hl
        add hl,hl
        add hl,hl
        ld e,b
        ld d,0
        add hl,de
        add hl,hl
        ld de,RESULTS
        add hl,de
        ld a,(hl)
        inc hl
        ld h,(hl)
        ld l,a
        pop de
        ret

; Print HL right-aligned in B columns, signed if C is 1; stars if it
; doesn't fit.
pnum:   push bc
        ld a,c
        ld (neg),a
        ld de,numbuf            ; six spaces
        ld b,6
pn1:    ld a,' '
        ld (de),a
        inc de
        djnz pn1
        ld a,(neg)
        or a
        jr z,pn2
        xor a
        bit 7,h
        jr z,pn2
        ex de,hl                ; negative: HL = -HL
        ld hl,0
        sbc hl,de
        ld a,1
pn2:    ld (neg),a
        ld de,numbuf+5
pn3:    push de
        call div10
        pop de
        add a,'0'
        ld (de),a
        dec de
        ld a,h
        or l
        jr nz,pn3
        ld a,(neg)
        or a
        jr z,pn4
        ld a,'-'
        ld (de),a
pn4:    pop bc                  ; B: the width
        ld a,6
        sub b
        ld c,a                  ; the first 6-B must be spaces
        ld hl,numbuf
        or a
        jr z,pn6
pn5:    ld a,(hl)
        cp ' '
        jr nz,pn_star
        inc hl
        dec c
        jr nz,pn5
pn6:    ld a,(hl)
        call pchar
        inc hl
        djnz pn6
        ret
pn_star:
        ld a,'*'
        call pchar
        djnz pn_star
        ret

; HL = HL / 10, A = the remainder
div10:  xor a
        ld b,16
dv1:    add hl,hl
        rla
        cp 10
        jr c,dv2
        sub 10
        inc l
dv2:    djnz dv1
        ret

; The print position: row B, column C
at:     ld a,b
        and 0x18
        or 0x40
        ld h,a
        ld a,b
        and 7
        rrca
        rrca
        rrca
        or c
        ld l,a
        ld (pos),hl
        ret

pstr:   ld a,(hl)
        or a
        ret z
        call pchar
        inc hl
        jr pstr

pchar:  push hl
        push de
        push bc
        ld l,a
        ld h,0
        add hl,hl
        add hl,hl
        add hl,hl
        ld de,(CHARS)
        add hl,de
        ld de,(pos)
        ld b,8
pc1:    ld a,(hl)
        ld (de),a
        inc hl
        inc d
        djnz pc1
        ld hl,pos
        inc (hl)
        pop bc
        pop de
        pop hl
        ret

; ---- snow (Q10d) -------------------------------------------------------

snow:   ld bc,0x1500
        call at
        ld hl,s_snow
        call pstr
        di
        im 1                    ; I is no longer a vector table
        ld a,0x40
        ld i,a
        ei
        ld e,10                 ; about 5 seconds of code running
sn1:    ld bc,0
sn2:    dec bc
        ld a,b
        or c
        jr nz,sn2
        dec e
        jr nz,sn1
        di
        ld a,TABLE/256
        ld i,a
        im 2
        ei
        ld bc,0x1500
        call at
        ld hl,s_snowd
        jp pstr

s_title: db "Q10 SCLD probe 1  2 frames/count",0
s_head:  db "pad  11 12 13 14 15 16 17 18",0
s_snow:  db "Snow test: I=40h, about 5 s    ",0
s_snowd: db "Snow test done: any specks?    ",0
l_ref:   db "REF ",0
l_none:  db "    ",0
l_r40:   db "R40 ",0
l_r7f:   db "R7F ",0
l_w5a:   db "W5A ",0
l_x5a:   db "X5A ",0
l_exr:   db "EXR ",0
l_dck:   db "DCK ",0
l_infb:  db "INFB",0
l_80fe:  db "80FE",0
l_40fe:  db "40FE",0
l_80ff:  db "80FF",0
l_40ff:  db "40FF",0
l_80f4:  db "80F4",0
l_40f4:  db "40F4",0
l_40fb:  db "40FB",0
l_frm:   db "FRM ",0

FLAG:   db 0
nfr:    db 0
curr:   db 0
curt:   dw 0
curres: dw 0
savesp: dw 0
savei:  db 0
neg:    db 0
numbuf: ds 6
pos:    dw 0
saveattr: ds 32

        assert $ <= TABLE

        org ISR                 ; count the frames down
        push af
        ld a,(FLAG)
        dec a
        ld (FLAG),a
        pop af
        ei
        ret

        assert $ <= RESULTS

        savebin "q10probe.bin",0x8000,STACK-0x8000
        savetap "q10probe-code.tap",CODE,"q10probe",0x8000,STACK-0x8000
