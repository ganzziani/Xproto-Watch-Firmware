#include <avr/io.h>
#include "hardware.h"

.extern Disp_send
.extern u8CursorX
.extern u8CursorY

// Other orientations, implement in the future if needed:
// 0 degree orientation:
// Disp_send.display_data[(uint16_t)(y<<4) + (x>>3)] |= (uint8_t)(0x80 >> (x & 0x07));
// 90 degree orientation
// y=127-y;
// Disp_send.display_data[(uint16_t)(x<<4) + (y>>3)] |= (uint8_t)(0x80 >> (y & 0x07));

#ifdef INVERT_DISPLAY

; Disp_send.display_data[(uint16_t)((127-x)*18) + (y>>3)] |= (uint8_t)(0x80 >> (y & 0x07));
.global set_pixel                   ; void setpixel(uint8_t x, uint8_t y);
set_pixel:                          ; Disp_send+2 is Disp_send.DataAddress
    lds     r30,Disp_send+2         ; r31:r30 = addr + 127*18
    lds     r31,Disp_send+3
    ldi     r25,0x12	            ; r25 = 18
    mul     r24,r25                 ; r1:r0 = x*18
    mov     r25,r22                 ; r25 = y
    lsr     r25
    lsr     r25                     ; r25 = y>>3
    lsr     r25
    sub     r30,r0                  ; z = r31:r30 = addr + 127*18 - x*18
    sbc     r31,r1
    clr     r1                      ; r1 = 0 gcc needs r1 cleared
    add     r30,r25                 ; z = r31:r30 -= x*18 + y>>3
    adc     r31,r1
    ; Address is ready, now put data
    andi    r22,0x07                ; y &= &0x07
    ldi     r24,0x80                ; r24 = 0x80
    rjmp    .+0x0002                ; Relative jump 
0:
    lsr     r24	                    ; r24 >>= 1
    dec     r22                     ; Decrement y
    brpl    0b                      ; repeat while y >= 0
    lac     z, r24                  ; AND bits in display data
    ret

; Disp_send.buffer[((uint16_t)((127-x)*18)) + (u8CursorY)] |= data;
.global display_or               ; void write_display(uint8_t data);
display_or:                      ; OR byte on display buffer
    lds     r30, u8CursorX          ; r30 = u8CursorX
    inc     r30                     ; r30++
    sts     u8CursorX, r30          ; u8CursorX = r30
    dec     r30                     ; r30 = original u8CursorX
    ldi     r31,0x12	            ; r31 = 18
    mul     r30,r31                 ; r1:r0 = x*18    
    lds     r30,Disp_send+2         ; r31:r30 = addr + 127*18
    lds     r31,Disp_send+3
    sub     r30,r0                  ; r31:r30 = addr + 127*18 - x*18
    sbc     r31,r1
    clr     r1                      ; r1 = 0 gcc needs r1 cleared
    lds     r0, u8CursorY           ; r0 = u8CursorY
    add     r30,r0                  ; r31:r30 += y
    adc     r31,r1
    ; Address is ready, now put data
    lac     z, r24                  ; OR bits in display data
    ret

#else

; Disp_send.display_data[(uint16_t)((127-x)*18) + (y>>3)] |= (uint8_t)(0x80 >> (y & 0x07));
.global set_pixel                   ; void setpixel(uint8_t x, uint8_t y);
set_pixel:                          ; Disp_send+2 is Disp_send.DataAddress
    lds     r30,Disp_send+2         ; r31:r30 = addr + 127*18
    lds     r31,Disp_send+3
    ldi     r25,0x12	            ; r25 = 18
    mul     r24,r25                 ; r1:r0 = x*18
    mov     r25,r22                 ; r25 = y
    lsr     r25
    lsr     r25                     ; r25 = y>>3
    lsr     r25
    sub     r30,r0                  ; r31:r30 = addr + 127*18 - x*18
    sbc     r31,r1
    clr     r1                      ; r1 = 0 gcc needs r1 cleared
    add     r30,r25                 ; r31:r30 -= x*18 + y>>3
    adc     r31,r1
    ; Address is ready, now put data
    andi    r22,0x07                ; y &= &0x07
    ldi     r24,0x80                ; r24 = 0x80
    rjmp    .+0x0002                ; Relative jump 
