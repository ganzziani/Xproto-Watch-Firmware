#ifndef _MOON_H
#define _MOON_H

#include "time.h"

extern const uint8_t MoonPhaseThresh[8];   // Phase thresholds: {7,52,66,111,125,170,184,229}
uint8_t CalculateMoonPhase(Type_Time *date);
void Moon(void);

#endif