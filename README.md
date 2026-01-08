Xproto-Watch
============

The project is developed using Microchip Studio 7 and version 4.0.0 of the AVR 8-Bit Toolchain.

The Oscilloscope Watch has all the features of a modern watch (time, calendar, alarm, etc...) combined with all the features of the popular Xprotolab (Oscilloscope, Waveform Generator, Logic Analyzer, Protocol Sniffer, Frequency Counter). The Xprotolab has been featured in several other online magazines, such as Hack a Day. and Elektor.

Features:

    Mixed Signal Oscilloscope: Simultaneous sampling of analog and digital signals.
    Advanced Trigger: Normal / Single / Auto, with rising or falling edge and adjustable trigger level.
    Meter Mode: Average, Peak to peak and Frequency readout.
    XY Mode (Plot Lissajous patterns or see the phase difference between two waveforms).
    Spectrum Analyzer with different windowing options and selectable vertical log.
    Horizontal and Vertical Cursors with automatic waveform measurements.
    Arbitrary Waveform Generator with Sweep on all parameters.
    Display options: Persistence, Different grid options, and more.
    Curve tracer function

General Specifications:

    ATXMEGA256A3U 256KB Flash, 16KB SRAM, 4KB EEPROM
    Sharp Memory LCD display 1.28", 128x128 pixels
    Approximate watch size: 2" x 1.6" x 0.6"
    Li-Ion Battery 400mAh
    Internal PDI interface.
    8 tactile switches
    USB connectivity: Windows, Linux, MAC, Android

Logic Analyzer specifications:

    8 Digital Inputs, 3.3V / 5V
    820k ohm input impedance
    Maximum sampling rate: 16MSPS
    Frequency counter: 16Mhz
    Protocol Sniffer: UART, I2C, SPI
    Internal pull up or pull down.
    Buffer size: 4096

Oscilloscope specifications:

    2 Analog Inputs
    Maximum Sampling rate: 4MSPS
    Analog Bandwidth: 200kHz
    Resolution: 8bits
    Input Impedance: 1MΩ 15pF
    Buffer size per channel: 4096
    Input Voltage Range: -14V to +20V

AWG specifications:

    1 Analog Output
    Maximum conversion rate: 1MSPS
    Analog Bandwidth: 50kHz
    Resolution: 8bits
    Output current > +/- 7mA
    Buffer size: 512
    Output Voltage: +/- 4V

Overall hardware block diagram:
<img width="1152" height="1003" alt="OW-Block_Diagram-1" src="https://github.com/user-attachments/assets/2c863071-3402-4acf-a937-3d8bb5933992" />

System architecture:
<img width="1152" height="860" alt="OW-Block_Diagram-2" src="https://github.com/user-attachments/assets/ea4f4227-9057-472e-b661-218465e0dba0" />

Clock and Frequency Counter:
<img width="1152" height="912" alt="OW-Block_Diagram-3" src="https://github.com/user-attachments/assets/edc9af9b-a5d9-45af-ba30-8bb7c8bc39f3" />
