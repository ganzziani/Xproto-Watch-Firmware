// Calculate moon phase and display it on screen

#include <avr/pgmspace.h>
#include "moon.h"
#include "main.h"
#include "utils.h"
#include "ffft.h"
#include "config.h"

// Synodic month length: 29.530588853 days
// 29.530588853 * 65536 = 1935316
#define MOON_CYCLE_LENGTH 1935316UL

uint8_t MoonPhase(Type_Time *date) {
    // Calculate moon phase by counting moon cycles from known date
    Type_Time KnownNewMoon;     // Known new moon: January 25th 1944
    KnownNewMoon.day = 25;
    KnownNewMoon.month = 1;
    KnownNewMoon.year = 1944-EPOCH_YEAR;
    uint32_t DaysSinceNewMoon = (uint32_t)DaysDifference(date, &KnownNewMoon) << 16;
    while(DaysSinceNewMoon>MOON_CYCLE_LENGTH) {
        DaysSinceNewMoon-=MOON_CYCLE_LENGTH;
    }
    uint8_t Phase = DaysSinceNewMoon>>13;   // Phase is now between 0 and 236
    return Phase;
}    

void Moon(void) {
    Type_Time date;
    uint8_t newday = 0; // check if redraw is needed
    date.day = NowDay;
    date.month = NowMonth;
    date.year = NowYear;
	uint8_t timeout=255;
    setbit(Misc, redraw);
	clrbit(WSettings, goback);
    do {
        if(testbit(Misc,userinput)) {
            uint8_t Phase = MoonPhase(&date);
            timeout=255;
            clrbit(Misc, userinput);
            if(testbit(Buttons,KML)) setbit(WSettings, goback);
            if(testbit(Buttons,KUR) || testbit(Buttons,KUL)) AddDay(&date);
            if(testbit(Buttons,KBR) || testbit(Buttons,KBL)) SubDay(&date);
            if(testbit(Buttons,K1)) { // Previous full moon
                uint8_t Phase1;
                Phase=0;
                do {
                    Phase1 = Phase;
                    SubDay(&date);
                    Phase = MoonPhase(&date);
                } while(!(Phase1>118 && Phase<=118)); // While not Full moon
                newday = 0; //redraw moon
            }
            if(testbit(Buttons,K2)) { // Today
                date.day = NowDay;
                date.month = NowMonth;
                date.year = NowYear;
            }                
            if(testbit(Buttons,K3)) { // Next full moon
                uint8_t Phase1;
                Phase=255;
                do {
                    Phase1 = Phase;
                    AddDay(&date);
                    Phase = MoonPhase(&date);
                } while(!(Phase1<118 && Phase>=118)); // While not Full moon
                newday = 0; //redraw moon
            }
        }
        if(newday!=date.day) {
            clr_display();
            newday=date.day;
            uint8_t Phase = MoonPhase(&date);
		    lcd_goto(34,0);
		    print5x8(&STRS_optionmenu[0][17]);    // STRS_optionmenu[0][17] contains the word Moon
		    print5x8(PSTR(" Phase"));
            PrintDate(34,1,&date);
            uint8_t index;
            if(Phase < 7) index = 0;           // New Moon: 0-6 (0-3%)
            else if(Phase < 52) index = 1;     // Waxing Crescent: 7-51 (3-22%)
            else if(Phase < 66) index = 2;     // First Quarter: 52-65 (22-28%)
            else if(Phase < 111) index = 3;    // Waxing Gibbous: 66-110 (28-47%)
            else if(Phase < 125) index = 4;    // Full Moon: 111-124 (47-53%)
            else if(Phase < 170) index = 5;    // Waning Gibbous: 125-169 (53-72%)
            else if(Phase < 184) index = 6;    // Last Quarter: 170-183 (72-78%)
            else if(Phase < 229) index = 7;    // Waning Crescent: 184-228 (78-97%)
            else index = 0;                    // Back to New Moon: 229-236
            lcd_goto(24,13); print5x8(STRS_MoonPhase[index]);
            lcd_goto(0,15);  print5x8(STR_MoonMenu);
            dma_display();
            // Moon bitmap with realistic phase shading
            uint8_t const *b = MoonBMP;
            uint8_t data;
            uint8_t *p = &Disp_send.DataAddress[(uint16_t)((-96)*18)+4];
            for (int8_t x = -32; x < 32; x++) {
                for (uint8_t row = 0; row < 8; row++) {
                    data = pgm_read_byte(b++);
                    uint8_t output = 0;
        
                    for (uint8_t bit = 0; bit < 8; bit++) {
                        int8_t y = (row * 8) + bit - 32;  // y coordinate relative to moon center
            
                        if (data & (0x80 >> bit)) {
                            // Pixel is part of the moon circle
                            uint8_t lit = 0;
                
                            // Calculate distance from center
                            int16_t r_squared = (int16_t)x * x + (int16_t)y * y;
                
                            if (r_squared <= 1024) {  // Within moon circle (32^2 = 1024)
                                if (Phase == 118) {
                                    lit = 1;  // Full moon
                                } else if (Phase == 0) {
                                    lit = 0;  // New moon
                                } else {
                                    uint8_t phase_dist;
                                    uint8_t waxing;
                        
                                    if (Phase < 118) {
                                        phase_dist = 118 - Phase;  // Waxing: 0-30 (0=full, 30=new)
                                        waxing = 0;
                                    } else {
                                        phase_dist = Phase - 118;   // Waning: 0-30 (0=full, 30=new)
                                        waxing = 1;
                                    }
                        
                                    int8_t shadow_x = Cosine60(phase_dist>>2, 32);  // Calculate terminator scale

                                    // Calculate ellipse_x for this y-coordinate
                                    int8_t ellipse_x;
                                    if (shadow_x == 0) {
                                        ellipse_x = 0;
                                    } else {
                                        // ellipse_x_squared = (1024-y^2) * x^2 / 1024
                                        int16_t y2 = (int16_t)y * y;                  // 0..1024
                                        int16_t sx2 = (int16_t)shadow_x * shadow_x;   // 0..1024
                                        int16_t ellipse_x_squared = sx2 - (((uint32_t)y2 * sx2) >> 10);
                                        ellipse_x = isqrt16(ellipse_x_squared);
                                    }

                                    // Calculate terminator_position based on shadow_x sign
                                    int8_t terminator_position = 0;
                                    if (shadow_x > 0) {
                                        terminator_position = ellipse_x;        // Positive: right side
                                    } else if (shadow_x < 0) {
                                        terminator_position = -ellipse_x;       // Negative: left side
                                    }

                                    // Apply lighting condition based on phase direction
                                    if (waxing) {
                                        lit = (x >= -terminator_position);    // Waxing: right-side lit
                                    } else {
                                        lit = (x <= terminator_position);     // Waning: left-side lit
                                    }
                                }
                            }
                            if (lit) {
                                output |= (0x80 >> bit);
                            }
                        }
                    }
                    *p++ = output;
                }
                p += 18 - 8;   // Next line
                if(!testbit(LCD_CTRL,LCD_CS)) dma_display();    // Send new data if previous transfer ended
            }
            WaitDisplay();
            dma_display();
        }
        WaitDisplay();
        SLP();
    } while(timeout-- && !testbit(WSettings, goback));
    clrbit(Misc, userinput);
    setbit(MStatus, update);
}
