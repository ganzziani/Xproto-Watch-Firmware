#ifndef DISPLAY_H
#define DISPLAY_H

#include <stdint.h>
#include "hardware.h"

#ifdef INVERT_DISPLAY
#define PIXEL_CLR 255
#define PIXEL_SET 0
#else
#define PIXEL_CLR 0
#define PIXEL_SET 255
#endif

#define PIXEL_TGL 1

typedef struct {
    uint8_t     *SPI_Address;
    uint8_t     *DataAddress;
    uint8_t     display_setup1[2];  // Mode + Column address
    uint8_t     display_data1[DISPLAY_DATA_SIZE];
    uint8_t     display_setup2[2];
	uint8_t     display_data2[DISPLAY_DATA_SIZE];
} Disp_data;

extern Disp_data Disp_send;
extern uint8_t u8CursorX, u8CursorY;

#define lcd_goto(x,y) do { u8CursorX=(x); u8CursorY=(y); } while(0)

/* EXTERN Function Prototype(s) */
void print3x6(const char *);
void print5x8(const char *);
void putchar10x15 (char);
void putchar3x6 (char);
void putchar5x8(char u8Char);
void printN3x6(uint8_t Data);
void print16_5x8(uint16_t Data);
void printN5x8(uint8_t Data);
void printN_5x8(uint8_t Data);
void printN11x21(uint8_t x, uint8_t y, uint8_t Data, uint8_t digits);
void SwitchBuffers(void);
void clr_display(void);
void clr_display_all(void);
void pixel(uint8_t x, uint8_t y, uint8_t c);
void sprite(uint8_t x, uint8_t y, const int8_t *ptr);
void lcd_line(uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2);
void lcd_hline(uint8_t x1, uint8_t x2, uint8_t y, uint8_t c);
void Rectangle(uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2, uint8_t c);
void fillRectangle(uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2, uint8_t c);
void fillTriangle(uint8_t x1,uint8_t y1,uint8_t x2,uint8_t y2,uint8_t x3,uint8_t y3, uint8_t c);
void lcd_circle(uint8_t x, uint8_t y, uint8_t radius, uint8_t c);
void circle_fill(uint8_t x,uint8_t y, uint8_t radius, uint8_t c);
void printV(int16_t Data, uint8_t gain, uint8_t CHCtrl);
void printF(uint8_t x, uint8_t y, int32_t Data);
void tiny_printp(uint8_t x, uint8_t y, const char *ptr);
void clearRectangle(uint8_t x, uint8_t y, uint8_t width, uint8_t height);
void bitmap(uint8_t x, uint8_t y, const uint8_t *BMP);
void bitmap_safe(int8_t x, int8_t y, const uint8_t *BMP, uint8_t c);
void printhex3x6(uint8_t n);           // Prints a HEX number
void printhex5x8(uint8_t Data);

void GLCD_LcdInit(void);
void LCD_PrepareBuffers(void);
void dma_display(void);
void WaitDisplay(void);

#endif
