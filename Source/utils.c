#include <avr/io.h>
#include <avr/pgmspace.h>
#include <avr/interrupt.h>
#include "main.h"
#include "utils.h"
#include "mygccdef.h"

// From Application Note AVR1003
void CCPWrite( volatile uint8_t * address, uint8_t value ) {
    uint8_t volatile saved_sreg = SREG;
    cli();
    #ifdef __ICCAVR__
    asm("movw r30, r16");
    #ifdef RAMPZ
    RAMPZ = 0;
    #endif
    asm("ldi  r16,  0xD8 \n"
    "out  0x34, r16  \n"
    #if (__MEMORY_MODEL__ == 1)
    "st     Z,  r17  \n");
    #elif (__MEMORY_MODEL__ == 2)
    "st     Z,  r18  \n");
    #else /* (__MEMORY_MODEL__ == 3) || (__MEMORY_MODEL__ == 5) */
    "st     Z,  r19  \n");
    #endif /* __MEMORY_MODEL__ */

    #elif defined __GNUC__
    volatile uint8_t * tmpAddr = address;
    #ifdef RAMPZ
    RAMPZ = 0;
    #endif
    asm volatile(
    "movw r30,  %0"	      "\n\t"
    "ldi  r16,  %2"	      "\n\t"
    "out   %3, r16"	      "\n\t"
    "st     Z,  %1"       "\n\t"
    :
    : "r" (tmpAddr), "r" (value), "M" (CCP_IOREG_gc), "i" (&CCP)
    : "r16", "r30", "r31"
    );
    #endif
    SREG = saved_sreg;
}

// Converts an uint to int, then returns the half
uint8_t half(uint8_t number) {
    int8_t temp;
    temp=(int8_t)(number-128);
    temp=temp/2;
    return (uint8_t)(temp+128);
}

// Converts an uint to int, then return the double, or saturates
uint8_t twice(uint8_t number) {
    int8_t temp;
    if(number<=64) return 0;
    if(number>=192) return 255;
    temp = (int8_t)(number-128);
    temp = temp*2;
    return (uint8_t)(temp+128);
}

// Receives a nibble, returns the corresponding ascii that represents the HEX value
char NibbleToChar(uint8_t nibble) {
    nibble=nibble+'0';          // '0' thru '9'
    if(nibble>'9') nibble+=7;   // 'A' thru 'F'
    return nibble;
}

union {
    uint32_t Seed32;
    uint16_t Seed16;
} Seed;

// Use the current time to randomize
void Randomize(uint16_t AddSeed) {
    Seed.Seed32 += AddSeed;
}

// Pseudo Random Number - Linear congruential generator
uint8_t prandom(void) {
    Seed.Seed32 = 25173 * Seed.Seed32 + 13849;
    return Seed.Seed32>>24;
}

// Quick Pseudo Random Number - Linear congruential generator
// This function has less randomness compared to prandom
uint8_t qrandom(void) {
    Seed.Seed16 = 25173 * Seed.Seed16 + 1;
    return Seed.Seed16>>8;
}

// 60 value sine table
const int8_t SIN60[] PROGMEM = {
    0,     13,  26,  39,  51,  63,  74,  84,  94, 102, 109, 116, 120, 124, 126,
    127,  126, 124, 120, 116, 109, 102,  94,  84,  74,  63,  51,  39,  26,  13,
    0,    -13, -26, -39, -51, -63, -74, -84, -94,-102,-109,-116,-120,-124,-126,
    -127,-126,-124,-120,-116,-109,-102, -94, -84, -74, -63, -51, -39, -26, -13
};

// Scaled Sine: 60 values -> 2*PI
int8_t Sine60(uint8_t angle, int8_t scale) {
    //    if(angle<0) angle=60-angle;
    while(angle>=60) angle-=60;
    int8_t temp = (int8_t)pgm_read_byte_near(SIN60+angle);
    return FMULS8R(temp, scale);
}

// Scaled Cosine: 60 values -> 2*PI
int8_t Cosine60(uint8_t angle, int8_t scale) {
    //    if(angle<0) angle=-angle;
    angle+=15;
    while(angle>=60) angle-=60;
    int8_t temp = (int8_t)pgm_read_byte_near(SIN60+angle);
    return FMULS8R(temp, scale);
}

// Tunes: Duration, Note1, Note2

uint8_t *TunePtr;  // Pointer to current tune note

