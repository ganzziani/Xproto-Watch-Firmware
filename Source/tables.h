#ifndef TABLES_H
#define TABLES_H

#include "stdint.h"
#include <avr/pgmspace.h>

extern const int8_t Sine[1024];
extern const int8_t Exponential[1024];
extern const int8_t Hamming[512];
extern const int8_t Hann[512];
extern const int8_t Blackman[512];
extern const int8_t BitReverse256[256];
extern const int16_t BitReverse512[512];
extern const int16_t BitReverse1024[1024];
extern const int16_t BitReverse2048[2048];
extern const int16_t BitReverse4096[4096];

#endif