/*****************************************************************************

Oscilloscope Watch

Gabotronics
March 2019

Copyright 2019 Gabriel Anzziani

This program is distributed under the terms of the GNU General Public License

ATXMEGA256A3U
Compiled with GCC, -Os optimizations

www.gabotronics.com
email me at: gabriel@gabotronics.com

*****************************************************************************/

#include <util/delay.h>
#include <avr/io.h>
#include <avr/pgmspace.h>
#include <string.h>
#include <avr/interrupt.h>
#include "main.h"
#include "utils.h"
#include "display.h"
#include "mygccdef.h"
#include "time.h"
#include "asmutil.h"
#include "build.h"

void face0(void);
void face1(void);
void face2(void);
void Stopwatch(void);
void CountDown(void);
uint8_t firstdayofmonth(time_var *timeptr);
void GetTimeTimer(void);
void SetMinuteInterrupt(void);
uint8_t DaysInMonth(time_var *timeptr);

const uint8_t monthDays[] PROGMEM = { 31,28,31,30,31,30,31,31,30,31,30,31 };

const char months[][10] PROGMEM = {             // Months
    "January", "February", "March", "April", "May", "June", "July", "August", "September", "October", "November", "December",
};

const char months_short[][4] PROGMEM = {        // Short Months
    "JAN", "FEB", "MAR", "APR", "MAY", "JUN", "JUL", "AUG", "SEP", "OCT", "NOV", "DEC",
};

const char days[][10] PROGMEM = {               // Days of the week
    "Saturday ", " Sunday  ", " Monday  ", " Tuesday ", "Wednesday", "Thursday ", " Friday  ",
};

const char days_short[][4] PROGMEM = {          // Short Days of the week
    "Sat", "Sun", "Mon", "Tue", "Wed", "Thu", "Fri"
};

time_var EEMEM saved_time = {   // Last known time
    BUILD_SECOND,				// sec      Seconds      [0-59]
    BUILD_MINUTE,				// min      Minutes      [0-59]
    BUILD_HOUR,					// hour     Hours        [0-23]
    BUILD_DAY,					// mday     Day          [1-31]
    BUILD_MONTH,				// mon      Month        [1-12]
    BUILD_YEAR,					// year     Year since 2000
    0,							// wday     Day of week  [0-6]   Saturday is 0
};

uint8_t EEMEM EE_Alarm_Hour = 8;
uint8_t EEMEM EE_Alarm_Minute = 0;
uint8_t EEMEM EE_WatchSettings = 0; // 24Hr format, Date Format, Hour Beep, Alarm On, 

// TO DO: Temperature compensation:
// f = fo (1-PPM(T-To))^2
// RTC will lose time if the temperature is increased or decreased from the room temperature value (25°C)
// Coefficient = ?T^2 x -0.036 ppm
//The firmware repeats the following steps once per minute to calculate and accumulate lost time.
// 1. The ADC is used to measure the die temperature from the on-chip temperature sensor.
// 2. The value measured by the ADC is then used to calculate the deviation in ppm, and the result is stored in memory.
// This indicates the number of microseconds that need to be compensated.
//
// At the end of a 24-hour period, the total accumulated error is added to the RTC time to complete the compensation
// process. The temperature is assumed to not vary widely within a one-minute period.


// The three interrupts below are Naked-ISR since there is no need to save any registers

// The two interrupts below generate a 1Hz signal used by the LCD to prevent image burn-in
// Changing a bit in a VPORT does not change the Status Register
ISR(RTC_COMP_vect, ISR_NAKED) { // RTC Compare, occurs every 1s, phase 180
    EXTCOMML();                 // Set LCD polarity inversion low
    asm("reti");
}
ISR(RTC_OVF_vect, ISR_NAKED) {  // RTC Overflow, occurs every 1s, phase 0
    EXTCOMMH();                 // Set LCD polarity inversion high
    asm("reti");
}

// 12 Hour interrupt - Toggle the T flag (used as AM/PM bit)
ISR(TCF0_OVF_vect, ISR_NAKED) {
	__asm__ volatile (
        "brts 1f" "\n\t"    // Branch if T is set
        "set"     "\n\t"    // Set T
        "reti"    "\n\t"
        "1:clt"   "\n\t"    // Clear T
        "reti"
	);    
}

