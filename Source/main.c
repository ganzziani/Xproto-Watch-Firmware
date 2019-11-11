/*****************************************************************************

Oscilloscope Watch

Gabotronics
December 2018

Copyright 2018 Gabriel Anzziani

This program is distributed under the terms of the GNU General Public License

Target: ATXMEGA256A3U
Compiled with GCC, -Os optimizations

www.gabotronics.com
email me at: gabriel@gabotronics.com

*****************************************************************************/

// TODO & Optimizations:
/*      Sniffer IRDA, 1 Wire, MIDI        
        Gain Calibration
		PC control digital lines -> bus driver! SPI / I2C / UART ...
        Custom AWG backup in User Signature Row
        UART Auto baud rate
        Programmer mode
        If no sleeptime, check if menu timeout works
        Check if pretrigger samples completed with DMA (last part of mso.c)
        Make Srate signed, so tests like Srate>=11 become Srate>=0
        Share buffer between CH1 and CH2
        MSO Logic Analyzer more SPS, with DMA
        Force trigger, Trigger timeout in menu
		USB Frame counter
        Use both ADCs
        Channel math in meter mode
		USE NVM functions from BOOT */
// TODO When 64k parts come out:
/*      Vertical zoom
        Use as a programmer
        Terminal mode
        FFT waterfall
        Bigger fonts
        Setting profiles
        Continuous data to USB from ADC
        Independent CH1 and CH2 Frequency measurements, up to 1Mhz
        1v/octave (CV/Gate) AWG control
        RMS
        Arbitrary math expression on AWG
        DAC Calibration
	    Use DMA for USART PC transfers
        Dedicated Bode plots
        Dedicated VI Curve
	    12bit with slow sampling
        Horizontal cursor on FFT
        16MSPS for logic data, 1/sinc(x) for analog
	    Show menu title */

/* Hardware resources:
    Timers:
        RTC   Second timer
        TCC0  Frequency counter time keeper
            Also used as split timer, source is Event CH7 (40.96mS)
            TCC0L Controls the auto trigger
            TCC0H Auto key repeat
        TCC1  Counts post trigger samples
              UART sniffer time base
        TCD0  Split timer, source is Event CH6 (1.024ms)
            TCD0L 40.96mS period - 24.4140625 Hz - Source for Event CH7
	        TCD0H Controls LCD refresh rate
	    TCD1  Overflow used for AWG
        TCE0  Sounds
              Frequency counter low 16bits
        TCE1  Controls Interrupt ADC (srate >= 11), srate: 6, 7, 8, 9, 10
              Fixed value for slow sampling
              Frequency counter high 16bits
        TCF0  Seconds counter
    Events:
	    CH0 TCE1 overflow used for ADC
	    CH1 ADCA CH0 conversion complete
        CH2 Input pin for frequency measuring
        CH3 TCD1 overflow used for DAC
        CH4 RTC overflow used for Time and freq. measuring
        CH5 TCE0 overflow used for freq. measuring
        CH6 CLKPER / 32768 -> every 1.024ms
        CH7 TCD0L underflow: 40.96mS period - 24.4140625 Hz
	DMAs:
	    CH0 ADC CH0  / SPI Sniffer MOSI / UART Sniffer
	    CH1 ADC CH1  / SPI Sniffer MISO
	    CH2 Port CHD / Display
	    CH3 AWG DAC
    USART:
	    USARTD0 for Memory LCD
	    USARTC0 for Sniffer
	    USARTE0 for External Interface Port
	RAM:
	    1024:   Display buffer
         128:   Endpoint0 out + in
         128:   Endpoint1 out + in
         768:   CH1+CH2+CHD Data
         256:   AWG Buffer
          31:   M
		1536:   Temp (FFT, logic sniffer)
        -----
        3871    Total + plus some global variables
    Interrupt Levels:
        TCE1:           High        Slow Sampling
        PORTC_INT1      High        SPI sniffer
        PORTC INT0      High        I2C sniffer
        TCC1            High        UART sniffer
        USARTE0:        Medium      Serial port
        USB BUSEVENT    Medium      USB Bus Event
        USB_TRNCOMPL    Medium      USB Transaction Complete
        PORTA INT0:     Medium      keys
        TCC0L:          Low         auto trigger
        TCC0H:          Low         auto keys
        RTC:            Low         sleep timeout, menu timeout
        TCD0H:          Low         Wake from sleep (LCD refresh rate)
*/
#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>
#include <avr/fuse.h>
#include "main.h"
#include "mso.h"
#include "logic.h"
#include "awg.h"
#include "USB\usb_xmega.h"
#include "USB\Descriptors.h"
#include "time.h"
#include "utils.h"
#include "build.h"

const char mainmenutxt[][18] PROGMEM = {           // Menus:
    "Digital Watch",    // Watch
    "Oscilloscope ",    // Oscilloscope
    "    Games    ",    // Games
    "   Settings  ",    // Settings
};

const char optionmenutxt[][22] PROGMEM = {           // Menus:
    "Time  Calendar  Tasks",    // Watch
    "Load    Save    Start",    // Oscilloscope
    "Snake    Pong   Chess",    // Games
    "Config Diagnose About",    // Settings
};
  
FUSES = {
	.FUSEBYTE0 = 0xFF,  // JTAG not used, ***NEEDS*** to be off
	.FUSEBYTE1 = 0x00,  // Watchdog Configuration
	.FUSEBYTE2 = 0xBF,  // Reset Configuration, BOD off during power down
	.FUSEBYTE4 = 0xF7,  // 4ms Start-up Configuration
	.FUSEBYTE5 = 0xDB,  // No EESAVE on chip erase, BOD sampled when active, 2.4V BO level
};

// Disable writing to the bootloader section
LOCKBITS = (0xBF);