0:
    lsr     r24	                    ; r24 >>= 1
    dec     r22                     ; Decrement y
    brpl    0b                      ; repeat while y >= 0
    las     z, r24                  ; OR bits in display data
    ret

; Disp_send.buffer[((uint16_t)((127-x)*18)) + (u8CursorY)] |= data;
.global display_or                  ; void write_display(uint8_t data);
display_or:                         ; OR byte on display buffer
    lds     r30, u8CursorX          ; r30 = u8CursorX
    inc     r30                     ; r30++
    sts     u8CursorX, r30          ; u8CursorX = r30
    dec     r30                     ; r30 = original u8CursorX
    ldi     r31,0x12	            ; r31 = 18
    mul     r30,r31                 ; r1:r0 = x*18    
    lds     r30,Disp_send+2         ; r31:r30 = addr + 127*18
    lds     r31,Disp_send+3
    sub     r30,r0                  ; r31:r30 = addr + 127*18 - x*18
    sbc     r31,r1
    clr     r1                      ; r1 = 0 gcc needs r1 cleared
    lds     r0, u8CursorY           ; r0 = u8CursorY
    add     r30,r0                  ; r31:r30 += y
    adc     r31,r1
    ; Address is ready, now put data
    las     z, r24                  ; OR bits in display data
    ret

#endif

; Disp_send.buffer[((uint16_t)((127-x)*18)) + (u8CursorY)] = data;
.global display_set                 ; void write_display(uint8_t data);
display_set:                        ; OR byte on display buffer
    lds     r30, u8CursorX          ; r30 = u8CursorX
    inc     r30                     ; r30++
    sts     u8CursorX, r30          ; u8CursorX = r30
    dec     r30                     ; r30 = original u8CursorX
    ldi     r31,0x12	            ; r31 = 18
    mul     r30,r31                 ; r1:r0 = x*18    
    lds     r30,Disp_send+2         ; r31:r30 = addr + 127*18
    lds     r31,Disp_send+3
    sub     r30,r0                  ; r31:r30 = addr + 127*18 - x*18
    sbc     r31,r1
    clr     r1                      ; r1 = 0 gcc needs r1 cleared
    lds     r0, u8CursorY           ; r0 = u8CursorY
    add     r30,r0                  ; r31:r30 += y
    adc     r31,r1
    ; Address is ready, now put data
    std     z+0, r24	            ; SET data in display
    ret


; Disp_send.buffer[((uint16_t)((127-x)*18)) + (u8CursorY)] ^= data;
.global display_xor               ; void write_display(uint8_t data);
display_xor:                      ; OR byte on display buffer
    lds     r30, u8CursorX          ; r30 = u8CursorX
    inc     r30                     ; r30++
    sts     u8CursorX, r30          ; u8CursorX = r30
    dec     r30                     ; r30 = original u8CursorX
    ldi     r31,0x12	            ; r31 = 18
    mul     r30,r31                 ; r1:r0 = x*18    
    lds     r30,Disp_send+2         ; r31:r30 = addr + 127*18
    lds     r31,Disp_send+3
    sub     r30,r0                  ; r31:r30 = addr + 127*18 - x*18
    sbc     r31,r1
    clr     r1                      ; r1 = 0 gcc needs r1 cleared
    lds     r0, u8CursorY           ; r0 = u8CursorY
    add     r30,r0                  ; r31:r30 += y
    adc     r31,r1
    ; Address is ready, now put data
    lat     z, r24                  ; XOR bits in display data
    ret

;----------------------------------------------------------------------------;
; ADD with Saturation
.global addwsat     ; uint8_t addwsat(uint8_t a, int8_t b);
addwsat:
    subi R24, 0x80  ; Transform [0x00, 0xff] -> [0x80, 0x7f]
.global saddwsat    ; uint8_t saddwsat(int8_t a, int8_t b);
saddwsat:
    add  R24, R22   ; add
    brvc 0f
    ; Signed overflow -> load MAX
    ldi  R24, 0x7f
    sbrc R22, 7
    ; R22 is negative -> load MIN
    ldi  R24, 0x80
    0:
    subi R24, 0x80  ; Back-Transform [0x80, 0x7f] -> [0x00, 0xff]
    ret

