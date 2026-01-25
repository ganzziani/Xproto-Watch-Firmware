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

// TO DO: If there are no alarms in the next 24hrs, show the day of next alarm

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
#include "moon.h"

void DigitalFace(void);
void AnalogFace(void);
void ShowAlarmTime(uint8_t col, uint8_t row);
void Stopwatch(void);
void CountDown(void);
void EditAlarms(void);
void FindNextAlarm(void);
uint8_t firstdayofmonth(Type_Time *timeptr);
uint8_t DaysInMonth(Type_Time *timeptr);
void BigPrintMonth(void);
void BigPrintYear(void);
void ShowMoonIcon(uint8_t row, uint8_t col);
void PrintHands(uint8_t h, uint8_t m, uint8_t s) ;

const uint8_t monthDays[] PROGMEM = { 31,28,31,30,31,30,31,31,30,31,30,31 };		// Days in a month
volatile uint8_t MoonPhase;

Type_Time EEMEM EE_saved_time = {   // Last known time
    BUILD_SECOND,               // sec      Seconds      [0-59]
    BUILD_MINUTE,               // min      Minutes      [0-59]
    BUILD_HOUR,                 // hour     Hours        [0-23]
    BUILD_DAY,                  // day      Day          [1-31]
    BUILD_MONTH,                // month    Month        [1-12]
    BUILD_YEAR+56,              // year     Year since 1944
    0,                          // wday     Day of week  [0-6]   Saturday is 0
};

Type_Alarm EEMEM EE_Alarms[] = {
    {  8,  0, 0b01111111, 0 },  // 08:00am Everyday
    {  7, 45, 0b00111110, 1 },  // 07:45am Weekdays
    { 14, 30, 0b00111110, 4 },  // 02:00pm Weekdays
    { 21,  0, 0b00111111, 9 },  // 09:00pm Monday thru Saturday
};

uint8_t EEMEM EE_WSettings = 0; // 24Hr format, Date Format, Hour Beep, Alarm On,

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

// TCF0 overflows every 43200 seconds (12 hours)
// Toggle the T flag (used as AM/PM bit)
ISR(TCF0_OVF_vect, ISR_NAKED) {
	__asm__ volatile (
        "brts 1f" "\n\t"    // Branch if T is set
        "set"     "\n\t"    // Set T
        "reti"    "\n\t"
        "1:clt"   "\n\t"    // Clear T
        "reti"
	);    
}

// TCF0 Compare Interrupt (every minute), disabled during oscilloscope mode
ISR(TCF0_CCA_vect) {
    TCF0.CCA+=60;                           // Prepare next interrupt on next minute
    if(TCF0.CCA>43199) TCF0.CCA=59;
    NowSecond=0;
    NowMinute++;                            // Update minute
    if(BattLevel==0) {
        BattLevel = MeasureVin(0);          // Measure Vin if last voltage was high
       // SecTimeout = 90;                    // No need to worry about power, show seconds
    }        
    if(NowMinute>=60) {
        NowMinute=0;
        NowHour++;                          // Update hour, and hourly tasks:
        BattLevel = MeasureVin(0);          // Measure Vin
        FindNextAlarm();                    // Find next alarm
        if(NowHour>=24) {
            setbit(WatchBits, dateChanged);
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
        }            
        if(testbit(WSettings,hourbeep)) {   // On the hour beep
            Sound(TuneBeep);
        }
    }
    setbit(Misc, redraw);
}

// Sync variables from TCF0
void GetTimeTimer(void) {
    cli();                          // Disable global interrupts
    uint8_t m=0,h=0;
    if(testbit(SREG, CPU_T_bp)) h=12;
    uint16_t TotalSeconds = TCF0.CNT;
    while (TotalSeconds>=3600)	{ h++; TotalSeconds-=3600; }
    while (TotalSeconds>=60)	{ m++; TotalSeconds-=60; }
    NowMinute = m;
    NowHour = h;
    NowSecond = TotalSeconds;
    sei();                          // Enable global interrupts
}

// Validate time variables and update TCF0
void SetTimeTimer(void) {
    cli();                          // Disable global interrupts
    if(AlarmMinute>=60) AlarmMinute=0;
    if(AlarmHour>=24) AlarmHour=0;
    if(NowSecond==255) NowSecond=59;
    if(NowSecond>=60) NowSecond=0;
    if(NowMinute==255) {
        NowMinute=59;
        NowHour--;
        setbit(WatchBits, hourChanged);
    }
    if(NowMinute>=60) {
        NowMinute=0;
        NowHour++;
        setbit(WatchBits, hourChanged);
    }
    if(NowHour==255) NowHour=23;
    if(NowHour>=24) NowHour=0;
    if(NowMonth==0)  NowMonth=12;
    if(NowMonth>12) NowMonth=1;
    uint8_t d=DaysInMonth(NOW);
    if(NowDay==0) NowDay=d;
    if(NowDay>d) NowDay=1;
    if(NowYear>99+56) NowYear=0;
    uint8_t hour=NowHour;
    CLT();                          // Clear T bit (used for am/pm)
    if(hour>=12) {
        SET();                      // Set T bit
        hour-=12;
    }
    TCF0.CNT = (hour*3600)+(NowMinute*60)+NowSecond;
    findweekday();
    SetMinuteInterrupt();
    sei();                          // Enable global interrupts
}

// Set TCF0 Compare Interrupt to next minute
void SetMinuteInterrupt(void) {
     uint8_t h=NowHour;
     if(h>=12) h-=12;
     uint16_t count = (h*3600)+((NowMinute+1)*60)-1;    // Max count value is 43199
     TCF0.CCA = count;
     TCF0.INTFLAGS = 0xF0;   // Clear compare flag
     TCF0.INTCTRLB = 0x02;   // 1 minute interrupt, medium level
}