uint8_t SP_ReadCalibrationByte(uint8_t location);
static inline void Restore(void);
//uint16_t readVCC(void);

// Big buffer to store large but temporary data
TempData T;

// Variables that need to be saved in NVM
NVMVAR M;

// EEProm variables
uint8_t EEMEM EESleepTime = 32;     // Sleep timeout in minutes
uint8_t EEMEM EEDACgain   = 0;      // DAC gain calibration
uint8_t EEMEM EEDACoffset = 0;      // DAC offset calibration
uint8_t EEMEM EECalibrated = 0xFF;  // Offset calibration done

//static void CalibrateDAC(void);
void SimpleADC(void);
void CalibrateOffset(void);
void CalibrateGain(void);
void AnalogOn(void);
void LowPower(void);
void Diagnose(void);
void About(void);

int main(void) {
    PR.PRGEN = 0b01011000;          // Power Reduction: USB, AES, EBI, EVSYS, only RTC and DMA on
    PR.PRPA  = 0b00000111;          // Power Reduction: DAC, ADC, AC
    PR.PRPB  = 0b00000111;          // Power Reduction: DAC, ADC, AC
    PR.PRPC  = 0b11111111;          // Power Reduction: TWI, USART0, USART1, SPI, HIRES, TC1, TC0
    PR.PRPD  = 0b11101111;          // Power Reduction: TWI,       , USART1, SPI, HIRES, TC1, TC0
    PR.PRPE  = 0b11111111;          // Power Reduction: TWI, USART0, USART1, SPI, HIRES, TC1, TC0
    PR.PRPF  = 0b11111110;          // Power Reduction: TWI, USART0, USART1, SPI, HIRES, TC1, TC0
    NVM.CTRLB = 0b00001100;         // EEPROM Mapping, Turn off bootloader flash
    // PORTS CONFIGURATION
    PORTA.DIR       = 0b00101110;   // 1.024V, BATTSENSE, CH2-AC, CH1, x, CH1-AC, LOGICPOW, 2.048V
    PORTA.PIN4CTRL  = 0x07;         // Input Disable on pin PA4
    PORTA.PIN6CTRL  = 0x07;         // Input Disable on pin PA6
    PORTA.PIN7CTRL  = 0x07;         // Input Disable on pin PA7
    PORTB.DIR       = 0b11000011;   // LOGICDIR, ANPOW, CH2, 1.024V, AWG, TRIG, x, x
    PORTB.PIN2CTRL  = 0x01;         // Sense rising edge (Freq. counter)    
	PORTB.PIN3CTRL	= 0x07;         // Input Disable on pin PB3
    PORTB.PIN4CTRL  = 0x07;         // Input Disable on pin PB4
    PORTB.PIN5CTRL  = 0x07;         // Input Disable on pin PB5
    PORTB.OUT       = 0b10000000;   // Logic port as input, Analog power off
    //PORTC.DIR = 0b00000000;       // LOGIC, register initial value is 0
    PORTC.INT0MASK  = 0x01;         // PC0 (SDA) will be the interrupt 0 source
    PORTC.INT1MASK  = 0x80;         // PC7 (SCK) will be the interrupt 1 source
    PORTD.DIR       = 0b00111111;   // D+, D-, LCDVDD, EXTCOMM, LCDIN, LCDDISP, LCDCLK, LCDCS
    PORTD.OUT       = 0b00100000;   // Power to LCD
    PORTE.DIR       = 0b00111111;   // Crystal, crystal, buzzer, buzzer, BATTSENSEPOW, RED, GRN, WHT
    PORTE.REMAP     = 0b00000001;   // Remap Ooutput Compare from 0 to 4
    PORTE.PIN4CTRL  = 0b01000000;   // Invert Pin 4 output
    PORTCFG.MPCMASK = 0xFF;
    PORTF.PIN0CTRL  = 0x58;         // Pull up on pin Port F, invert input
    PORTF.INTCTRL   = 0x02;         // PORTA will generate medium level interrupts
    PORTF.INT0MASK  = 0xFF;         // All inputs are the source for the interrupt
    PORTCFG.VPCTRLA = 0x41;         // VP1 Map to PORTE, VP0 Map to PORTB
    PORTCFG.VPCTRLB = 0x32;         // VP3 Map to PORTD, VP2 Map to PORTC    
    
    LowPower();                     // Analog off, Slow CPU
    SwitchBuffers();
	clr_display();
    DMA.CTRL          = 0x80;       // Enable DMA, single buffer, round robin
    
    // TIME CONTROL!
    CLK.RTCCTRL = CLK_RTCSRC_TOSC_gc | CLK_RTCEN_bm;    // clkRTC=1.024kHz from external 32.768kHz crystal oscillator
    RTC.PER = 511;					// To generate 1Hz: ((PER+1)/(clkRTC*Prescale) = 1 seconds --> PER = 511
    RTC.COMP = 255;					// Memory LCD needs 0.5Hz min on EXTCOMM
    RTC.CTRL = 0x02;                // Prescale by 2 -> RTC counting at 512Hz
    RTC.INTCTRL = 0x05;             // Generate low level interrupts (Overflow and Compare)
    EVSYS.CH4MUX    = 0x08;         // Event CH4 = RTC overflow (every second)
    TCF0.PER = 43199;               // 43200 seconds = 12 hours
    TCF0.CTRLA = 0x0C;              // Source is Event CH4
    TCF0.INTCTRLA = 0x01;           // 12 hour interrupt, low level interrupt
    eeprom_read_block(NOW, &saved_time, sizeof(time_var));   // Load latest known time
    SetTimeTimer();                 // Load TCF0 Timer with current time variables
    PMIC.CTRL = 0x07;               // Enable High, Medium and Low level interrupts
    sei();                          // Enable global interrupts    

    Randomize();                    // Randomize random number generator
    About();                        // Go to About on startup
    RST.STATUS = 0x3F;              // Clear Reset flags
    Watch();                        // Go to watch mode after reset
    clrbit(Misc, userinput);
    
    Buttons=K1;
    Menu=1;   // Menu item
    uint8_t old_item=0;
	uint8_t timeout=0;
    for(;;) {
        // Check User Input
        if(testbit(Misc, userinput)) {
            clrbit(Misc, userinput);
            if(testbit(Buttons,KUR) || testbit(Buttons,KBR)) Menu++;
            if(testbit(Buttons,KUL) || testbit(Buttons,KBL)) Menu--;
            switch(Menu) {
                if(testbit(Buttons,K1) || testbit(Buttons,K1) || testbit(Buttons,K1)) { // Prepare to do animation after action
                    old_item=255;
                }
                case 1:     // Watch Menu
                    if(testbit(Buttons,K1)) Watch();
                    if(testbit(Buttons,K2)) Calendar();
                    //if(testbit(Buttons,K3)) Tasks();
                break;
                case 2:     // Oscilloscope Menu
                    if(testbit(Buttons,K3)) {
						eeprom_write_block(NOW, &saved_time, sizeof(time_var));
                        AnalogOn();
                        RTC.INTCTRL = 0x00;                     // Disable RTC interrupts
                        TCF0.INTCTRLB = 0x00;                   // Disable 1 minute interrupt
                        // During the Scope mode, TCD0 will generate 1Hz for the Memory LCD EXTCOMM
                        TCD0.CTRLB = 0b00010000;                // Enable HCMPENA, pin4
                        TCD0.CCAH = 128;                        // Automatic EXTCOMM with Timer D0                        
                        MSO();                                  // go to MSO
                        eeprom_read_block(NOW, &saved_time, sizeof(time_var));   // Load latest known time
                        GetTimeTimer();                         // Sync variables from TCF0
                        findweekday(NOW);
                        WatchBits = 0;
                        WSettings = 0;
                        TCD0.CTRLB = 0;
                        TCD0.CCAH = 0;
                        SetMinuteInterrupt();                   // Set minute interrupt
                        RTC.INTCTRL = 0x05;
                        setbit(MStatus, update);
                    }                        
                break;
                case 3:     // Games Menu
                    if(testbit(Buttons,K1)) Snake();
                    if(testbit(Buttons,K2)) Pong();
                    if(testbit(Buttons,K3)) { CPU_Fast(); Chess(); }
                break;
                case 4:     // Settings Menu
                    //if(testbit(Buttons,K1)) Config();
                    if(testbit(Buttons,K2)) Diagnose();
                    if(testbit(Buttons,K3)) About();
                break;
            }
        } else timeout++;
        if(testbit(MStatus, update)) {
            int8_t step, from;
            if(Menu>old_item) { step=-15; from=133; }
            else { step=15; from=-101; }
            LowPower();         // Analog off, Slow CPU
            Sound(NOTE_B7,NOTE_B7);
            for(int8_t n=step; n<118 && n>-118; n+=step) {	// Slide animation
                if(testbit(Misc, userinput)) {   // Button pressed during animation
                    clrbit(Misc, userinput);
                    Sound(NOTE_B7,NOTE_B7);
                    old_item=Menu;
                    if(testbit(Buttons,KUR) || testbit(Buttons,KBR)) {
                        Menu++;
                        n=step=-15;
                        from=133;
                    }
                    if(testbit(Buttons,KUL) || testbit(Buttons,KBL)) {
                        Menu--;
                        n=step=15;
                        from=-101;
                    }                
                }
                if(Menu>4) Menu=1;
                if(Menu==0) Menu=4; 
                if(step>1) step--;
                if(step<-1) step++;
                dma_display();
                SwitchBuffers();
                clr_display();
                bitmap_safe(16+n,4,(uint8_t *)pgm_read_word(Icons+old_item),1);
                bitmap_safe(from+n,4,(uint8_t *)pgm_read_word(Icons+Menu),5);
                WaitDisplay();
                SoundOff();
            }
            lcd_goto(28,2);
            lcd_put5x8(mainmenutxt[Menu-1]);
            lcd_goto(0,15);
            lcd_put5x8(optionmenutxt[Menu-1]);
            WaitDisplay();
            dma_display();
            old_item=Menu;
            timeout=0;
            clrbit(MStatus, update);
        }
        if(timeout>=250) {  // Go back to Watch mode
            timeout=0;
            Watch();
        }
        WaitDisplay();
        SLP();          // Sleep
    }        
    return 0;
}

