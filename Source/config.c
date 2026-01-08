// Configuration options and miscellaneous
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include "main.h"
#include "utils.h"
#include "games.h"
#include "build.h"
#include "config.h"
#include "time.h"

// Diagnostic tests
void Diagnose(void) {
    uint8_t bar=0, speed=0;
    setbit(MStatus, update);
    setbit(Misc,bigfont);
    clrbit(WSettings, goback);
    PR.PRPC  &= 0b11111100;         // Enable TCC0 TCC1 clocks
    EVSYS.CH0MUX = 0b11000000;      // Event CH0 = TCC0 overflow
    TCC0.CNT = 0;
    TCC0.PER = 59999;
    TCC1.CNT = 0;
    TCC1.PER = 59999;
    TCC0.CTRLA = 3;                 // CPU clock
    TCC1.CTRLA = 0b00001000;        // Source is Event CH0
    LOGIC_ON();
    do {
        uint8_t temp = TCF0.CNTL;
        if((temp&0x01) == 0) ONGRN();
        else if((temp&0x01) == 1) ONRED();
        if((temp&0x03) == 2) Sound(TuneBeep);
        clr_display();
        lcd_goto(0,0);
        print5x8(VERSION); print5x8(PSTR(" Build: "));
        printhex5x8(BUILD_NUMBER>>8); printhex5x8(BUILD_NUMBER&0x00FF);
        lcd_goto(0,1); print5x8(PSTR("TimerC: ")); printhex5x8(TCC1.CNTH); printhex5x8(TCC1.CNTL);
        lcd_goto(0,2); print5x8(PSTR("TimerF: ")); printhex5x8(TCF0.CNTH); printhex5x8(TCF0.CNTL);
        lcd_goto(0,3); print5x8(PSTR("XMEGA:  rev")); putchar5x8('A'+MCU.REVID);
        lcd_goto(0,4); print5x8(PSTR("Logic:  ")); printhex5x8(VPORT2.IN);   // Shows the logic input data
        lcd_goto(0,5); print5x8(PSTR("Reset:  ")); printhex5x8(RST.STATUS);    // Show reset cause
        ANALOG_ON();
        lcd_goto(0,6); print5x8(PSTR("Vin:    ")); print16_5x8(MeasureVin(1));
        cli();
        SecTimeout = SREG;  // MeasureVCC alters the T bit!
        lcd_goto(0,7); print5x8(PSTR("VCC:    ")); print16_5x8(MeasureVCC());
        SREG = SecTimeout;
        sei();
        lcd_goto(0,8); print5x8(PSTR("VRef:   ")); print16_5x8(MeasureVRef());
        ANALOG_OFF();
        lcd_goto(0,9); print5x8(PSTR("Clock:  ")); printhex5x8(CLK.CTRL); printhex5x8(OSC.CTRL); printhex5x8(OSC.STATUS); printhex5x8(OSC.XOSCFAIL);
        lcd_goto(0,15); print5x8(PSTR("OFFSET  LIGHT   SPEED"));
        OFFRED();
        OFFGRN();
        for(uint8_t i=0; i<16; i++) {           // Print GPIO registers
            if(i<8) lcd_goto(i*16,10);
            else lcd_goto((i-8)*16,11);
            printhex5x8(*((uint8_t *)i));
        }
        if(testbit(Misc,userinput)) {
            clrbit(Misc, userinput);
            if(testbit(Buttons,KML)) setbit(WSettings, goback);
            if(testbit(Buttons,K1)) { CalibrateOffset(); setbit(Misc, redraw); }
            if(testbit(Buttons,K2)) {
                togglebit(VPORT1.OUT, LEDWHITE);   // Check backlight
            }
            if(testbit(Buttons,K3)) {
                speed = !speed;
                if(speed) CPU_Fast();
                else CPU_Slow();
            }
        }
        uint8_t *DisplayPointer = Disp_send.DataAddress + bar;
        for(uint8_t i=0; i<128; i++) {
            *DisplayPointer = ~(*DisplayPointer);
            DisplayPointer -= 18;   // Next column
        }
        bar++; if(bar>=16) bar=0;
        dma_display();
        WaitDisplay();
        SLP();          // Sleep
    } while(!testbit(WSettings, goback));
    OFFWHITE();
    LOGIC_OFF();
    TCC0.CTRLA = 0;
    TCC1.CTRLA = 0;
    PR.PRPC  |= 0b00000011;         // Disable TCC0 TCC1C clocks
    setbit(MStatus, update);
}

