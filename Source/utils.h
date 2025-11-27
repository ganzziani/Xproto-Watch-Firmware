#ifndef _UTILS_H
#define _UTILS_H

#include <stdint.h>

// Music notes frequency values for 8bit clock at 250kHz
//      Note       Value     Actual  Desired
#define NOTE_B5      252 //  988.14   987.77
#define NOTE_C6      238 // 1046.02  1046.5
#define NOTE_C_6     224 // 1111.11  1108.73
#define NOTE_D6      212 // 1173.70  1174.66
#define NOTE_D_6     200 // 1243.78  1244.51
#define NOTE_E6      189 // 1315.78  1318.51
#define NOTE_F6      178 // 1396.64  1396.91
#define NOTE_F_6     168 // 1479.28  1479.98
#define NOTE_G6      158 // 1572.32  1567.98
#define NOTE_G_6     149 // 1666.67  1661.22
#define NOTE_A6      141 // 1760.56  1760
#define NOTE_A_6     133 // 1865.67	 1864.66
#define NOTE_B6      126 // 1968.50  1975.53
#define NOTE_C7      118 // 2100.84  2093
#define NOTE_C_7     112 // 2212.38  2217.46
#define NOTE_D7      105 // 2358.49  2349.32
#define NOTE_D_7     99  // 2500     2489.02
#define NOTE_E7      94  // 2631.57  2637.02
#define NOTE_F7      88  // 2808.98  2793.83
#define NOTE_F_7     83  // 2976.19  2959.96
#define NOTE_G7      79  // 3125     3135.96
#define NOTE_G_7     74  // 3333.33  3322.44
#define NOTE_A7      70  // 3521.12  3520
#define NOTE_A_7     66  // 3731.34  3729.31
#define NOTE_B7      62  // 3968.25  3951.07
#define NOTE_C8      59  // 4166.67  4186.01
#define NOTE_C_8     55  // 4464.28  4434.92
#define NOTE_D8      52  // 4716.98  4698.63
#define NOTE_D_8     49  // 5000     4978.03
#define NOTE_E8      46  // 5319.14  5274.04
#define NOTE_F8      44  // 5555.55  5587.65
#define NOTE_F_8     41  // 5952.38  5919.91
#define NOTE_G8      39  // 6250     6271.93
#define NOTE_G_8     37  // 6578.94  6644.88
#define NOTE_A8      34  // 7142.85  7040
#define NOTE_A_8     33  // 7352.94  7458.62
#define NOTE_B8      31  // 7812.5   7902.13

extern const uint8_t    TuneIntro[];
extern const uint8_t    TuneBeep[];
extern const uint8_t    TuneAlarm0[];
extern const uint8_t    TuneAlarm1[];
extern const uint8_t    TuneAlarm2[];
extern const uint8_t    TuneAlarm3[];
extern const uint8_t    TuneAlarm4[];
extern const uint8_t    TuneAlarm5[];
extern const uint8_t    TuneAlarm6[];
extern const uint8_t    TuneAlarm7[];
extern const uint8_t    TuneAlarm8[];
extern const uint8_t    TuneAlarm9[];
extern const uint16_t   TuneAlarms[];

uint8_t ReadCalibrationByte(uint8_t location);	// Read out calibration byte
void CCPWrite(volatile uint8_t * address, uint8_t value);
uint8_t half(uint8_t number);       // Converts an uint to int, then returns the half
uint8_t twice(uint8_t number);      // Converts an uint to int, then return the double, or saturates
char NibbleToChar(uint8_t nibble);  // Converts a nibble to the corresponding ASCII representing the HEX value
void Randomize(uint16_t AddSeed);   // Add to the seed to randomize the random number generator
uint8_t prandom(void);              // Pseudo Random Number - Linear congruential generator
uint8_t qrandom(void);              // Quick Pseudo Random Number
int8_t Sine60(uint8_t angle, int8_t scale);        // Scaled Sine: 60 values -> 2*PI
int8_t Cosine60(uint8_t angle, int8_t scale);      // Scaled Cosine: 60 values -> 2*PI
void Sound(const uint8_t *tune);
void SoundOff(void);

#endif