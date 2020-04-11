/****************************************************************************

Oscilloscope Watch

Gabotronics
December 2018

Copyright 2018 Gabriel Anzziani

This program is distributed under the terms of the GNU General Public License 

www.gabotronics.com
email me at: gabriel@gabotronics.com

*****************************************************************************/

#ifndef _TIME_H
#define _TIME_H

#include <stdint.h>

typedef struct {
    uint8_t sec;                 // Seconds.        [0-59]
    uint8_t min;                 // Minutes.        [0-59]
    uint8_t hour;                // Hours.          [0-23]
    uint8_t mday;                // Day.            [1-31]
    uint8_t mon;                 // Month.          [1-12]
    uint8_t year;                // Year since 2000
    uint8_t wday;                // Day of week.    [0-6]
} time_var;

#define LEAP_YEAR(year) ((year%4)==0)   // Valid for the range the watch works on

extern volatile time_var now;
extern time_var EEMEM saved_time;

void gettime(time_var *timep);
void settime(time_var *timep);
void Watch(void);
void Calendar(void);
void findweekday(time_var *timeptr);
void SetTimeTimer(void);
void GetTimeTimer(void);
void SetMinuteInterrupt(void);

#define MAX_FACES       3   // Number of implemented watch faces

// Change item definitions
#define SET_SECOND          1
#define SET_MINUTE          2
#define SET_HOUR            3
#define SET_DAY             4
#define SET_MONTH           5
#define SET_YEAR            6
#define SET_ALARM_MINUTE    7
#define SET_ALARM_HOUR      8

// Day of the week definitions
#define SATURDAY        0
#define SUNDAY          1
#define MONDAY          2
#define TUESDAY         3
#define WEDNESDAY       4
#define THURSDAY        5
#define FRIDAY          6

// Month definitions
#define JANUARY         1
#define FEBRUARY        2
#define MARCH           3
#define APRIL           4
#define MAY             5
#define JUNE            6
#define JULY            7
#define AUGUST          8
#define SEPTEMBER       9
#define OCTOBER         10
#define NOVEMBER        11
#define DECEMBER        12

#endif
