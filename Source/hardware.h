/****************************************************************************

Oscilloscope Watch

Gabotronics
December 2018

Copyright 2018 Gabriel Anzziani

This program is distributed under the terms of the GNU General Public License 

www.gabotronics.com
email me at: gabriel@gabotronics.com

*****************************************************************************/

#ifndef HARDWARE_H
#define HARDWARE_H

#define MENUPULL    0x18    // Menu button Pull down, invert pin

#define LCDVDD          5           // LCD VDD
#define LCD_DISP        2           // DISPLAY ON / OFF
#define	LCD_CS		    0           // Chip select
#define	LCD_CTRL        VPORT3.OUT

// PORT DEFINITIONS
#define LEDWHITE        0           // PORTE
#define LEDGRN          1           // PORTE
#define LEDRED          2           // PORTE
#define BATT_CIR        3           // PORTE
#define ANPOW           6           // PORTB
#define LOGIC_DIR       7           // PORTB
#define EXTCOMM         4           // PORTD

#define ONWHITE()       setbit(VPORT1.OUT, LEDWHITE)
#define OFFWHITE()      clrbit(VPORT1.OUT, LEDWHITE)
#define ONGRN()         setbit(VPORT1.OUT, LEDGRN)
#define OFFGRN()        clrbit(VPORT1.OUT, LEDGRN)
#define TOGGLE_GREEN()  PORTE.OUTTGL = 0x02
#define ONRED()         setbit(VPORT1.OUT, LEDRED)
#define OFFRED()        clrbit(VPORT1.OUT, LEDRED)
#define BATT_TEST_ON()  setbit(VPORT1.OUT, BATT_CIR)
#define BATT_TEST_OFF() clrbit(VPORT1.OUT, BATT_CIR)
#define TOGGLE_RED()    PORTE.OUTTGL = 0x04
#define ANALOG_ON()     setbit(VPORT0.OUT, ANPOW)
#define ANALOG_OFF()    clrbit(VPORT0.OUT, ANPOW)
#define LOGIC_ON()      PORTA.OUTSET = 0x02
#define LOGIC_OFF()     PORTA.OUTCLR = 0x02
#define LOGIC_DIROUT()  clrbit(VPORT0.OUT, LOGIC_DIR)
#define LOGIC_DIRIN()   setbit(VPORT0.OUT, LOGIC_DIR)
#define EXTCOMMH()      setbit(VPORT3.OUT, EXTCOMM)
#define EXTCOMML()      clrbit(VPORT3.OUT, EXTCOMM)
#define SECPULSE()      testbit(VPORT3.OUT, EXTCOMM)    // Half second high, Half second low

// Port definitions for Assembly code

#define EXTPIN 0x0012,2 // External trigger pin is VPORT0.2
#define CH1ADC 0x0224   // ADCA CH0.RESL
#define CH2ADC 0x0264   // ADCB CH0.RESL


#define AWG_SCALE 16    // Oscilloscope Watch has 4V output on the AWG

#define BUFFER_AWG          256     // Buffer size for the AWG output
#define BUFFER_I2C          2048    // Buffer size for the I2C sniffer
#define BUFFER_SERIAL       1280    // Buffer size for SPI or UART Sniffer
#define DATA_IN_PAGE_SERIAL 80      // Data that fits on a page in the sniffer
#define DATA_IN_PAGE_I2C    128     // Data that fits on a page in the sniffer
#define LCD_LINES           16      // Text lines on the display

#endif