const unsigned char TuneIntro[] PROGMEM = {
    20, NOTE_C8, 10, 0,
    20, NOTE_D8, 10, 0,
    40, NOTE_E8, 
    0
};

const unsigned char TuneBeep[] PROGMEM = {
    8, NOTE_G6, 16, NOTE_C7, 0
};

const unsigned char TuneAlarm0[] PROGMEM = {    // 3 quick beeps
    10,  NOTE_B7, 20,  0,
    10,  NOTE_B7, 20,  0,
    10,  NOTE_B7, 100, 0,
    0
};

const unsigned char TuneAlarm1[] PROGMEM = {    // Rising tone
    10,  NOTE_E7, 20,  0,
    10,  NOTE_G7, 20,  0,
    10,  NOTE_B7, 100,0,
    0
};

const unsigned char TuneAlarm2[] PROGMEM = {    // Beep every 500ms
    25,  NOTE_B7, 97, 0,
    0
};

const unsigned char TuneAlarm3[] PROGMEM = {    // 2 notes
    50,  NOTE_B7, 20, 0,
    50,  NOTE_F7, 20, 0,
    0
};

const unsigned char TuneAlarm4[] PROGMEM = {    // Adventures of Gabito
    20,  NOTE_C7, 20,  0,
    20,  NOTE_G6, 20,  0,
    20,  NOTE_C7, 20,  0,
    20,  NOTE_G6, 20,  0,
    20,  NOTE_C7, 20,  0,
    20,  NOTE_G6, 20,  0,
    20,  NOTE_C7, 20,  0,
    20,  NOTE_G6, 20,  0,
    20,  NOTE_D7, 60,  0,
    20,  NOTE_G6, 60,  0,
    20,  NOTE_D7, 60,  0,
    20,  NOTE_G6, 60,  0,
    20,  NOTE_C7, 60,  0,
    20,  NOTE_G7, 20,  0,
    20,  NOTE_F7, 60,  0,
    20,  NOTE_E7, 60,  0,
    20,  NOTE_C7, 140, 0,
    0
};

const unsigned char TuneAlarm5[] PROGMEM = {    // full notes
    20,  NOTE_G6,
    20,  NOTE_A6,
    20,  NOTE_B6,
    20,  NOTE_C7,
    20,  NOTE_D7,
    20,  NOTE_E7,
    20,  NOTE_G7,
    0
};

const unsigned char TuneAlarm6[] PROGMEM = {    // notes up and down
    25,  NOTE_C7,
    25,  NOTE_D7,
    25,  NOTE_E7,
    25,  NOTE_G7,
    25,  NOTE_E7,
    25,  NOTE_D7,
    0
};

const unsigned char TuneAlarm7[] PROGMEM = {    // My First Song
    20,  NOTE_C7, 25, 0,
    20,  NOTE_C7, 25, 0,
    20,  NOTE_C7, 25, 0,
    20,  NOTE_C7, 25, 0,
    20,  NOTE_G7, 25, 0,
    20,  NOTE_G7, 25, 0,
    20,  NOTE_G7, 25, 0,
    20,  NOTE_G7, 25, 0,
    20,  NOTE_F7, 25, 0,
    20,  NOTE_F7, 25, 0,
    20,  NOTE_F7, 25, 0,
    20,  NOTE_F7, 25, 0,
    20,  NOTE_G7, 25, 0,
    20,  NOTE_G7, 25, 0,
    20,  NOTE_G7, 25, 0,
    20,  NOTE_G7, 75, 0,
    0
};

const unsigned char TuneAlarm8[] PROGMEM = {    // Medicine
    20,  NOTE_G6, 25, 0,
    20,  NOTE_C7, 25, 0,
    20,  NOTE_G7, 25, 0,
    20,  NOTE_C7, 25, 0,
    20,  NOTE_G7, 25, 0,
    20,  NOTE_C7, 25, 0,
    20,  NOTE_G6, 25, 0,
    20,  NOTE_C7, 25, 0,
    20,  NOTE_G7, 25, 0,
    20,  NOTE_C7, 25, 0,
    20,  NOTE_F7, 25, 0,
    20,  NOTE_C7, 25, 0,   
    20,  NOTE_D7, 25, 0,
    20,  NOTE_C7, 75, 0,
    0
};