void Watch(void) {
    Buttons = 0;
    Menu = 0;   // Selected item to change
    WSettings = eeprom_read_byte(&EE_WSettings);
    BattLevel = MeasureVin(0);
    T.TIME.oldBattery = 0;
    SecTimeout = 60;
    GetTimeTimer();                     // Sync NOW variables from TCF0
    CPU_Fast();
    findweekday();
    FindNextAlarm();
    clr_display();
    clrbit(MStatus, goback);
    setbit(Misc, redraw);
    setbit(WatchBits, dateChanged);
    setbit(WatchBits, hourChanged);
    CPU_Slow();    
    do {                                    // Loop cycle time is 0.5 seconds
        // Check user input
        if(testbit(Misc, userinput)) {
            SecTimeout = 60;                    // Show seconds display for one minute
            clrbit(Misc, userinput);
            if(Menu==0) {                       // Currently not changing time
                if(testbit(Buttons,KML) && !testbit(MStatus, sound_on)) { // Exit watch if no sound active
                    setbit(MStatus, goback);
                }
                if(testbit(Buttons, K1)) Stopwatch();
                if(testbit(Buttons, K2)) CountDown();
                if(testbit(Buttons, K3)) EditAlarms();
                if(testbit(Buttons, KUR)) {
                    if(!testbit(WSettings, analog_face)) togglebit(WSettings, style);
                    clrbit(WSettings, analog_face);
                }
                if(testbit(Buttons, KBR)) {
                    if(testbit(WSettings, analog_face)) togglebit(WSettings, style);
                    setbit(WSettings, analog_face);
                }                        
                if(testbit(Buttons, KUL)) setbit(VPORT1.OUT, LEDWHITE); // Turn on backlight
                if(testbit(Buttons, KBL)) {
                    if(testbit(WatchBits,keyrep)) {     // Check if this is a long press
                        Menu=SET_SECOND;                // User can change the time now
                        clrbit(WatchBits,keyrep);       // Prevent repeat key
                        Buttons = 0;
                    }
                }
                clr_display();
                setbit(WatchBits, hourChanged);     // Re Print hour
                setbit(WatchBits, dateChanged);     // Re Print date
            } else {   // Changing time
                uint8_t OldMenu=Menu;
                if(testbit(Buttons,KML)) Menu=0;
                if(testbit(Buttons,KBL)) {
                    Menu++;
                    if(Menu>SET_YEAR) Menu=0;
                }
                if(OldMenu!=Menu) {
                    if(testbit(WSettings, style)) {
                        clr_display();  // Clear screen
                    }
                    FindNextAlarm();        // Update next alarm
                }
                if(Menu==0) {               // No longer adjusting the time
                    setbit(WatchBits, dateChanged); // Make sure to clear the cursor
                }                    
                cli();  // Prevent the RTC interrupt from changing the time in this block
                switch(Menu) {
                    case SET_SECOND:
                        if(testbit(Buttons,KUR)) NowSecond++;
                        if(testbit(Buttons,KBR)) NowSecond--;
                    break;
                    case SET_MINUTE:
                        if(testbit(Buttons,KUR)) NowMinute++;
                        if(testbit(Buttons,KBR)) NowMinute--;
                    break;
                    case SET_HOUR:
                        setbit(WatchBits, hourChanged);
                        if(testbit(Buttons,KUR)) NowHour++;
                        if(testbit(Buttons,KBR)) NowHour--;
                    break;
                    case SET_DAY:
                        setbit(WatchBits, dateChanged);
                        if(testbit(Buttons,KUR)) NowDay++;
                        if(testbit(Buttons,KBR)) NowDay--;
                    break;
                    case SET_MONTH:
                        setbit(WatchBits, dateChanged);
                        if(testbit(Buttons,KUR)) NowMonth++;
                        if(testbit(Buttons,KBR)) NowMonth--;
                    break;
                    case SET_YEAR:
                        setbit(WatchBits, dateChanged);
                        if(testbit(Buttons,KUR)) NowYear++;
                        if(testbit(Buttons,KBR)) NowYear--;
                    break;
                }
                SetTimeTimer();     // Validate time variables, update TCF0, enable interrupts
            }
            if(testbit(MStatus, sound_on)) {  // User cancelled the alarm, find next alarm
                FindNextAlarm();
                SoundOff();
                clrbit(MStatus, sound_on);    // Turn off alarm sound
            }
            setbit(Misc, redraw);               // Redraw screen
        }
        // When to refresh the screen? ->
        if(Menu ||                          // When user is changing the time
            testbit(Misc, redraw) ||        // Update requested (at least every minute)
            (SECPULSE() && SecTimeout) ||   // Every second when displaying seconds
            testbit(MStatus, sound_on)) {   // The alarm is active (to toggle bell)
            clrbit(Misc, redraw);
            if(SecTimeout) {
                SecTimeout--;
                if(SecTimeout==0) {                     // Timeout expired
                    Menu=0;                             // No longer adjusting time
                    setbit(Misc, redraw);               // Clears the seconds display
                    setbit(WatchBits, hourChanged);     // Re Print hour
                    setbit(WatchBits, dateChanged);     // Re Print date
                }
            }                
            if(AlarmHour==NowHour && AlarmMinute==NowMinute && testbit(MStatus, alarm_on)) {
                if(NowSecond<=1) setbit(MStatus, sound_on);   // Enable Alarm until user cancels
                if(testbit(MStatus, sound_on) && TCD1.CTRLA==0) { // Sound on and no tune active
                    Sound((uint8_t *)pgm_read_word(TuneAlarms+T.TIME.AlarmTune));
                }
                SecTimeout = 60;    // Show seconds during alarm
            } else {
                if(testbit(MStatus, sound_on)) {
                    FindNextAlarm();                // An alarm just finished, find next alarm
                    clrbit(MStatus, sound_on);
                }
            }                
            GetTimeTimer();                 // Update time variables before showing data on screen
            if(testbit(WSettings, analog_face)) AnalogFace();
            else DigitalFace();
            dma_display();
            // Available CPU time while waiting to finish the DMA
            // Use this time to do some work
            MoonPhase = CalculateMoonPhase(NOW);  // Phase: [0, 236]
            WaitDisplay();                  // Finish transmission
        }
        if(testbit(Misc,keyrep)) {  // Repeat key or long press
            setbit(Misc, userinput);
        }
        // Go to Sleep if:
        if( TCD1.CTRLA==0 &&            // Not playing sounds
            TCD0.CTRLA==0 &&            // Not checking for Auto Repeat Key
            !testbit(Misc, redraw) &&   // Don't need to update the screen
            !testbit(Misc,userinput)    // No user input
            ) {
            SLEEP.CTRL = SLEEP_SMODE_PSAVE_gc | SLEEP_SEN_bm;
            SLP();
        } else if(TCD1.CTRLA) {         // Playing sounds -> go to idle to keep peripherals running
            SLEEP.CTRL = SLEEP_SMODE_IDLE_gc | SLEEP_SEN_bm;
            SLP();
        }
    } while(!testbit(MStatus, goback));
    // Going to Main menu...
    eeprom_write_byte(&EE_WSettings, WSettings);    // Save Watch settings
    Menu=1;                                         // Set Main Menu: Watch
    SecTimeout = 120;                               // Timeout before going to Watch mode
}

