// OW Bugs:
// exit scope from slow sampling rates, causes a blank vertical line on the scope icon
// pressing buttons multiple times on the Diagnose screen causes a reset
// TODO & Optimizations:
/*      Custom bootloader
            - Save constants tables in bootloader
            - Calibration in User Signature Row
	    Crystal check
        Gain Calibration
        Detect low voltage with comparator
        Check if pretrigger samples completed with DMA (last part of mso.c)
        Share buffer between CH1 and CH2
        MSO Logic Analyzer more SPS, with DMA
        Force trigger, Trigger timeout in menu
        USE NVM functions from BOOT
        Sniffer IRDA, 1 Wire, MIDI
		PC control digital lines -> bus driver! SPI / I2C / UART ...
        Custom AWG backup in User Signature Row
        UART Auto baud rate
        Programmer mode
        If no sleeptime, check if menu timeout works
		USB Frame counter
        Channel math in meter mode
        Vertical zoom
        Filter mode (Audio in -> Filter -> Audio Out)
        Pulse width and Period vmeasurements
        Add CRC to serial communication
        Logic Port Invert: Select individual channels
        Protocol trigger
        Terminal mode
        FFT waterfall
        Setting profiles
        Continuous data to USB from ADC
        Independent CH1 and CH2 Frequency measurements, up to 1Mhz
        1v/octave (CV/Gate) AWG control
        RMS
        Arbitrary math expression on AWG
        DAC Calibration
	    Use DMA for USART PC transfers
        Dedicated Bode plots - Goertzel algorithm?
        Dedicated VI Curve
	    12bit with slow sampling
        Horizontal cursor on FFT
        16MSPS for logic data, 1/sinc(x) for analog
	    Show menu title */

/* Hardware resources:
    Timers:
        RTC   Second timer
        TCC0  Split timer, source is Event CH7 (40.96mS)
              TCC0L Controls the auto trigger - M.Ttimeout sets the timeout
              TCC0H Auto key repeat - Timeout is 1.024 second
              Also used as Frequency counter time keeper
              Watch: 100Hz Stopwatch timer
        TCC1  Counts post trigger samples
              UART sniffer time base
              Watch: 1 minute Stopwatch timer
        TCD0  Split timer, source is Event CH6 (1.024ms)
            TCD0L 40.96mS period - 24.4140625 Hz - Source for Event CH7
	        TCD0H Controls LCD refresh rate
	    TCD1  Watch: Sounds duration
              Scope: Overflow used for AWG
        TCE0  Watch: Sounds frequencies
              Scope: Frequency counter low 16bits
        TCE1  Controls Interrupt ADC (srate >= 11), srate: 6, 7, 8, 9, 10
              Fixed value for slow sampling
              Frequency counter high 16bits
        TCF0  Global: Seconds counter, source is Event CH4 (every 1 sec)
    Events:
	    CH0 TCE1 overflow used for ADC
	    CH1 ADCA CH0 conversion complete
        CH2 Input pin for frequency measuring
        CH3 TCD1 overflow used for DAC
        CH4 RTC overflow -> every 1 sec. Used for Time and freq. measuring
        CH5 TCE0 overflow used for freq. measuring
        CH6 Scope: CLKPER / 32768 -> 976.5625 - 1.024ms
            Watch: CLKPER / 8192 -> 244.140625Hz - 4.096ms
        CH7 TCD0L underflow: 40.96mS period - 24.4140625 Hz
	DMAs:
	    CH0 ADC CH0  / SPI Sniffer MOSI / UART Sniffer
	    CH1 ADC CH1  / SPI Sniffer MISO
	    CH2 Port CHD / Display
	    CH3 AWG DAC
    USART:
	    USARTD0 for Display
	    USARTC0 for Sniffer
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
        USB BUSEVENT    Medium      USB Bus Event
        USB_TRNCOMPL    Medium      USB Transaction Complete
        PORTA INT0:     Medium      keys
        TCC0L:          Low         auto trigger
        TCC0H:          Low         auto keys
        RTC:            Low         sleep timeout, menu timeout
        TCD0H:          Low         Watch mode Auto Repeat Key
*/
#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>
#include <avr/fuse.h>
#include "main.h"
#include "mso.h"
#include "logic.h"
#include "awg.h"
#include "interface.h"
#include "USB\usb_xmega.h"
#include "USB\Descriptors.h"
#include "time.h"
#include "utils.h"
#include "config.h"
#include "moon.h"