/*
// Waits for the DMA to complete (the DMA's ISR will SET LCD_CS)
void WaitDisplay(void) {
    uint16_t n=0;
    while(testbit(LCD_CTRL,LCD_CS)) {   // Wait for transfer complete
        _delay_us(1);
        n++;
        if(n==20000) break;     // timeout ~ 20mS
    }
    OFFGRN();
    OFFRED();    
}
*/
// Tactile Switches - This is configured as medium level interrupt
ISR(PORTF_INT0_vect) {
    uint8_t i,in,j=0;
    // Debounce: need to read 10 consecutive equal numbers
    OFFGRN();   // Avoid having the LED on during this interrupt
    OFFRED();
    for(i=10; i>0; i--) {
        delay_ms(1);
		in = PORTF.IN;              // Read port
		if(j!=in) { j=in; i=10; }   // Value changed
	}
    Buttons = in;
    if(testbit(Buttons,KUL) && testbit(Buttons, KUR) && testbit(Buttons,KML)) { // Enter bootloader
        eeprom_write_block(NOW, &saved_time, sizeof(time_var));             // Save current time
        Jump_boot();
    }
    if(Buttons) {
        setbit(MStatus, update);         // Valid key
        setbit(Misc, userinput);
        // TD0H used for auto repeat key
        if(TCC0.CTRLA == 0x0F) {             // Not doing a frequency count
            TCC0.CNTH = 24;                             // Restart timer
            setbit(TCC0.INTFLAGS, TC2_HUNFIF_bp);       // Clear trigger timeout interrupt
            TCC0.INTCTRLA |= TC2_HUNFINTLVL_LO_gc;      // Enable Auto Key interrupt
        }
    }
    else clrbit(Misc,keyrep);
}