// Digital Watch Face
void DigitalFace(void) {
    if(testbit(WSettings, style) && Menu==0) {      // Minimalistic style -> Only print hours and minutes
        if(testbit(WatchBits, hourChanged)) {
            clrbit(WatchBits, hourChanged);
            uint8_t H=NowHour;
            if(!testbit(WSettings, time24)) {
                if(H>=12) H-=12;
                if(H==0) H=12;
            }
            if(H>=20) { H-=20; bitmap(0,5,Digit2_22x48); }
            else if(H>=10) { H-=10; bitmap(0,5,Digit1_22x48); }
            else  clearRectangle(0,5,22,6);
            bitmap(25,5,(const uint8_t *)pgm_read_word(Digits_22x48+H));
            bitmap(49,7,Dots_4x16);     // Time semicolon
        }
        uint8_t Ones=NowMinute, Tens=0;
        while (Ones>=10) { Tens++; Ones-=10; }
        bitmap(55,5,(const uint8_t *)pgm_read_word(Digits_22x48+Tens));
        bitmap(80,5,(const uint8_t *)pgm_read_word(Digits_22x48+Ones));
    } else {
        if(testbit(WatchBits, hourChanged) || Menu) {       // Update these items every hour
            clrbit(WatchBits, hourChanged);
            if(testbit(WSettings, time24)) {                // Print AM/PM bitmap
                clearRectangle(105,9,16,1);
            }            
            else {
                if(testbit(SREG, CPU_T_bp)) bitmap(105,9,PM);
                else bitmap(105,9,AM);
            }
            uint8_t H=NowHour;
            if(!testbit(WSettings, time24)) {
                if(H>=12) H-=12;
                if(H==0) H=12;
            }
            if(H>=20) { H-=20; bitmap(0,9,Digit2_22x48); }
            else if(H>=10) { H-=10; bitmap(0,9,Digit1_22x48); }
            else  clearRectangle(0,9,22,6);
            bitmap(25,9,(const uint8_t *)pgm_read_word(Digits_22x48+H));
            if(SECPULSE() && Menu==SET_HOUR) {    // Flash when changing
                fillRectangle(0,115,21,119,PIXEL_TGL);
                fillRectangle(25,115,46,119,PIXEL_TGL);
            }
			if(BattLevel) {
                if(testbit(WatchBits, dateChanged) || T.TIME.oldBattery != BattLevel) {
			        bitmap(2,0,BattIcon);
                    uint8_t battery_blocks = BattLevel-1;
                    uint8_t x=4, y=5;
                    while(battery_blocks>0) {
                        set_pixel(x,y--);
                        if(y<2) {
                            y=5; x++;
                        }
                        battery_blocks--;
                    }
                    T.TIME.oldBattery = BattLevel;
                }                    
			}
			else bitmap(2,0,BattPwrIcon); // If BattLevel is zero, Vin is  above 4.2V -> the device is charging
			ShowAlarmTime(74, 0);
			lcd_goto(118,0);
			if(testbit(MStatus,alarm_on) &&											// Show bell character
			(NowSecond&0x01 || !testbit(MStatus, sound_on))) putchar5x8(CHAR_BELL);   // Blink character at alarm time
			else putchar5x8(' ');
        }            
        {
            uint8_t Ones=NowMinute, Tens=0;
            while (Ones>=10) { Tens++; Ones-=10; }
            bitmap(55,9,(const uint8_t *)pgm_read_word(Digits_22x48+Tens));
            bitmap(80,9,(const uint8_t *)pgm_read_word(Digits_22x48+Ones));
        }        
        if(SECPULSE() && Menu==SET_MINUTE) {     // Flash when changing
            fillRectangle(55,115,76,119,PIXEL_TGL);
            fillRectangle(80,115,101,119,PIXEL_TGL);
        }
        if(SecTimeout) {                                // Display seconds before timeout
            printN11x21(104, 12, NowSecond, 2);
            if(SECPULSE() && Menu==SET_SECOND) {        // Flash when changing
                fillRectangle(104,114,114,116,PIXEL_TGL);
                fillRectangle(117,114,127,116,PIXEL_TGL);
            }
        } else {    // Not displaying seconds, erase area
            clearRectangle(104,12,24,3);
        }
        if(testbit(WatchBits, dateChanged) || Menu>=SET_DAY) {  // Update these items every day
            clrbit(WatchBits, dateChanged);
			bitmap(49,11,Dots_4x16);    // Time semicolon
            lcd_goto(40,2);
            print5x8(STRS_Days[NowWeekDay]);
            u8CursorX = 1;
            if(!testbit(WSettings, PostYear)) {
                BigPrintYear();
                bitmap(u8CursorX,5,Dash_6x8);      // Date dash
                u8CursorX += 8;
            }
            if(!testbit(WSettings, PostMonth)) {
                BigPrintMonth();
                bitmap(u8CursorX,5,Dash_6x8);      // Date dash
                u8CursorX += 8;
            }
            {
                uint8_t Ones=NowDay, Tens=0; // Day.         [1-31]
                while (Ones>=10) { Tens++; Ones-=10; }
                bitmap(u8CursorX,4,(const uint8_t *)pgm_read_word(Digits_12x24+Tens));
                bitmap(u8CursorX+14,4,(const uint8_t *)pgm_read_word(Digits_12x24+Ones));
            }        
            if(SECPULSE() && Menu==SET_DAY) {  // Flash when changing
                fillRectangle(u8CursorX,53,u8CursorX+11,55,PIXEL_TGL);
                fillRectangle(u8CursorX+14,53,u8CursorX+25,55,PIXEL_TGL);
            }
            u8CursorX += 29;
            if(testbit(WSettings, PostMonth)) {
                bitmap(u8CursorX,5,Dash_6x8);      // Date dash
                u8CursorX += 8;
                BigPrintMonth();
            }
            if(testbit(WSettings, PostYear)) {
                bitmap(u8CursorX,5,Dash_6x8);      // Date dash
                u8CursorX += 8;
                BigPrintYear();
            }
            if(testbit(WSettings, ShowMoon)) {
                ShowMoonIcon(17, 2);
            }                
        }
    }
}