// 1 Minute interrupt, disabled during oscilloscope mode
ISR(TCF0_CCA_vect) {
    TCF0.CCA+=60;   // Set next interrupt on next minute
    if(TCF0.CCA>43199) TCF0.CCA=59;
    NowSecond=0;
    NowMinute++;                            // Update minute
    if(NowMinute>=60) {
        NowMinute=0;
        NowHour++;                          // Update hour
        if(NowHour>=24) {
            NowHour=0;
            NowWeekDay++;                   // Update weekday
            if(NowWeekDay>=7) NowWeekDay=0;
            NowDay++;                       // Update day
            if(NowDay>DaysInMonth(NOW)) {
                NowDay=1;
                NowMonth++;                 // Update month
                if(NowMonth>DECEMBER) {
                    NowMonth=JANUARY;
                    NowYear++;              // Update year
                }
            }
            T.TIME.battery = MeasureVin(0);
        }            
        if(testbit(WSettings,hourbeep)) {   // On the hour beep
            Sound(NOTE_B7,NOTE_B7);
        }
    }
    setbit(Misc, redraw);
}

// Sync variables from TCF0
void GetTimeTimer(void) {
    uint16_t TotalSeconds;
    uint8_t m=0,h=0;
    if(testbit(SREG, CPU_T_bp)) h=12;
    TotalSeconds = TCF0.CNT;
    while (TotalSeconds>=3600)	{ h++; TotalSeconds-=3600; }
    while (TotalSeconds>=60)	{ m++; TotalSeconds-=60; }
    NowMinute = m;
    NowHour = h;
    NowSecond = TotalSeconds;
}

// Validate time variable and update TCF0
void SetTimeTimer(void) {
    cli();
    if(AlarmMinute>=60) AlarmMinute=0;
    if(AlarmHour>=24) AlarmHour=0;
    if(NowSecond>=60) NowSecond=0;
    if(NowMinute>=60) NowMinute=0;
    if(NowHour>=24) NowHour=0;
    if(NowMonth==0 || NowMonth>12) NowMonth=1;
    if(NowDay==0 || NowDay>DaysInMonth(NOW)) NowDay=1;
    if(NowYear>99) NowYear=0;
    uint8_t hour=NowHour;
    CLT();                          // Clear T bit (used for am/pm)
    if(hour>=12) {
        SET();                      // Set T bit
        hour-=12;
    }
    TCF0.CNT = (hour*3600)+(NowMinute*60)+NowSecond;
    findweekday(NOW);
    SetMinuteInterrupt();
    sei();
}

// Set TCF0 compare interrupt to next minute
void SetMinuteInterrupt(void) {
     uint8_t h=NowHour;
     if(h>=12) h-=12;
     uint16_t count = (h*3600)+((NowMinute+1)*60)-1;
     if(count>43199) {      // Interrupt above max 12 hours?
         count-=43199;
     }
     TCF0.CCA = count;
     TCF0.INTFLAGS = 0xF0;   // Clear compare flag
     TCF0.INTCTRLB = 0x02;   // 1 minute interrupt, medium level
}

