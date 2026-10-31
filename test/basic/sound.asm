; sound: the TS2068's beeper and AY, for comparing the machine's sound
; with fuse-tspico's (tools/host/compare_sound.py).
;
; A HOME ROM (the EXROM is empty). Running from reset, it plays, about
; 250 frames in all:
;
;   1  a beeper sweep: port FE bit 4 (EAR), the pitch falling
;   2  MIC alone: port FE bit 3, which Fuse also mixes in, quietly
;   3  silence
;   4  the AY from a table: three tones, a pitch change, noise, and the
;      envelope shapes (decay, sawtooth, triangle, attack-and-hold)
;   5  AY sample playback: R8 rewritten many times a frame, tone and
;      noise off, so every write lands mid-frame
;   6  the beeper and an AY tone together
;
; then halts with interrupts on, so the sound decays to silence.
;
; sjasmplus sound.asm  ->  sound-home.rom, sound-exrom.rom

        DEVICE NOSLOT64K

ULA     equ 0xfe
AYREG   equ 0xf5
AYDAT   equ 0xf6
BORDER  equ 0x01                ; blue; bit 3 (MIC) clear means MIC on

        org 0x0000
        di
        ld sp,0xfff0
        im 1
        jp start

        org 0x0038
        ei
        ret

        org 0x0100
start:  ei
        halt

; 1: beeper sweep. E sets the half-period; 40 half-periods a step.
        ld e,20
        ld c,BORDER | 0x08      ; MIC off, EAR off
sweep:  ld d,40
burst:  ld a,c
        xor 0x10
        ld c,a
        out (ULA),a
        ld b,e
dly:    djnz dly
        dec d
        jr nz,burst
        ld a,e
        add a,4
        ld e,a
        cp 200
        jr c,sweep

; 2: MIC alone, EAR held off
        ld c,BORDER | 0x08
        ld d,0                  ; 256 half-periods
mic:    ld a,c
        xor 0x08
        ld c,a
        out (ULA),a
        ld b,60
mdly:   djnz mdly
        dec d
        jr nz,mic
        ld a,BORDER | 0x08
        out (ULA),a

; 3: silence
        ld b,10
quiet:  halt
        djnz quiet

; 4: the AY table: n, then n (register, value) pairs, then frames to
; wait. n = FFh ends it.
        ld hl,aytable
step:   ld a,(hl)
        inc hl
        cp 0xff
        jr z,samples
        ld b,a
pair:   ld a,(hl)
        inc hl
        out (AYREG),a
        ld a,(hl)
        inc hl
        out (AYDAT),a
        djnz pair
        ld b,(hl)
        inc hl
wait:   halt
        djnz wait
        jr step

; 5: sample playback on channel A: a ramp, 16 levels, for 3 x 256 writes
samples:
        ld a,8
        out (AYREG),a
        ld e,3
sloop:  ld d,0
swrite: ld a,d
        and 0x0f
        out (AYDAT),a
        ld b,12
sdly:   djnz sdly
        dec d
        jr nz,swrite
        dec e
        jr nz,sloop
        xor a
        out (AYDAT),a

; 6: beeper and AY tone A together
        ld a,0
        out (AYREG),a
        ld a,0x40
        out (AYDAT),a
        ld a,7
        out (AYREG),a
        ld a,0x3e
        out (AYDAT),a
        ld a,8
        out (AYREG),a
        ld a,15
        out (AYDAT),a
        ld c,BORDER | 0x08
        ld d,0
both:   ld a,c
        xor 0x10
        ld c,a
        out (ULA),a
        ld b,90
bdly:   djnz bdly
        dec d
        jr nz,both
        ld a,8
        out (AYREG),a
        xor a
        out (AYDAT),a

done:   halt
        jr done

aytable:
        db 10                   ; three tones
        db 0,0xfe, 1,0x00, 2,0x50, 3,0x01, 4,0xc0, 5,0x01
        db 7,0x38, 8,15, 9,12, 10,10
        db 20
        db 3                    ; new pitches
        db 0,0x7f, 2,0xa8, 4,0xe0
        db 10
        db 2                    ; noise on C instead of its tone
        db 6,10, 7,0x1c
        db 15
        db 4                    ; A on the envelope: triangle
        db 8,0x10, 11,0x00, 12,0x08, 13,0x0e
        db 30
        db 2                    ; sawtooth down
        db 12,0x02, 13,0x08
        db 20
        db 2                    ; decay once
        db 12,0x10, 13,0x00
        db 20
        db 1                    ; attack and hold
        db 13,0x0d
        db 15
        db 4                    ; all off
        db 7,0x3f, 8,0, 9,0, 10,0
        db 5
        db 0xff

        SAVEBIN "sound-home.rom", 0x0000, 0x4000

        org 0x0000
        ds 0x4000, 0xff
        SAVEBIN "sound-exrom.rom", 0x0000, 0x4000