const unsigned char TuneAlarm9[] PROGMEM = {    // Vengaboys
    20,  NOTE_G7, 10,  0,
    20,  NOTE_G7, 40,  0,
    20,  NOTE_G7, 70,  0,
    20,  NOTE_G7, 40,  0,
    20,  NOTE_G7, 10,  0,
    20,  NOTE_G7, 40,  0,
    20,  NOTE_G7, 70,  0,
    20,  NOTE_G7, 40,  0,
    20,  NOTE_A7, 10,  0,
    20,  NOTE_A7, 40,  0,
    20,  NOTE_A7, 70,  0,
    20,  NOTE_A7, 40,  0,
    20,  NOTE_B7, 10,  0,
    20,  NOTE_B7, 40,  0,
    20,  NOTE_B7, 70,  0,
    20,  NOTE_C8, 40,  0,
    20,  NOTE_E7, 10,  0,
    20,  NOTE_E7, 40,  0,
    20,  NOTE_E7, 70,  0,
    20,  NOTE_E7, 40,  0,
    20,  NOTE_E7, 10,  0,
    20,  NOTE_E7, 40,  0,
    20,  NOTE_E7, 70,  0,
    20,  NOTE_E7, 40,  0,
    20,  NOTE_D7, 10,  0,
    20,  NOTE_D7, 40,  0,
    20,  NOTE_D7, 70,  0,
    20,  NOTE_D7, 40,  0,
    20,  NOTE_C7, 10,  0,
    20,  NOTE_C7, 40,  0,
    20,  NOTE_C7, 70,  0,
    20,  NOTE_C7, 40,  0,
    20,  NOTE_G7, 10,  0,
    20,  NOTE_G7, 40,  0,
    20,  NOTE_G7, 70,  0,
    20,  NOTE_G7, 40,  0,
    20,  NOTE_A7, 10,  0,
    20,  NOTE_A7, 40,  0,
    20,  NOTE_A7, 70,  0,
    20,  NOTE_A7, 40,  0,
    20,  NOTE_B7, 10,  0,
    20,  NOTE_B7, 40,  0,
    20,  NOTE_B7, 70,  0,
    20,  NOTE_B7, 40,  0,
    20,  NOTE_C8, 10,  0,
    20,  NOTE_C8, 40,  0,
    20,  NOTE_C8, 70,  0,
    20,  NOTE_C8, 40,  0,
    20,  NOTE_E7, 10,  0,
    20,  NOTE_E7, 40,  0,
    20,  NOTE_E7, 70,  0,
    20,  NOTE_E7, 40,  0,
    20,  NOTE_E7, 10,  0,
    20,  NOTE_E7, 40,  0,
    20,  NOTE_E7, 70,  0,
    20,  NOTE_E7, 40,  0,
    20,  NOTE_D7, 10,  0,
    20,  NOTE_D7, 40,  0,
    20,  NOTE_D7, 70,  0,
    20,  NOTE_D7, 40,  0,
    20,  NOTE_C7, 10,  0,
    20,  NOTE_C7, 40,  0,
    20,  NOTE_C7, 70,  0,
    20,  NOTE_C7, 40,  0,
    20,  NOTE_F7, 10,  0,
    20,  NOTE_F7, 40,  0,
    20,  NOTE_F7, 70,  0,
    20,  NOTE_F7, 40,  0,
    20,  NOTE_G7, 10,  0,
    20,  NOTE_G7, 40,  0,
    20,  NOTE_G7, 70,  0,
    20,  NOTE_G7, 40,  0,
    20,  NOTE_A7, 10,  0,
    20,  NOTE_A7, 40,  0,
    20,  NOTE_A7, 70,  0,
    20,  NOTE_A7, 40,  0,
    20,  NOTE_C8, 10,  0,
    20,  NOTE_C8, 40,  0,
    20,  NOTE_C8, 70,  0,
    20,  NOTE_C8, 40,  0,
    20,  NOTE_C8, 10,  0,
    20,  NOTE_C8, 40,  0,
    20,  NOTE_C8, 70,  0,
    20,  NOTE_C8, 40,  0,
    20,  NOTE_C8, 10,  0,
    20,  NOTE_C8, 40,  0,
    20,  NOTE_C8, 70,  0,
    20,  NOTE_D8, 40,  0,
    20,  NOTE_A_7, 10,  0,
    20,  NOTE_A_7, 40,  0,
    20,  NOTE_A_7, 70,  0,
    20,  NOTE_A_7, 40,  0,
    20,  NOTE_A_7, 10,  0,
    20,  NOTE_A_7, 40,  0,
    20,  NOTE_A_7, 70,  0,
    20,  NOTE_A_7, 40,  0,
    20,  NOTE_A_7, 10,  0,
    20,  NOTE_A_7, 40,  0,
    20,  NOTE_A_7, 70,  0,
    20,  NOTE_A_7, 40,  0,
    20,  NOTE_A_7, 10,  0,
    20,  NOTE_A_7, 40,  0,
    20,  NOTE_A_7, 70,  0,
    20,  NOTE_C8, 40,  0,
    20,  NOTE_G7, 10,  0,
    20,  NOTE_G7, 40,  0,
    20,  NOTE_G7, 70,  0,
    20,  NOTE_G7, 40,  0,
    20,  NOTE_G7, 10,  0,
    20,  NOTE_G7, 40,  0,
    20,  NOTE_G7, 70,  0,
    20,  NOTE_G7, 40,  0,
    20,  NOTE_G7, 10,  0,
    20,  NOTE_G7, 40,  0,
    20,  NOTE_G7, 70,  0,
    20,  NOTE_G7, 40,  0,
    20,  NOTE_D7, 10,  0,
    20,  NOTE_D7, 40,  0,
    20,  NOTE_D7, 70,  0,
    20,  NOTE_D7, 40,  0,
    20,  NOTE_C8, 10,  0,
    20,  NOTE_C8, 40,  0,
    20,  NOTE_C8, 70,  0,
    20,  NOTE_C8, 40,  0,
    20,  NOTE_B7, 10,  0,
    20,  NOTE_B7, 40,  0,
    20,  NOTE_B7, 70,  0,
    20,  NOTE_B7, 40,  0,
    0,
};

