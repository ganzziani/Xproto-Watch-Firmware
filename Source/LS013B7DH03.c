/* ----------------------------------------------------------------------------
	Driver for the LS013B7DH03.c Memory LCD

	Gabriel Anzziani
    www.gabotronics.com

-----------------------------------------------------------------------------*/

// LCD Display is 128x128 pixels and is physically oriented at 90 degrees with
// respect to the orientation shown on the data sheet.
// Each bytes controls eight pixels
// At the end of each 16 byte, there are 2 trailer bytes

// Text
// Line  - -  -  -  -  -  -  -  -  ...  -  -  -  -  -  -  -
//  0    | 2286 2268                      .  .  .  36 18 0 |
//  1    | 2287                                       19 1 |
//  2    |                                               2 |
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
// 14    |                                              13 |
// 15    | 2300                                      32 14 |
// 16    | 2301 2283                     .  .  .  51 33 15 |
         -  -  -  -  -  -  -  -  -  ...  -  -  -  -  -  -  -

// To optimize memory access, Disp_send.buffer will be set to point at byte 2286
// which corresponds on the Xproto Watch to the upper left corner

/******************************************************************************
/                       DECLARATIONS / DEFINITIONS                            /
/ ****************************************************************************/

#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <avr/pgmspace.h>
#include "display.h"
#include "mygccdef.h"
#include "hardware.h"
#include "main.h"

/* Local functions prototypes */
void LcdDataWrite (unsigned char);
void LcdWaitBusy (void);

/******************************************************************************
/                               PROGRAM CODE                                  /
/ ****************************************************************************/

/*-----------------------------------------------------------------------------
LCD Initialization
	GLCD_LcdINIT()
-----------------------------------------------------------------------------*/
void GLCD_LcdInit(void)	{
    // Initialize USARTD0 for OLED
    USARTD0.BAUDCTRLA = FBAUD2M;	    // SPI clock rate for display, CPU is at 2MHz
    USARTD0.CTRLC     = 0xC0;   		// Master SPI mode,
    USARTD0.CTRLB     = 0x08;   		// Enable TX
    // Recommended power up sequence
    //setbit(LCD_CTRL,LCDVDD);            // Power for display
    //_delay_ms(1);
    //clrbit(LCD_CTRL, LCD_DISP);         // DISP OFF
    setbit(LCD_CTRL, LCD_DISP);         // DISP ON
	LCD_PrepareBuffers();
}

// Prepare buffers for future writes
// Writing Multiple Lines to LCD:
// First Byte:  Command
// Lines:       Line Number, Data (16 bytes), Trailer
// Last Byte:   Trailer
void LCD_PrepareBuffers(void) {
    Disp_send.display_setup[0] = DYNAMIC_MODE;  // Command
    Disp_send.display_setup[1] = 128;           // Address of line 1 ('1' bit reversed)
    T.TIME.display_setup2[0] = DYNAMIC_MODE;  // Command
    T.TIME.display_setup2[1] = 128;           // Address of line 1 ('1' bit reversed)
    for(uint8_t i=0; i<128; i++) {
	    uint8_t r=i+2;                          // Address of line 2
	    REVERSE(r);                             // The address needs to be bit reversed
	    // Each line needs 18 bytes of data, prepare the first two bytes:
	    Disp_send.display_data[16+i*18] = 0;    // Trailer
	    Disp_send.display_data[17+i*18] = r;    // Address (or Trailer of last line)
	    T.TIME.buffer2[16+i*18] = 0;    // Trailer
	    T.TIME.buffer2[17+i*18] = r;    // Address (or Trailer of last line)
	    T.TIME.buffer3[16+i*18] = 0;    // Trailer
	    T.TIME.buffer3[17+i*18] = r;    // Address (or Trailer of last line)
    }
    Disp_send.display_data[DISPLAY_DATA_SIZE-1] = STATIC_MODE;
    T.TIME.buffer2[DISPLAY_DATA_SIZE-1] = STATIC_MODE;
    T.TIME.buffer3[DISPLAY_DATA_SIZE-1] = STATIC_MODE;
}

void GLCD_LcdOff(void)	{
	cli();  // disable interrupts
    LcdInstructionWrite(CLEAR_ALL);     // Clear Screen
    clrbit(LCD_CTRL, LCD_DISP);         // DISP OFF
    TCD0.CTRLB = 0b00000000;            // Disable HCMPENA, pin4
    _delay_us(30);                      // TCOM
	sei();
    clrbit(VPORT3.OUT, 2);              // Power down board
}

