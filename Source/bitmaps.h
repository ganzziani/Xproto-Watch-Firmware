#ifndef _BITMAPS_H
#define _BITMAPS_H

#include <stdint.h>
#include <avr/eeprom.h>

#define FONT3x6_d1      0x06    // Long d character part 1
#define FONT3x6_d2      0x0E    // Long d character part 2
#define FONT3x6_m       0x08    // End of long m character
#define CHAR_BELL       0x80

extern const uint8_t    Logo[];
extern const uint8_t    Font3x6[];
extern const uint8_t    Font10x15[];
extern const int8_t     TrigDown[];
extern const int8_t     TrigUp[];
extern const int8_t     TrigDual[];
extern const uint8_t    PulsePosIcon[];
extern const uint8_t    PulseNegIcon[];
extern const uint8_t    BattIcon[];
extern const uint8_t    Font5x8[];
extern const uint8_t    Digit0_11x21[];
extern const uint8_t    Digit1_11x21[];
extern const uint8_t    Digit2_11x21[];
extern const uint8_t    Digit3_11x21[];
extern const uint8_t    Digit4_11x21[];
extern const uint8_t    Digit5_11x21[];
extern const uint8_t    Digit6_11x21[];
extern const uint8_t    Digit7_11x21[];
extern const uint8_t    Digit8_11x21[];
extern const uint8_t    Digit9_11x21[];
extern const uint16_t   Digits_11x21[];
extern const uint8_t    Digit0_12x24[];
extern const uint8_t    Digit1_12x24[];
extern const uint8_t    Digit2_12x24[];
extern const uint8_t    Digit3_12x24[];
extern const uint8_t    Digit4_12x24[];
extern const uint8_t    Digit5_12x24[];
extern const uint8_t    Digit6_12x24[];
extern const uint8_t    Digit7_12x24[];
extern const uint8_t    Digit8_12x24[];
extern const uint8_t    Digit9_12x24[];
extern const uint8_t    Dash_6x8[];
extern const uint16_t   Digits_12x24[];
extern const uint8_t    Digit0_22x48[];
extern const uint8_t    Digit1_22x48[];
extern const uint8_t    Digit2_22x48[];
extern const uint8_t    Digit3_22x48[];
extern const uint8_t    Digit4_22x48[];
extern const uint8_t    Digit5_22x48[];
extern const uint8_t    Digit6_22x48[];
extern const uint8_t    Digit7_22x48[];
extern const uint8_t    Digit8_22x48[];
extern const uint8_t    Digit9_22x48[];
extern const uint16_t   Digits_22x48[];
extern const uint8_t    Dots_4x16[];
extern const uint8_t    PM[];
extern const uint8_t    AM[];
extern const uint8_t    BattPwrIcon[];
extern const uint8_t    ScopeBMP[];
extern const uint8_t    WatchBMP[];
extern const uint8_t    GamesBMP[];
extern const uint8_t    SettingsBMP[];
extern const uint16_t   ChessBMPs[];
extern const uint8_t    MoonBMP[];
extern uint8_t EEMEM    EEDISPLAY[2048];

#endif