; Digital Trigger Rising Edge
;----------------------------------------------------------------------------;
;
.global trigupCHD
.func trigupCHD
trigupCHD:
    ldi ZL, lo8(pm(tableup))
    ldi ZH, hi8(pm(tableup))
    add ZL, r24
    adc ZH, r1
    ijmp
tableup:
    rjmp trigpc0
    rjmp trigpc1
    rjmp trigpc2
    rjmp trigpc3
    rjmp trigpc4
    rjmp trigpc5
    rjmp trigpc6
    rjmp trigpc7
    rjmp trigextu
.endfunc

; trigger on PC0
;----------------------------------------------------------------------------;
;
.global trigpc0
.func trigpc0
trigpc0:
    sbic    0x000B,0        ; 2 cycle ; Status update? (GPIOB)
    ret
    sbic    0x001A,0        ; Check if bit VPORT2.0 is low
    rjmp    trigpc0
0:
    sbic    0x000B,0        ; 2 cycle ; Status update? (GPIOB)
    ret
    sbis    0x001A,0        ; Check if bit VPORT2.0 is high
    rjmp    0b
    rjmp    post
.endfunc

; trigger on PC1
;----------------------------------------------------------------------------;
;
.global trigpc1
.func trigpc1
trigpc1:
    sbic    0x000B,0        ; 2 cycle ; Status update? (GPIOB)
    ret
    sbic    0x001A,1        ; Check if bit VPORT2.1 is low
    rjmp    trigpc1
0:
    sbic    0x000B,0        ; 2 cycle ; Status update? (GPIOB)
    ret
    sbis    0x001A,1        ; Check if bit VPORT2.1 is high
    rjmp    0b
    rjmp    post
.endfunc

; trigger on PC2
;----------------------------------------------------------------------------;
;
.global trigpc2
.func trigpc2
trigpc2:
    sbic    0x000B,0        ; 2 cycle ; Status update? (GPIOB)
    ret
    sbic    0x001A,2        ; Check if bit VPORT2.1 is low
    rjmp    trigpc2
0:
    sbic    0x000B,0        ; 2 cycle ; Status update? (GPIOB)
    ret
    sbis    0x001A,2        ; Check if bit VPORT2.1 is high
    rjmp    0b
    rjmp    post
.endfunc

; trigger on PC3
;----------------------------------------------------------------------------;
;
.global trigpc3
.func trigpc3
trigpc3:
    sbic    0x000B,0        ; 2 cycle ; Status update? (GPIOB)
    ret
    sbic    0x001A,3        ; Check if bit VPORT2.1 is low
    rjmp    trigpc3
0:
    sbic    0x000B,0        ; 2 cycle ; Status update? (GPIOB)
    ret
    sbis    0x001A,3        ; Check if bit VPORT2.1 is high
    rjmp    0b
    rjmp    post
.endfunc


; trigger on PC4
;----------------------------------------------------------------------------;
;
.global trigpc4
.func trigpc4
trigpc4:
    sbic    0x000B,0        ; 2 cycle ; Status update? (GPIOB)
    ret
    sbic    0x001A,4        ; Check if bit VPORT2.1 is low
    rjmp    trigpc4
0:
    sbic    0x000B,0        ; 2 cycle ; Status update? (GPIOB)
    ret
    sbis    0x001A,4        ; Check if bit VPORT2.1 is high
    rjmp    0b
    rjmp    post
.endfunc


; trigger on PC5
;----------------------------------------------------------------------------;
;
.global trigpc5
.func trigpc5
trigpc5:
    sbic    0x000B,0        ; 2 cycle ; Status update? (GPIOB)
    ret
    sbic    0x001A,5        ; Check if bit VPORT2.1 is low
    rjmp    trigpc5
0:
    sbic    0x000B,0        ; 2 cycle ; Status update? (GPIOB)
    ret
    sbis    0x001A,5        ; Check if bit VPORT2.1 is high
    rjmp    0b
    rjmp    post
.endfunc


; trigger on PC6
;----------------------------------------------------------------------------;
;
.global trigpc6
.func trigpc6
trigpc6:
    sbic    0x000B,0        ; 2 cycle ; Status update? (GPIOB)
    ret
    sbic    0x001A,6        ; Check if bit VPORT2.1 is low
    rjmp    trigpc6