void PrintDate(uint8_t row, uint8_t col) {
    lcd_goto(row, col);
    if(NowYear<56) {
        print5x8(PSTR("19")); printN5x8(44+NowYear);
    }
    else {
        print5x8(PSTR("20")); printN5x8(NowYear-56);
    }
    putchar5x8('/');
    printN5x8(NowMonth); putchar5x8('/');
    printN5x8(NowDay);
}

void About(void) {
    Sound(TuneIntro);
    clr_display();
    u8CursorX=30; u8CursorY=1;
    uint16_t pointer = (uint16_t)Logo;
    for(uint8_t i=0; i<69; i++) {
        display_or(pgm_read_byte_near(pointer++)); // Print logo
    }
    lcd_goto(10,3);
    print5x8(&STRS_mainmenu[1][0]);    // STRS_mainmenu[1][0] contains the word Oscilloscope
    print5x8(&STRS_mainmenu[0][3]);    // STRS_mainmenu[0][2] contains the word Watch
    lcd_goto(0,13);
    print5x8(VERSION); print5x8(PSTR(" Build: "));
    printhex5x8(BUILD_NUMBER>>8); printhex5x8(BUILD_NUMBER&0x00FF);
    lcd_goto(0,14); print5x8(PSTR("Build Date 20")); printN5x8(BUILD_YEAR);
    putchar5x8('/'); printN5x8(BUILD_MONTH);
    putchar5x8('/'); printN5x8(BUILD_DAY);
    lcd_goto(0,15); print5x8(PSTR("RST:")); printhex5x8(RST.STATUS);    // Show reset cause
    uint8_t timeout=120;
    do {
        ANALOG_ON();        // Turn on analog circuits to be ready to read Vref
        GetTimeTimer();     // Sync variables from TCF0
        PrintDate(34,6);
        lcd_goto(40,7);
        printN_5x8(NowHour); putchar5x8(':');
        printN5x8(NowMinute); putchar5x8(':');
        printN5x8(NowSecond);
        dma_display();
        WaitDisplay();
        int16_t Vref = MeasureVRef();
        if(Vref<1997 || Vref>2099) {    // Vref outside +/- 2.5%
            ONRED();
        } else OFFRED();
        ANALOG_OFF();
        if(TCD1.CTRLA==0) SLP();          // Sleep if not sound playing
        else wait_ms(255);
    } while(timeout-- && !testbit(Misc,userinput));
    clrbit(Misc, userinput);
    setbit(MStatus, update);
}

void Profiles(void) {
    clrbit(WSettings, goback);
    uint8_t slot=0;
    do {
        clr_display();
        lcd_goto(0,0); print5x8(&STRS_mainmenu[1][0]);    // STRS_mainmenu[1][0] contains the word Oscilloscope
        print5x8(STR_Profiles);
        for(uint8_t i=0; i<8; i++) {
            lcd_goto(5,i+2);
            if(i==slot) {
                putchar5x8('-'); putchar5x8(0x81); // Print arrow
            }
            print5x8(PSTR(" Slot: "));
            putchar5x8('0'+i);
        }
        lcd_goto(0,15); print5x8(PSTR("RESTORE  LOAD    SAVE"));
        if(testbit(Misc,userinput)) {
            clrbit(Misc, userinput);
            if(testbit(Buttons,KBR) || testbit(Buttons,KBL)) slot++;
            if(testbit(Buttons,KUR) || testbit(Buttons,KUL)) slot--;
            if(testbit(Buttons,KML)) setbit(WSettings, goback);
            if(testbit(Buttons,K1)) {   // Restore Default Settings
                LoadProfile(0);
            }
            if(testbit(Buttons,K2)) {   // Load Profile
                LoadProfile(slot+1);
            }
            if(testbit(Buttons,K3)) {   // Save Profile
                SaveProfile(slot);
            }
            if(slot>=8) slot=0;
        }
        dma_display();
        WaitDisplay();
        SLP();          // Sleep
    } while(!testbit(WSettings, goback));
    setbit(MStatus, update);
}

extern const NVMVAR FLM;
extern NVMVAR EEMEM EEM_User[8];
extern NVMVAR EEMEM EEM;

