#include <avr/pgmspace.h>
#include "main.h"

//#define SPANISH

const char VERSION[]    PROGMEM = "FW 2.72";

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

#ifdef SPANISH

const char STRS_mainmenu[][17] PROGMEM = {           // Menus:
    "    Reloj   ",    // Watch
    "Osciloscopio",    // Oscilloscope
    "  -Juegos-  ",    // Games
    "   Ajustes  ",    // Settings
};

const char STRS_optionmenu[][22] PROGMEM = {           // Menus:
    "Hora Calendario      ",    // Watch
    "Cargar Guardar  Start",    // Oscilloscope
    "Snake   Pong  Ajedrez",    // Games
    "Ajuste Diagnos. Sobre",    // Settings
};

const char STRS_Months[][11] PROGMEM = {             // Months
    "Enero", "Febrero", "Marzo", "Abril", "Mayo", "Junio", "Julio", "Augosto", "Septiembre", "Octubre", "Noviembre", "Diciembre"
};
const char STRS_Months_short[][4] PROGMEM = {        // Short Months
    "ENE", "FEB", "MAR", "ABR", "MAY", "JUN", "JUL", "AGO", "SEP", "OCT", "NOV", "DIC"
};
const char STRS_Days[][10] PROGMEM = {               // Days of the week
    " Domingo ", "  Lunes  ", " Martes  ", "Miercoles", " Jueves  ", " Viernes ", " Sabado  "
};
const char STRS_Days_short[][4] PROGMEM = {          // Short Days of the week
    "Do", "Lu", "Ma", "Mi", "Ju", "Vi", "Sa"
};
const char STR_Weekdays[] PROGMEM = "Do Lu Ma Mi Ju Vi Sa";

const char STR_White[]      PROGMEM = "Blanco: ";
const char STR_Black[]      PROGMEM = "Negro:  ";
const char STR_Level[]      PROGMEM = "Nivel:  ";
const char STR_Player[]     PROGMEM = "Jugador";
const char STR_Human[]      PROGMEM = "Humano";
const char STR_GameMenu[]   PROGMEM = "Jugadr1 Jugadr2 Start";

#else   // English

const char STRS_mainmenu[][17] PROGMEM = {           // Menus:
    "    Watch   ",    // Watch
    "Oscilloscope",    // Oscilloscope
    " - Games -  ",    // Games
    "  Settings  ",    // Settings
};

const char STRS_optionmenu[][22] PROGMEM = {           // Menus:
    "Time  Calendar       ",    // Watch
    "Load    Save    Start",    // Oscilloscope
    "Snake   Pong    Chess",    // Games
    "Config Diagnose About",    // Settings
};

const char STRS_Months[][11] PROGMEM = {             // Months
    "January", "February", "March", "April", "May", "June", "July", "August", "September", "October", "November", "December",
};
const char STRS_Months_short[][4] PROGMEM = {        // Short Months
    "JAN", "FEB", "MAR", "APR", "MAY", "JUN", "JUL", "AUG", "SEP", "OCT", "NOV", "DEC",
};
const char STRS_Days[][10] PROGMEM = {               // Days of the week
    " Sunday  ", " Monday  ", " Tuesday ", "Wednesday", "Thursday ", " Friday  ", "Saturday "
};
const char STRS_Days_short[][4] PROGMEM = {          // Short Days of the week
    "Su", "Mo", "Tu", "We", "Th", "Fr", "Sa"
};
const char STR_Weekdays[] PROGMEM = "Su Mo Tu We Th Fr Sa";

const char STR_White[]      PROGMEM = "White:  ";
const char STR_Black[]      PROGMEM = "Black:  ";
const char STR_Level[]      PROGMEM = "Level:  ";
const char STR_Player[]     PROGMEM = "Player ";
const char STR_Human[]      PROGMEM = "Human ";
const char STR_GameMenu[]   PROGMEM = "Player1 Player2 Start";

#endif

const char STR_CPU[] PROGMEM = "CPU   ";
const char STR_P1[] PROGMEM = " 1: ";
const char STR_P2[] PROGMEM = " 2: ";