void Watch(void) {
    uint8_t newface=0, Faces=0;
    uint8_t Change_timeout=0;
    Buttons = 0;
    clrbit(WatchBits, goback);
    clr_display();
    SwitchBuffers();
    clr_display();
    setbit(Misc, redraw);
    Menu = 0;   // Selected item to change
    AlarmHour = eeprom_read_byte(&EE_Alarm_Hour);
    AlarmMinute = eeprom_read_byte(&EE_Alarm_Minute);
    T.TIME.battery = MeasureVin(0);
    do {                    // Cycle time is 0.5 seconds
                                                        // When to refresh screen? ->
        if( Menu ||                                     // When user is changing the time
            testbit(Misc, redraw) ||                    // Update requested (at least every minute)
            (SECPULSE() && testbit(WSettings,disp_secs))    // Every second when displaying seconds is enabled
            ) {
            clrbit(Misc, redraw);
            if(AlarmHour==NowHour && AlarmMinute==NowMinute) {
                setbit(Misc, redraw);
                Sound(NOTE_B7,NOTE_B7);
            }
            // Send previous frame to display, then switch buffers
            if(testbit(WSettings,disp_secs) && !Menu) {    // Don't use double buffer when user is changing
                dma_display();                              // the time or when refreshing the screen every minute
                SwitchBuffers();
            }
            switch(Faces) {
                case 0: face0(); break;
                case 1: face1(); break;
				case 2: face2(); break;
            }
            // If not using double buffer, then send the data to the LCD now
            if(!testbit(WSettings,disp_secs) || Menu) {
                dma_display();
            }
            if(Menu) {    // Read Time Timer when the user is changing the time
                GetTimeTimer();
                if(Change_timeout) Change_timeout--;   // Decrease changing time timeout
                else {
                    Menu=0;
                    setbit(Misc, redraw);
                }
            }
            else {                  // Otherwise, just increase the seconds to save CPU time
                NowSecond++;
                if(NowSecond>=60) NowSecond=0;   
            }
        }
        // Check user input
        if(testbit(Misc, userinput)) {
            clrbit(Misc, userinput);
            if(Menu==0) {                         // Currently not changing time
                GetTimeTimer();
                if(testbit(Buttons,KML)) {
                    setbit(WatchBits, goback);
                }
                if(testbit(Buttons, K1)) Stopwatch();
                if(testbit(Buttons, K2)) CountDown();
                //if(testbit(Buttons, K3)) Alarms();
                if(testbit(Buttons, KUR)) {
                    if(Faces==0) newface = MAX_FACES-1;
                    else newface = Faces-1;
                }
                if(testbit(Buttons, KBR)) newface = Faces + 1;
                if(testbit(Buttons, KUL)) {     // Toggle second display
                    setbit(Misc, redraw);
                    togglebit(WSettings, disp_secs);
                }                    
                if(testbit(Buttons, KBL)) {
                    if(testbit(WatchBits,longpress)) {   // Check if this is a long press
                        Change_timeout = 120;   // 120 half seconds -> 1 Minute
                        Menu=SECOND;
                        clrbit(WatchBits,longpress);     // Prevent repeat key
                        clrbit(WatchBits,prepress);
                    }
                }
                if(testbit(Buttons,KUL)) {
                    if(testbit(WatchBits,longpress)) {   // Check if this is a long press
                        Change_timeout = 120;   // 120 half seconds -> 1 Minute
                        Menu=ALARM_MINUTE;
                        clrbit(WatchBits,longpress);     // Prevent repeat key
                        clrbit(WatchBits,prepress);
                    }
                }
            } else {   // Changing time
                if(testbit(Buttons,KML)) Menu=0;
                if(testbit(Buttons,KBL)) {
                    if(!testbit(WatchBits,longpress)) { // Already changing time -> go to next item
                        Menu++;
                        if(Menu>YEAR) {
                            Menu=0;
                        }
                    }
                }
                if(testbit(Buttons,KUL)) {
                    if(!testbit(WatchBits,longpress)) { // Already changing time -> go to next item
                        Menu++;
                        if(Menu>ALARM_HOUR) {
                            Menu=0;
                            eeprom_write_byte(&EE_Alarm_Hour, AlarmHour);
                            eeprom_write_byte(&EE_Alarm_Minute, AlarmMinute);
                        }
                    }
                }
                cli();  // Prevent the RTC interrupt from changing the time in this block
                switch(Menu) {
                    case SECOND:
                        if(testbit(Buttons,KUR)) NowSecond++;
                        if(testbit(Buttons,KBR)) NowSecond--;
                    break;
                    case MINUTE:
                        if(testbit(Buttons,KUR)) NowMinute++;
                        if(testbit(Buttons,KBR)) if(NowMinute) NowMinute--; else NowMinute=59;
                    break;
                    case HOUR:
                        if(testbit(Buttons,KUR)) NowHour++;
                        if(testbit(Buttons,KBR)) if(NowHour) NowHour--; else NowHour=23;
                    break;
                    case DAY:
                        if(testbit(Buttons,KUR)) NowDay++;
                        if(testbit(Buttons,KBR)) if(NowDay>1) NowDay--; else NowDay=DaysInMonth(NOW);
                    break;
                    case MONTH:
                        if(testbit(Buttons,KUR)) NowMonth++;
                        if(testbit(Buttons,KBR)) if(NowMonth>1) NowMonth--; else NowMonth=12;
                    break;
                    case YEAR:
                        if(testbit(Buttons,KUR)) if(NowYear<98) NowYear++;
                        if(testbit(Buttons,KBR)) if(NowYear) NowYear--;
                    break;
                    case ALARM_MINUTE:
                        if(testbit(Buttons,KUR)) AlarmMinute++;
                        if(testbit(Buttons,KBR)) if(AlarmMinute) AlarmMinute--; else AlarmMinute=59;
                    break;
                    case ALARM_HOUR:
                        if(testbit(Buttons,KUR)) AlarmHour++;
                        if(testbit(Buttons,KBR)) if(AlarmHour) AlarmHour--; else AlarmHour=23;
                    break;
                }
                SetTimeTimer();
                sei();
            }
            if(Faces!=newface) {    // The watch face has changed
                clr_display();
                SwitchBuffers();
                clr_display();
                if(newface>=MAX_FACES) Faces = 0;
                else Faces=newface;
                switch(Faces) {     // Initialize face
                    case 0: break;
                    case 1:
                    // Pre calculate background image, save in buffer 3
                    CPU_Fast();
                    Disp_send.buffer=T.TIME.buffer3+127*18;
                    clr_display_3();
                    for(uint8_t i=0; i<60; i++) {   // Circumference markers
                        lcd_line(63+Sine60(i,60),63-Cosine60(i,60),
                        63+Sine60(i,63),63-Cosine60(i,63));
                    }
                    // Print big numbers
                    lcd_goto(54,2); GLCD_Bigchar(1); GLCD_Bigchar(2);   // 12
                    lcd_goto(108,8); GLCD_Bigchar(3);                   // 3
                    lcd_goto(58,14); GLCD_Bigchar(6);                   // 6
                    lcd_goto(8,8); GLCD_Bigchar(9);                     // 9
                    SwitchBuffers();
                    CPU_Slow();
                    break;
                    case 2: 
                        T.TIME.x = 0;
                        T.TIME.y = 0;
                    break;
                }
            }
            setbit(Misc, redraw);         
        }
        if(Buttons) {   // Buttons are still pressed -> prepress -> longpress
            if(testbit(WatchBits,prepress)) {
                setbit(Misc, userinput);
                setbit(WatchBits,longpress);
            } else setbit(WatchBits,prepress);
        } else {
            clrbit(WatchBits,longpress);
            clrbit(WatchBits,prepress);
        }            
        WaitDisplay();  // Wait until all LCD data has been sent
        SoundOff();
        SLP();          // Sleep
    } while(!testbit(WatchBits,goback));
}

