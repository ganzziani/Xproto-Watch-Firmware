//	128x128 Graphic LCD management for LS013B7DH03 driver

#ifndef LS013B7DH03_H
#define LS013B7DH03_H

#define DYNAMIC_MODE        0b10000000      // Write Single or Multiple Line
#define STATIC_MODE         0b00000000      // Set Static Mode
#define CLEAR_ALL           0b00100000      // Clear screen

#define DISPLAY_DATA_SIZE	(128*18)    // 128*(Data + Dummy + Addresses)
#define FBAUD32M            14          // BSEL=14 (SPI clock = 1.06MHz, LS013B7DH03 max is 1.1MHz)
#define FBAUD2M             0           // BSEL=0  (SPI clock = 1.00MHz, LS013B7DH03 max is 1.1MHz)
#define DISPLAY_MAX_X       127         // LCD Display max x coordinate
#define DISPLAY_MAX_Y       127         // LCD Display max y coordinate
#define TEXT_LAST_LINE      15          // LCD Display last text line number

#endif
