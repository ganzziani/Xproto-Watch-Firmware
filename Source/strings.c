#include <avr/pgmspace.h>
#include "main.h"

//#define SPANISH

const char VERSION[]    PROGMEM = "2.91";

// Strings with special characters:
// 0x1F = delta
const char STR_1_over_delta_T[] PROGMEM = { ' ', '1', 0x16, 0x1F, 'T', '=', 0 };  // 1/delta T
const char STR_delta_V[]        PROGMEM = { 0x1F, 'V', '=', 0 };        // delta V
const char STR_KHZ[]            PROGMEM = "KHZ";                        // KHz
const char STR_KCNT[]           PROGMEM = "KCNT";                       // C
const char STR_STOP[]           PROGMEM = "STOP";                       // STOP
const char STR_mV[]             PROGMEM = { ' ', 0x1A, 0x1B, 'V', 0 };  // mV
const char STR_V[]              PROGMEM = " V";                         // V
const char STR_Vdiv[]		    PROGMEM = { 'V', 0x1C, 0x1D, 0x1E, 0 }; // V/div
const char STR_Sdiv[]		    PROGMEM = { 'S', 0x1C, 0x1D, 0x1E, 0 }; // S/div
const char STR_F1[]             PROGMEM = "F1: ";                       // 1:
const char STR_F2[]             PROGMEM = "F2: ";                       // 2:

const char STRS_mainmenu[][17] PROGMEM = {           // Menus:
    "    Watch   ",    // Watch
    "Oscilloscope",    // Oscilloscope
    " - Games -  ",    // Games
    "  Settings  ",    // Settings
};

const char STRS_optionmenu[][22] PROGMEM = {           // Menus:
    "Time  Calendar  Astro",    // Watch
    "Start   Image Profile",    // Oscilloscope
    "Snake    Qix    Chess",    // Games
    "Config Diagnose About",    // Settings
    "Easy    Normal   Hard",    // Qyx Menu
};

const char STRS_Months[][11] PROGMEM = {             // Months
    "January", "February", "March", "April", "May", "June", "July", "August", "September", "October", "November", "December",
};

const char STRS_Days[][10] PROGMEM = {               // Days of the week
    " Sunday  ", " Monday  ", " Tuesday ", "Wednesday", "Thursday ", " Friday  ", "Saturday "
};

const char STR_Weekdays[] PROGMEM = "Su Mo Tu We Th Fr Sa";

const char STR_White[]      PROGMEM = "White: ";
const char STR_Black[]      PROGMEM = "Black: ";
const char STR_Level[]      PROGMEM = "Level:";
const char STR_Player[]     PROGMEM = "Player";
const char STR_Human[]      PROGMEM = "Human";
const char STR_Snake[]      PROGMEM = "Snake";
const char STR_GameMenu1[]  PROGMEM = "Player1 Player2 Start";
const char STR_GameMenu2[]  PROGMEM = "Players  Level  Start";
const char STR_MoonMenu[]   PROGMEM = "Prev.FM Today Next.FM";

const char STRS_MoonPhase[][16] PROGMEM = {
    "   New Moon    ",
    "Waxing Crescent",
    " First Quarter ",
    "Waxing Gibbous ",
    "   Full Moon   ",
    "Waning Gibbous ",
    " Last Quarter  ",
    "Waning Crescent",
};

const char STRS_Settings[][17] PROGMEM = {
    " Hourly Beep    ",
    " 24 Hour Format ",
    " Year at the end",
    " Month after day",
    " Show Moon icon ",
//    " Sunrise &Sunset ",
};

const char STR_ON[] PROGMEM = " ON";
const char STR_OFF[] PROGMEM = " OFF";
const char STR_CPU[] PROGMEM = "CPU   ";
const char STR_P1[] PROGMEM = " 1: ";
const char STR_P2[] PROGMEM = " 2: ";

const char STR_am[] PROGMEM = "am";
const char STR_pm[] PROGMEM = "pm";
const char STR_Reset[] PROGMEM = " RST:";