// Auto repeat key
ISR(TCC2_HUNF_vect) {
    TCC0.INTCTRLA &= ~TC2_HUNFINTLVL_LO_gc;         // Disable Auto Key interrupt
    if(Buttons) setbit(Misc,keyrep);
}

void CPU_Fast(void) {
    SetMainBuffer();        // Use main LCD buffer
    // Clock Settings
    OSC.XOSCCTRL = 0xCB;    // Crystal type 0.4-16 MHz XTAL - 16K CLK Start Up time
    OSC.PLLCTRL = 0xC2;     // XOSC is PLL Source - 2x Factor (32MHz)
    OSC.CTRL |= OSC_RC2MEN_bm | OSC_XOSCEN_bm;
    delay_ms(2);
    // Switch to internal 2MHz if crystal fails
    if(!testbit(OSC.STATUS,OSC_XOSCRDY_bp)) {   // Check XT ready flag
        OSC.XOSCCTRL = 0x00;    // Disable external oscillators
        // Not entering, comment to save
        //OSC.PLLCTRL = 0x10;     // 2MHz is PLL Source - 16x Factor (32MHz)
    }
    OSC.CTRL = OSC_RC2MEN_bm | OSC_RC32MEN_bm | OSC_PLLEN_bm | OSC_XOSCEN_bm;
    delay_ms(2);
    CCPWrite(&CLK.CTRL, CLK_SCLKSEL_PLL_gc);    // Switch to PLL clock
    // Clock OK!
    OSC.CTRL = OSC_RC32MEN_bm | OSC_PLLEN_bm | OSC_XOSCEN_bm;    // Disable internal 2MHz

    if(CLK.CTRL & CLK_SCLKSEL_RC32M_gc) {   // Clock error?
        tiny_printp(0,7,PSTR("NO XT"));
    }
    USARTD0.BAUDCTRLA = FBAUD32M;	    // SPI clock rate for display, CPU is at 32MHz    
    NVM.CTRLB = 0b00001100;             // EEPROM Mapping, Turn off bootloader flash, Turn on EEPROM
}

void CPU_Slow(void) {
    OSC.CTRL |= OSC_RC2MEN_bm;          // Enable internal 2MHz
    do {
        delay_ms(2);
    } while((OSC.STATUS&0x01)==0);      // Wait until 2MHz is stable
    CCPWrite(&CLK.CTRL, CLK_SCLKSEL_RC2M_gc);    // Switch to 2MHz clock
    do {
        OSC.XOSCCTRL = 0x00;                // Disable external oscillators
        OSC.PLLCTRL = 0;
        delay_ms(1);
        OSC.CTRL = OSC_RC2MEN_bm;
    } while(OSC.CTRL!=OSC_RC2MEN_bm);
    USARTD0.BAUDCTRLA = FBAUD2M;	    // SPI clock rate for display, CPU is at 2MHz
    NVM.CTRLB = 0b00001110;             // EEPROM Mapping, Turn off bootloader flash, Turn off EEPROM
}

void AnalogOn(void) {
    ANALOG_ON();
    // Power reduction: Stop unused peripherals
    PR.PRGEN = 0x18;        // Stop: AES, EBI
    PR.PRPA  = 0x04;        // Stop: DAC
    PR.PRPB  = 0x01;        // Stop: AC
    PR.PRPC  = 0x7C;        // Stop: TWI, USART0, USART1, SPI, HIRES
    PR.PRPD  = 0x6C;        // Stop: TWI,       , USART1, SPI, HIRES
    PR.PRPE  = 0x6C;        // Stop: TWI,       , USART1, SPI, HIRES
    CPU_Fast();
}

// Analog off, Slow CPU
void LowPower(void) {
    GLCD_LcdInit();                 // Initialize LCD
    CCPWrite(&WDT.CTRL, WDT_CEN_bm); // Watchdog off
    SLEEP.CTRL = SLEEP_SMODE_PSAVE_gc | SLEEP_SEN_bm;
    ANALOG_OFF();
    TCC0.CTRLA = 0;                 // Stop timer prior to reset
    TCC0.CTRLFSET   = 0x0F;         // Reset timer
    TCC1.CTRLA = 0;                 // Stop timer prior to reset
    TCC1.CTRLFSET   = 0x0F;         // Reset timer
    TCD0.CTRLA = 0;                 // Stop timer prior to reset
    TCD0.CTRLFSET   = 0x0F;         // Reset timer
    TCD1.CTRLA = 0;                 // Stop timer prior to reset
    TCD1.CTRLFSET   = 0x0F;         // Reset timer
    TCE0.CTRLA = 0;                 // Stop timer prior to reset
    TCE0.CTRLFSET   = 0x0F;         // Reset timer
    TCE1.CTRLA = 0;                 // Stop timer prior to reset
    TCE1.CTRLFSET   = 0x0F;         // Reset timer
    DMA.CTRL        = 0x00;         // Disable DMA
    DMA.CTRL        = 0x40;         // Reset DMA
    ADCA.CTRLA      = 0x00;         // Disable ADC
    ADCB.CTRLA      = 0x00;         // Disable ADC
    // POWER REDUCTION: Stop unused peripherals - Stop everything but RTC and DMA
    PR.PRGEN = 0b01011000;          // Stop: USB, AES, EBI, EVSYS, only RTC and DMA on
    PR.PRPA  = 0b00000111;          // Stop: DAC, ADC, AC
    PR.PRPB  = 0b00000111;          // Stop: DAC, ADC, AC
    PR.PRPC  = 0b11111111;          // Stop: TWI, USART0, USART1, SPI, HIRES, TC1, TC0
    PR.PRPD  = 0b11101111;          // Stop: TWI,       , USART1, SPI, HIRES, TC1, TC0
    PR.PRPE  = 0b11111111;          // Stop: TWI, USART0, USART1, SPI, HIRES, TC1, TC0
    PR.PRPF  = 0b11111110;          // Stop: TWI, USART0, USART1, SPI, HIRES, TC1, TC0
	PORTA.OUTCLR = 0b00100100;		// Turn off opto relays
    CPU_Slow();
    DMA.CTRL          = 0x80;       // Enable DMA
}