/*-------------------------------------------------------------------------------
Send instruction to the LCD
	LcdInstructionWrite (uint8_t u8Instruction)
		u8Instruction = Instruction to send to the LCDCHSIZE 2 2469
-------------------------------------------------------------------------------*/
void LcdInstructionWrite (uint8_t u8Instruction) {
    setbit(LCD_CTRL, LCD_CS);			// Select
    _delay_us(6);                       // tsSCS
    USARTD0.DATA= u8Instruction;        // Send command
    while(!testbit(USARTD0.STATUS,6));  // Wait until transmit done
    setbit(USARTD0.STATUS,6);
    USARTD0.DATA = 0;                   // Trailer (dummy data)
    while(!testbit(USARTD0.STATUS,6));  // Wait until transmit done
    setbit(USARTD0.STATUS,6);    
    _delay_us(2);                       // thSCS
    clrbit(LCD_CTRL, LCD_CS);			// Select
}

// Transfer display buffer to LCD
// 2306 bytes will be sent in 18.448ms
void dma_display(void) {
    setbit(LCD_CTRL, LCD_CS);			// Select
    setbit(DMA.CH2.CTRLA,6);            // reset DMA CH2
    _delay_us(6);                       // tsSCS
    DMA.CH2.ADDRCTRL  = 0b00010000;     // Increment source, Destination fixed
    DMA.CH2.TRFCNT    = DISPLAY_DATA_SIZE+2;
    DMA.CH2.DESTADDR0 = (((uint16_t) &USARTD0.DATA)>>0*8) & 0xFF;
    DMA.CH2.DESTADDR1 = (((uint16_t) &USARTD0.DATA)>>1*8) & 0xFF;
    DMA.CH2.TRIGSRC   = DMA_CH_TRIGSRC_USARTD0_DRE_gc;
    DMA.CH2.SRCADDR0  = (((uint16_t)(Disp_send.spidata))>>0*8) & 0xFF;
    DMA.CH2.SRCADDR1  = (((uint16_t)(Disp_send.spidata))>>1*8) & 0xFF;
    DMA.CH2.CTRLB     = 0b00010001;     // Low priority interrupt on complete
    DMA.CH2.CTRLA     = 0b10000100;     // no repeat, 1 byte burst
}

// Partial transfer display buffer to LCD
// n*18+1 bytes will be sent in 18.448ms
void dma_displayn(uint8_t n) {
    setbit(LCD_CTRL, LCD_CS);			// Select
    setbit(DMA.CH2.CTRLA,6);            // reset DMA CH2
    _delay_us(6);                       // tsSCS
//    setbit(USARTD0.STATUS,6);
    USARTD0.DATA= DYNAMIC_MODE;        // Send command

    DMA.CH2.ADDRCTRL  = 0b00010000;     // Increment source, Destination fixed
    DMA.CH2.TRFCNT    = (uint16_t)n*18+1;
    DMA.CH2.DESTADDR0 = (((uint16_t) &USARTD0.DATA)>>0*8) & 0xFF;
    DMA.CH2.DESTADDR1 = (((uint16_t) &USARTD0.DATA)>>1*8) & 0xFF;
    DMA.CH2.TRIGSRC   = DMA_CH_TRIGSRC_USARTD0_DRE_gc;
    DMA.CH2.SRCADDR0  = (((uint16_t)(Disp_send.spidata))>>0*8) & 0xFF;
    DMA.CH2.SRCADDR1  = (((uint16_t)(Disp_send.spidata))>>1*8) & 0xFF;
    DMA.CH2.CTRLB     = 0b00010001;     // Low priority interrupt on complete
    while(!testbit(USARTD0.STATUS,6));  // Wait until transmit done
    DMA.CH2.CTRLA     = 0b10000100;     // no repeat, 1 byte burst
}

// DMA done, now at most 2 bytes are left to be sent
ISR(DMA_CH2_vect) {
    PORTE.OUT=0;    // Turn off LEDs
    if(CLK.CTRL==0) {   // Running at 2MHz
        _delay_us(8);
    }
    else {              // Running at 32MHz
        _delay_us(8*22);
    }        
    clrbit(LCD_CTRL, LCD_CS);			    // DeSelect
    setbit(DMA.INTFLAGS, 0);
}
