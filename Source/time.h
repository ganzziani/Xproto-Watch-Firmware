#ifndef _TIME_H
#define _TIME_H

#include <stdint.h>

#define EPOCH_YEAR      1944

typedef struct {
    uint8_t sec;                 // Seconds         [0-59]
    uint8_t min;                 // Minutes         [0-59]
    uint8_t hour;                // Hours           [0-23]
    uint8_t day;                 // Day             [1-31]
    uint8_t month;               // Month           [1-12]
    uint8_t year;                // Year since 1944
    uint8_t wday;                // Day of week     [0-6] Sunday is 0
} Type_Time;

typedef struct {
    uint8_t hour;
    uint8_t minute;
    uint8_t active;
    uint8_t tune;
} Type_Alarm;

#define LEAP_YEAR(year) (!(year&0x03))   // Year divisible by 4? This check works for the year range of 2000 thru 2099

extern volatile Type_Time now;
extern Type_Time EEMEM EE_saved_time;
extern uint8_t EEMEM EE_WSettings;      // 24Hr format, Date Format, Hour Beep, Alarm On,

void Watch(void);
void Calendar(void);
void findweekday(void);
void SetTimeTimer(void);
void GetTimeTimer(void);
void SetMinuteInterrupt(void);
uint8_t DaysInMonth(Type_Time *timeptr);
uint16_t DaysAwayfromToday(Type_Time *timeptr);
uint8_t FutureDate(Type_Time *timeptr);

// Change item definitions
#define SET_SECOND          1
#define SET_MINUTE          2
#define SET_HOUR            3
#define SET_DAY             4
#define SET_MONTH           5
#define SET_YEAR            6

// Day of the week definitions
#define SUNDAY          0
#define MONDAY          1
#define TUESDAY         2
#define WEDNESDAY       3
#define THURSDAY        4
#define FRIDAY          5
#define SATURDAY        6

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