FUSES = {
	.FUSEBYTE0 = 0xFF,  // JTAG not used, ***NEEDS*** to be off
	.FUSEBYTE1 = 0x00,  // Watchdog Configuration
	.FUSEBYTE2 = 0xBF,  // Reset Configuration, BOD off during power down
	.FUSEBYTE4 = 0xF7,  // 4ms Start-up Configuration
	.FUSEBYTE5 = 0xDB,  // No EESAVE on chip erase, BOD sampled when active, 2.4V BO level
};

// Disable writing to the bootloader section
LOCKBITS = (0xBF);

//static inline void Restore(void);

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
void CalibrateGain(void);
void AnalogOn(void);
void LowPower(void);

int main(void) {
    PR.PRGEN = 0b01011000;          // x USB x AES EBI RTC EVSYS DMA
    PR.PRPA  = 0b00000111;          // x x x x x DAC ADC AC
    PR.PRPB  = 0b00000111;          // x x x x x DAC ADC AC
    PR.PRPC  = 0b11111111;          // x TWI USART1 USART0 SPI HIRES TC1 TC0
    PR.PRPD  = 0b11101111;          // x TWI USART1 USART0 SPI HIRES TC1 TC0
    PR.PRPE  = 0b11111111;          // x TWI USART1 USART0 SPI HIRES TC1 TC0
    PR.PRPF  = 0b11111110;          // x TWI USART1 USART0 SPI HIRES TC1 TC0
    NVM.CTRLB = 0b00001100;         // EEPROM Mapping, Turn off bootloader flash
    // PORTS CONFIGURATION
    PORTA.DIR       = 0b00101110;   // 1.024V, BATTSENSE, CH2-AC, CH1, x, CH1-AC, LOGICPOW, 2.048V
    PORTA.PIN4CTRL  = 0x07;         // Digital Input Disable on pin PA4
    PORTA.PIN6CTRL  = 0x07;         // Digital Input Disable on pin PA6
    PORTA.PIN7CTRL  = 0x07;         // Digital Input Disable on pin PA7
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
    TCF0.PER = 43199;               // 43200 seconds = 12 hours
    TCF0.CTRLA = 0x0C;              // Source is Event CH4
    TCF0.INTCTRLA = 0x01;           // 12 hour interrupt, low level interrupt
    eeprom_read_block(NOW, &EE_saved_time, sizeof(Type_Time));  // Load latest known time
    WSettings = eeprom_read_byte(&EE_WSettings);                // Load Watch settings
    SetTimeTimer();                 // Validate time variables, update TCF0, enable interrupts
    PMIC.CTRL = 0x07;               // Enable High, Medium and Low level interrupts

    Randomize(TCF0.CNT);            // Randomize random number generator with current time
    About();                        // Go to About on startup
    RST.STATUS = 0x3F;              // Clear Reset flags
    clrbit(Misc, userinput);
    Watch();
    uint8_t old_menu=1;
    for(;;) {
        LowPower();                 // Analog off, Slow CPU
        if(testbit(Misc, userinput)) {
            clrbit(Misc, userinput);
            if(testbit(Buttons,KUR) || testbit(Buttons,KBR)) Menu++;
            if(testbit(Buttons,KUL) || testbit(Buttons,KBL)) Menu--;
            switch(Menu) {
                case 1:     // Watch Menu
                    if(testbit(Buttons,K1)) {
                        Watch();
                        old_menu=1;
                    }                        
                    if(testbit(Buttons,K2)) Calendar();
                    if(testbit(Buttons,K3)) Moon();
                break;
                case 2:     // Oscilloscope Menu
                    if(testbit(Buttons,K1)) {   // Profile
                        Profiles();
                    }
                    if(testbit(Buttons,K2)) {   // Show Screenshot
                        ShowScreenshot();
                    }
                    if(testbit(Buttons,K3)) {   // Start Oscilloscope
                        AnalogOn();
                        USB_ResetInterface();                   // Initialize USB
                        eeprom_write_block(NOW, &EE_saved_time, sizeof(Type_Time)); // Save date and time
                        RTC.INTCTRL = 0x00;                     // Disable RTC interrupts (TCD0 will handle EXTCOMM)
                        TCF0.INTCTRLB = 0x00;                   // Disable 1 minute interrupt (Time and date updated when MSO ends)
                        // During the Scope mode, TCD0 will generate the signal for the Memory LCD EXTCOMM
                        TCD0.INTCTRLA = 0;                      // Disable TCD0 interrupts (Watch mode Auto Key interrupt)
                        TCD0.CTRLB = 0b00010000;                // Enable HCMPENA, pin4
                        TCD0.CCAH = 128;                        // Automatic EXTCOMM with Timer D0, 3.814697265625 Hz
                        uint16_t tempCNTF = TCF0.CNT;           // Save timer F
                        MSO();                                  // go to MSO
                        USB.CTRLB = 0;                          // USB Disattach
                        USB.ADDR = 0;
                        USB.CTRLA = 0;
                        OSC.CTRL &= ~OSC_RC32MEN_bm;
                        eeprom_read_block(NOW, &EE_saved_time, sizeof(Type_Time));   // Load latest known date and time
                        if(NowHour>=12) {
                            SET();                              // Set T bit
                            if(tempCNTF>TCF0.CNT) {             // Midnight passed while in MSO
                                CLT();                          // It should be morning
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
                        }
                        GetTimeTimer();                         // Sync variables from TCF0
                        WatchBits = 0;
                        MStatus = 0;
                        WSettings = eeprom_read_byte(&EE_WSettings);    // Load Watch settings
                        TCD0.CTRLB = 0;
                        TCD0.CCAH = 0;
                        ADCA.EVCTRL = 0;
                        ADCB.EVCTRL = 0;
                        setbit(DMA.CH0.CTRLA,6);    // reset DMA CH0
                        setbit(DMA.CH2.CTRLA,6);    // reset DMA CH2
                        setbit(DMA.CH1.CTRLA,6);    // reset DMA CH1
                        SetMinuteInterrupt();       // Set minute interrupt
                        RTC.INTCTRL = 0x05;         // Generate low level interrupts (Overflow and Compare)
                        setbit(MStatus, update);
                    }
                    Menu=2;
                break;
                case 3:     // Games Menu
                    if(testbit(Buttons,K1)) Snake();
                    if(testbit(Buttons,K2)) Pong();
                    if(testbit(Buttons,K3)) { CPU_Fast(); Chess(); }
                    Menu=3;
                break;
                case 4:     // Settings Menu
                    if(testbit(Buttons,K1)) OWSettings();
                    if(testbit(Buttons,K2)) Diagnose();
                    if(testbit(Buttons,K3)) About();
                    Menu=4;
                break;
            }
            SecTimeout = 120;
            setbit(MStatus, update);
        }
        if(testbit(MStatus, update)) {
            CPU_Fast();
            Sound(TuneBeep);
            int8_t n, step, from;
            if(Menu==old_menu) {        // No animation, just print the bmp
                n = 117;
                step = 1;
                from=-101;
            } else if(Menu>old_menu) {  // Slide left
                n = step=-15;
                from=133;
            } else {                    // Slide right
                n = step=15;
                from=-101;
            }
            for(; n<118 && n>-118; n+=step) {   // Slide animation
                if(testbit(Misc, userinput)) {  // Button pressed during animation
                    clrbit(Misc, userinput);
                    Sound(TuneBeep);
                    old_menu=Menu;
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
                SwitchBuffers();
                clr_display();
                bitmap_safe(from+n,4,(uint8_t *)pgm_read_word(BMPs+Menu),PIXEL_SET);
                bitmap_safe(16+n,4,(uint8_t *)pgm_read_word(BMPs+old_menu),PIXEL_SET);
                WaitDisplay();
                dma_display();
            }
            old_menu=Menu;
            lcd_goto(28,2); print5x8(STRS_mainmenu[Menu-1]);
            lcd_goto(1,15); print5x8(STRS_optionmenu[Menu-1]);
            WaitDisplay();  // Finish previous transmission
            dma_display();
            clrbit(MStatus, update);
        }
        if(SecTimeout) {    // Stay in main menu...
            WaitDisplay();  // Finish sending display data
            if(TCD1.CTRLA==0) {  // Not playing sounds
                SecTimeout--;
                SLP();          // Sleep
            }
        } else {            // Timeout expired
            Watch();        // Go to watch mode
        }
    }        
    return 0;
}

// Tactile Switches - This is configured as medium level interrupt
ISR(PORTF_INT0_vect) {
    uint8_t in,j=0;
    // Debounce: need to read 10 consecutive equal numbers
    OFFGRN();   // Avoid having the LED on during this interrupt
    OFFRED();
    for(uint8_t i=16; i>0; i--) {
        delay_ms(1);
		in = PORTF.IN;              // Read port
		if(j!=in) { j=in; i=10; }   // Value changed
	}
    Buttons = in;
    if(testbit(Buttons,KUL) && testbit(Buttons, KUR) && testbit(Buttons,KML)) { // Enter bootloader
        eeprom_write_block(NOW, &EE_saved_time, sizeof(Type_Time));             // Save current time
        ProcessCommand(0xBB, 0);  // Enter bootloader
    }
    if(Buttons) {
        setbit(MStatus, update);         // Valid key
        setbit(Misc, userinput);
        if(PR.PRPA==0x00) {  // Scope mode (Analog peripherals on)
            // TCC0H used for auto repeat key
            if(TCC0.CTRLA == 0x0F) {                        // Not doing a frequency count
                TCC0.CNTH = 20;                             // Restart timer - 819.2mS timeout
                setbit(TCC0.INTFLAGS, TC2_HUNFIF_bp);       // Clear trigger timeout interrupt
                TCC0.INTCTRLA |= TC2_HUNFINTLVL_LO_gc;      // Enable Auto Key interrupt
            }
        } else {            // Watch mode
            PR.PRPD &= 0b11111110;          // Enable TCD0 clock
            TCD0.CTRLA = 0;                 // Stop timer prior to reset
            TCD0.CTRLFSET = 0x0F;           // Reset timer
            TCD0.PER = 244;                 // 1003.52ms Timeout
            TCD0.INTCTRLA = 0x01;           // Enable Auto Key interrupt
            TCD0.CTRLA = 0b00001110;        // Clock select is event Event CH6 (4.096mS per count)
            
        }            
    } else {
        // No keys pressed -> disable auto repeat key interrupt
        if(PR.PRPA==0x00) {  // Scope mode (Analog peripherals on)
            TCC0.INTCTRLA &= ~TC2_HUNFINTLVL_LO_gc; // Disable Auto Key interrupt
        } else {            // Watch mode
            TCD0.CTRLA = 0;            // Stop timer
            TCD0.INTCTRLA = 0;         // Disable Auto Key interrupt
            PR.PRPD |= 0b00000001;     // Disable TCD0 clock            
        }
        clrbit(Misc,keyrep);
        clrbit(VPORT1.OUT, LEDWHITE);   // Turn off backlight
    }        
}

// Set auto repeat key flag - Scope mode
ISR(TCC2_HUNF_vect) {
    TCC0.INTCTRLA &= ~TC2_HUNFINTLVL_LO_gc;         // Disable Auto Key interrupt
    if(Buttons) setbit(Misc,keyrep);
}

// Set auto repeat key flag - Watch mode
ISR(TCD0_OVF_vect) {
    TCD0.CTRLA = 0;            // Stop timer
    TCD0.INTCTRLA = 0;         // Disable Auto Key interrupt
    PR.PRPD |= 0b00000001;     // Disable TCD0 clock
    if(Buttons) setbit(Misc,keyrep);
}
    
// Read out calibration byte.
uint8_t ReadCalibrationByte(uint8_t location) {
    uint8_t result;
    // Load the NVM Command register to read the calibration row.
    NVM_CMD = NVM_CMD_READ_CALIB_ROW_gc;
    result = pgm_read_byte(location);
    // Clean up NVM Command register.
    NVM_CMD = NVM_CMD_NO_OPERATION_gc;
    return result;
}

void CPU_Fast(void) {
    // Clock Settings
    OSC.XOSCCTRL = 0xCB;    // Crystal type 0.4-16 MHz XTAL - 16K CLK Start Up time
    OSC.CTRL |= OSC_RC2MEN_bm | OSC_XOSCEN_bm;          // 1. Enable reference clock source
    OSC.PLLCTRL = 0xC2;     // 2. Set the multiplication factor and select the clock reference for the PLL - XOSC is PLL Source - 2x Factor (32MHz)
    uint8_t n=0;
    do {                    // 3. Wait until the clock reference source is stable.
        if(testbit(OSC.STATUS,OSC_XOSCRDY_bp)) break;   // Check XT ready flag
        _delay_us(50);
    } while(++n);
    
    // Switch to internal 2MHz if crystal fails
    if(!testbit(OSC.STATUS, OSC_XOSCRDY_bp)) {   // Check XT ready flag
        OSC.XOSCCTRL = 0x00;    // Disable external oscillators
        // Not entering, comment to save
        //OSC.PLLCTRL = 0x10;     // 2MHz is PLL Source - 16x Factor (32MHz)
    }
    OSC.CTRL = OSC_RC2MEN_bm | OSC_PLLEN_bm | OSC_XOSCEN_bm;    // 4. Enable the PLL.
    do {
        if(testbit(OSC.STATUS,OSC_PLLRDY_bp)) break;   // Check PLL ready flag
        _delay_us(50);
    } while(++n);
    CCPWrite(&CLK.CTRL, CLK_SCLKSEL_PLL_gc);    // Switch to PLL clock
    // Clock OK!
    OSC.CTRL = OSC_PLLEN_bm | OSC_XOSCEN_bm;    // Disable internal 2MHz
    if(CLK.CTRL & CLK_SCLKSEL_RC32M_gc) {   // Clock error?
        tiny_printp(0,7,PSTR("NO XT"));
    }
    USARTD0.BAUDCTRLA = FBAUD32M;	    // SPI clock rate for display, CPU is at 32MHz    
    NVM.CTRLB = 0b00001100;             // EEPROM Mapping, Turn off bootloader flash, Turn on EEPROM
    EVSYS.CH6MUX    = 0x8F;             // Event CH6 = CLKPER / 32768
}

void CPU_Slow(void) {
    OSC.CTRL |= OSC_RC2MEN_bm;          // Enable internal 2MHz
    uint8_t n=0;
    do {                                // Wait until 2MHz is stable
        if(testbit(OSC.STATUS, OSC_RC2MRDY_bp)) break;   // Check RC 2MHz ready flag
        _delay_us(50);
    } while(++n);
    CCPWrite(&CLK.CTRL, CLK_SCLKSEL_RC2M_gc);    // Switch to 2MHz clock
    do {
        OSC.XOSCCTRL = 0x00;                // Disable external oscillators
        OSC.PLLCTRL = 0;
        _delay_us(50);
        OSC.CTRL = OSC_RC2MEN_bm;
    } while(OSC.CTRL!=OSC_RC2MEN_bm);
    USARTD0.BAUDCTRLA = FBAUD2M;	    // SPI clock rate for display, CPU is at 2MHz
    NVM.CTRLB = 0b00001110;             // EEPROM Mapping, Turn off bootloader flash, Turn off EEPROM
    EVSYS.CH6MUX    = 0x8D;             // Event CH6 = CLKPER / 8192 (244.140625Hz / 4.096ms)
}

void AnalogOn(void) {
    ANALOG_ON();
    LOGIC_ON();
    // Power reduction: Stop unused peripherals - Set bit to stop peripheral
    PR.PRGEN = 0b00011000;  // x USB x AES EBI RTC EVSYS DMA
    PR.PRPA  = 0x00;        // x x x x x DAC ADC AC
    PR.PRPB  = 0x01;        // x x x x x DAC ADC AC
    PR.PRPC  = 0b01111100;  // x TWI USART1 USART0 SPI HIRES TC1 TC0
    PR.PRPD  = 0b01101100;  // x TWI USART1 USART0 SPI HIRES TC1 TC0
    PR.PRPE  = 0b01111100;  // x TWI USART1 USART0 SPI HIRES TC1 TC0
    CPU_Fast();
}

// Analog off, Slow CPU
void LowPower(void) {
    GLCD_LcdInit();                 // Initialize LCD
    CCPWrite(&WDT.CTRL, WDT_CEN_bm); // Watchdog off
    SLEEP.CTRL = SLEEP_SMODE_PSAVE_gc | SLEEP_SEN_bm;
    ANALOG_OFF();
    LOGIC_OFF();
    TCC0.CTRLA      = 0;            // Stop timer prior to reset
    TCC0.CTRLFSET   = 0x0F;         // Reset timer
    TCC1.CTRLA      = 0;            // Stop timer prior to reset
    TCC1.CTRLFSET   = 0x0F;         // Reset timer
    TCD0.CTRLA      = 0;            // Stop timer prior to reset
    TCD0.CTRLFSET   = 0x0F;         // Reset timer
    TCD1.CTRLA      = 0;            // Stop timer prior to reset
    TCD1.CTRLFSET   = 0x0F;         // Reset timer
    TCE0.CTRLA = 0;                 // Stop timer prior to reset
    TCE0.CTRLFSET   = 0x0F;         // Reset timer
    TCE1.CTRLA      = 0;            // Stop timer prior to reset
    TCE1.CTRLFSET   = 0x0F;         // Reset timer
    DMA.CTRL        = 0x00;         // Disable DMA
    DMA.CTRL        = 0x40;         // Reset DMA
    ADCA.CTRLA      = 0x00;         // Disable ADC
    ADCB.CTRLA      = 0x00;         // Disable ADC
    // Event System
    EVSYS.CH0MUX    = 0;            // Disable TCE1 overflow used for ADC event
    EVSYS.CH1MUX    = 0;            // Disable ADCA CH0 conversion complete event
    EVSYS.CH2MUX    = 0;            // Disable Input pin for frequency measuring event
    EVSYS.CH3MUX    = 0;            // Disable TCD1 overflow used for DAC event
    EVSYS.CH4MUX    = 0x08;         // Event CH4 = RTC overflow (every second)
    EVSYS.CH5MUX    = 0;            // Disable TCE0 overflow used for freq. measuring event
    EVSYS.CH7MUX    = 0;            // Disable TCD0L underflow: 40.96mS period - 24.4140625 Hz event
    // POWER REDUCTION: Stop unused peripherals - Stop everything but RTC and DMA
    PR.PRGEN = 0b01011000;          // Stop: USB, AES, EBI, only EVSYS, RTC and DMA on
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

// Calibrate offset, inputs must be connected to ground
void CalibrateOffset(void) {
    Buttons=0;
    clr_display();
    print5x8(PSTR("DISCONNECT CH1,CH2"));
    lcd_goto(108,15); print5x8(PSTR("GO"));
    dma_display(); WaitDisplay();
    while(!Buttons);
    AnalogOn();
    if(testbit(Buttons,K3)) {
        uint8_t i,s=0;
        int16_t avrg1, avrg2;
        clr_display();
        TCF0.INTCTRLB = 0x00;               // Disable 1 minute interrupt to prevent writing GPIO0
	    for(Srate=0; Srate<8; Srate++) {	// Cycle thru first 8 SamplingRates
            i=6; do {                       // Cycle thru all the gains
                int8_t  *q1, *q2;  // temp pointers to signed 8 bits
                s++;
                M.CH1gain=i;
                M.CH2gain=i;
                SimpleADC();
                q1=T.IN.CH1;
                q2=T.IN.CH2;
                // Calculate offset for CH1
                avrg1=0;
                avrg2=0;
                uint8_t j=0;
                do {
            	    avrg1+= (*q1++);
            	    avrg2+= (*q2++);
                } while(++j);
                int8_t avrg8=avrg1>>8;
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
        SetMinuteInterrupt();       // Set minute interrupt
    }
    Buttons=0;
    LowPower();     // Analog off, Slow CPU
}

// Calibrate gain, inputs must be connected to 4.000V
/*void CalibrateGain(void) {
    #ifndef NODISPLAY
        Buttons=0;
        clr_display();
        print3x6(PSTR("NOW CONNECT 4.000V"));
        tiny_printp(116,7,PSTR("GO"));
        dma_display();
        while(!Buttons);
    #else
        setbit(Buttons,K3);
    #endif
    if(testbit(Buttons,K3)) {
        int16_t offset;
        int32_t avrg1=0, avrg2=0;
        clr_display();
        // Calculate offset for Meter in VDC
        ADCA.CTRLB = 0x90;          // signed mode, no free run, 12 bit right adjusted
        ADCA.PRESCALER = 0x07;      // Prescaler 512 (500kHZ ADC clock)
        ADCB.CTRLB = 0x90;          // signed mode, no free run, 12 bit right adjusted
        ADCB.PRESCALER = 0x07;      // Prescaler 512 (500kHZ ADC clock)
        uint8_t i=0;
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
}*/

// Fill up channel data buffers
void SimpleADC(void) {
	Apply();
	delay_ms(64);
    StartDMAs();
	delay_ms(16);
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

// Delay in mili seconds, take into account current CPU speed
void delay_ms(uint8_t n) {
    WDR();  // Clear watchdog
    while(n--) {
        if(CLK.CTRL==0) {   // CPU is running at 2MHz
            _delay_us(62);
        }
        else {              // CPU is running at 32MHz
            _delay_us(999);
        }
    }
}

// Delay in mili seconds, take into account current CPU speed
// If there is user input, exit
void wait_ms(uint8_t n) {
    WDR();  // Clear watchdog
    while(n--) {
        if(CLK.CTRL==0) {   // CPU is running at 2MHz
            _delay_us(62);
        }
        else {              // CPU is running at 32MHz
            _delay_us(999);
        }
        if(testbit(Misc,userinput)) return;
    }
}

const uint16_t Batt_Levels[12] PROGMEM = {
    3400, 3500, 3570, 3640, 3710, 3780, 3850, 3920, 3990, 4060, 4130, 4200
};

// Measure Vin with 40kOhm load, assuming that VCC is 3V
// scale == 0, return value between 1 and 44, or 0 if charging
// scale != 0, return ADC result
int16_t MeasureVin(uint8_t scale) {
    BATT_TEST_ON();                 // Connect 40kOhm load
    PR.PRPA  &= 0b11111101;         // Enable ADCA module
    ADCA.CTRLA   = 0x01;            // Enable ADC
    ADCA.CTRLB   = 0x70;            // Limit ADC current, signed mode, no free run, 12 bit right
    ADCA.REFCTRL = 0x40;            // REF = VCC/2 (1.5V)
	ADCA.CH0.MUXCTRL = 0b00110100;  // Channel 0 input: ADC6 pin - INTGND
    ADCA.PRESCALER = 0x00;          // DIV4 (CPU is running at 2MHz)
    int16_t adc_read=0;
    for(uint8_t i=0; i<16; i++) {   // Add 16 measurements
        ADCA.CH0.CTRL = 0x9F;       // Start conversion, Differential with gain (0.5x)
        while(ADCA.INTFLAGS==0);
        ADCA.INTFLAGS = 0x01;       // Clear interrupt flag
        adc_read += (int16_t)ADCA.CH0RES;
    }
    BATT_TEST_OFF();
	// Vin = 2*V(ADC6)
	// RES*Vref/2048 = V(AD6)/2 = Vin/4     // (The gain is 0.5)
	// RES*4*1500/2048 = Vin
	// Vin = (6000*RES)/2048 = (375*16*RES) / 2048
	int16_t volt=((int32_t)(adc_read)*375)>>11;
    ADCA.REFCTRL = 0x00;            // Bandgap off    
    PR.PRPA  |= 0b00000010;         // Disable ADCA
    if(scale) return volt;
    if(volt>4350) return 0;
    if(volt>=4199) return 45;
    for(uint8_t i=0; i<12; i++) {
		uint16_t Level1 = (uint16_t)pgm_read_word_near(Batt_Levels+i);
        if(volt<Level1) {        // Check in which range is the voltage
            if(i==0) return 1;
			uint16_t Level2 = (uint16_t)pgm_read_word_near(Batt_Levels+i-1);
            // Linear interpolation
            uint8_t j = 0;
            uint8_t v = volt - Level2;
            uint8_t delta = (Level1 - Level2)>>2;
            while(v > delta) {
                v-=delta;
                j++;
            }
            return i*4+j-3;
        }
    }
    return 0;		                // Charging, Voltage >= 4300
}

// Measure External 2.048V reference assuming VCC/2 is 1.5V
int16_t MeasureVRef(void) {
	PR.PRPA  &= 0b11111101;         // Enable ADCA module
	ADCA.CTRLA   = 0x01;            // Enable ADC
	ADCA.CTRLB   = 0x70;            // Limit ADC current, signed mode, no free run, 12 bit right
	ADCA.REFCTRL = 0x40;            // REF = VCC/2 (1.5V)
    ADCA.INTFLAGS = 0xFF;           // Clear all interrupt flag
	ADCA.CH0.MUXCTRL   = 0x38;      // Channel 0 input: ADC7 pin  (1.024V)
	if(CLK.CTRL==0) {               // CPU is running at 2MHz
		ADCA.PRESCALER = 0x00;      // DIV4
	}
	else {                          // CPU is running at 32MHz
		ADCA.PRESCALER = 0x04;      // DIV64
	}
    int16_t adc_read=0;
    for(uint8_t i=0; i<16; i++) {   // Add 16 measurements
	    ADCA.CH0.CTRL = 0x81;       // Start conversion, Single-ended positive input signal
	    while(ADCA.INTFLAGS==0);
	    ADCA.INTFLAGS = 0x01;       // Clear interrupt flag
        adc_read += (int16_t)ADCA.CH0RES;
        Randomize(ADCA.CH0RES);     // Also use the ADC measurement to further randomize the seed
        delay_ms(1);
    }        
	// VRef = 2*V(ADC7)
	// RES*Vref/2048 = V(ADC7) = VRef/2
	// RES*2*1500/2048 = VRef
	// Vin = (375 * RES) / 256 = (375 * 16*RES) / 4096
	int16_t volt=((int32_t)(adc_read)*375) >> 12;
	ADCA.REFCTRL = 0x00;            // Bandgap off
	PR.PRPA  |= 0b00000010;         // Disable ADCA
	return volt;
}

// Measure VCC (3V) assuming ADC7 is 1.024V
int16_t MeasureVCC(void) {
    PR.PRPA  &= 0b11111101;         // Enable ADCA module
    ADCA.CTRLA   = 0x01;            // Enable ADC
    ADCA.CTRLB   = 0x10;            // No limit ADC current, signed mode, no free run, 12 bit right
    ADCA.REFCTRL = 0x40;            // REF = VCC/2 (1.5V)
    ADCA.CH0.MUXCTRL    = 0x38;     // Channel 0 input: ADC7 pin (1.024V)
	delay_ms(1);
    ADCA.INTFLAGS = 0xFF;           // Clear all interrupt flag
	if(CLK.CTRL==0) {               // CPU is running at 2MHz
		ADCA.PRESCALER = 0x02;      // DIV16
	}
	else {                          // CPU is running at 32MHz
		ADCA.PRESCALER = 0x07;      // DIV512
	}
    int16_t adc_read=0;
    for(uint8_t i=0; i<16; i++) {   // Add 16 measurements
        ADCA.CH0.CTRL = 0x81;       // Start conversion, Single-ended positive input signal
    	while(ADCA.INTFLAGS==0);
    	ADCA.INTFLAGS = 0x01;       // Clear interrupt flag
        adc_read += (int16_t)ADCA.CH0RES;
        delay_ms(1);
    }
	// RES*Vref/2048 = V(ADC7)
	// RES*(VCC/2)/2048 = 1024
    // VCC = 1024*2048*2/RES = 16*1024*2048*2/16*RES
	int16_t volt=67108864/(int32_t)adc_read;
	ADCA.REFCTRL = 0x00;            // Bandgap off
	PR.PRPA  |= 0b00000010;         // Disable ADCA
	return volt;
}