0:
    sbic    0x000B,0        ; 2 cycle ; Status update? (GPIOB)
    ret
    sbis    0x001A,6        ; Check if bit VPORT2.1 is high
    rjmp    0b
    rjmp    post
.endfunc


; trigger on PC7
;----------------------------------------------------------------------------;
;
.global trigpc7
.func trigpc7
trigpc7:
    sbic    0x000B,0        ; 2 cycle ; Status update? (GPIOB)
    ret
    sbic    0x001A,7        ; Check if bit VPORT2.1 is low
    rjmp    trigpc7
0:
    sbic    0x000B,0        ; 2 cycle ; Status update? (GPIOB)
    ret
    sbis    0x001A,7        ; Check if bit VPORT2.1 is high
    rjmp    0b
    rjmp    post
.endfunc

; trigger up on External Trigger pin
;----------------------------------------------------------------------------;
;
.global trigextu
.func trigextu
trigextu:
    sbic    0x000B,0        ; 2 cycle ; Status update? (GPIOB)
    ret
    sbic    EXTPIN          ; Check if external trigger pin is low
    rjmp    trigextu
0:
    sbic    0x000B,0        ; 2 cycle ; Status update? (GPIOB)
    ret
    sbis    EXTPIN          ; Check if external trigger pin is high
    rjmp    0b
    rjmp    post
.endfunc

; Digital Trigger Rising Edge
;----------------------------------------------------------------------------;
;
.global trigdownCHD
.func trigdownCHD
trigdownCHD:
    ldi ZL, lo8(pm(tabledown))
    ldi ZH, hi8(pm(tabledown))
    add ZL, r24
    adc ZH, r1
    ijmp
tabledown:
    rjmp trigpc0d
    rjmp trigpc1d
    rjmp trigpc2d
    rjmp trigpc3d
    rjmp trigpc4d
    rjmp trigpc5d
    rjmp trigpc6d
    rjmp trigpc7d
    rjmp trigextd
.endfunc

; trigger on PC0
;----------------------------------------------------------------------------;
;
.global trigpc0d
.func trigpc0d
trigpc0d:
    sbic    0x000B,0        ; 2 cycle ; Status update? (GPIOB)
    ret
    sbis    0x001A,0        ; Check if bit VPORT2.0 is high
    rjmp    trigpc0d
0:
    sbic    0x000B,0        ; 2 cycle ; Status update? (GPIOB)
    ret
    sbic    0x001A,0        ; Check if bit VPORT2.0 is low
    rjmp    0b
    rjmp    post
.endfunc

; trigger on PC1
;----------------------------------------------------------------------------;
;
.global trigpc1d
.func trigpc1d
trigpc1d:
    sbic    0x000B,0        ; 2 cycle ; Status update? (GPIOB)
    ret
    sbis    0x001A,1        ; Check if bit VPORT2.1 is high
    rjmp    trigpc1d
0:
    sbic    0x000B,0        ; 2 cycle ; Status update? (GPIOB)
    ret
    sbic    0x001A,1        ; Check if bit VPORT2.1 is low
    rjmp    0b
    rjmp    post
.endfunc

; trigger on PC2
;----------------------------------------------------------------------------;
;
.global trigpc2d
.func trigpc2d
trigpc2d:
    sbic    0x000B,0        ; 2 cycle ; Status update? (GPIOB)
    ret
    sbis    0x001A,2        ; Check if bit VPORT2.1 is high
    rjmp    trigpc2d
0:
    sbic    0x000B,0        ; 2 cycle ; Status update? (GPIOB)
    ret
    sbic    0x001A,2        ; Check if bit VPORT2.1 is low
    rjmp    0b
    rjmp    post
.endfunc


; trigger on PC3
;----------------------------------------------------------------------------;
;
.global trigpc3d
.func trigpc3d
trigpc3d:
    sbic    0x000B,0        ; 2 cycle ; Status update? (GPIOB)
    ret
    sbis    0x001A,3        ; Check if bit VPORT2.1 is high
    rjmp    trigpc3d
0:
    sbic    0x000B,0        ; 2 cycle ; Status update? (GPIOB)
    ret
    sbic    0x001A,3        ; Check if bit VPORT2.1 is low
    rjmp    0b
    rjmp    post
.endfunc