// From Application Note AVR1003
void CCPWrite( volatile uint8_t * address, uint8_t value ) {
    uint8_t volatile saved_sreg = SREG;
    cli();

#ifdef __ICCAVR__
	asm("movw r30, r16");
#ifdef RAMPZ
	RAMPZ = 0;
#endif
	asm("ldi  r16,  0xD8 \n"
	    "out  0x34, r16  \n"
#if (__MEMORY_MODEL__ == 1)
	    "st     Z,  r17  \n");
#elif (__MEMORY_MODEL__ == 2)
	    "st     Z,  r18  \n");
#else /* (__MEMORY_MODEL__ == 3) || (__MEMORY_MODEL__ == 5) */
	    "st     Z,  r19  \n");
#endif /* __MEMORY_MODEL__ */

#elif defined __GNUC__
	volatile uint8_t * tmpAddr = address;
#ifdef RAMPZ
	RAMPZ = 0;
#endif
	asm volatile(
		"movw r30,  %0"	      "\n\t"
		"ldi  r16,  %2"	      "\n\t"
		"out   %3, r16"	      "\n\t"
		"st     Z,  %1"       "\n\t"
		:
		: "r" (tmpAddr), "r" (value), "M" (CCP_IOREG_gc), "i" (&CCP)
		: "r16", "r30", "r31"
		);

#endif
	SREG = saved_sreg;
}

// Read out calibration byte.
uint8_t SP_ReadCalibrationByte(uint8_t location) {
	uint8_t result;
	/* Load the NVM Command register to read the calibration row. */
	NVM_CMD = NVM_CMD_READ_CALIB_ROW_gc;
 	result = pgm_read_byte(location);
	/* Clean up NVM Command register. */
 	NVM_CMD = NVM_CMD_NO_OPERATION_gc;
	return result;
}

// Calibrate offset, inputs must be connected to ground
void CalibrateOffset(void) {
    int8_t  *q1, *q2, avrg8;  // temp pointers to signed 8 bits
    uint8_t i,j,s=0;
    int16_t avrg1, avrg2;
    Buttons=0;
    clr_display();
    lcd_put5x8(PSTR("DISCONNECT CH1,CH2"));
    lcd_goto(108,15); lcd_put5x8(PSTR("GO"));
    dma_display(); WaitDisplay();
    while(!Buttons);
    AnalogOn();
    if(testbit(Buttons,K3)) {
        clr_display();
	    for(Srate=0; Srate<8; Srate++) {	// Cycle thru first 8 srates
            i=6; do {
                s++;
                M.CH1gain=i;
                M.CH2gain=i;
                SimpleADC();
                q1=T.IN.CH1;
                q2=T.IN.CH2;
                // Calculate offset for CH1
                avrg1=0;
                avrg2=0;
                j=0; do {
            	    avrg1+= (*q1++);
            	    avrg2+= (*q2++);
                } while(++j);
                avrg8=avrg1>>8;
                ONGRN();
                eeprom_write_byte((uint8_t *)&offset8CH1[Srate][i], avrg8);
                j = 32+avrg8; // add 32 to center on screen
                if(j<64) lcd_line(s,96,s,j+64);
                else ONRED();
                avrg8=avrg2>>8;
                eeprom_write_byte((uint8_t *)&offset8CH2[Srate][i], avrg8);
                j = 32+avrg8; // add 32 to center on screen
                if(j<64) lcd_line(s+64,96,s+64,j+64);
                else ONRED();
                dma_display(); WaitDisplay();
            } while(i--);
        }
        // Calculate offset for Meter in VDC
        avrg1=0;
        avrg2=0;
        ADCA.CTRLB = 0x10;          // signed mode, no free run, 12 bit right adjusted
        ADCA.PRESCALER = 0x07;      // Prescaler 512 (500kHZ ADC clock)
        ADCB.CTRLB = 0x10;          // signed mode, no free run, 12 bit right adjusted
        ADCB.PRESCALER = 0x07;      // Prescaler 512 (500kHZ ADC clock)
        i=0;
        do {
            ADCA.CH0.CTRL     = 0x83;   // Start conversion, Differential input with gain
            ADCB.CH0.CTRL     = 0x83;   // Start conversion, Differential input with gain
            delay_ms(1);
            avrg1+= (int16_t)ADCA.CH0.RES;  // Measuring 0V, should not overflow 16 bits
            avrg2+= (int16_t)ADCB.CH0.RES;  // Measuring 0V, should not overflow 16 bits
        } while(++i);
        eeprom_write_word((uint16_t *)&offset16CH1, avrg1/*+0x08*/);
        eeprom_write_word((uint16_t *)&offset16CH2, avrg2/*+0x08*/);
        eeprom_write_byte(&EECalibrated, 0);    // Calibration complete!
    }
    Buttons=0;
    LowPower();     // Analog off, Slow CPU
}

