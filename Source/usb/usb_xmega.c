// Minimal USB Stack for ATxmega32a4u and related
// http://nonolithlabs.com
// (C) 2011 Kevin Mehall (Nonolith Labs) <km@kevinmehall.net>
//
// Heavily borrows from LUFA
// Copyright 2011  Dean Camera (dean [at] fourwalledcubicle [dot] com)
//
// Licensed under the terms of the GNU GPLv3+

#include <util/delay.h>
#include <avr/wdt.h>
#include "main.h"
#include "usb_xmega.h"
#include "mso.h"
#include "interface.h"

// Volatile?
uint8_t ep0_buf_in[USB_EP0SIZE];
uint8_t ep0_buf_out[USB_EP0SIZE];
// XMega manual: The pointer to the endpoint configuration table must be aligned to a 16-bit word
//USB_EP_pair_t endpoints[USB_MAXEP+1];

volatile uint8_t USB_DeviceState;
volatile uint8_t USB_Device_ConfigurationNumber;
static inline void EVENT_USB_Device_ControlRequest(struct USB_Request_Header* req);

static inline void USB_handleSetAddress(USB_Request_Header_t* req) {
	uint8_t    DeviceAddress = (req -> wValue & 0x7F);
	endpoints[0].out.STATUS &= ~(USB_EP_SETUP_bm | USB_EP_BUSNACK0_bm);
	USB_ep_in_start(0, 0);
	while (!(endpoints[0].in.STATUS & USB_EP_TRNCOMPL0_bm)); // wait for status stage to complete
	USB.ADDR = DeviceAddress;
	USB_DeviceState = (DeviceAddress) ? DEVICE_STATE_Addressed : DEVICE_STATE_Default;
}

#if !defined(NO_INTERNAL_SERIAL) && (USE_INTERNAL_SERIAL != NO_DESCRIPTOR)
inline void USB_Device_GetSerialString(uint16_t* const UnicodeString) __attribute__ ((nonnull (1)));
inline void USB_Device_GetSerialString(uint16_t* const UnicodeString) {
	//uint_reg_t CurrentGlobalInt = GetGlobalInterruptMask();
	uint8_t SigReadAddress = INTERNAL_SERIAL_START_ADDRESS;
	//GlobalInterruptDisable();

	for (uint8_t SerialCharNum = 0; SerialCharNum < (INTERNAL_SERIAL_LENGTH_BITS / 4); SerialCharNum++) {
		uint8_t SerialByte;

		NVM.CMD    = NVM_CMD_READ_CALIB_ROW_gc;
		SerialByte = pgm_read_byte(SigReadAddress);

		if (SerialCharNum & 0x01) {
			SerialByte >>= 4;
			SigReadAddress++;
		}
		SerialByte &= 0x0F;
		UnicodeString[SerialCharNum] = ((SerialByte >= 10) ?
		(('A' - 10) + SerialByte) : ('0' + SerialByte));
	}
	NVM.CMD = NVM_CMD_NO_OPERATION_gc;
	//SetGlobalInterruptMask(CurrentGlobalInt);
}

static inline void USB_Device_GetInternalSerialDescriptor(void) {
	struct {
		USB_Descriptor_Header_t Header;
		uint16_t                UnicodeString[INTERNAL_SERIAL_LENGTH_BITS / 4];
	}* SignatureDescriptor = (void*) ep0_buf_in;

	SignatureDescriptor->Header.Type = DTYPE_String;
	SignatureDescriptor->Header.Size = USB_STRING_LEN(INTERNAL_SERIAL_LENGTH_BITS / 4);

	USB_Device_GetSerialString(SignatureDescriptor->UnicodeString);
	USB_ep_in_start(0, sizeof(*SignatureDescriptor));
}
#endif

static inline void USB_handleGetDescriptor(USB_Request_Header_t* req) {
	const void* DescriptorPointer;
	uint16_t  DescriptorSize;

	#if !defined(NO_INTERNAL_SERIAL) && (USE_INTERNAL_SERIAL != NO_DESCRIPTOR)
	if (req->wValue == ((DTYPE_String << 8) | USE_INTERNAL_SERIAL)){
		USB_Device_GetInternalSerialDescriptor();
	}
	#endif

	if ((DescriptorSize = CALLBACK_USB_GetDescriptor(req->wValue, req->wIndex, &DescriptorPointer))){
		if (DescriptorSize > req->wLength) DescriptorSize=req->wLength;
		USB_ep0_send_progmem(DescriptorPointer, DescriptorSize);
	}
	/* Error -> Stall endpoint */
	//return false;
}

