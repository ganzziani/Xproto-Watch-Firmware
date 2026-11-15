#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>
#include "main.h"
#include "mso.h"
#include "logic.h"
#include "display.h"
#include "awg.h"
#include "interface.h"
#include "usb\usb_xmega.h"
#include "config.h"

// Monochrome 128x128 bitmap header for the LS013B7DH03 display
const uint8_t BMP[62] PROGMEM = {
    0x42,0x4D,0x3E,0x08,0x00,0x00,0x00,0x00,0x00,0x00,0x3E,0x00,0x00,0x00,0x28,0x00,
    0x00,0x00,0x80,0x00,0x00,0x00,0x80,0x00,0x00,0x00,0x01,0x00,0x01,0x00,0x00,0x00,
    0x00,0x00,0x00,0x08,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xff,0xff,0xff,0x00 };

// The screenshot as a byte stream: the 62 byte BMP header, then the display
// transposed into 128 rows of 16 bytes, bottom row first (BMP order).
// The Watch has no external serial interface, so the stream is only read over USB.
#define BMP_SIZE    (62 + 2048)

// Pixel (x,y) of the active display buffer is at
// DataAddress - x*18 + (y>>3),   bit 0x80 >> (y&7)
static uint8_t bmp_byte(uint16_t o, uint8_t *base) {
    if(o < 62) return pgm_read_byte_near(BMP + o);
    o -= 62;
    uint8_t y = 127 - (uint8_t)(o >> 4);                // Display y of this BMP row
    uint8_t mask = 0x80 >> (y & 0x07);                  // Bit of y inside its byte
    uint8_t *p = base - (uint8_t)(o & 0x0F)*8*18 + (y >> 3);
    uint8_t data = 0;
    for(uint8_t i=0; i<8; i++) {
        data <<= 1;
        if(*p & mask) data |= 1;
        p -= 18;                                        // x++, one line further
    }
    #ifdef INVERT_DISPLAY
    data = ~data;       // Ink is a cleared bit on an inverted display
    #endif
    return data;
}