void ShowMoonIcon(uint8_t row, uint8_t col) {
    uint8_t index, Phase;
    Phase = MoonPhase;
    if(Phase < 7) index = 0;            // New Moon: 0-6 (0-3%)
    else if(Phase < 52) index = 8;      // Waxing Crescent: 7-51 (3-22%)
    else if(Phase < 66) index = 16;      // First Quarter: 52-65 (22-28%)
    else if(Phase < 111) index = 24;     // Waxing Gibbous: 66-110 (28-47%)
    else if(Phase < 125) index = 32;     // Full Moon: 111-124 (47-53%)
    else if(Phase < 170) index = 40;     // Waning Gibbous: 125-169 (53-72%)
    else if(Phase < 184) index = 48;     // Last Quarter: 170-183 (72-78%)
    else if(Phase < 229) index = 56;     // Waning Crescent: 184-228 (78-97%)
    else index = 0;                     // Back to New Moon: 229-236
    lcd_goto(row, col);
    putData(&MoonPhaseBMP[index], 8);
}

void BigPrintMonth(void) {
    uint8_t Ones=NowMonth;    // Month.       [1-12]
    uint8_t x = u8CursorX;
    uint8_t pos = x+6;
    if(Ones>=10) {
        Ones-=10; pos+=6;
        bitmap(x,4,Digit1_8x24);
        clearRectangle(x+8,4,4,3);  // Clean leftover from single digit month
    }
    else clearRectangle(x,4,24,3);  // Clean leftover from double digits
    bitmap(pos,4,(const uint8_t *)pgm_read_word(Digits_12x24+Ones));
    if(SECPULSE() && Menu==SET_MONTH) {  // Flash when changing
        fillRectangle(u8CursorX,53,u8CursorX+23,55,PIXEL_TGL);
    }
    u8CursorX += 27;
}

void BigPrintYear(void) {
    uint8_t Ones=NowYear, Tens=0, Hund=0, Thou=2; // Year
    uint8_t x = u8CursorX;
    if(NowYear<(uint8_t)(2000-EPOCH_YEAR)) {
        Thou=1; Hund=9; Ones+=44;
    } else Ones-=56;
    while (Ones>=10) { Tens++; Ones-=10; }
    bitmap(x   ,4,(const uint8_t *)pgm_read_word(Digits_12x24+Thou));
    bitmap(x+14,4,(const uint8_t *)pgm_read_word(Digits_12x24+Hund));
    bitmap(x+28,4,(const uint8_t *)pgm_read_word(Digits_12x24+Tens));
    bitmap(x+42,4,(const uint8_t *)pgm_read_word(Digits_12x24+Ones));
    if(SECPULSE() && Menu==SET_YEAR) {
        fillRectangle(x   ,53,x+11,55,PIXEL_TGL);
        fillRectangle(x+14,53,x+25,55,PIXEL_TGL);
        fillRectangle(x+28,53,x+39,55,PIXEL_TGL);
        fillRectangle(x+42,53,x+53,55,PIXEL_TGL);
    }
    u8CursorX += 57;
}    

// Analog Watch Face
void AnalogFace(void) {
    if(testbit(WatchBits, dateChanged)) {       // Update these items every day
        clr_display();
        clrbit(WatchBits, dateChanged);
        if(!testbit(WSettings, style)) {
            lcd_goto( 58,  1); printN_5x8(12);  // Hour numbers around the circle
            lcd_goto( 82,  2); printN_5x8(1);
            lcd_goto(100,  5); printN_5x8(2);
            lcd_goto(108,  8); printN_5x8(3);
            lcd_goto(100, 11); printN_5x8(4);
            lcd_goto( 80, 13); printN_5x8(5);
            lcd_goto( 55, 14); printN_5x8(6);
            lcd_goto( 31, 13); printN_5x8(7);
            lcd_goto( 11, 11); printN_5x8(8);
            lcd_goto(  3,  8); printN_5x8(9);
            lcd_goto( 14,  5); printN_5x8(10);
            lcd_goto( 30,  2); printN_5x8(11);
            for(uint8_t i=0; i<60; i++) {       // Circumference markers
                lcd_line(63+Sine60(i,60),63-Cosine60(i,60),
                63+Sine60(i,63),63-Cosine60(i,63));
            }
            ShowAlarmTime(42, 10);
            // Date
            lcd_goto(52, 4);
            if(testbit(WSettings, PostMonth)) {
                printN3x6(NowDay);
                putchar3x6(' ');
            }
            print3x6(STRS_Months_short[NowMonth-1]);
            if(!testbit(WSettings, PostMonth)) {
                putchar3x6(' ');
                printN3x6(NowDay);
            }
            if(testbit(WSettings, ShowMoon)) {
                ShowMoonIcon(60, 3);
            }
        }            
    } else {    // Erase previous
        PrintHands(T.TIME.oldHour, T.TIME.oldMinute, T.TIME.oldSecond);
    }
    uint8_t s,m,h;
    s = T.TIME.oldSecond = NowSecond;
    m = T.TIME.oldMinute = NowMinute;
    h = T.TIME.oldHour   = NowHour;
    if(!testbit(WSettings, style)) {
        lcd_goto(60,11);
        if(testbit(MStatus,alarm_on) &&                                           // Show bell character
        (NowSecond&0x01 || !testbit(MStatus, sound_on))) putchar5x8(CHAR_BELL);   // Blink character at alarm time
        else putchar5x8(' ');
    }
    PrintHands(h, m, s);
}