; trigger on PC4
;----------------------------------------------------------------------------;
;
.global trigpc4d
.func trigpc4d
trigpc4d:
    sbic    0x000B,0        ; 2 cycle ; Status update? (GPIOB)
    ret
    sbis    0x001A,4        ; Check if bit VPORT2.1 is high
    rjmp    trigpc4d
0:
    sbic    0x000B,0        ; 2 cycle ; Status update? (GPIOB)
    ret
    sbic    0x001A,4        ; Check if bit VPORT2.1 is low
    rjmp    0b
    rjmp    post
.endfunc


; trigger on PC5
;----------------------------------------------------------------------------;
;
.global trigpc5d
.func trigpc5d
trigpc5d:
    sbic    0x000B,0        ; 2 cycle ; Status update? (GPIOB)
    ret
    sbis    0x001A,5        ; Check if bit VPORT2.1 is high
    rjmp    trigpc5d
0:
    sbic    0x000B,0        ; 2 cycle ; Status update? (GPIOB)
    ret
    sbic    0x001A,5        ; Check if bit VPORT2.1 is low
    rjmp    0b
    rjmp    post
.endfunc


; trigger on PC6
;----------------------------------------------------------------------------;
;
.global trigpc6d
.func trigpc6d
trigpc6d:
    sbic    0x000B,0        ; 2 cycle ; Status update? (GPIOB)
    ret
    sbis    0x001A,6        ; Check if bit VPORT2.1 is high
    rjmp    trigpc6d
0:
    sbic    0x000B,0        ; 2 cycle ; Status update? (GPIOB)
    ret
    sbic    0x001A,6        ; Check if bit VPORT2.1 is low
    rjmp    0b
    rjmp    post
.endfunc


; trigger on PC7
;----------------------------------------------------------------------------;
;
.global trigpc7d
.func trigpc7d
trigpc7d:
    sbic    0x000B,0        ; 2 cycle ; Status update? (GPIOB)
    ret
    sbis    0x001A,7        ; Check if bit VPORT2.1 is high
    rjmp    trigpc7d
0:
    sbic    0x000B,0        ; 2 cycle ; Status update? (GPIOB)
    ret
    sbic    0x001A,7        ; Check if bit VPORT2.1 is low
    rjmp    0b
    rjmp    post
.endfunc

; trigger down on External Trigger pin
;----------------------------------------------------------------------------;
;
.global trigextd
.func trigextd
trigextd:
    sbic    0x000B,0        ; 2 cycle ; Status update? (GPIOB)
    ret
    sbis    EXTPIN          ; Check if external trigger pin is high
    rjmp    trigextd
0:
    sbic    0x000B,0        ; 2 cycle ; Status update? (GPIOB)
    ret
    sbic    EXTPIN          ; Check if external trigger pin is low
    rjmp    0b
    rjmp    post
.endfunc

; falling edge trigger detect on CH1
;----------------------------------------------------------------------------;
;                 data1  ___.
;                            \
;  triggervalue - - - - - - - \
;                              \.___ data2
;
.global trigdownCH1
.func trigdownCH1
trigdownCH1:
    subi    r24, 0x83       ; Transform [0x00, 0xff] -> [0x80, 0x7f]
                            ; r24 = (signed)(tlevelo - 3)
0:
    lds     r0, CH1ADC      ; 2 cycle : r21 = CH1ADC
    cp      r24, r0         ; 1 cycle ; compare triggervalue with data1
    brge    1f              ; 1 cycle ; next step if tlevelo >= data1
    sbis    0x000B,0        ; 1 cycle ; Status update? (GPIOB)
    rjmp    0b              ; 2 cycles
                            ; 17 cycles
    ret
1:
    subi    r24, 0xFA       ; r24 = (signed)(tlevelo + 3)
2:
    lds     r0,CH1ADC       ; 2 cycle : r0 = CH1ADC
    cp      r24,r0          ; 1 cycle ; compare triggervalue with data2
    brlt    post            ; 1 cycle ; finish if tlevelo < data2
    sbis    0x000B,0        ; 1 cycle ; Status update? (GPIOB)
    rjmp    2b              ; 2 cycles
                            ; 17 cycles
    ret
.endfunc