// Calibrate gain, inputs must be connected to 4.000V
void CalibrateGain(void) {
    uint8_t i=0;
    static int32_t avrg1, avrg2;
    int16_t offset;
    #ifndef NODISPLAY
        Buttons=0;
        clr_display();
        lcd_putsp(PSTR("NOW CONNECT 4.000V"));
        tiny_printp(116,7,PSTR("GO"));
        dma_display();
        while(!Buttons);
    #else
        setbit(Buttons,K3);
    #endif
    if(testbit(Buttons,K3)) {
        clr_display();
        // Calculate offset for Meter in VDC
        avrg1=0;
        avrg2=0;
        ADCA.CTRLB = 0x90;          // signed mode, no free run, 12 bit right adjusted
        ADCA.PRESCALER = 0x07;      // Prescaler 512 (500kHZ ADC clock)
        ADCB.CTRLB = 0x90;          // signed mode, no free run, 12 bit right adjusted
        ADCB.PRESCALER = 0x07;      // Prescaler 512 (500kHZ ADC clock)
        i=0;
        do {
            ADCA.CH0.CTRL     = 0x83;   // Start conversion, Differential input with gain
            ADCB.CH0.CTRL     = 0x83;   // Start conversion, Differential input with gain
            delay_ms(1);
            avrg1-= (int16_t)ADCA.CH0.RES;
            avrg2-= (int16_t)ADCB.CH0.RES;
        } while(++i);
        // Vcal = 4V
        // Amp gain = 0.18
        // ADC Reference = 1V
        // 12 bit signed ADC -> Max = 2047
        // ADC cal = 4*.18*2047*256 = 377303
        // ADCcal = ADCmeas * (2048+cal)/2048
		offset=(int16_t)eeprom_read_word((uint16_t *)&offset16CH1);      // CH1 Offset Calibration
		avrg1+=offset;
        avrg1 = (377303*2048l-avrg1*2048)/avrg1;
        eeprom_write_byte((uint8_t *)&gain8CH1, avrg1);
		offset=(int16_t)eeprom_read_word((uint16_t *)&offset16CH2);      // CH2 Offset Calibration
		avrg2+=offset;
        eeprom_write_byte((uint8_t *)&gain8CH2, avrg2);
    }
    Buttons=0;
}

// Fill up channel data buffers
void SimpleADC(void) {
	Apply();
	_delay_ms(64);
    StartDMAs();
	_delay_ms(16);
    ADCA.CTRLB = 0x14;          // Stop free run of ADC (signed mode, no free run, 8 bit)
    ADCB.CTRLB = 0x14;          // Stop free run of ADC (signed mode, no free run, 8 bit)
    // Disable DMAs
    clrbit(DMA.CH0.CTRLA, 7);
    clrbit(DMA.CH2.CTRLA, 7);
    clrbit(DMA.CH1.CTRLA, 7);
}

/*
// Calibrate DAC gain and offset, connect AWG to CH1
// Adjust with rotary encoders
static void CalibrateDAC(void) {
    uint8_t i, step=0, data, average;
    uint8_t test, bestoffset, bestgain, bestmeasure1;
    uint16_t sum, bestmeasure2;
    clr_display();

    ADCA.CH0.CTRL = 0x03 | (6<<2);       // Set gain 6
    CH1.offset=(signed char)eeprom_read_byte(&offsetsCH1[6]);

    AWGAmp=127;         // Amplitude range: [0,127]
    AWGtype=1;          // Waveform type
    AWGduty=256;        // Duty cycle range: [0,512]
    AWGOffset=0;        // 0V offset
    desiredF = 100000;  // 1kHz
    BuildWave();
    while(step<7) {
        while(!testbit(TCD0.INTFLAGS, TC1_OVFIF_bp));   // wait for refresh timeout
        setbit(TCD0.INTFLAGS, TC1_OVFIF_bp);
        // Acquire data

        // Display waveform
        i=0; sum=0;
        do {
            data=addwsat(CH1.data[i],CH1.offset);
            sum+=data;
            set_pixel(i>>1, data>>2);    // CH1
        } while(++i);
        average=(uint8_t)(sum>>8);

        switch(step) {
            case 0: // Connect AWG to CH1
                tiny_printp(0,0,PSTR("AWG Calibration Connect AWG CH1 Press 5 to start"));
                step++;
            break;
            case 1:
                if(key) {
                    if(key==KC) step++;
                    else step=7;         // Did not press 5 -> exit
                }
            break;
            case 2: // Output 0V from AWG
                AWGAmp=1;         // Amplitude range: [0,127]
                AWGtype=1;        // Waveform type
                BuildWave();
                tiny_printp(0,3,PSTR("Adjusting offset"));
                // ADS931 power, output enable, CH gains
//                PORTE.OUT = 0;
                CH1.offset=(signed char)eeprom_read_byte(&offsetsCH1[0]);
                step++;
                bestoffset = 0;
                test = 0;
                bestmeasure1=0;
                DACB.OFFSETCAL = 0;
            break;
            case 3: // Adjust Offset
                if(abs((int16_t)average-128)<abs((int16_t)bestmeasure1-128)) {    // Current value is better
                    bestoffset = test;
                    bestmeasure1=average;
                    lcd_goto(0,4);
                    if(bestoffset>=0x40) printN(0x40-bestoffset);
                    else printN(bestoffset);
                }
                lcd_line(0,bestmeasure1>>1,127,bestmeasure1>>1);
                test++;
                DACB.OFFSETCAL = test;
                if(test>=128) {
                    step++;
                    DACB.OFFSETCAL = bestoffset;   // Load DACA offset calibration
                }
            break;
            case 4: // Output -1.75V from AWG
                AWGAmp=0;           // Full Amplitude
                AWGtype=1;          // Waveform type
                AWGOffset=112;      // Offset = -1.75
                BuildWave();
                tiny_printp(0,5,PSTR("Adjusting gain"));
//                PORTE.OUT = 4;  // 0.5V / div
                CH1.offset=(signed char)eeprom_read_byte(&offsetsCH1[4]);
                step++;
                bestgain = 0;
                test=0;
                bestmeasure2=0;
                DACB.GAINCAL = 0;
            break;
            case 5: // Adjust gain
                // (1.75/0.5)*32+128)*256 = 61440
                if(abs((int32_t)sum-61696)<abs((int32_t)bestmeasure2-61696)) {    // Current value is better
                    bestgain = test;
                    bestmeasure2=sum;
                    lcd_goto(0,6);
                    if(bestgain>=0x40) printN(0x40-bestgain);
                    else printN(bestgain);
                }
                test++;
                DACB.GAINCAL = test;
                if(test>=128) {
                    step++;
                    DACB.GAINCAL = bestgain;
                }
            break;
            case 6: // Calibration complete
                // Save calibration results
                AWGAmp=0;
                eeprom_write_byte(&EEDACoffset, bestoffset);    // Save offset calibration
                eeprom_write_byte(&EEDACgain, bestgain);        // Save gain calibration
                tiny_printp(0,15,PSTR("Cal complete"));
                step++;
            break;
        }
    }
    // Restore Waveform
    LoadAWGvars();              // Load AWG settings
    BuildWave();                // Construct AWG waveform
}*/