void PrintHands(uint8_t h, uint8_t m, uint8_t s) {
    if(h>=12) h-=12;
    h=h*5+m/12; // Add minutes/12 to hour needle (5 transitions per hour)
    // Hours
    fillTriangle(63+Sine60(h-5+60, 8), 63-Cosine60(h-5+60, 8),    // Add 60 to keep angle positive
    63+Sine60(h+5,    8), 63-Cosine60(h+5,    8),
    63+Sine60(h,     36), 63-Cosine60(h,     36), PIXEL_TGL);
    // Minutes
    fillTriangle(63+Sine60(m-4+60, 8), 63-Cosine60(m-4+60, 8),
    63+Sine60(m+4,    8), 63-Cosine60(m+4,    8),
    63+Sine60(m,     50), 63-Cosine60(m,     50), PIXEL_TGL);    
    // Seconds
    if(!testbit(WSettings, style) && SecTimeout) {    
        lcd_line_c(63,63,63+Sine60(s,54),63-Cosine60(s,54), PIXEL_TGL);
    }
}

void ShowAlarmTime(uint8_t col, uint8_t row) {
    lcd_goto(col, row);
    uint8_t hour = AlarmHour;
    if(testbit(MStatus,alarm_on)) {
        uint8_t n=AlarmHour;
        if(n>=12) n-=12;
        if(n==0) n=12;
        if(!testbit(WSettings, time24)) printN_5x8(n);
        else printN_5x8(hour);
        putchar5x8(':');
        printN5x8(AlarmMinute);
        if(!testbit(WSettings, time24)) {
            if(hour>=12) print5x8(STR_pm);
            else print5x8(STR_am);
        } else print5x8(PSTR("  "));
    } else print5x8(PSTR("       "));
}    

void CountDown(void) {
    uint8_t hour=0,minute=0,second=0;
    uint8_t lapl=2, lap=0;
    uint8_t start=0, clock=0, clear=1;
    uint8_t timeout=240;
    clrbit(MStatus, goback);
    do {
        if(testbit(Misc,userinput)) {
            clrbit(Misc, userinput);
            timeout=240;
            if(testbit(Buttons,KUR)) {          // START / STOP
                start = !start;
            }
            if(testbit(Buttons,K1)) hour++;
            if(testbit(Buttons,K2)) minute++;
            if(minute>=60) minute=0;
            if(testbit(Buttons,K3)) second++;
            if(second>=60) second=0;
            if(testbit(Buttons,KBR)) {
                if(start) {    // LAP
                    lap++;
                    lapl++; if(lapl>=16) lapl=3;
                    lcd_goto(1,lapl); print5x8(PSTR("  Lap ")); printN5x8(lap); putchar5x8(':');
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
            if(testbit(Buttons,KML)) setbit(MStatus, goback);
        }
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
                            Sound(TuneAlarm0);   // Beep
                            second=0;
                            minute=0;
                        }                            
                    }
                }
            }
        } else {
            if(--timeout==0) setbit(MStatus, goback);
        }
        if(clear) {
            clear=0;
            clr_display();
            lcd_goto(4,15); print5x8(PSTR("+H       +M       +S"));
        }
        lcd_goto(51,1); if(!start || SECPULSE()) putchar5x8(':'); else putchar5x8(' ');
        lcd_goto(81,1); if(!start || SECPULSE()) putchar5x8(':'); else putchar5x8(' ');
        printN11x21(86, 0, second, 2);
        printN11x21(56, 0, minute, 2);
        printN11x21(13, 0, hour,   3);
        dma_display();
        WaitDisplay();                  // Double buffering not needed in countdown (slow refresh rate)
        if(TCD1.CTRLA==0) SLP();        // Sleep
    } while(!testbit(MStatus, goback));
    clrbit(MStatus, goback);
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
    clrbit(MStatus, goback);
    do {
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
                    lcd_goto(6,lapl); print5x8(PSTR("Lap ")); printN5x8(lap); putchar5x8(':');
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
            if(testbit(Buttons,KML)) setbit(MStatus, goback);
        }
        if(clear) { clr_display(); clear=0; }
        lcd_goto(39,1); putchar5x8(':');
        lcd_goto(69,1); putchar5x8(':');
        lcd_goto(99,1); putchar5x8(':');
        printN11x21(104, 0, hundredth, 2);
        printN11x21(74,  0, second,    2);
        printN11x21(44,  0, minute,    2);
        printN11x21(1,   0, hour,      3);
        WaitDisplay();
        dma_display();
        if(TCC1.CTRLA==0) {         // Not running
            if(--timeout==0) setbit(MStatus, goback);
            WaitDisplay();
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
    } while(!testbit(MStatus, goback));
    clrbit(MStatus, goback);
    TCC0.CTRLA = 0;
    TCC1.CTRLA = 0;
    PR.PRPC  |= 0b00000011;         // Disable TCC0 TCC1 clocks
}