; rising edge trigger detect on CH1
;----------------------------------------------------------------------------;
;                              .___ data2
;                              /
;  triggervalue - - - - - - - /
;                  data1 ___./
;
.global trigupCH1
.func trigupCH1
trigupCH1:
    subi    r24, 0x7D       ; Transform [0x00, 0xff] -> [0x80, 0x7f]
                            ; r24 = (signed)(tlevelo + 3)
0:
    lds     r0,CH1ADC       ; 2 cycle : r0 = CH1ADC
    cp      r24, r0         ; 1 cycle ; compare triggervalue with data1
    brlt    1f              ; 1 cycle ; next step if tlevelo < data1
    sbis    0x000B,0        ; 1 cycle ; Status update? (GPIOB)
    rjmp    0b              ; 2 cycles
                            ; 7 cycles
    ret
1:
    subi    r24, 0x06       ; r24 = (signed)(tlevelo - 3)
2:
    lds     r0,CH1ADC       ; 2 cycle : r0 = CH1ADC
    cp      r24,r0          ; 1 cycle ; compare triggervalue with data2
    brge    post            ; 1 cycle ; finish if tlevelo >= data2
    sbis    0x000B,0        ; 1 cycle ; Status update? (GPIOB)
    rjmp    2b              ; 2 cycles
                            ; 7 cycles
    ret
.endfunc

; Post trigger
;----------------------------------------------------------------------------;
;
post:
    ; Start TCC1 and count remaining samples
    LDI     R24,0x09        ; Event CH1 (ADCA CH0 conversion complete)
    STS     0x0840,R24      ; Store 0x09 in TCC1.CTRLA (count ADC event)
    sbi     0x000B, 5       ; Set triggered bit in MStatus (GPIOB)
    in      r24, 0x0000     ; load srate (GPIO0) in r24
    cpi     r24,11          ; compare srate with 11
    brcc    1f              ; exit if srate>=11

    LDI     R24,0x14        ; Prepare R24: signed mode, NO free run, 8 bit
0:
    lds     R0, 0x084C      ; TCC1.INTFLAGS
    sbrs    R0,0            ; jump if overflow
    rjmp    0b

    STS     0x0A40,R1       ; Stop Timer TCE1 (for srate > 5)
    STS     0x0201,R24      ; Store 0x14 in ADCA.CTRLB (signed mode, NO free run, 8 bit)
    sbic    0x000B,0        ; 1 cycle ; Status update? (GPIOB)
    cbi     0x000B, 5       ; Clear trigger if update was set (key pressed and disrupted acq)
1:
    ret

; falling edge trigger detect on CH2
;----------------------------------------------------------------------------;
;                 data1  ___.
;                            \
;  triggervalue - - - - - - - \
;                              \.___ data2
;
.global trigdownCH2
.func trigdownCH2
trigdownCH2:
    subi    r24, 0x83       ; Transform [0x00, 0xff] -> [0x80, 0x7f]
                            ; r24 = (signed)(tlevelo - 3)
0:
    lds     r0,CH2ADC       ; 2 cycle : r0 = CH2ADC
    cp      r24, r0         ; 1 cycle ; compare triggervalue with data1
    brge    1f              ; 1 cycle ; next step if tlevelo >= data1
    sbis    0x000B,0        ; 1 cycle ; Status update? (GPIOB)
    rjmp    0b              ; 2 cycles
                            ; 7 cycles
    ret
1:
    subi    r24, 0xFA        ; r24 = (signed)(tlevelo + 3)
2:
    lds     r0,CH2ADC       ; 2 cycle : r0 = CH2ADC
    cp      r24,r0          ; 1 cycle ; compare triggervalue with data2
    brlt    post            ; 1 cycle ; finish if tlevelo < data2
    sbis    0x000B,0        ; 1 cycle ; Status update? (GPIOB)
    rjmp    2b              ; 2 cycles
                            ; 7 cycles
    ret
.endfunc

; rising edge trigger detect on CH2
;----------------------------------------------------------------------------;
;                              .___ data2
;                              /
;  triggervalue - - - - - - - /
;                  data1 ___./
;
.global trigupCH2
.func trigupCH2
trigupCH2:
    subi    r24, 0x7D       ; Transform [0x00, 0xff] -> [0x80, 0x7f]
                            ; r24 = (signed)(tlevelo + 3)