// Digital Watch Face
void face0(void) {
    uint8_t d=0;
    if(SECPULSE() || Buttons) {  // Blink item to be changed unless buttons are pressed
        setbit(WatchBits,blink);
    } else clrbit(WatchBits,blink);
    if((testbit(WSettings,disp_secs) || Menu) &&         // Displaying seconds or changing time
       (testbit(WatchBits,blink) || Menu!=SECOND)) {  // Flash when changing
        printN_7seg(104, 11, NowSecond, 2);
    } else {    // Not displaying seconds, erase area
        fillRectangle(104,88,127,108,0);
    }
    if(testbit(WatchBits,blink) || Menu!=MINUTE) {  // Flash when changing
        uint8_t n=NowMinute; d=0;
        while (n>=10) { d++; n-=10; }
        bitmap(55,9,(const uint8_t *)pgm_read_word(DIGITS+d));
        bitmap(80,9,(const uint8_t *)pgm_read_word(DIGITS+n));
    } else {
        fillRectangle(55,72,101,111,0);
    }
    if(testbit(WatchBits,blink) || Menu!=HOUR) {  // Flash when changing
        uint8_t n=NowHour;
        if(testbit(SREG, CPU_T_bp)) bitmap(105,9,PM);
        else bitmap(105,9,AM);
        if(n>=12) n-=12;
        if(n==0) n=12;
        if(n>=10) { n-=10; bitmap(0,9,DIGI1); }
        else  fillRectangle(17,72,21,111,0);
        bitmap(25,9,(const uint8_t *)pgm_read_word(DIGITS+n));
    } else {
        fillRectangle(0,72,46,111,0);
    }
    bitmap(49,9,DOTS);
    // Date
    bitmap(118,0,BELL);
    uint8_t temp_battery = T.TIME.battery;
    if(temp_battery) {
		bitmap(2,0,BATTERY);
		fillRectangle(4,2,4+temp_battery-1,5,255);
	}
	else bitmap(2,0,BATTERY_POWER);
    lcd_goto(74,0);
    uint8_t hour = AlarmHour;
    if(testbit(WatchBits,blink) || Menu!=ALARM_HOUR) {  // Flash when changing
        uint8_t n=AlarmHour;
        if(n>=12) n-=12;
        if(n==0) n=12;
        if(!testbit(WSettings, time24)) printN_5x8(n);
        else printN_5x8(hour);
    } else { putchar5x8(' '); putchar5x8(' '); }
    putchar5x8(':');
    if(testbit(WatchBits,blink) || Menu!=ALARM_MINUTE) {  // Flash when changing
        printN5x8(AlarmMinute);
    }  else { putchar5x8(' '); putchar5x8(' '); }
    if(!testbit(WSettings, time24)) {
        if(hour>12) lcd_put5x8(PSTR("pm"));
        else lcd_put5x8(PSTR("am"));
    }        
    lcd_goto(40,2);
    lcd_put5x8(days[NowWeekDay]);
    if(testbit(WatchBits,blink) || Menu!=DAY) {  // Flash when changing
        uint8_t n=NowDay; d=0; // Day.         [1-31]
        while (n>=10) { d++; n-=10; }
        bitmap(101,4,(const uint8_t *)pgm_read_word(mDIGITS+d));
        bitmap(115,4,(const uint8_t *)pgm_read_word(mDIGITS+n));
        } else {
        fillRectangle(101,32,129,55,0);
    }
    if(testbit(WatchBits,blink) || Menu!=MONTH) {  // Flash when changing
        uint8_t n=NowMonth;    // Month.       [1-12]
        if(n>=10) { n-=10; bitmap(70,4,mDIGI1b); }
        bitmap(75,4,(const uint8_t *)pgm_read_word(mDIGITS+n));
        } else {
        fillRectangle(70,32,89,55,0);
    }
    if(testbit(WatchBits,blink) || Menu!=YEAR) {
        uint8_t n=NowYear; d=0; // Year
        while (n>=10) { d++; n-=10; }
        bitmap(1,4,mDIGI2);
        bitmap(15,4,mDIGI0);
        bitmap(29,4,(const uint8_t *)pgm_read_word(mDIGITS+d));
        bitmap(43,4,(const uint8_t *)pgm_read_word(mDIGITS+n));
    } else {
        fillRectangle(1,32,55,55,0);
    }
    bitmap(57,4,mDIGIdash);
    bitmap(89,4,mDIGIdash);
}

