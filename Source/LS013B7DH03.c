/* ----------------------------------------------------------------------------
	Driver for the LS013B7DH03.c Memory LCD

	Gabriel Anzziani
    www.gabotronics.com

-----------------------------------------------------------------------------*/

// LCD Display is 128x128 pixels and is physically oriented on the watch at 90
// degrees with respect to the orientation shown on the data sheet.
// Each bytes controls eight pixels
// At the end of each 16 bytes, there are 2 trailer bytes

// Text
// Line  -  -  -  -  -  -  -  -  ...  -  -  -  -  -  -  -  -
//  0    | 2286 2268 2250                 .  .  .  36 18 0 |
//  1    | 2287 2269                                  19 1 |
//  2    | 2288                                          2 |
//  3    |                                               . |
//  4    |                                               . |
//  5    |                                               . |
//  6 ===+                                                 |
//  7 ===+                                                 |
//  9 ===+                                                 |
// 10 ===+                                                 |
// 11    |                                               . |
// 12    |                                               . |
// 13    |                                               . |
// 14    | 2299                                         13 |
// 15    | 2300 2282                                 32 14 |
// 16    | 2301 2283 2265                         51 33 15 |
//       -  -  -  -  -  -  -  -  ...  -  -  -  -  -  -  -  -
// dmy     2302 2284 2266                         52 34 16
// adr     2303 2285 2267                         53 35 17

// To optimize memory access, display_data will be set to point at byte 2286
// which corresponds on the Xproto Watch to the upper left corner

// LS013B7DH03 Commands
// M0 M1 M2 DMY DMY DMY DMY DMY

// M0: H' the module enters Dynamic Mode, where pixel data will be updated.
// M0: 'L' the module remains in Static Mode, where pixel data is retained

// M1: VCOM When M1 is 'H' then VCOM = 'H' is output. If M1 is 'L' then VCOM = 'L' is output.
// When EXTMODE = 'H', M1 value = XX (don’t care)

// M2 CLEAR ALL When M2 is 'L' then all flags are cleared. When a full display clearing is required,
// set M0 and M2 = HIGH and set all display data to white.

#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <avr/pgmspace.h>
#include "main.h"
#include "mygccdef.h"

/*-----------------------------------------------------------------------------
LCD Initialization
	GLCD_LcdINIT()
-----------------------------------------------------------------------------*/
void GLCD_LcdInit(void)	{
    // Initialize USARTD0 for OLED
    USARTD0.BAUDCTRLA = FBAUD2M;	    // SPI clock rate for display, CPU is at 2MHz
    USARTD0.CTRLC     = 0xC0;   		// Master SPI mode,
    USARTD0.CTRLB     = 0x08;   		// Enable TX
    //setbit(LCD_CTRL,LCDVDD);          // Already powered in main.c
    setbit(LCD_CTRL, LCD_DISP);         // DISP ON
	LCD_PrepareBuffers();
}

// Prepare buffers for future writes
// Writing Multiple Lines to LCD:
// First Byte:  Command
// Lines:       Line Number, Data (16 bytes), Trailer
// Last Byte:   Trailer
void LCD_PrepareBuffers(void) {
    Disp_send.display_setup1[0] = DYNAMIC_MODE;  // Command
    Disp_send.display_setup1[1] = 128;           // Reversed address of line 1
    Disp_send.display_setup2[0] = DYNAMIC_MODE;  // Command
    Disp_send.display_setup2[1] = 128;           // Reversed address of line 1
    uint8_t *p1 = Disp_send.display_data1;
    uint8_t *p2 = Disp_send.display_data2;
    for(uint8_t i=0; i<128; i++) {              // Each line needs 18 bytes of data
        for(uint8_t j=0; j<17; j++) {
            #ifdef INVERT_DISPLAY
            *p1++=255;
            *p2++=255;
            #else
            *p1++=0;
            *p2++=0;
            #endif
        }
	    uint8_t r=i+2;                          // Address of line 2
	    REVERSE(r);                             // The address needs to be bit reversed
        *p1++=r;                                // Address (or Trailer of last line)
        *p2++=r;                                // Address (or Trailer of last line)
    }
}

// Transfer display buffer to LCD
// 2306 bytes to be sent:
//     Mode
//     Reversed address, Line   1 Data, Dummy
//     Reversed address, Line   2 Data, Dummy
//     ...
//     Reversed address, Line 128 Data, Dummy
//     Dummy
// Total time:
// 18.448ms in Slow mode (2306 * 1us      * 8)
// 17.295ms in Fast mode (2306 * 0.9375us * 8)
void dma_display(void) {
    setbit(LCD_CTRL, LCD_CS);			// Select
    setbit(DMA.CH2.CTRLA,6);            // reset DMA CH2
    _delay_us(6);                       // tsSCS (6uS)
    DMA.CH2.ADDRCTRL  = 0b00010000;     // Increment source, Destination fixed
    DMA.CH2.TRFCNT    = DISPLAY_DATA_SIZE+2;
    DMA.CH2.DESTADDR0 = (((uint16_t) &USARTD0.DATA)>>0*8) & 0xFF;
    DMA.CH2.DESTADDR1 = (((uint16_t) &USARTD0.DATA)>>1*8) & 0xFF;
    DMA.CH2.TRIGSRC   = DMA_CH_TRIGSRC_USARTD0_DRE_gc;
    DMA.CH2.SRCADDR0  = (((uint16_t)(Disp_send.SPI_Address))>>0*8) & 0xFF;
    DMA.CH2.SRCADDR1  = (((uint16_t)(Disp_send.SPI_Address))>>1*8) & 0xFF;
    DMA.CH2.CTRLA     = 0b10000100;     // Enable DMA, no repeat, 1 byte burst
    USARTD0.STATUS    = 0b11000000;     // Clear Interrupt Flags
    USARTD0.CTRLA     = 0b00000100;     // Enable Transmit Complete interrupt
}

// LCD Transmission complete
ISR(USARTD0_TXC_vect) {
    _delay_us(2);                       // thSCS (2uS)
    clrbit(LCD_CTRL, LCD_CS);			// DeSelect
    USARTD0.CTRLA = 0;                  // Disable USART interrupt
    _delay_us(1);                       // twSCSL (2uS)
    OFFGRN();                           // At least another 1uS spent
    OFFRED();                           // before next transmission
}

// Waits for the DMA to complete (the USART's ISR will clear LCD_CS)
void WaitDisplay(void) {
    uint16_t n=0;
    WDR();
    while(testbit(LCD_CTRL,LCD_CS)) {   // Wait for transfer complete
        _delay_us(1);
        n++;
        if(n>=20000) break;     // timeout ~ 20mS
    }
}