extern const NVMVAR FLM;
static inline void Restore(void) {
    memcpy_P(0,  &FLGPIO, 12);
    memcpy_P(&M, &FLM, sizeof(NVMVAR));
    ONGRN();
    SaveEE();
    OFFGRN();
    Buttons=0;
}

void PowerDown(void) {
	USB.CTRLB = 0;          // USB Disattach
	USB.ADDR = 0;
	USB.CTRLA = 0;
    PORTD.OUT = 0;              // Turn off display
    PORTE.OUT = 0;              // Turn off LEDs
    SaveEE();               // Save MSO settings
    //GLCD_LcdOff();
    while(Buttons);
    PORTE.OUTCLR = 0x01;    // Power up clear
    SLP();                  // Sleep
    GLCD_LcdInit();
    PORTE.OUTSET = 0x01;    // Power up
	Buttons=0;
	USB_ResetInterface();
}

// Wake up from sleep
ISR(TCD2_HUNF_vect) {
    SLEEP.CTRL = 0x00;
}

// Delay in mili seconds, take into account current CPU speed
void delay_ms(uint16_t n) {
    while(n--) {
        if(CLK.CTRL==0) {   // CPU is running at 2MHz
            _delay_us(62);
        }
        else {              // CPU is running at 32MHz
            _delay_us(999);
        }
    }
}

// Dual Tone sound
void Sound(uint8_t F1, uint8_t F2) {
    if(testbit(WSettings,sound_off)) return;
    PR.PRPE  &= 0b11111110;         // Enable TC1E clock
    TCE0.CTRLE = 0x02;              // Timer TCE0 Split Mode
    TCE0.CTRLB = 0b00100001;        // Enable output compares, H->B, L->A
    TCE0.PERH = F1;                 // Frequency 1
    TCE0.PERL = F2;                 // Frequency 2
    TCE0.CCBH = F1>>1;              // Duty cycle 50%
    TCE0.CCAL = F2>>1;              // Duty cycle 50%
    if(CLK.CTRL==0) {               // CPU is running at 2MHz
        TCE0.CTRLA = 0x04;          // Enable timer, Prescale 8 -> clock is 250kHz
    }
    else {                          // CPU is running at 32MHz
        TCE0.CTRLA = 0x05;          // Enable timer, Prescale 64 -> clock is 500kHz
    }
}

void SoundOff(void) {
    TCE0.CTRLA = 0;                 // Disable timer
    TCE0.CTRLB = 0;
    PORTE.OUT = 0x00;
    PR.PRPE  |= 0b00000001;         // Disable TC1E clock
} 

// Measure battery under load
// scale = 0, return value between 0 and 10
// scale != 0, return ADC result
int16_t MeasureBattery(uint8_t scale) {
    ANALOG_ON();
    PR.PRPA  &= 0b11111101;         // Enable ADCA module
    BATT_TEST_ON();
    delay_ms(25);
    ADCA.CTRLA   = 0x01;            // Enable ADC
    ADCA.CTRLB   = 0x70;            // Limit ADC current, signed mode, no free run, 12 bit right
    ADCA.REFCTRL = 0x20;            // REF = AREF (2.048V)
    if(CLK.CTRL==0) {               // CPU is running a t 2MHz
        ADCA.PRESCALER = 0x00;      // DIV4
    }
    else {                          // CPU is running at 32MHz
        ADCA.PRESCALER = 0x05;      // DIV128
    }
    ADCA.CH0.MUXCTRL    = 0x3A;     // Channel 0 input: ADC7 pin - ADC6 pin
    ADCA.CH0.CTRL       = 0x83;     // Start conversion, Differential with gain
    while(ADCA.INTFLAGS==0);
    ADCA.INTFLAGS = 0x01;           // Clear interrupt flag
    // Volt (mv) = 2048 - (RESL * 1000*VREF/2048) = 2048 - 2*RES
    int16_t volt=2048-2*ADCA.CH0RES;
    ANALOG_OFF();
    BATT_TEST_OFF();
    ADCA.REFCTRL = 0x00;            // Bandgap off    
    PR.PRPA  |= 0b00000010;         // Disable ADCA
    if(scale) return volt;
    //if(volt>=4500) return 12;     // Charging
    if(volt>=4100) return 11;       // Estimated charge
    if(volt>=4000) return 10;
    if(volt>=3900) return 9;
    if(volt>=3800) return 8;
    if(volt>=3750) return 7;
    if(volt>=3700) return 6;
    if(volt>=3650) return 5;
    if(volt>=3600) return 4;
    if(volt>=3550) return 3;
    if(volt>=3525) return 2;
    if(volt>=3500) return 1;
    return 0;
}