// Analog Watch Face
void face1(void) {
    uint8_t s,m,h;    
    m=NowMinute; s=NowSecond;
    h=NowHour;
    if(h>=12) h-=12;
    h=h*5+m/12; // Add minutes/12 to hour needle
    // Background image
    memcpy(Disp_send.buffer-127*18,T.TIME.buffer3,DISPLAY_DATA_SIZE);
    // Date
    lcd_goto(50,10); lcd_putsp(months_short[NowMonth-1]); GLCD_Putchar(' '); printN(NowDay);
    // Hours
    fillTriangle(63+Sine60(h-5+60,8),63-Cosine60(h-5+60,8),
                 63+Sine60(h+5,8),   63-Cosine60(h+5,8),
                 63+Sine60(h,36),    63-Cosine60(h,36), 1);
    // Minutes
    fillTriangle(63+Sine60(m-4+60,8),63-Cosine60(m-4+60,8),
                 63+Sine60(m+4,8),63-Cosine60(m+4,8),
                 63+Sine60(m,50),  63-Cosine60(m,50), 1);
    // Seconds           
    if(testbit(WSettings,disp_secs)) {    
        lcd_line(63,63,63+Sine60(s,54),63-Cosine60(s,54));
    }        
}

typedef struct {
    fixed xx0;      // Location x
    fixed yy0;      // Location y
    int8_t inc;     // Zoom
    uint8_t iter;   // Maximum iterations
    fixed c0x;      // Julia constant x
    fixed c0y;      // Julia constant y
} fractal_struct;