// Returns the amount of data that needs to be sent from the ep0 buffer
uint8_t ProcessCommand(uint8_t Command, uint8_t usb) {
	uint8_t *p;
	uint8_t i=0,n=0;
    uint8_t index,value;
    const USB_Request_Header_t* req = (void *) ep0_buf_out;
    OFFGRN();   // In case the LED was on
   if(usb) { 
        if(Command=='K') {
            if(!(req->wValue & 0x0100)) KeyInput(lobyte(req->wValue));
            p=(uint8_t *)&Misc; for(; i<4; i++) ep0_buf_in[i]=*p++;   // GPIOC..F
            return 4;
        }
        if(Command=='r' || Command=='R') {
            uint16_t a = req->wIndex;
            n = lobyte(req->wValue);
            if(n > sizeof(ep0_buf_in)) n = sizeof(ep0_buf_in);
            for(; i<n; i++, a++)
                ep0_buf_in[i] = (Command=='r') ? *(uint8_t *)a : eeprom_read_byte((const uint8_t *)a);
            return n;
        }
    }
    switch(Command) {
        case 'a':   // Send firmware version
            p = (uint8_t *)(VERSION+3);
			for(;i<4;i++) ep0_buf_in[i]=pgm_read_byte(p++); // Read from the Version string
            n=4;
        break;
        case 'C':   // Send BMP
            if(usb) {
                // The BMP stream is read in 64 byte blocks
                uint16_t o = req->wIndex * 64;
				// Check which buffer needs to be sent
                uint8_t *base = Disp_send.DataAddress;
                if(Srate<11 && MFFT>=0x20 && !testbit(MStatus,gosniffer)) {
                    if(base == Disp_send.display_data1+127*18) base = Disp_send.display_data2+127*18;
                    else base = Disp_send.display_data1+127*18;
                }
                for(; i<64 && o<BMP_SIZE; i++, o++) ep0_buf_in[i] = bmp_byte(o, base);
                n = i;
            }
        break;
		case 'b':	// Write byte to GPIO or M
            if(usb) {
                index=lobyte(req->wIndex);
                value=lobyte(req->wValue);
            }
            // Check index range: index selects GPIO0..11 and then M[0..sizeof(M)-1];
            if(index >= 12 + sizeof(NVMVAR)) break;
            // Write to the corresponding register
            if(index==0 && Srate>10) clrbit(MStatus, triggered);    // prevents bad wave when changing from 20 to 10ms/div
            if(index<=5 || index==35 || index==38 || index==12 || index==13 ||
            (index>=24 && index<=28))  setbit(MStatus, update);      // Changing trigger
            if(index<=13) {
                T.SCOPE.MeterFreq = 0;			// Prevent sending outdated data
                setbit(MStatus, updatemso);		// Settings are changing
            }
            if(index>=36) setbit(MStatus, updateawg);
            if(index==8) T.SCOPE.adjusting=0;   // Mode change: cancel auto setup (as RestorefromMeter does)
            if(index<12) p=(uint8_t *)index;	    // Accessing GPIO
            else {
                index-=12;
                p=(uint8_t *)(&M);			// Accessing M
                p+=index;
            }
            *p=value;		                // Write data
        break;
		case 'c':	// Set Frequency (4 bytes)
			p=(uint8_t *)&M.AWGdesiredF;
            if(usb) {
       			*p++=lobyte(req->wIndex);
				*p++=hibyte(req->wIndex);
				*p++=lobyte(req->wValue);
				*p++=hibyte(req->wValue);
            }
			setbit(MStatus, updateawg);
        break;
		case 'd':   // Store settings
            OFFRED();
		    SaveEE();
		break;
		case 'e':   // Save AWG in RAM to EE
            OFFRED();
		    eeprom_write_block(AWGBuffer, EEwave, 256);
		break;
        case 'f':   // Stop
            setbit(MStatus,update);
            setbit(MStatus,stop);
		break;
        case 'g':   // Start
            setbit(MStatus,update);
            clrbit(MStatus,stop);
		break;
        case 'h':   // Force Trigger
            setbit(MStatus,update);
            setbit(MStatus,triggered);
		break;
        case 'i':   // Auto Setup
            setbit(MStatus,update);
            AutoSet();
		break;            
		case 'j':	// Set Post Trigger (2 bytes)
		    p=(uint8_t *)&M.Tpost;
            if(usb) {
                *p++=lobyte(req->wValue);
                *p++=hibyte(req->wValue);
            }            
            CheckPost();    // Check Post trigger value
            setbit(MStatus,update);
		break;
        case 'k':   // Restore factory settings
            LoadProfile(0);
            setbit(MStatus,update);
            setbit(MStatus, updatemso);     // Apply settings
            setbit(MStatus, updateawg);     // Generate wave                       
        break;
        case 'm':   // Send METER measurement
            p = (uint8_t *)(&T.SCOPE.MeterFreq);
            for(uint8_t i=0; i<4; i++) ep0_buf_in[i]=*p++;
            p = (uint8_t *)(&T.SCOPE.CH1);
            for(uint8_t i=4; i<4+sizeof(T.SCOPE.CH1)+sizeof(T.SCOPE.CH2); i++) ep0_buf_in[i]=*p++;
            // The PC needs the timebase, the auto-setup state and the gains that
            // produced those readings: the meter restarts its auto-setup for every
            // measurement, and reading these separately races it. adjusting==0 marks
            // the settled reading the device itself displays.
            ep0_buf_in[14] = Srate;
            ep0_buf_in[15] = T.SCOPE.adjusting;
            ep0_buf_in[16] = M.CH1gain;
            ep0_buf_in[17] = M.CH2gain;
            n=18;
        break;
        case 'u':   // Send settings to PC
            p=(uint8_t *)0;  for(; i<12; i++) ep0_buf_in[i]=*p++;   // GPIO
            p=(uint8_t *)&M; for(; i<44; i++) ep0_buf_in[i]=*p++;   // M
            n=44;
        break;
		case 0xBB: // disconnect from USB, jump to bootloader
            if(usb) {
                //USB_ep0_wait_for_complete();
			    USB_ep0_in_start(0);
                endpoints[0].out.STATUS &= ~(USB_EP_SETUP_bm | USB_EP_TRNCOMPL0_bm | USB_EP_BUSNACK0_bm);
                while (!(endpoints[0].out.STATUS & USB_EP_TRNCOMPL0_bm) && !(endpoints[0].in.STATUS & USB_EP_TRNCOMPL0_bm)){};
            }                
			cli();
			delay_ms(10);
			USB.CTRLB &= ~USB_ATTACH_bm;    // disconnects the device from the USB lines
			delay_ms(100);
            int *start_app_key = (int*)0x32D4;  // Bootloader will check this address
            *start_app_key=0x55AA;              // Looking for this value
			RST.STATUS = 0xFF;                  // Clear reset flags
            CCPWrite(&RST.CTRL, RST_SWRST_bm);  // Software Reset!
		break;        
    }
    return n;
}