void Diagnose(void) {
    uint8_t bar=0;
    int16_t batt=0;
    setbit(MStatus, update);
    setbit(Misc,bigfont);
    clrbit(WatchBits,goback);
    PR.PRPC  &= 0b11111100;         // Enable TCC0 TCC1 clocks
    EVSYS.CH0MUX = 0b11000000;      // Event CH0 = TCC0 overflow
    TCC0.CNT = 0;
    TCC0.PER = 59999;
    TCC1.CNT = 0;
    TCC1.PER = 59999;
    TCC0.CTRLA = 3;                 // CPU clock
    TCC1.CTRLA = 0b00001000;        // Source is Event CH0
    do {
        uint8_t temp = TCF0.CNTL;
        batt = MeasureBattery(1);
        if((temp&0x03) == 0) ONWHITE();
        else if((temp&0x03) == 1) ONGRN();
        else if((temp&0x03) == 2) ONRED();
        else Sound(NOTE_B7,NOTE_B7);
        clr_display();
        lcd_goto(0,0);
        lcd_put5x8(VERSION); lcd_put5x8(PSTR(" Build: "));
        printHEX5x8(BUILD_NUMBER>>8); printHEX5x8(BUILD_NUMBER&0x00FF);
        lcd_goto(0,1); lcd_put5x8(PSTR("TimerC: "));
        lcd_goto(0,2); lcd_put5x8(PSTR("TimerF: "));
        lcd_goto(0,3); lcd_put5x8(PSTR("XMEGA rev")); putchar5x8('A'+MCU.REVID);
        lcd_goto(0,4); lcd_put5x8(PSTR("Logic: "));
        lcd_goto(0,5); lcd_put5x8(PSTR("Reset: "));
        lcd_goto(0,6); lcd_put5x8(PSTR("Battery: "));
        lcd_goto(0,7); lcd_put5x8(PSTR("Clock: "));
        lcd_goto(0,15); lcd_put5x8(PSTR("OFFSET   FAST    SLOW"));
        SoundOff();
        VPORT1.OUT = 0; // Turn off LEDs
        lcd_goto(64,1); printHEX5x8(TCC1.CNTH); printHEX5x8(TCC1.CNTL);
        lcd_goto(64,2); printHEX5x8(TCF0.CNTH); printHEX5x8(TCF0.CNTL);
        lcd_goto(64,4); printHEX5x8(VPORT2.IN);   // Shows the logic input data
        lcd_goto(64,5); printHEX5x8(RST.STATUS);    // Show reset cause
        lcd_goto(64,6); print16_5x8(batt);
        lcd_goto(64,7); printHEX5x8(CLK.CTRL); printHEX5x8(OSC.CTRL); printHEX5x8(OSC.STATUS); printHEX5x8(OSC.XOSCFAIL);
        for(uint8_t i=0; i<16; i++) {           // Print GPIO registers
            if(i<8) lcd_goto(i*16,8);
            else lcd_goto((i-8)*16,9);
            printHEX5x8(*((uint8_t *)i));
        }
        if(testbit(Misc,userinput)) {
            clrbit(Misc, userinput);
            if(testbit(Buttons,KML)) setbit(WatchBits,goback);
            if(testbit(Buttons,K1)) { CalibrateOffset(); setbit(Misc, redraw); }
            if(testbit(Buttons,K2)) CPU_Fast();
            if(testbit(Buttons,K3)) CPU_Slow();
        }
        u8CursorX=0; u8CursorY=bar;
        for(uint8_t i=0; i<128; i++) display_xor(255);
        bar++; if(bar>=16) bar=0;
        dma_display();
        SwitchBuffers();
        WaitDisplay();
        SLP();          // Sleep
    } while(!testbit(WatchBits,goback));
    TCC0.CTRLA = 0;
    TCC1.CTRLA = 0;
    PR.PRPC  |= 0b00000011;         // Disable TCC0 TCC1C clocks
    setbit(MStatus, update);
}

void About(void) {
    clr_display();
    u8CursorX=30; u8CursorY=1;
    uint16_t pointer = LOGO;
    for(uint8_t i=0; i<69; i++) {
        display_set(pgm_read_byte_near(pointer++));
    }
    lcd_goto(10,3); lcd_put5x8(PSTR("Oscilloscope Watch"));
    lcd_goto(0,13);
    lcd_put5x8(VERSION); lcd_put5x8(PSTR(" Build: "));
    printHEX5x8(BUILD_NUMBER>>8); printHEX5x8(BUILD_NUMBER&0x00FF);
    lcd_goto(0,14); lcd_put5x8(PSTR("Build Date 20")); printN5x8(BUILD_YEAR);
    putchar5x8('/'); printN5x8(BUILD_MONTH);
    putchar5x8('/'); printN5x8(BUILD_DAY);
    lcd_goto(0,15); lcd_put5x8(PSTR("Reset Cause: "));
    printHEX5x8(RST.STATUS);    // Show reset cause
    dma_display();
    WaitDisplay();
    do {
        SLP();          // Sleep
    } while(!testbit(Misc,userinput));
    clrbit(Misc, userinput);
    setbit(MStatus, update);
}