0:
    lds     r0,CH2ADC       ; 2 cycle : r0 = CH2ADC
    cp      r24,r0          ; 1 cycle ; compare triggervalue with data1
    brlt    1f              ; 1 cycle ; next step if tlevelo < data1
    sbis    0x000B,0        ; 1 cycle ; Status update? (GPIOB)
    rjmp    0b              ; 2 cycles
                            ; 7 cycles
    ret
1:
    subi     r24, 0x06       ; r24 = (signed)(tlevelo - 3)
2:
    lds     r0,CH2ADC       ; 2 cycle : r0 = CH2ADC
    cp      r24,r0          ; 1 cycle ; compare triggervalue with data1
    brge    post            ; 1 cycle ; finish if tlevelo >= data2
    sbis    0x000B,0        ; 1 cycle ; Status update? (GPIOB)
    rjmp    2b              ; 2 cycles
                            ; 7 cycles
    ret
.endfunc

; negative slope trigger detect on CH1
;----------------------------------------------------------------------------;
;                    old ___.
;                            \
;                           ||\
;                           >< \.___ new
;                    triggervalue
.global slopedownCH1
.func slopedownCH1
slopedownCH1:
    push    r23
    lds     r0,CH1ADC       ; 2 cycle : r0 = CH1ADC old value
0:
    lds     r23,CH1ADC      ; 2 cycle : r23 = CH1ADC
    cp      r0,r23          ; 1 cycle ; determine difference
    mov     r0,r23          ; 1 cycle ; copy new to old
    brlt    2f              ; next if old < new
    sbis    0x000B,0        ; 1 cycle ; Status update? (GPIOB)
    rjmp    0b              ; 2 cycles
                            ; 17 cycles
    pop     r23
    ret
2:
    lds     r23,CH1ADC      ; 2 cycle : r23 = CH1ADC
    sub     r0,r23          ; 1 cycle ; determine difference
    cp      r24,r0          ; 1 cycle ; compare with triggervalue
    mov     r0,r23          ; 1 cycle ; copy new to old
    brge    3f              ; 1 cycle ; tlevelo < data ?
    sbis    0x000B,0        ; 1 cycle ; Status update? (GPIOB)
    rjmp    2b              ; 2 cycles
    pop     r23
    ret
.endfunc

; positive slope trigger detect on CH1
;----------------------------------------------------------------------------;
;                              .___ new
;                              /
;                             /||
;                    old ___./ ><
;                           triggervalue
.global slopeupCH1
.func slopeupCH1
slopeupCH1:
    push    r23
    lds     r0,CH1ADC       ; 2 cycle : r0 = CH1ADC old value
0:
    lds     r23,CH1ADC      ; 2 cycle : r1 = CH1ADC
    cp      r23,r0          ; 1 cycle ; determine difference
    mov     r0,r23          ; 1 cycle ; copy new to old
    brlt    2f              ; next if old > new
    sbis    0x000B,0        ; 1 cycle ; Status update? (GPIOB)
    rjmp    0b              ; 2 cycles
                            ; 17 cycles
    pop     r23
    ret
2:
    lds     r23,CH1ADC      ; 2 cycle : r1 = CH1ADC
    sub     r0,r23          ; 1 cycle ; determine difference
    cp      r0,r24          ; 1 cycle ; compare with triggervalue
    mov     r0,r23          ; 1 cycle ; copy new to old
    brge    3f              ; 1 cycle ; tlevelo < data ?
    sbis    0x000B,0        ; 1 cycle ; Status update? (GPIOB)
    rjmp    2b              ; 2 cycles
    pop     r23
    ret
3:  pop     r23
4:  rjmp    post
.endfunc

; negative slope trigger detect on CH2
;----------------------------------------------------------------------------;
;                    old ___.
;                            \
;                           ||\
;                           >< \.___ new
;                    triggervalue
.global slopedownCH2
.func slopedownCH2
slopedownCH2:
    push    r23
    lds     r0,CH2ADC       ; 2 cycle : r0 = CH2ADC old value
0:
    lds     r23,CH2ADC      ; 2 cycle : r23 = CH2ADC
    cp      r0,r23          ; 1 cycle ; determine difference
    mov     r0,r23          ; 1 cycle ; copy new to old
    brlt    2f              ; next if old + 3 < new
    sbis    0x000B,0        ; 1 cycle ; Status update? (GPIOB)
    rjmp    0b              ; 2 cycles
                            ; 17 cycles
    pop     r23
    ret