const uint16_t TuneAlarms[] PROGMEM = {
    (uint16_t)&TuneAlarm0,
    (uint16_t)&TuneAlarm1,
    (uint16_t)&TuneAlarm2,
    (uint16_t)&TuneAlarm3,
    (uint16_t)&TuneAlarm4,
    (uint16_t)&TuneAlarm5,
    (uint16_t)&TuneAlarm6,
    (uint16_t)&TuneAlarm7,
    (uint16_t)&TuneAlarm8,
    (uint16_t)&TuneAlarm9,
};

// Dual Tone sound. CPU must be running at 2MHz
void Sound(const uint8_t *tune) {
    TunePtr = tune;
    PR.PRPE &= 0b11111110;          // Enable TCE1 clock (note frequencies)
    PR.PRPD &= 0b11111101;          // Enable TCD1 clock (note duration)
    TCE0.CTRLE = 0x02;              // Timer TCE0 Split Mode
    TCD1.CNT = 0;
    TCD1.PER = 1;
    TCD1.INTCTRLA = 0x01;           // Low level interrupt
    TCD1.CTRLA = 0b00001110;        // Clock select is event Event CH6 (4.096mS per count)
}

void SoundOff(void) {
    TCE0.CTRLA = 0;             // Disable timers
    TCE0.CTRLB = 0;
    TCD1.CTRLA = 0;
    clrbit(VPORT1.OUT, BUZZER1);
    clrbit(VPORT1.OUT, BUZZER2);
    TCD1.INTCTRLA = 0x00;
    PR.PRPE |= 0b00000001;      // Disable TCE0 clock
    PR.PRPD |= 0b00000010;      // Disable TCD1 clock
}

// Note duration complete. Load next note
ISR(TCD1_OVF_vect) {
    uint8_t length = pgm_read_byte(TunePtr++);
    uint8_t note =   pgm_read_byte(TunePtr++);
    if(length) {                    // Play the notes
        TCD1.PER = length;          // Length
        if(note) {
            TCE0.CTRLB = 0b00100001;    // Enable output compares, H->B, L->A
            TCE0.PERH = note;           // Frequency 1
            TCE0.PERL = note;           // Frequency 2
            TCE0.CNT = 0;
            TCE0.CCBH = note>>1;        // Duty cycle 50%
            TCE0.CCAL = note>>1;        // Duty cycle 50%
            if(CLK.CTRL==0) {           // CPU is running at 2MHz
                TCE0.CTRLA = 0x04;      // Enable timer, Prescale 8 -> clock is 250kHz        
            } else {
                TCE0.CTRLA = 0x05;      // Enable timer, Prescale 64-> clock is 500kHz
            }                
        } else {                        // Silence
            TCE0.CTRLB = 0;
            TCE0.CTRLA = 0;
            clrbit(VPORT1.OUT, BUZZER1);
            clrbit(VPORT1.OUT, BUZZER2);
        }
    } else {                        // Sound Off
        SoundOff();
    }
}