void EditAlarms(void) {
    Type_Alarm Alarms[4];
    eeprom_read_block(&Alarms, &EE_Alarms, 4*sizeof(Type_Alarm));
    uint8_t oldWSettings = MStatus;
    clrbit(MStatus, goback);
    setbit(Misc, redraw);
    uint8_t selectx=0, selecty=0;
    do {
        if(testbit(Misc,userinput)) {
            clrbit(Misc, userinput);
            setbit(Misc, redraw);
            SecTimeout = 60;
            SoundOff();
            if(testbit(Buttons, KUL)) setbit(VPORT1.OUT, LEDWHITE); // Turn on backlight
            if(testbit(Buttons, KUR) || testbit(Buttons, KBR)) {
                if(selectx<8) {     // Toggle bits in the active field
                    uint8_t bit = 0x80>>selectx;
                    Alarms[selecty].active ^= bit;
                }
            }
            if(testbit(Buttons, KUR)) {
                if(selectx==8) {    // Hour selection
                    Alarms[selecty].hour++;
                }
                if(selectx==9) {    // Minute selection
                    Alarms[selecty].minute++;
                }
                if(selectx==10) {    // Tune selection
                    Alarms[selecty].tune++;
                }
            }                
            if(testbit(Buttons, KBR)) {
                if(selectx==8) {    // Hour selection
                    Alarms[selecty].hour--;
                }
                if(selectx==9) {    // Minute selection
                    Alarms[selecty].minute--;
                }
                if(selectx==10) {    // Tune selection
                    Alarms[selecty].tune--;
                }
            }
            if(Alarms[selecty].hour>=24) Alarms[selecty].hour = 0;
            if(Alarms[selecty].minute>=60) Alarms[selecty].minute = 0;
            if(Alarms[selecty].tune>=10) Alarms[selecty].tune = 0;
            if(testbit(Buttons,K1)) {
                selectx--;
                if(selectx>=11) selectx = 10;
            }
            if(testbit(Buttons,K3)) {
                selectx++;
                if(selectx>=11) selectx=0;
            }
            if(testbit(Buttons,KUL)) {
                selecty--;
                if(selecty>=4) selecty=3;
            }
            if(testbit(Buttons,K2) || testbit(Buttons,KBL)) {
                selecty++;
                if(selecty>=4) selecty=0;
            }
            if(testbit(Buttons,KML)) setbit(MStatus, goback);
        }
        if(testbit(Misc, redraw)) {
            clrbit(Misc, redraw);
            clr_display();
            lcd_goto(4,15); print5x8(PSTR("<-      Next      ->"));
            for(uint8_t i=0, j=0; i<4; i++, j+=4) {    // Print 4 alarms
                lcd_goto(4,j); print5x8(PSTR("Alarm ")); putchar5x8('1'+i); putchar5x8(':');
                if(Alarms[i].active & 0x80) fillRectangle(2, i*32, 45,6+(i)*32,PIXEL_TGL);
                setbit(MStatus, alarm_on);
                AlarmHour = Alarms[i].hour;
                AlarmMinute = Alarms[i].minute;
                ShowAlarmTime(57, j);
                lcd_goto(112,j);
                putchar5x8(CHAR_BELL);
                putchar5x8(0x30+Alarms[i].tune);
                // Weekdays
                lcd_goto(4,j+1); print5x8(STR_Weekdays);
                for(uint8_t bit=0x40, k=0; bit; bit=bit>>1, k++) {
                    if(Alarms[i].active & bit) {
                        fillRectangle(2+k*18, (j+1)*8-1,16+k*18,(j+2)*8-1,PIXEL_TGL);
                    }
                }
            }
        }
        if(selectx==0) {                        // Alarm number selection
            Rectangle(2, selecty*32, 45,6+(selecty)*32,PIXEL_TGL);
        } else if(selectx<=7) {                 // Weekday selection
            uint8_t x = selectx-1;
            Rectangle(2+x*18, 7+selecty*32,16+x*18,15+selecty*32,PIXEL_TGL);
        } else if(selectx==8) {                 // Hour selection
            Rectangle(56, selecty*32, 69,6+(selecty)*32,PIXEL_TGL);
        } else if(selectx==9) {                 // Minute selection
            Rectangle(73, selecty*32, 87,6+(selecty)*32,PIXEL_TGL);
        } else {                                // Tune selection
            Rectangle(110, selecty*32, 124,6+(selecty)*32,PIXEL_TGL);
            if(TCD1.CTRLA==0) {                 // No tune active
                Sound((uint8_t *)pgm_read_word(TuneAlarms+Alarms[selecty].tune));
            }
        }
        dma_display();
        WaitDisplay();
        if(testbit(Misc,keyrep)) {  // Repeat key or long press
            setbit(Misc, userinput);
        }
        // Go to Sleep if:
        if( TCD1.CTRLA==0 &&            // Not playing sounds
            TCD0.CTRLA==0 &&            // Not checking for Auto Repeat Key
            !testbit(Misc, redraw) &&   // Don't need to update the screen
            !testbit(Misc,userinput)    // No user input
            ) {
            SecTimeout--;
            SLEEP.CTRL = SLEEP_SMODE_PSAVE_gc | SLEEP_SEN_bm;
            SLP();
        } else if(TCD1.CTRLA) {         // Playing sounds -> go to idle to keep peripherals running
            SLEEP.CTRL = SLEEP_SMODE_IDLE_gc | SLEEP_SEN_bm;
            SLP();
        }        
        
    } while(!testbit(MStatus, goback) && SecTimeout);
    eeprom_write_block(&Alarms, &EE_Alarms, 4*sizeof(Type_Alarm));
    MStatus = oldWSettings;
    SoundOff();
    FindNextAlarm();
    clrbit(MStatus, goback);
    SecTimeout = 60;
}

