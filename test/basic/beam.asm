; beam: changes to the picture while the beam draws it (PLAN.md Phase 3,
; "Display", Q7).
;
; A HOME ROM and an EXROM it doesn't use. It fills both display files,
; then every frame, timed from the interrupt:
;
;   - border stripes, an OUT to port FE every few lines, from the top
;     border into the screen's side borders
;   - attribute row 10 rewritten while the beam is inside it, and put back
;     at the next interrupt
;   - port FF: HIRES for 20 lines (its border is the colour pair's paper),
;     then the second display file, then extended colour, then back
;
; Drawn from screen RAM once a frame, none of this shows; following the
; beam as Fuse does (display_dirty.c, display_border.c), all of it does,
; split where the beam was. tools/host/compare_fuse.py --rom beam compares
; the picture with Fuse's.
;
; sjasmplus beam.asm  ->  beam-home.rom, beam-exrom.rom

        DEVICE NOSLOT64K

DEC     equ 0xff
ROW10   equ 0x5800 + 10 * 32

        org 0x0000
        di
        ld sp,0xfff0
        im 1
        jp start

        org 0x0038
isr:    push af
        push bc
        push de
        push hl
        ld hl,0x5800 + 10 * 32  ; attribute row 10 as it was
        ld b,32
isr1:   ld a,l
        and 0x7f
        ld (hl),a
        inc hl
        djnz isr1
        ld a,7
        out (0xfe),a
        xor a
        out (DEC),a
        pop hl
        pop de
        pop bc
        pop af
        ei
        ret

        org 0x0100
start:  ld hl,0x4000            ; bitmap: the address's low byte
fill1:  ld (hl),l
        inc hl
        ld a,h
        cp 0x58
        jr nz,fill1
fill2:  ld a,l                  ; attributes: the low byte, no FLASH
        and 0x7f
        ld (hl),a
        inc hl
        ld a,h
        cp 0x5b
        jr nz,fill2
        ld hl,0x6000            ; second bitmap: high xor low
fill3:  ld a,h
        xor l
        ld (hl),a
        inc hl
        ld a,h
        cp 0x78
        jr nz,fill3
fill4:  ld a,l                  ; second attributes: the low byte inverted
        cpl
        and 0x7f
        ld (hl),a
        inc hl
        ld a,h
        cp 0x7b
        jr nz,fill4

frame:  ei
        halt

        ld bc,200               ; to about line 8 of the top border
        call wait

        ld e,1                  ; 16 stripes of about 3 lines
        ld d,16
stripe: ld a,e
        and 7
        out (0xfe),a
        inc e
        ld bc,23
        call wait
        dec d
        jr nz,stripe

        ld bc,400               ; into attribute row 10
        call wait
        ld hl,newattr
        ld de,ROW10
        ld bc,32
        ldir

        ld bc,264               ; HIRES, colour pair 3
        call wait
        ld a,0x06 | ( 3 << 3 )
        out (DEC),a
        ld bc,172
        call wait
        xor a
        out (DEC),a

        ld bc,129               ; the second display file
        call wait
        ld a,0x01
        out (DEC),a
        ld bc,138
        call wait
        ld a,0x02               ; extended colour
        out (DEC),a
        ld bc,138
        call wait
        xor a
        out (DEC),a
        jr frame

; About 26 T-states for each count in BC
wait:   dec bc
        ld a,b
        or c
        jr nz,wait
        ret

newattr:
        db 0x0e, 0x16, 0x1e, 0x26, 0x2e, 0x36, 0x3e, 0x06
        db 0x4e, 0x56, 0x5e, 0x66, 0x6e, 0x76, 0x7e, 0x46
        db 0x0e, 0x16, 0x1e, 0x26, 0x2e, 0x36, 0x3e, 0x06
        db 0x4e, 0x56, 0x5e, 0x66, 0x6e, 0x76, 0x7e, 0x46

        SAVEBIN "beam-home.rom", 0x0000, 0x4000

        org 0x0000
        ds 0x4000, 0xff
        SAVEBIN "beam-exrom.rom", 0x0000, 0x4000
