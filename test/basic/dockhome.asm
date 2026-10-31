; dockhome: DOCK/EXROM vs HOME contention on the TS2068 (PLAN.md Phase 2,
; §3 "Contention").
;
; A HOME ROM and a 16K EXROM. Each test syncs to the frame interrupt, then
; counts passes of a loop until the next one. The loop's one access under
; test is at 0x4000-0x7FFF, where HOME has RAM5 (the screen) and the SCLD
; can page in DOCK or EXROM instead. Contended accesses make passes
; slower, so a lower count means more contention.
;
;   1  read  (0x4000), HOME RAM5        5  run code at 0x4000, HOME RAM5
;   2  read  (0x4000), EXROM paged in   6  run code at 0x4000, EXROM
;   3  read  (0x4000), DOCK paged in    7  IN from port 40FBh, HOME
;   4  read  (0x8000), HOME (reference) 8  IN from port 40FBh, EXROM
;
; Then it leaves the counts in registers and halts with interrupts off:
; BC=1 DE=2 HL=3, BC'=4 DE'=5 HL'=6, IX=7 IY=8. Fuse's automation reports
; registers, and tools/host/compare_fuse.py --rom timing reads them there.
;
; Fuse contends DOCK and EXROM like HOME, so in Fuse 2 = 3 = 1, 6 = 5 and
; 8 = 7. If the TS2068 contends only HOME, as David Anderson reports, then
; 2 = 3 = 4, and 6 and 8 rise above 5 and 7. On real hardware, read the
; registers by other means; that's the measurement Q10 asks for.
;
; sjasmplus dockhome.asm  ->  dockhome-home.rom, dockhome-exrom.rom

        DEVICE NOSLOT64K

FLAG    equ 0xf000              ; HOME RAM, chunk 7: never paged here
RESULTS equ 0xf010
HSR     equ 0xf4
DEC     equ 0xff
CHUNK2  equ 0x04                ; HSR bit for 0x4000-0x5FFF
EXROM   equ 0x80                ; DEC bit 7: EXROM, not DOCK

        org 0x0000
        di
        ld sp,0xfff0
        im 1
        jp start

        org 0x0038
isr:    push af
        xor a
        ld (FLAG),a
        pop af
        ei
        ret

        org 0x0100
start:  ld hl,xcode             ; the code test 5 runs, into HOME RAM5
        ld de,0x4000
        ld bc,xcode_end-xcode
        ldir
        ld ix,RESULTS

        ld hl,0x4000            ; 1: HOME
        call rmeasure

        ld a,EXROM              ; 2: EXROM
        out (DEC),a
        ld a,CHUNK2
        out (HSR),a
        ld hl,0x4000
        call rmeasure

        xor a                   ; 3: DOCK (HSR still pages chunk 2)
        out (DEC),a
        ld hl,0x4000
        call rmeasure

        xor a                   ; 4: HOME, uncontended
        out (HSR),a
        ld hl,0x8000
        call rmeasure

        call xmeasure           ; 5: run at 0x4000, HOME

        ld a,EXROM              ; 6: run at 0x4000, EXROM
        out (DEC),a
        ld a,CHUNK2
        out (HSR),a
        call xmeasure

        xor a                   ; 7: port, HOME
        out (HSR),a
        out (DEC),a
        ld bc,0x40fb
        call pmeasure

        ld a,EXROM              ; 8: port, EXROM
        out (DEC),a
        ld a,CHUNK2
        out (HSR),a
        ld bc,0x40fb
        call pmeasure

        xor a
        out (HSR),a
        out (DEC),a

        ld ix,(RESULTS+12)
        ld iy,(RESULTS+14)
        ld bc,(RESULTS+6)
        ld de,(RESULTS+8)
        ld hl,(RESULTS+10)
        exx
        ld bc,(RESULTS+0)
        ld de,(RESULTS+2)
        ld hl,(RESULTS+4)
        di
done:   halt
        jr done

; Sync to the frame, then count passes until the next interrupt clears FLAG.
sync:   ei
        halt
        ld de,0
        ld a,1
        ld (FLAG),a
        ret

store:  ld (ix+0),e
        ld (ix+1),d
        inc ix
        inc ix
        ret

rmeasure:
        call sync
rloop:  ld a,(hl)               ; the access under test
        inc de
        ld a,(FLAG)
        or a
        jr nz,rloop
        jr store

xmeasure:
        ld hl,0x8000
        call sync
        call 0x4000
        jr store

pmeasure:
        call sync
ploop:  in a,(c)                ; the access under test
        inc de
        ld a,(FLAG)
        or a
        jr nz,ploop
        jr store

; The loop tests 5 and 6 run at 0x4000: every opcode fetch is there.
xcode:
        DISP 0x4000
xloop:  ld a,(hl)
        inc de
        ld a,(FLAG)
        or a
        jr nz,xloop
        ret
        ENT
xcode_end:

        SAVEBIN "dockhome-home.rom", 0x0000, 0x4000

; The EXROM: the same loop at its start, which the SCLD shows at 0x4000
; (an even 8K chunk sees a 16K EXROM's first half).
        org 0x0000
        ds 0x4000, 0xff
        org 0x0000
        DISP 0x4000
xloop2: ld a,(hl)
        inc de
        ld a,(FLAG)
        or a
        jr nz,xloop2
        ret
        ENT
        SAVEBIN "dockhome-exrom.rom", 0x0000, 0x4000