// Finds the next alarm within 24 hours
void FindNextAlarm(void) {
    Type_Alarm Alarms[4];
    eeprom_read_block(&Alarms, &EE_Alarms, 4*sizeof(Type_Alarm));
    uint8_t Tomorrow = NowWeekDay+1;
    if(Tomorrow>=7) Tomorrow=0;
    uint8_t Todaybit=0x40;
    Todaybit = Todaybit >> NowWeekDay; 
    uint8_t Tomorrowbit=0x40;
    Tomorrowbit = Tomorrowbit >> Tomorrow;
    clrbit(MStatus, alarm_on);
	setbit(WatchBits, hourChanged);
    for(uint8_t i=0; i<4; i++) {        // Search the 4 alarms
        if(Alarms[i].active & 0x80) {   // This alarm is active
            uint8_t NewAlarmHour   = Alarms[i].hour;
            uint8_t NewAlarmMinute = Alarms[i].minute;
            // Check if there is an alarm for tomorrow
            if(Alarms[i].active & Tomorrowbit) {
                // Compare only if this alarm occurs before NOW (it is within 24 hours)
                if(((NowHour ==NewAlarmHour) && (NowMinute>NewAlarmMinute)) ||
                    (NowHour > NewAlarmHour)) {
                    if(!testbit(MStatus, alarm_on)) {     // Not found the next alarm yet
                        setbit(MStatus, alarm_on);        // Just copy new alarm
                        AlarmHour   = NewAlarmHour;
                        AlarmMinute = NewAlarmMinute;
                        T.TIME.AlarmTune = Alarms[i].tune;
                    } else {                                // There was a previous alarm, copy if new alarm occurs sooner
                        if(((NowHour ==AlarmHour) && (NowMinute>AlarmMinute)) ||    // Current alarm will occur tomorrow
                        (NowHour > AlarmHour)) {                    
                            if(((AlarmHour ==NewAlarmHour) && (AlarmMinute>NewAlarmMinute)) ||  // Compare and use soonest alarm
                            (AlarmHour > NewAlarmHour)) {
                                AlarmHour   = NewAlarmHour;
                                AlarmMinute = NewAlarmMinute;
                                T.TIME.AlarmTune = Alarms[i].tune;
                            }                                                       // Keep existing alarm if it occurs today
                        }                        
                    }
                }
            }
            // Check if there is an alarm for today
            if(Alarms[i].active & Todaybit) {
                // Compare only if this alarm occurs now or after NOW
                if(((NowHour ==NewAlarmHour) && (NowMinute<=NewAlarmMinute)) ||
                    (NowHour < NewAlarmHour)) {
                    if(!testbit(MStatus, alarm_on)) {     // Not found the next alarm yet
                        setbit(MStatus, alarm_on);        // Just copy new alarm
                        AlarmHour   = NewAlarmHour;
                        AlarmMinute = NewAlarmMinute;
                        T.TIME.AlarmTune = Alarms[i].tune;
                    } else {                                // There was a previous alarm, copy if new alarm occurs sooner
                        if(((NowHour ==AlarmHour) && (NowMinute>AlarmMinute)) ||    // Current alarm will occur tomorrow, copy this new alarm
                        (NowHour > AlarmHour)) {
                            AlarmHour   = NewAlarmHour;
                            AlarmMinute = NewAlarmMinute;
                            T.TIME.AlarmTune = Alarms[i].tune;
                        } else {                                                    // Otherwise, compare and use soonest alarm
                            if(((AlarmHour ==NewAlarmHour) && (AlarmMinute>NewAlarmMinute)) ||
                                (AlarmHour > NewAlarmHour)) {
                                AlarmHour   = NewAlarmHour;
                                AlarmMinute = NewAlarmMinute;
                                T.TIME.AlarmTune = Alarms[i].tune;
                            }
                        }
                    }
                }
            }                
        }
    }    
}

void Calendar(void) {
    Type_Time showdate;
    showdate.day = NowDay;
    showdate.month = NowMonth;
    showdate.year = NowYear;
    showdate.wday = NowWeekDay;
    clrbit(MStatus, goback);
    setbit(Misc, redraw);
    do {
        if(testbit(Misc, redraw)) {
            uint8_t wday, mdays;
            clrbit(Misc, redraw);
            clr_display();
            lcd_goto(28,0);
            print5x8(STRS_Months[showdate.month-1]);
            if(showdate.year<56) {
                print5x8(PSTR(" 19")); printN5x8(44+showdate.year);
            }
            else {
                print5x8(PSTR(" 20")); printN5x8(showdate.year-56);
            }                
            lcd_goto(4,2); print5x8(STR_Weekdays);
            for(uint8_t i=27; i<=123; i+=16) {
                lcd_hline(1,126,i,PIXEL_SET);
            }
            for(uint8_t i=1; i<128; i+=18) {
                lcd_line(i,27,i,123);
            }
            wday=firstdayofmonth(&showdate);
            mdays=pgm_read_byte_near(monthDays+showdate.month-1);
            for(uint8_t day=1,j=4,d=wday; day<=mdays; day++,d++) {
                if(d==7) { j+=2; d=0; } // Print next week line
                lcd_goto(d*18+5,j); printN5x8(day);
                if(day==NowDay && showdate.year==NowYear && showdate.month==NowMonth) { // Highlight today
                    fillRectangle(d*18+2,j*8-4,d*18+18,j*8+10,PIXEL_TGL);
                }
                if(day==showdate.day) {  // Highlight selected day
                    Rectangle(d*18+3,j*8-3,d*18+17,j*8+9,PIXEL_TGL);
                }
            }
            dma_display();
            WaitDisplay();
        }
        if(testbit(Misc,userinput)) {
            clrbit(Misc, userinput);
            setbit(Misc, redraw);
            if(testbit(Buttons, KUL)) {
                if(showdate.year<156) showdate.year++;
                setbit(VPORT1.OUT, LEDWHITE); // Turn on backlight
            }                
            if(testbit(Buttons,KBL)) {
                if(showdate.year>0) showdate.year--;
            }
            if(testbit(Buttons, K1)) {
                if(showdate.day>1) showdate.day--;
            }
            if(testbit(Buttons, K3)) {
                if(showdate.day<DaysInMonth(&showdate)) showdate.day++;
            }
            if(testbit(Buttons, K2)) {
                showdate.day = NowDay;
                showdate.month = NowMonth;
                showdate.year = NowYear;
                showdate.wday = NowWeekDay;
            }
            if(testbit(Buttons,KBR)) {
                if(showdate.month>1) showdate.month--;
                else if(showdate.year) {
                    showdate.year--;
                    showdate.month=12;
                }
            }
            if(testbit(Buttons,KUR)) {
                if(showdate.month<12) showdate.month++;
                else if(showdate.year<156) {
                    showdate.month=1;
                    showdate.year++;
                }
            }
            if(testbit(Buttons,KML)) setbit(MStatus, goback);
        }
        SLP();          // Sleep
    } while(!testbit(MStatus, goback));
}

// Returns the column number for the calendar display, 1944-01-01 was Saturday
uint8_t firstdayofmonth(Type_Time *timeptr) {
    uint8_t days = (timeptr->year);                 // The first weekday of the year increases on each year
    for (uint8_t y=0; y<timeptr->year; y++) {       // On leap years, it increases by 2
	    if (LEAP_YEAR(y)) days++;
    }
    for (uint8_t m=0; m<(timeptr->month)-1; m++) {    // Advance days for previous months in this year
	    days += pgm_read_byte_near(monthDays+m)-28;
	    if (m==1 && LEAP_YEAR(timeptr->year)) days++;
    }
	days += 6;                                      // Add 6 so that Saturday is on 6th column
	while(days>=7) days-=7;                         // Calculate modulo to return the weekday
    return days;                            
}

