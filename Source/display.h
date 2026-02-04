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
void putchar3x6 (char);
void printN3x6(uint8_t Data);
void printhex3x6(uint8_t n);           // Prints a HEX number
void print5x8(const char *);
void putchar5x8(char u8Char);
void print16_5x8(uint16_t Data);
void printN5x8(uint8_t Data);
void printN_5x8(uint8_t Data);
void printhex5x8(uint8_t Data);
void printN11x21(uint8_t x, uint8_t y, uint8_t Data, uint8_t digits);
void SwitchBuffers(void);
void clr_display(void);
void displayBlack(void);
void clr_display_all(void);
void pixel(uint8_t x, uint8_t y, uint8_t c);
void sprite(uint8_t x, uint8_t y, const int8_t *ptr);
void lcd_line_c(uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2, uint8_t c);
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
void bitmap(uint8_t x, uint8_t y, uint8_t *BMP);
void bitmap_safe(int8_t x, int8_t y, uint8_t *BMP, uint8_t c);

void GLCD_LcdInit(void);
void LCD_PrepareBuffers(void);
void dma_display(void);
void WaitDisplay(void);

// Copy data from program memory to data memory with stride of 18
// src: pointer to program memory (PROGMEM)
// dest: pointer to data memory
// count: number of bytes to copy
#define SetPData(dest, src, count)                  \
do {                                                \
    const uint8_t *_src = (const uint8_t *)(src);   \
    uint8_t *_dest = (uint8_t *)(dest);             \
    uint8_t _count = (uint8_t)(count);              \
    asm volatile (                                  \
        "1:"                        "\n\t"          \
        "lpm   r0, Z+"              "\n\t"          \
        "st    X, r0"               "\n\t"          \
        "sbiw  r26, 18"             "\n\t"          \
        "dec   %[c]"                "\n\t"          \
        "brne  1b"                  "\n\t"          \
        : [c] "+r" (_count),                        \
          [d] "+x" (_dest),                         \
          [s] "+z" (_src)                           \
        :                                           \
        : "r0", "memory"                            \
    );                                              \
} while(0)

#ifdef INVERT_DISPLAY

// Send data from program memory to data memory with stride of 18, CLEAR the bits
// src: pointer to program memory (PROGMEM)
// dest: pointer to data memory
// count: number of bytes to copy
#define SendBitsPData(dest, src, count)             \
do {                                                \
    const uint8_t *_src = (const uint8_t *)(src);   \
    uint8_t *_dest = (uint8_t *)(dest);             \
    uint8_t _count = (uint8_t)(count);              \
    asm volatile (                                  \
        "1:"                        "\n\t"          \
        "lpm   r0, Z+"              "\n\t"          \
        "com   r0"                  "\n\t"          \
        "ld    r18, X"              "\n\t"          \
        "and   r18, r0"             "\n\t"          \
        "st    X, r18"              "\n\t"          \
        "sbiw  r26, 18"             "\n\t"          \
        "dec   %[c]"                "\n\t"          \
        "brne  1b"                  "\n\t"          \
        : [c] "+r" (_count),                        \
          [d] "+x" (_dest),                         \
          [s] "+z" (_src)                           \
        :                                           \
        : "r0", "r18", "memory"                     \
    );                                              \
} while(0)

#else

// Send data from program memory to data memory with stride of 18, OR the bits
// src: pointer to program memory (PROGMEM)
// dest: pointer to data memory
// count: number of bytes to copy
#define SendBitsPData(dest, src, count)             \
do {                                                \
    const uint8_t *_src = (const uint8_t *)(src);   \
    uint8_t *_dest = (uint8_t *)(dest);             \
    uint8_t _count = (uint8_t)(count);              \
    asm volatile (                                  \
        "1:"                        "\n\t"          \
        "lpm   r0, Z+"              "\n\t"          \
        "ld    r18, X"              "\n\t"          \
        "or    r18, r0"             "\n\t"          \
        "st    X, r18"              "\n\t"          \
        "sbiw  r26, 18"             "\n\t"          \
        "dec   %[c]"                "\n\t"          \
        "brne  1b"                  "\n\t"          \
        : [c] "+r" (_count),                        \
          [d] "+x" (_dest),                         \
          [s] "+z" (_src)                           \
        :                                           \
        : "r0", "r18", "memory"                     \
    );                                              \
} while(0)

#endif

#endif