static inline void USB_handleSetConfiguration(USB_Request_Header_t* req) {
	USB_ep_in_start(0, 0);
	USB_Device_ConfigurationNumber = (uint8_t)(req -> wValue);

	if (USB_Device_ConfigurationNumber) USB_DeviceState = DEVICE_STATE_Configured;
	else							    USB_DeviceState = (USB.ADDR) ? DEVICE_STATE_Configured : DEVICE_STATE_Powered;
}

void USB_ResetInterface(void) {
	// USB D- and D+ Calibration registers
	USB.CAL0 = SP_ReadCalibrationByte(offsetof(NVM_PROD_SIGNATURES_t, USBCAL0) );
	USB.CAL1 = SP_ReadCalibrationByte(offsetof(NVM_PROD_SIGNATURES_t, USBCAL1) );

	CLK.USBCTRL = CLK_USBSRC_RC32M_gc | CLK_USBSEN_bm;
	USB.EPPTR = (uint16_t)endpoints;
	USB.ADDR = 0;

	// Configure Endpoint 0 for CONTROL transfers
	endpoints[0].out.STATUS = 0;
	endpoints[0].out.CTRL = USB_EP_TYPE_CONTROL_gc | USB_EP_size_to_gc(USB_EP0SIZE);
	endpoints[0].out.DATAPTR = (uint16_t)ep0_buf_out;
	endpoints[0].in.STATUS = USB_EP_BUSNACK0_bm;
	endpoints[0].in.CTRL = USB_EP_TYPE_CONTROL_gc | USB_EP_size_to_gc(USB_EP0SIZE);
	endpoints[0].in.DATAPTR = (uint16_t)ep0_buf_in;
	// Configure Endpoint 1 for BULK transfers, Multipacket enabled
	endpoints[1].in.STATUS = USB_EP_BUSNACK0_bm | USB_EP_TRNCOMPL0_bm;
	endpoints[1].in.CTRL = USB_EP_TYPE_BULK_gc | USB_EP_MULTIPKT_bm | USB_EP_size_to_gc(64);
	endpoints[1].in.DATAPTR = (uint16_t)DC.CH1data;
	endpoints[1].out.STATUS = 0; // Accept new data
	endpoints[1].out.CTRL = USB_EP_TYPE_BULK_gc | USB_EP_MULTIPKT_bm | USB_EP_size_to_gc(64);
	endpoints[1].out.DATAPTR = (uint16_t)AWGBuffer;
	endpoints[1].out.CNT = 0;
	endpoints[1].out.AUXDATA = 256;

	USB.CTRLA		= USB_ENABLE_bm | USB_SPEED_bm | USB_MAXEP; // Enable USB at Full Speed, USB_MAXEP endpoints
	USB.INTCTRLA	= USB_BUSEVIE_bm | USB_INTLVL1_bm;			// Enable interrupt for Suspend, Resume or Reset Bus events
	USB.INTCTRLB	= USB_TRNIE_bm | USB_SETUPIE_bm;			// Enable Transaction Complete Interrupts
	USB.CTRLB	    = USB_ATTACH_bm;							// Attaches the device to the USB lines
	// USB Clock
	OSC.DFLLCTRL = OSC_RC32MCREF_USBSOF_gc; // Configure DFLL for 48MHz, calibrated by USB SOF
	DFLLRC32M.CALB = SP_ReadCalibrationByte(offsetof(NVM_PROD_SIGNATURES_t, USBRCOSC));
	DFLLRC32M.COMP1 = 0x1B; // Xmega AU manual, p41
	DFLLRC32M.COMP2 = 0xB7;
	DFLLRC32M.CTRL = DFLL_ENABLE_bm;
	OSC.CTRL |= OSC_RC32MEN_bm;
}

void USB_ep0_send_progmem(const uint8_t* addr, uint8_t size) {
	uint8_t *buf = ep0_buf_in;
	uint8_t remaining = size;
	while (remaining--) {
		*buf++ = pgm_read_byte(addr++);
	}
	USB_ep_in_start(0, size);
}

/* USB bus event interrupt includes :
 * - USB line events SOF, reset, suspend, resume, wakeup
 * - endpoint control errors underflow, overflow, stall  */
ISR(USB_BUSEVENT_vect) {
	if (USB.STATUS & USB_BUSRST_bm) {   // A reset condition has been detected
		USB.CTRLA=0;					// Disable USB
		USB_ResetInterface();
	}
	USB.INTFLAGSACLR=0xFF;
}