// Finds the correct day of the week for the current date. 1944-01-01 was Saturday (Sunday is 0)
void findweekday(void) {
    uint8_t days = NowYear;                         // The first weekday of the year increases on each year
    for (uint8_t y=0; y<NowYear; y++) {             // On leap years, it increases by 2
        if (LEAP_YEAR(y)) days++;
    }
    for (uint8_t m=0; m<NowMonth-1; m++) {          // Advance days for previous months in this year
        days += pgm_read_byte_near(monthDays+m)-28;
        if (m==1 && LEAP_YEAR(NowYear)) days++;
    }
    days += NowDay;                                 // Add remaining days in this month
    days -=2;                                       // Subtract 2 so Sunday is 0
	while(days>=7) days-=7;                         // Calculate modulo to return the weekday
    NowWeekDay = days;                              // Return week day. Saturday is 0
}

// Returns the number of days in the month in the date pointed by timeptr
uint8_t DaysInMonth(Type_Time *timeptr) {
    uint8_t daysinmonth;
    daysinmonth = pgm_read_byte_near(monthDays+timeptr->month-1);
    if (timeptr->month==2 && LEAP_YEAR(timeptr->year)) daysinmonth++;
    return daysinmonth;
}

// Absolute days away from Now date
uint16_t DaysAwayfromToday(Type_Time *timeptr) {
    uint16_t days=0;
    int8_t delta;
    uint8_t y1 = NowYear;
    uint8_t y2 = timeptr->year;
    if(y2>=y1) delta = 1; else delta = -1;
    while(y1!=y2) {     // Count days for every year
        days += 365;
        if (LEAP_YEAR(y1)) days++;
        y1+=delta;
    }     
    uint8_t m1 = NowMonth-1;
    uint8_t m2 = timeptr->month-1;
    if(m2>=m1) delta = 1; else delta = -1;
    while(m1!=m2) {     // Count days for every month
        days += pgm_read_byte_near(monthDays+m1);
        if (m1==1 && LEAP_YEAR(NowYear)) days++;
        m1+=delta;
    }
    uint8_t d1 = NowDay;
    uint8_t d2 = timeptr->day;
    if(d2>=d1) delta = d2-d1; else delta = d1-d2;
    days += delta;      // Add remaining days
    return days;
}

uint32_t DaysSinceEpoch(const Type_Time *t) {
    uint32_t days = 0;
    // Years
    for (uint16_t y = 0; y < t->year; y++) {
        days += 365;
        if (LEAP_YEAR(y)) {
            days++;
        }
    }
    // Months
    for (uint8_t m = 1; m < t->month; m++) {
        days += pgm_read_byte_near(monthDays + (m - 1));
        if (m == 2 && LEAP_YEAR(t->year)) {
            days++;
        }
    }
    // Days
    days += (t->day - 1);
    return days;
}

// Distance in days from two dates
uint16_t DaysDifference(const Type_Time *t1, const Type_Time *t2) {
    uint32_t d1 = DaysSinceEpoch(t1);
    uint32_t d2 = DaysSinceEpoch(t2);
    return (d1 > d2) ? (d1 - d2) : (d2 - d1);
}

// Add a day to a date
void AddDay(Type_Time *timeptr) {
    uint8_t day = timeptr->day;
    uint8_t month = timeptr->month;
    uint8_t year = timeptr->year;
    day++;
    if(day>DaysInMonth(timeptr)) {
        day=1;
        month++;
        if(month>12) {
            month=1;
            year++;
        }
    }
    timeptr->day = day;
    timeptr->month = month;
    timeptr->year = year;
}

// Add a day to a date
void SubDay(Type_Time *timeptr) {
    uint8_t day = timeptr->day;
    uint8_t month = timeptr->month;
    uint8_t year = timeptr->year;
    day--;
    if(day==0) {
        month--;
        if(month==0) {
            year--;
            month=12;
        }
        timeptr->month = month;
        timeptr->year = year;
        day=DaysInMonth(timeptr);
    }
    timeptr->day = day;
}

// Compare dates
uint8_t CompareDate(const Type_Time *timeptr1, const Type_Time *timeptr2) {
    uint8_t y1 = timeptr1->year;
    uint8_t y2 = timeptr2->year;
    if(y2>y1) return 1;
    if(y2<y1) return 0;
    uint8_t m1 = timeptr1->month;
    uint8_t m2 = timeptr2->month;
    if(m2>m1) return 1;
    if(m2<m1) return 0;
    uint8_t d1 = timeptr1->day;
    uint8_t d2 = timeptr2->day;
    if(d2>d1) return 1;
    return 0;
}

void PrintYear(uint8_t year) {
    if(year<56) {
        print5x8(PSTR("19")); printN5x8(44+year);
    }
    else {
        print5x8(PSTR("20")); printN5x8(year-56);
    }
}

void PrintDate(uint8_t row, uint8_t col, Type_Time *timeptr) {
    uint8_t day;
    uint8_t month;
    uint8_t year;
    if(timeptr==0) {    // Print today's date
        day = NowDay;
        month = NowMonth;
        year = NowYear;
    } else {
        day = timeptr->day;
        month = timeptr->month;
        year = timeptr->year;
    }
    lcd_goto(row, col);
    if(!testbit(WSettings, PostYear)) {
        PrintYear(year);
        putchar5x8('/');
    }
    if(!testbit(WSettings, PostMonth)) {
        printN5x8(month);
        putchar5x8('/');
    }        
    printN5x8(day);
    if(testbit(WSettings, PostMonth)) {
        putchar5x8('/');
        printN5x8(month);
    }
    if(testbit(WSettings, PostYear)) {
        putchar5x8('/');
        PrintYear(year);
    }        
}

void PrintTime(uint8_t row, uint8_t col) {
    uint8_t Hour=NowHour;
    if(!testbit(WSettings, time24)) {
        if(Hour>=12) Hour-=12;
        if(Hour==0) Hour=12;
    }
    lcd_goto(row,col);
    printN_5x8(Hour); putchar5x8(':');
    printN5x8(NowMinute); putchar5x8(':');
    printN5x8(NowSecond);
    if(!testbit(WSettings, time24)) {
        if(testbit(SREG, CPU_T_bp)) print5x8(STR_pm);
        else print5x8(STR_am);
    }        
}