2:
    lds     r23,CH2ADC      ; 2 cycle : r23 = CH2ADC
    sub     r0,r23          ; 1 cycle ; determine difference
    cp      r24,r0          ; 1 cycle ; compare with triggervalue
    mov     r0,r23          ; 1 cycle ; copy new to old
    brge    3b              ; 1 cycle ; tlevelo < data ?
    sbis    0x000B,0        ; 1 cycle ; Status update? (GPIOB)
    rjmp    2b              ; 2 cycles
    pop     r23
    ret
.endfunc

; positive slope trigger detect on CH2
;----------------------------------------------------------------------------;
;                              .___ new
;                              /
;                             /||
;                    old ___./ ><
;                           triggervalue
.global slopeupCH2
.func slopeupCH2
slopeupCH2:
    push    r23
    lds     r0,CH2ADC       ; 2 cycle : r0 = CH2ADC old value
0:
    lds     r23,CH2ADC      ; 2 cycle : r23 = CH2ADC
    cp      r23,r0          ; 1 cycle ; determine difference
    mov     r0,r23          ; 1 cycle ; copy new to old
    brlt    2f              ; next if old > new
    sbis    0x000B,0        ; 1 cycle ; Status update? (GPIOB)
    rjmp    0b              ; 2 cycles
                            ; 17 cycles
    pop     r23
    ret
2:
    lds     r23,CH2ADC      ; 2 cycle : r23 = CH2ADC
    sub     r0,r23          ; 1 cycle ; determine difference
    cp      r0,r24          ; 1 cycle ; compare with triggervalue
    mov     r0,r23          ; 1 cycle ; copy new to old
    brge    3b              ; 1 cycle ; tlevelo < data ?
    sbis    0x000B,0        ; 1 cycle ; Status update? (GPIOB)
    rjmp    2b              ; 2 cycles
    pop     r23
    ret
.endfunc

; window trigger detect on CH1
;----------------------------------------------------------------------------;
; r22 contains window1, r24 contains window2
.global windowCH1
.func windowCH1
windowCH1:
    subi    r24, 0x80       ; Transform [0x00, 0xff] -> [0x80, 0x7f]
    subi    r22, 0x80       ; Transform [0x00, 0xff] -> [0x80, 0x7f]
0:
    lds     r0, CH1ADC      ; 2 cycle : r21 = CH1ADC
    cp      r24, r0         ; 1 cycle ; compare window1 with data
    brlt    4b              ; 1 cycle ; continue if window1 >= data
    cp      r0,r22          ; 1 cycle ; compare window2 with data
    brlt    4b              ; 1 cycle ; continue if window2 < data
    sbis    0x000B,0        ; 1 cycle ; Status update? (GPIOB)
    rjmp    0b              ; 2 cycles
                            ; 17 cycles
    ret

.endfunc

; window trigger detect on CH2
;----------------------------------------------------------------------------;

.global windowCH2
.func windowCH2
windowCH2:
    subi    r24, 0x80       ; Transform [0x00, 0xff] -> [0x80, 0x7f]
    subi    r22, 0x80       ; Transform [0x00, 0xff] -> [0x80, 0x7f]
0:
    lds     r0, CH2ADC      ; 2 cycle : r0 = CH2ADC
    cp      r24, r0         ; 1 cycle ; compare window1 with data
    brlt    4b              ; 1 cycle ; continue if window1 >= data
    cp      r0,r22          ; 1 cycle ; compare window2 with data
    brlt    4b              ; 1 cycle ; continue if window2 < data
    sbis    0x000B,0        ; 1 cycle ; Status update? (GPIOB)
    rjmp    0b              ; 2 cycles
                            ; 17 cycles
    ret
.endfunc

.section .bss
.global endpoints   // Endpoints configuration tables
.align 2            // Align to word
endpoints:  .dc.b 1,2,3,4,5,6,7,8    ; ep0 out
            .dc.b 1,2,3,4,5,6,7,8    ; ep0 in
            .dc.b 1,2,3,4,5,6,7,8    ; ep1 out
            .dc.b 1,2,3,4,5,6,7,8    ; ep1 in