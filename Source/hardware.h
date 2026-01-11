// XprotoWatch
// Hardware specific definitions

#ifndef HARDWARE_H
#define HARDWARE_H

//#define INVERT_DISPLAY

#include "LS013B7DH03.h"

#define MENUPULL            0x18        // Menu button Pull down, invert pin
#define BUFFER_AWG          256         // Buffer size for the AWG output
#define LCD_LINES           16          // Text lines on the display
#define BUFFER_SERIAL       1280        // Buffer size for SPI or UART Sniffer
#define BUFFER_I2C          2048        // Buffer size for the I2C sniffer
#define DATA_IN_PAGE_I2C    128         // Data that fits on a page in the sniffer
#define DATA_IN_PAGE_SERIAL 80          // Data that fits on a page in the sniffer

#define LCDVDD              5           // LCD VDD
#define LCD_DISP            2           // DISPLAY ON / OFF
#define LCD_CS              0           // Chip Select
#define	LCD_CTRL        VPORT3.OUT

// PORT DEFINITIONS
#define BUFFVCC             1           // PORTA
#define ANPOW               6           // PORTB VPORT0
#define LOGIC_DIR           7           // PORTB VPORT0
#define LEDWHITE            0           // PORTE VPORT1
#define LEDGRN              1           // PORTE VPORT1
#define LEDRED              2           // PORTE VPORT1
#define BATT_CIR            3           // PORTE VPORT1
#define BUZZER1             4           // PORTE VPORT1
#define BUZZER2             5           // PORTE VPORT1
#define EXTCOMM             4           // PORTD VPORT3

#define EXT_TRIGGER         0x5A        // Event CH2 = PORTB Pin 2 (External Trigger)

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
#define CH1_AC_CPL()    PORTA.OUTSET = 0x04
#define CH1_DC_CPL()    PORTA.OUTCLR = 0x04
#define CH2_AC_CPL()    PORTA.OUTSET = 0x20
#define CH2_DC_CPL()    PORTA.OUTCLR = 0x20
#define LOGIC_DIROUT()  clrbit(VPORT0.OUT, LOGIC_DIR)
#define LOGIC_DIRIN()   setbit(VPORT0.OUT, LOGIC_DIR)
#define EXTCOMMH()      setbit(VPORT3.OUT, EXTCOMM)
#define EXTCOMML()      clrbit(VPORT3.OUT, EXTCOMM)
#define SECPULSE()      testbit(VPORT3.OUT, EXTCOMM)    // Half second high, Half second low

// Port definitions for Assembly code

#define EXTPIN              0x0012,2 // External trigger pin is VPORT0.2
#define CH1ADC              0x0224   // ADCA CH0.RESL
#define CH2ADC              0x0264   // ADCB CH0.RESL

#define AWG_SCALE           16       // Oscilloscope Watch has 4V output on the AWG

#endif