/* USB transfer complete interrupt includes events about endpoint transfer on all endpoints. */
ISR(USB_TRNCOMPL_vect) {
    ONRED();
	if (endpoints[0].out.STATUS & USB_EP_SETUP_bm) {	// Setup transaction complete
	    USB_Request_Header_t* req = (void *) ep0_buf_out;
	    if ((req->bmRequestType & CONTROL_REQTYPE_TYPE) == REQTYPE_STANDARD) {
		    switch (req->bRequest) {    // Check for standard requests
			    case REQ_GetStatus:
			        ep0_buf_in[0] = 0;	// No remote wakeup, no self power
			        ep0_buf_in[1] = 0;
			        USB_ep_in_start(0, 2);
			    break;
			    case REQ_ClearFeature:
			    case REQ_SetFeature:    USB_ep_in_start(0, 0);    		    break;
			    case REQ_SetAddress:    USB_handleSetAddress(req);          break;
			    case REQ_GetDescriptor: USB_handleGetDescriptor(req);       break;
			    case REQ_GetConfiguration:
			        ep0_buf_in[0] = USB_Device_ConfigurationNumber;
			        USB_ep_in_start(0, 1);
			    break;
			    case REQ_SetConfiguration: USB_handleSetConfiguration(req); break;
		    }
		} else EVENT_USB_Device_ControlRequest(req);  // Vendor defined request
		endpoints[0].out.STATUS &= ~(USB_EP_SETUP_bm | USB_EP_BUSNACK0_bm | USB_EP_TRNCOMPL0_bm);
	} else if(endpoints[0].out.STATUS & USB_EP_TRNCOMPL0_bm) {	// OUT transaction complete on endpoint 0

		endpoints[0].out.STATUS &= ~(USB_EP_TRNCOMPL0_bm | USB_EP_BUSNACK0_bm);
	} else if(endpoints[1].out.STATUS & USB_EP_TRNCOMPL0_bm) {  // OUT transaction complete on endpoint 1
    	endpoints[1].out.CNT = 0;
	    endpoints[1].out.AUXDATA = 256;
		endpoints[1].out.STATUS &= ~(USB_EP_BUSNACK0_bm | USB_EP_BUSNACK1_bm | USB_EP_TRNCOMPL0_bm | USB_EP_TRNCOMPL1_bm);
	}
	OFFRED();
	USB.FIFORP=0;   // Workaround to clear TRINF flag
	USB.INTFLAGSBCLR=USB_SETUPIF_bm|USB_TRNIF_bm;
}

/** Event handler for the library USB Control Request reception event. */
static inline void EVENT_USB_Device_ControlRequest(struct USB_Request_Header* req) {
	uint8_t *p;
	uint8_t i=0;
    //RTC.CNT=0;  // Clear screen saver timer
	if ((req->bmRequestType & CONTROL_REQTYPE_TYPE) == REQTYPE_VENDOR) {
		switch(req->bRequest) {
			case 'a': // Firmware version
				USB_ep0_send_progmem((uint8_t*)(VERSION+3), 4);
				return;
			case 'b':	// Write byte to GPIO or M
                WriteByte(lobyte(req->wIndex),lobyte(req->wValue));
            break;
			case 'c':	// Set Frequency (4 bytes)
                p=(uint8_t *)&M.AWGdesiredF;
    			*p++=lobyte(req->wIndex);
				*p++=hibyte(req->wIndex);
				*p++=lobyte(req->wValue);
				*p++=hibyte(req->wValue);
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
            case 'j':   // Set Post Trigger
                p=(uint8_t *)&M.Tpost;
                *p++=lobyte(req->wValue);
                *p++=hibyte(req->wValue);
                CheckPost();
                setbit(MStatus,update);
            break;
			case 'u':   // Send settings to PC
				p=(uint8_t *)0;  for(i=0; i<12; i++) ep0_buf_in[i]=*p++;
	            p=(uint8_t *)&M; for(   ; i<44; i++) ep0_buf_in[i]=*p++;
				USB_ep_in_start(0, 44);
				return;
			case 0xBB: // disconnect from USB, jump to bootloader
				USB_ep_in_start(0, 0);
				USB_ep0_wait_for_complete();
				Jump_boot();
					
/*		    default:    // Unknown request
    			endpoints[0].out.CTRL |= USB_EP_STALL_bm;
	    		endpoints[0].in.CTRL |= USB_EP_STALL_bm;*/
		}
        USB_ep_in_start(0, 0);
	}
}
/*
void Jump_boot1(void) {
	cli();
	_delay_ms(10);
	USB.CTRLB &= ~USB_ATTACH_bm;    // disconnects the device from the USB lines
	_delay_ms(100);
	void (*start_bootloader) (void) = (void (*)(void))(BOOT_SECTION_START/2+0x1FC/2);
	EIND = BOOT_SECTION_START>>17;
	start_bootloader();
}*/

void Jump_boot(void) {
	cli();
	_delay_ms(10);
	USB.CTRLB &= ~USB_ATTACH_bm;    // disconnects the device from the USB lines
	CCPWrite(&RST.CTRL, RST_SWRST_bm);  // Software Reset!
}