// Hand picked constants and locations for the Julia fractal
const fractal_struct MyFractals[7] PROGMEM = {
    {  199,  -38, 2, 64, -230, 52 },    // Saturday:
    {  210,  -26, 3, 56, -205, 33 },    // Sunday:
    {  153,   36, 2, 76, -179, 78 },    // Monday:
    {   -5,    7, 4, 72, -179, 70 },    // Tuesday:   2 ellipses
    { -253,   19, 2, 47, -179, 70 },    // Wednesday:
    {   -4, -165, 1, 38, -179, 70 },    // Thursday:
    {  270,  -35, 1, 94, -179, 70 },    // Friday:
};
    
// Fractals Watch Face
void face2(void) {
    lcd_goto(0,0); printN5x8(NowMonth); putchar5x8('/'); printN5x8(NowDay);
    lcd_goto(99,0); 
    if(!testbit(WSettings, time24) && NowHour>12) printN_5x8(NowHour-12);
    else printN_5x8(NowHour);
    putchar5x8(':'); printN5x8(NowMinute);
    fractal_struct f;
    memcpy_P(&f, &MyFractals[NowWeekDay], sizeof(fractal_struct));    // Load fractal settings
    for(uint8_t ii=0; ii<8; ii++) {    // 8 pixels at a time
        // Linear Pixel Shuffling for Image Processing
        #define G1 189
        #define G2 277
        #define increment (G2 - G1)
        do {    /* determine next pixel */
            T.TIME.x = ( T.TIME.x + increment ) % G1;
            T.TIME.y = ( T.TIME.y + increment ) % G2;
        } while ((T.TIME.x >=128) || (T.TIME.y >= 128));
        
        int8_t px,py;
        px = (int8_t)(T.TIME.x)-64;  
        py = (int8_t)(T.TIME.y)-64;
        // Julia Fractal
        fixed x2,y2,x0,y0,x,y;
        uint8_t max=0;
        x0 = f.xx0 + f.inc*px;
        y0 = f.yy0 + f.inc*py;
        x = x0;
        y = y0;
        do { // check if the point belongs to the set
            x2 = multfix(x,x);              // x^2
            y2 = multfix(y,y);              // y^2
            y = multfix(x,y);               // x*y
            y = multfix(float2fix(2),y);    // 2*x*y
            y = y + f.c0y;
            x = x2 - y2 + f.c0x;
            if(++max>=f.iter) { pixel(px+64,py+64,255); break; }
        } while ((x2 + y2 < float2fix(4)));
    }    
}

void CountDown(void) {
    uint8_t hour=0,minute=0,second=0;
    uint8_t lapl=2, lap=0;
    uint8_t start=0, clock=0, clear=1;
    uint8_t timeout=240;
    clrbit(WatchBits, goback);
    do {
        if(start) {
            if(SECPULSE() && clock==0) clock=1;
            else if(!SECPULSE() && clock==1) {
                clock=0;            
                second--;
                if(second==255) {
                    second=59;
                    minute--;
                    if(minute==255) {
                        minute=59;
                        if(hour) hour--;
                        else {  // Reached zero
                            Sound(NOTE_B7,NOTE_B7);   // Beep
                            second=0;
                            minute=0;
                        }                            
                    }
                }
            }
        } else {
            if(--timeout==0) setbit(WatchBits, goback);
        }
        if(clear) {
            clear=0;
            clr_display();
            lcd_goto(4,15); lcd_put5x8(PSTR("+H       +M       +S"));
        }
        lcd_goto(51,1); if(!start || SECPULSE()) putchar5x8(':'); else putchar5x8(' ');
        lcd_goto(81,1); if(!start || SECPULSE()) putchar5x8(':'); else putchar5x8(' ');
        printN_7seg(86, 0, second, 2);
        printN_7seg(56, 0, minute, 2);
        printN_7seg(13, 0, hour,   3);
        dma_display();
        if(testbit(Misc,userinput)) {
            clrbit(Misc, userinput);
            timeout=240;
            if(testbit(Buttons,KUR)) {          // START / STOP
                start = !start;
            }
            if(testbit(Buttons,K1)) hour++; 
            if(testbit(Buttons,K2)) minute++; if(minute>=60) minute=0;
            if(testbit(Buttons,K3)) second++; if(second>=60) second=0;
            if(testbit(Buttons,KBR)) {
                if(start) {    // LAP
                    lap++;
                    lapl++; if(lapl>=16) lapl=3;
                    lcd_goto(1,lapl); lcd_put5x8(PSTR("  Lap ")); printN5x8(lap); putchar5x8(':');
                    if(hour<100) putchar5x8(' ');
                    printN5x8(hour); putchar5x8(':');
                    printN5x8(minute); putchar5x8(':');
                    printN5x8(second);
                } else {            // Stopped -> clear counter
                    hour=0; minute=0; second=0;
                    lapl = 2; lap=0;
                    clear = 1;
                }
            }
            if(testbit(Buttons,KML)) setbit(WatchBits, goback);
        }
        WaitDisplay();
        SoundOff();
        SLP();          // Sleep
    } while(!testbit(WatchBits,goback));
    clrbit(WatchBits, goback);
    clr_display();
    setbit(Misc, redraw);
}