void LoadProfile(uint8_t profile) {
    uint8_t GPIOtemp[12];
    NVMVAR Mtemp;
    ONGRN();
    if(profile) {                                       // Load settings from EEPROM
        profile--;
        eeprom_read_block(GPIOtemp, EEGPIO_User[profile], 12);
        eeprom_read_block(&Mtemp, &EEM_User[profile], sizeof(NVMVAR));
    } else {
        memcpy_P(GPIOtemp,  &FLGPIO, 12);               // Load settings from Flash
        memcpy_P(&Mtemp, &FLM, sizeof(NVMVAR));
    }
    eeprom_write_block(GPIOtemp, &EEGPIO, 12);          // Save settings
    eeprom_write_block(&Mtemp, &EEM, sizeof(NVMVAR));
    OFFGRN();
}

void SaveProfile(uint8_t profile) {
    ONRED();
    uint8_t GPIOtemp[12];
    NVMVAR Mtemp;
    eeprom_read_block(GPIOtemp, &EEGPIO, 12);                       // Load current settings
    eeprom_read_block(&Mtemp, &EEM, sizeof(NVMVAR));
    eeprom_write_block(GPIOtemp, EEGPIO_User[profile], 12);         // Save settings to EEPROM
    eeprom_write_block(&Mtemp, &EEM_User[profile], sizeof(NVMVAR));
    OFFRED();
}

void SaveScreenshot(void) {
    uint8_t *p=Disp_send.SPI_Address+2;  // Locate pointer at start of active buffer;
    for(uint8_t i=0; i<128; i++) {
        eeprom_busy_wait();
        eeprom_write_block(p, &EEDISPLAY[i*16], 16);
        p+=18;   // Next line
    }
}

void ShowScreenshot(void) {
    clrbit(WSettings, goback);
    clr_display();
    uint8_t *p=Disp_send.SPI_Address+2;  // Locate pointer at start of active buffer;
    for(uint8_t i=0; i<128; i++) {
        eeprom_busy_wait();
        eeprom_read_block(p, &EEDISPLAY[i*16], 16);
        p+=18;   // Next line
    }
    do {
        if(testbit(Misc,userinput)) {
            clrbit(Misc, userinput);
            if(testbit(Buttons,KML)) setbit(WSettings, goback);
        }
        dma_display();
        WaitDisplay();
        SLP();          // Sleep
    } while(!testbit(WSettings, goback));
    setbit(MStatus, update);
}

void OWSettings(void) {
    WSettings = eeprom_read_byte(&EE_WSettings);
    clrbit(WSettings, goback);
    uint8_t select=0;
    do {
        clr_display();
        lcd_goto(28,0); print5x8(&STRS_mainmenu[3][0]);    // STRS_mainmenu[3][0] contains the word Settings
        lcd_goto(16,2); print5x8(PSTR("Hourly Beep"));
        if(testbit(WSettings, hourbeep)) print5x8(STR_ON); else print5x8(STR_OFF);
        lcd_goto(16,3); print5x8(PSTR("24 Hour Format"));
        if(testbit(WSettings, time24))   print5x8(STR_ON); else print5x8(STR_OFF);
        for(uint8_t i=0; i<3; i++) {
            lcd_goto(2,i+2);
            if(i==select) {
                putchar5x8('-'); putchar5x8(0x81); // Print arrow
            }
        }
        lcd_goto(0,15); print5x8(PSTR("TOGGLE"));
        if(testbit(Misc,userinput)) {
            clrbit(Misc, userinput);
            if(testbit(Buttons,KBR) || testbit(Buttons,KBL)) select++;
            if(testbit(Buttons,KUR) || testbit(Buttons,KUL)) select--;
            if(testbit(Buttons,KML)) setbit(WSettings, goback);
            if(testbit(Buttons,K1)) {   // Toggle
                switch(select) {
                    case 0: togglebit(WSettings, hourbeep); break;
                    case 1: togglebit(WSettings, time24);   break;
                }
            }
            if(select>=2) select=0;
        }
        dma_display();
        WaitDisplay();
        SLP();          // Sleep
    } while(!testbit(WSettings, goback));
    eeprom_write_byte(&EE_WSettings, WSettings);    // Save Watch settings
    setbit(MStatus, update);
}