void Stopwatch(void) {
    uint8_t hour=0,minute=0,second=0,hundredth=0;
    uint8_t lapl=2, lap=0, clear=1;
    uint8_t timeout=240;
    PR.PRPC  &= 0b11111100;         // Enable TCC0 TCC1 clocks
    EVSYS.CH0MUX = 0b11000000;      // Event CH0 = TCC0 overflow
    TCC0.CNT = 0;
    TCC0.PER = 19999;               // 100Hz
    TCC1.CNT = 0;
    TCC1.PER = 5999;                // 1 minute
    clrbit(WatchBits, goback);
    do {
        if(clear) { clr_display(); clear=0; }
        lcd_goto(39,1); putchar5x8(':');
        lcd_goto(69,1); putchar5x8(':');
        lcd_goto(99,1); putchar5x8(':');
        printN_7seg(104, 0, hundredth, 2);
        printN_7seg(74,  0, second,    2);
        printN_7seg(44,  0, minute,    2);
        printN_7seg(1,   0, hour,      3);
        dma_display();
        if(testbit(Misc,userinput)) {
            clrbit(Misc, userinput);
            timeout=240;
            if(testbit(Buttons,KUR)) {          // START / STOP
                if(TCC1.CTRLA) {
                    TCC0.CTRLA = 0;
                    TCC1.CTRLA = 0;
                } else {
                    TCC0.CTRLA = 1;                 // 2MHz clock
                    TCC1.CTRLA = 0b00001000;        // Source is Event CH0 (100Hz)
                }                    
            }
            if(testbit(Buttons,KBR)) {
                if(TCC1.CTRLA) {    // LAP
                    lap++;
                    lapl++; if(lapl>=16) lapl=3;
                    lcd_goto(6,lapl); lcd_put5x8(PSTR("Lap ")); printN5x8(lap); putchar5x8(':');
                    if(hour<100) putchar5x8(' ');
                    printN5x8(hour); putchar5x8(':');
                    printN5x8(minute); putchar5x8(':');
                    printN5x8(second); putchar5x8(':');
                    printN5x8(hundredth);
                } else {            // Stopped -> clear counter
                    hour=0; minute=0; second=0; hundredth=0;
                    lapl=2; lap=0;
                    TCC0.CNT = 0;
                    TCC1.CNT = 0;
                    clear = 1;
                }         
            }
            if(testbit(Buttons,KML)) setbit(WatchBits, goback);
        }
        WaitDisplay(); 
        if(TCC1.CTRLA==0) {         // Not running
            if(--timeout==0) setbit(WatchBits, goback);
            SLP();                  // Sleep
        } else {                    // Running, update variables
            if(TCC1.INTFLAGS&0x01) {
                TCC1.INTFLAGS = 0xFF;
                minute++;
                if(minute>=60) {
                    minute=0;
                    hour++;
                }
            }
            second = 0;
            uint16_t temp = TCC1.CNT;
            while(temp>=100) { second++; temp-=100; }
            hundredth=lobyte(temp);
        }
    } while(!testbit(WatchBits,goback));
    clrbit(WatchBits, goback);
    TCC0.CTRLA = 0;
    TCC1.CTRLA = 0;
    PR.PRPC  |= 0b00000011;         // Disable TCC0 TCC1C clocks
    clr_display();
    setbit(Misc, redraw);
}

void Calendar(void) {
    time_var showdate;
    showdate.mday = NowDay;
    showdate.mon  = NowMonth;
    showdate.year = NowYear;
    showdate.wday = NowWeekDay;
    clrbit(WatchBits, goback);
    setbit(Misc, redraw);
    do {
        if(testbit(Misc, redraw)) {
            uint8_t wday, mdays;
            clrbit(Misc, redraw);
            clr_display();
            lcd_goto(28,0);
            lcd_put5x8(months[showdate.mon-1]); lcd_put5x8(PSTR(" 20")); printN5x8(showdate.year);
            lcd_goto(4,2); lcd_put5x8(PSTR("Su Mo Tu We Th Fr Sa"));
            for(uint8_t i=27; i<=123; i+=16) {
                lcd_hline(1,126,i,255);
            }
            for(uint8_t i=1; i<128; i+=18) {
                lcd_line(i,27,i,123);
            }
            wday=firstdayofmonth(&showdate);
            mdays=pgm_read_byte_near(monthDays+showdate.mon-1);
            for(uint8_t day=1,j=4,d=wday; day<=mdays; day++,d++) {
                if(d==7) { j+=2; d=0; } // Print next week line
                lcd_goto(d*18+5,j); printN5x8(day);
                if(day==NowDay && showdate.year==NowYear && showdate.mon==NowMonth) { // Highlight today
                    fillRectangle(d*18+2,j*8-4,d*18+18,j*8+10,1);
                }
                if(day==showdate.mday) {  // Highlight selected day
                    Rectangle(d*18+3,j*8-3,d*18+17,j*8+9,1);
                }
            }
            dma_display();
            WaitDisplay();
        }
        if(testbit(Misc,userinput)) {
            clrbit(Misc, userinput);
            setbit(Misc, redraw);
            if(testbit(Buttons, KBL)) {
                if(showdate.mday>1) showdate.mday--;
            }
            if(testbit(Buttons, KBR)) {
                if(showdate.mday<DaysInMonth(&showdate)) showdate.mday++;
            }
            if(testbit(Buttons,KUL)) {
                if(showdate.mon>1) showdate.mon--;
                else if(showdate.year) {
                    showdate.year--;
                    showdate.mon=12;
                }
            }
            if(testbit(Buttons,KUR)) {
                if(showdate.mon<12) showdate.mon++;
                else if(showdate.year<100) {
                    showdate.mon=1;
                    showdate.year++;
                }
            }
            if(testbit(Buttons,KML)) setbit(WatchBits, goback);
        }
        SLP();          // Sleep
    } while(!testbit(WatchBits, goback));
}

// Returns the column number for the calendar display, 2000-01-01 was Saturday
uint8_t firstdayofmonth(time_var *timeptr) {
    uint16_t days = (timeptr->year)*365;            // Add days for previous years
    for (uint8_t i=0; i<timeptr->year; i++) {       // Add extra days for leap years
        if (LEAP_YEAR(i)) days++;
    }
    for (uint8_t i=0; i<(timeptr->mon)-1; i++) {    // Add days for previous months in this year
        days += pgm_read_byte_near(monthDays+i);
        if (i==1 && LEAP_YEAR(timeptr->year)) days++;
    }
    return ((days+6)%7);                            // Add 6 so that Saturday is on 6th column
}

// Finds the correct day of the week for a given date. 2000-01-01 was Saturday
void findweekday(time_var *timeptr) {
    uint16_t days = (timeptr->year)*365;            // Add days for previous years
    for (uint8_t i=0; i<timeptr->year; i++) {       // Add extra days for leap years
        if (LEAP_YEAR(i)) days++;
    }
    for (uint8_t i=0; i<(timeptr->mon)-1; i++) {    // Add days for previous months in this year
        days += pgm_read_byte_near(monthDays+i);
        if (i==1 && LEAP_YEAR(timeptr->year)) days++;
    }
    days += timeptr->mday-1;                        // Add remaining days in this month
    timeptr->wday = (days%7);                       // Return week day. Saturday is 0
}

// Returns number of days on this month
uint8_t DaysInMonth(time_var *timeptr) {
    uint8_t daysinmonth;
    daysinmonth = pgm_read_byte_near(monthDays+timeptr->mon-1);
    if (timeptr->mon==2 && LEAP_YEAR(timeptr->year)) daysinmonth++;
    return daysinmonth;
}
