#include <stdint.h>
#include <avr/pgmspace.h>
#include <avr/interrupt.h>
#include "display.h"
#include "mygccdef.h"
#include "main.h"
#include "mso.h"
#include "utils.h"

Disp_data Disp_send;
uint8_t   u8CursorX, u8CursorY;

void SwitchBuffers(void) {
    uint8_t *NewDataAddress;
    uint8_t *NewSPIAddress;
    if(!testbit(WatchBits, disp_select)) {                  // Which display buffer is active?
        setbit(WatchBits, disp_select);
        NewDataAddress=Disp_send.display_data1+127*18;      // Locate pointer at upper left byte
        NewSPIAddress=Disp_send.display_setup1;
    }
    else {
        clrbit(WatchBits, disp_select);
        NewDataAddress=Disp_send.display_data2+127*18;      // Locate pointer at upper left byte
        NewSPIAddress=Disp_send.display_setup2;
    }
    Disp_send.DataAddress=NewDataAddress;
    Disp_send.SPI_Address=NewSPIAddress;

}

// Clear active display buffer
void clr_display(void) {
    uint8_t *p=Disp_send.SPI_Address+2;  // Locate pointer at start of active buffer;
    #ifdef INVERT_DISPLAY
    const uint8_t cleared=255;
    #else
    const uint8_t cleared=0;
    #endif
    for(uint8_t i=128; i; i--) { // Erase all 2048 bytes in the buffer
        // Unroll inner loop for speed - Clear 16 lines
        *p++=cleared; *p++=cleared; *p++=cleared; *p++=cleared;
        *p++=cleared; *p++=cleared; *p++=cleared; *p++=cleared;
        *p++=cleared; *p++=cleared; *p++=cleared; *p++=cleared;
        *p++=cleared; *p++=cleared; *p++=cleared; *p++=cleared;
        p+=2;   // Skip line LCD setup
    }
    lcd_goto(0,0);
}

// Clear display buffers 1 and 2
void clr_display_all(void) {
    uint8_t *p;
    p=Disp_send.display_data1;
    for(uint8_t i=0; i<2; i++) {
        for(uint8_t j=0; j<128; j++) {
            for(uint8_t k=0; k<16; k++) {
                #ifdef INVERT_DISPLAY
                *p++=255;
                #else
                *p++=0;
                #endif
            }
            p+=2;   // Skip line LCD setup
        }
        p=Disp_send.display_data2;
    }
}

// Sprites, each byte pair represents next pixel relative position
void sprite(uint8_t x, uint8_t y, const int8_t *ptr) {
    do {
        int8_t a=pgm_read_byte(ptr++);  // Get next x
        int8_t b=pgm_read_byte(ptr++);  // Get next y
        if((uint8_t)a==255) return;     // 255 marks the end of the sprite
        set_pixel(x+a,y+b);
    } while(1);
}

// Print a char on the display using the 3x6 font
void putchar3x6(char u8Char) {
    uint16_t pointer;
	uint8_t data;
	pointer = (unsigned int)(Font3x6)+(u8Char-20)*(3);
    if(u8Char!='\n') {
        // Draw a char: 3 bytes
        for(uint8_t i=3; i; i--) {
            data = pgm_read_byte_near(pointer++);
		    if(testbit(Misc,negative)) data = ~(data|128);
		    display_or(data);
	    }
    }
    // Special characters
    if(u8Char==0x1C) {       // Begin long 'd' character
        display_or(FONT3x6_d1);
    }
    else if(u8Char==0x1D) {  // Complete long 'd' character
        display_or(FONT3x6_d2);
        u8CursorX++;
    }
    else if(u8Char==0x1A) {  // Complete long 'm' character
        display_or(FONT3x6_m);
    }
    else if(u8CursorX < 128) {  // if not then insert a space before next letter
		data = 0;
		if(testbit(Misc,negative)) data = 127;
		display_or(data);
	}
    if(u8CursorX>=128 || u8Char=='\n') {    // Next line
        u8CursorX = 0; u8CursorY++;
    }
}

// Print a string in program memory to the display
void print3x6(const char *ptr) {
    char c;
    while ((c=pgm_read_byte(ptr++)) != 0x00) {
        putchar3x6(c);
    }
}

// Print Number using  the 3x6 font
void printN3x6(uint8_t Data) {
    uint8_t d=0x30;
	while (Data>=100)	{ d++; Data-=100; }
    if(d>0x30) putchar3x6(d);
	d=0x30;
	while (Data>=10)	{ d++; Data-=10; }
    putchar3x6(d);
    putchar3x6(Data+0x30);
}

// Prints a HEX number
void printhex3x6(uint8_t n) {
    uint8_t temp;
    temp = n>>4;   putchar3x6(NibbleToChar(temp));
    temp = n&0x0F; putchar3x6(NibbleToChar(temp));
}

extern const uint16_t milivolts[];

// Print Voltage, multiply by 10 if using the x10 probe
void printV(int16_t Data, uint8_t gain, uint8_t CHCtrl) {
    int32_t Data32 = (int32_t)Data*milivolts[gain];
    if(testbit(CHCtrl,chx10)) Data32*=10;
    printF(u8CursorX,u8CursorY,Data32/8);
}    

// Print Fixed point Number with 5 digits
// or Print Long integer with 7 digits
void printF(uint8_t x, uint8_t y, int32_t Data) {
	uint8_t D[8]={0,0,0,0,0,0,0,0},point=0;
    uint8_t *DisplayPointer = Disp_send.DataAddress -(x)*18 + (y);
    lcd_goto(x,y);
    if(Data<0) {    // Negative number: Print minus
        Data=-Data;
        if(testbit(Misc,bigfont)) { // putchar10x15('-');
            for(uint8_t i=4; i; i--) {
                #ifdef INVERT_DISPLAY
                *DisplayPointer = ~0xC0;
                #else
                *DisplayPointer = 0xC0;
                #endif
                DisplayPointer -=18;   // Go to next column
            }
            DisplayPointer -=36;
        }            
        else putchar3x6('-');
    }
    else {          // Positive number: Print space
        if(testbit(Misc,bigfont)) { // putchar10x15(' ');
            DisplayPointer -=108;   // Move 6 columns
        }            
        else putchar3x6(' ');
    }
    if(testbit(Misc,negative)) {   // 7 digit display
        point=3;
    }
    else {  // 4 digit display
	    if(Data>=999900000L) Data = 9999;
	    else if(Data>=100000000L)  Data = (Data/100000);
	    else if(Data>=10000000L) {
    	    Data = (Data/10000);
    	    point = 1;
	    }
	    else if(Data>=1000000L) {
    	    Data = (Data/1000);
    	    point = 2;
	    }
	    else {
    	    Data = (Data/100);
    	    point = 3;
	    }
    }
    
    uint8_t i=7;
    do {    // Decompose number
        uint32_t power;
        power=pgm_read_dword_near(Powersof10+i);
        while(Data>=power) { D[i]++; Data-=power; }
    } while(i--);

    if(testbit(Misc, negative)) i=7;    // To print all digits
    else i=3;
	for(; i!=255; i--) {
		if(testbit(Misc,bigfont)) {
            // putchar10x15();
            uint8_t *TempPointer = DisplayPointer;
            uint16_t FontPointer = (unsigned int)(Font10x15)+(D[i])*20;
            // Upper side
            DisplayPointer--;
            for(uint8_t i=10; i; i--) {
                *DisplayPointer = pgm_read_byte_near(FontPointer++);
                DisplayPointer -= 18;
            }
            // Lower Side
            DisplayPointer = TempPointer;
            for(uint8_t i=10; i; i--) {
                *DisplayPointer = pgm_read_byte_near(FontPointer++);
                DisplayPointer -= 18;
            }
            DisplayPointer -=36;
			if(point==i) { // putchar10x15('.'); Small point to Save space
                for(uint8_t j=2; j; j--) {
                    *DisplayPointer = 0x06;
                    DisplayPointer -= 18;
                }
                DisplayPointer -=36;
            }                
		}
		else {
			putchar3x6(0x30+D[i]);
			if(point==i) putchar3x6('.');
		}
	}
}

// Print small font text at x,y from program memory
void tiny_printp(uint8_t x, uint8_t y, const char *ptr) {
    lcd_goto(x,y);
    print3x6(ptr);
}

// Safe pixel on display buffer with "color"
// show represents the probability of setting the pixel
// with show==0 to clear, and show==255 to set
// Special case show==1 to toggle
void pixel(uint8_t x, uint8_t y, uint8_t show) {
    uint8_t offset;
    uint8_t *p=Disp_send.DataAddress;
    if(x>=128 || y>=128) return;
    p = p - (uint16_t)(x*18) + (y>>3);              // Calculate position
    offset = (uint8_t)(0x80 >> (y & 0x07));
    if(show==0)         *p &= ~offset;                      // CLEAR
    else if(show==1)    *p ^= offset;                       // TOGGLE
    else if(show==255 || (show>prandom())) *p |= offset;    // SET
}

// Horizontal line
void lcd_hline(uint8_t x1, uint8_t x2, uint8_t y, uint8_t c) {
    if(x1>=192) x1=0; else if(x1>=128) x1=127;  // Handle overflow
    if(x2>=192) x2=0; else if(x2>=128) x2=127;
	if(x1>=x2) SWAP(x1,x2);
	for(;x1<=x2;x1++) pixel(x1,y,c);
}

void Rectangle(uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2, uint8_t c) {
    lcd_hline(x1,x2,y1,c);
    lcd_hline(x1,x2,y2,c);
	if(y1>=y2) SWAP(y1,y2);
	for(;y1<=y2;y1++) { pixel(x1,y1,c); pixel(x2,y1,c); }
}

void fillRectangle(uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2, uint8_t c) {
    if(y1>=y2) SWAP(y1,y2);    
    while(y1<=y2) {
        lcd_hline(x1,x2,y1++,c);
    }
}

// Fill a triangle - Bresenham method
// Original from http://www.sunshine2k.de/coding/java/TriangleRasterization/TriangleRasterization.html
void fillTriangle(uint8_t x1,uint8_t y1,uint8_t x2,uint8_t y2,uint8_t x3,uint8_t y3, uint8_t c) {
	uint8_t t1x,t2x,y,minx,maxx,t1xp,t2xp;
	uint8_t changed1 = 0;
	uint8_t changed2 = 0;
	int8_t signx1,signx2,dx1,dy1,dx2,dy2;
	uint8_t e1,e2;
    // Sort vertices
	if (y1>y2) { SWAP(y1,y2); SWAP(x1,x2); }
	if (y1>y3) { SWAP(y1,y3); SWAP(x1,x3); }
	if (y2>y3) { SWAP(y2,y3); SWAP(x2,x3); }

	t1x=t2x=x1; y=y1;   // Starting points

	dx1 = (int8_t)(x2 - x1); if(dx1<0) { dx1=-dx1; signx1=-1; } else signx1=1;
	dy1 = (int8_t)(y2 - y1);
 
	dx2 = (int8_t)(x3 - x1); if(dx2<0) { dx2=-dx2; signx2=-1; } else signx2=1;
	dy2 = (int8_t)(y3 - y1);
	
	if (dy1 > dx1) {   // swap values
        SWAP(dx1,dy1);
		changed1 = 1;
	}
	if (dy2 > dx2) {   // swap values
        SWAP(dy2,dx2);
		changed2 = 1;
	}
	
	e2 = (uint8_t)(dx2>>1);
    // Flat top, just process the second half
    if(y1==y2) goto next;
    e1 = (uint8_t)(dx1>>1);
	
	for (uint8_t i = 0; i < dx1;) {
		t1xp=0; t2xp=0;
		if(t1x<t2x) { minx=t1x; maxx=t2x; }
		else		{ minx=t2x; maxx=t1x; }
        // process first line until y value is about to change
		while(i<dx1) {
			i++;			
			e1 += dy1;
	   	   	while (e1 >= dx1) {
				e1 -= dx1;
   	   	   	   if (changed1) t1xp=signx1;//t1x += signx1;
				else          goto next1;
			}
			if (changed1) break;
			else t1x += signx1;
		}
	// Move line
	next1:
        // process second line until y value is about to change
		while (1) {
			e2 += dy2;		
			while (e2 >= dx2) {
				e2 -= dx2;
				if (changed2) t2xp=signx2;//t2x += signx2;
				else          goto next2;
			}
			if (changed2)     break;
			else              t2x += signx2;
		}
	next2:
		if(minx>t1x) minx=t1x;
        if(minx>t2x) minx=t2x;
		if(maxx<t1x) maxx=t1x;
        if(maxx<t2x) maxx=t2x;
	   	lcd_hline(minx, maxx, y,c);    // Draw line from min to max points found on the y
		// Now increase y
		if(!changed1) t1x += signx1;
		t1x+=t1xp;
		if(!changed2) t2x += signx2;
		t2x+=t2xp;
    	y += 1;
		if(y==y2) break;
		
   }
	next:
	// Second half
	dx1 = (int8_t)(x3 - x2); if(dx1<0) { dx1=-dx1; signx1=-1; } else signx1=1;
	dy1 = (int8_t)(y3 - y2);
	t1x=x2;
 
	if (dy1 > dx1) {   // swap values
        SWAP(dy1,dx1);
		changed1 = 1;
	} else changed1=0;
	
	e1 = (uint8_t)(dx1>>1);
	
	for (uint8_t i = 0; i<=dx1; i++) {
		t1xp=0; t2xp=0;
		if(t1x<t2x) { minx=t1x; maxx=t2x; }
		else		{ minx=t2x; maxx=t1x; }
	    // process first line until y value is about to change
		while(i<dx1) {
    		e1 += dy1;
	   	   	while (e1 >= dx1) {
				e1 -= dx1;
   	   	   	   	if (changed1) { t1xp=signx1; break; }//t1x += signx1;
				else          goto next3;
			}
			if (changed1) break;
			else   	   	  t1x += signx1;
			i++;
		}
	next3:
        // process second line until y value is about to change
		while (t2x!=x3) {
			e2 += dy2;
	   	   	while (e2 >= dx2) {
				e2 -= dx2;
				if(changed2) t2xp=signx2;
				else          goto next4;
			}
			if (changed2)     break;
			else              t2x += signx2;
		}	   	   
	next4:

		if(minx>t1x) minx=t1x;
        if(minx>t2x) minx=t2x;
		if(maxx<t1x) maxx=t1x;
        if(maxx<t2x) maxx=t2x;
	   	lcd_hline(minx, maxx, y,c);    // Draw line from min to max points found on the y
		// Now increase y
		if(!changed1) t1x += signx1;
		t1x+=t1xp;
		if(!changed2) t2x += signx2;
		t2x+=t2xp;
    	y += 1;
		if(y>y3) return;
	}
}

/*
// Fill a triangle - slope method
void fillTriangleslope(uint8_t x0, uint8_t y0,uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2, uint8_t color) {
 	uint8_t a, b, y, last;
  	// Sort coordinates by Y order (y2 >= y1 >= y0)
  	if (y0 > y1) { SWAP(y0, y1); SWAP(x0, x1); }
  	if (y1 > y2) { SWAP(y2, y1); SWAP(x2, x1); }
  	if (y0 > y1) { SWAP(y0, y1); SWAP(x0, x1); }
  
  	if(y0 == y2) { // All on same line case
    	a = b = x0;
    	if(x1 < a)      a = x1;
    	else if(x1 > b) b = x1;
    	if(x2 < a)      a = x2;
    	else if(x2 > b) b = x2;
        lcd_hline(a, b, y0);
        return;
    }

    int8_t
        dx01 = x1 - x0,
        dy01 = y1 - y0,
        dx02 = x2 - x0,
        dy02 = y2 - y0,
        dx12 = x2 - x1,
        dy12 = y2 - y1;
    int16_t sa = 0, sb = 0;

    // For upper part of triangle, find scanline crossings for segment
    // 0-1 and 0-2.  If y1=y2 (flat-bottomed triangle), the scanline y
    // is included here (and second loop will be skipped, avoiding a /
    // error there), otherwise scanline y1 is skipped here and handle
    // in the second loop...which also avoids a /0 error here if y0=y
    // (flat-topped triangle)
    if(y1 == y2) last = y1;   // Include y1 scanline
    else         last = y1-1; // Skip it

    for(y=y0; y<=last; y++) {
        a   = x0 + sa / dy01;
        b   = x0 + sb / dy02;
        sa += dx01;
        sb += dx02;
        // longhand a = x0 + (x1 - x0) * (y - y0) / (y1 - y0)
        //          b = x0 + (x2 - x0) * (y - y0) / (y2 - y0)
        lcd_hline(a, b, y);
    }

    // For lower part of triangle, find scanline crossings for segment
    // 0-2 and 1-2.  This loop is skipped if y1=y2
    sa = dx12 * (y - y1);
    sb = dx02 * (y - y0);
    for(; y<=y2; y++) {
        a   = x1 + sa / dy12;
        b   = x0 + sb / dy02;
        sa += dx12;
        sb += dx02;
        // longhand a = x1 + (x2 - x1) * (y - y1) / (y2 - y1)
        //          b = x0 + (x2 - x0) * (y - y0) / (y2 - y0)
        lcd_hline(a, b, y);
    }
}


// Draws a circle with center at x,y with given radius.
// Set show to 1 to draw pixel, set to 0 to hide pixel.
void lcd_circle(uint8_t x, uint8_t y, uint8_t radius, uint8_t c) {
    uint8_t xc = 0;
    uint8_t yc = radius;
    int p = 3 - (radius<<1);
    while (xc <= yc) {
        pixel(x + xc, y + yc, c);
        pixel(x + xc, y - yc, c);
        pixel(x - xc, y + yc, c);
        pixel(x - xc, y - yc, c);
        pixel(x + yc, y + xc, c);
        pixel(x + yc, y - xc, c);
        pixel(x - yc, y + xc, c);
        pixel(x - yc, y - xc, c);
        if (p < 0) p += (xc++ << 2) + 6;
        else p += ((xc++ - yc--)<<2) + 10;
    }
}

// Circle_fill - Draws and fills a circle
void circle_fill(uint8_t x,uint8_t y, uint8_t radius, uint8_t c) {
    uint8_t xc = 0;
    uint8_t yc = radius;
    int p = 3 - (radius<<1);
    while (xc <= yc) {
		lcd_hline(x - xc, x + xc, y - yc, c);
		lcd_hline(x - xc, x + xc, y + yc, c);
		lcd_hline(x - yc, x + yc, y - xc, c);
		lcd_hline(x - yc, x + yc, y + xc, c);
        if (p < 0) p += (xc++ << 2) + 6;
        else p += ((xc++ - yc--)<<2) + 10;
    }
}
*/
/*-------------------------------------------------------------------------------
Print a char on the LCD
	GLCD_Putchar (uint8_t u8Char)
		u8Char = char to display
-------------------------------------------------------------------------------*/
void putchar5x8(char u8Char) {
    if(u8Char=='\n') {    // Next line
        u8CursorX = 0; u8CursorY++;
        return;
    }
    uint16_t FontPointer = (unsigned int)(Font5x8)+(u8Char-0x20)*(5);
	uint8_t data;
    uint8_t *DisplayPointer = Disp_send.DataAddress -(u8CursorX)*18 + (u8CursorY);
    // Draw a char: 5 bytes
    for(uint8_t i = 5; i; i--) {
        data = pgm_read_byte_near(FontPointer++);
        #ifdef INVERT_DISPLAY
        if(!testbit(Misc,negative)) data = ~data;
        #else
		if(testbit(Misc,negative)) data = ~data;
        #endif
		*DisplayPointer = data;
        DisplayPointer -=18;    // Go to next column
    }
    u8CursorX += 5;
    if(u8CursorX < 128) {  // Insert a space before next letter
        #ifdef INVERT_DISPLAY
        data = 0xFF;
        if(testbit(Misc,negative)) data = 0;
        #else
        data = 0;
		if(testbit(Misc,negative)) data = 0xFF;
        #endif
		*DisplayPointer = data;
        u8CursorX++;
    }
}

/*-------------------------------------------------------------------------------
Print a string on the LCD from a string in program memory
	GLCD_Printf (uint8_t *au8Text) 
		*au8Text = string to display
-------------------------------------------------------------------------------*/
void print5x8(const char *ptr) {
    char c;
    while ((c=pgm_read_byte(ptr++)) != 0x00) {
        putchar5x8(c);
    }
}

// Print Number 0-9999
void print16_5x8(uint16_t Data) {
    uint8_t d=0x30,h=0x30,t=0x30, tt=0x30;
    while (Data>=10000)	{ tt++; Data-=10000; }
    while (Data>=1000)	{ t++; Data-=1000; }
    while (Data>=100)	{ h++; Data-=100; }
    while (Data>=10)	{ d++; Data-=10; }
    if(tt!=0x30) putchar5x8(tt);
    putchar5x8(t);
    putchar5x8(h);
    putchar5x8(d);
    putchar5x8(Data+0x30);
}

// Print Number 0-255
void printN5x8(uint8_t Data) {
    uint8_t d=0x30;
	while (Data>=100)	{ d++; Data-=100; }
    if(d!=0x30) putchar5x8(d);
    d=0x30;
	while (Data>=10)	{ d++; Data-=10; }
    putchar5x8(d);
    putchar5x8(Data+0x30);
}

// Print Number 0-99
void printN_5x8(uint8_t Data) {
    uint8_t d=0x30;
    while (Data>=10)	{ d++; Data-=10; }
    if(d!=0x30) putchar5x8(d); else putchar5x8(' ');
    putchar5x8(Data+0x30);
}

// Print Number 0-255 with the 11x21 font
void printN11x21(uint8_t x, uint8_t y, uint8_t Data, uint8_t digits) {
    uint8_t n=Data, d=0, h=0;
    while(n>=100) { h++; n-=100; }
    while (n>=10) { d++; n-=10; }
    if(digits>=3) {
        bitmap(x,   y,(const uint8_t *)pgm_read_word(Digits_11x21+h));
        x+=13;
    }        
    bitmap(x,y,(const uint8_t *)pgm_read_word(Digits_11x21+d));
    bitmap(x+13,y,(const uint8_t *)pgm_read_word(Digits_11x21+n));
}

// Print Number
void printhex5x8(uint8_t n) {
    uint8_t temp;
    temp = n>>4;   putchar5x8(NibbleToChar(temp));
    temp = n&0x0F; putchar5x8(NibbleToChar(temp));
}

// Fast rectangle erase
void clearRectangle(uint8_t x, uint8_t y, uint8_t width, uint8_t height) {
    uint8_t *p;
    p= &Disp_send.DataAddress[(uint16_t)((-x)*18)+y];
  	for (uint8_t col=0; col < width; col++) {
		for (uint8_t row=0; row<height; row++) {
            #ifdef INVERT_DISPLAY
            *p++=0xFF;
            #else
            *p++=0;
            #endif
		}
        p-=18+height;   // Next line
	}
}

/*----------------------------------------------------------------------------
Send a run length encoded image from program memory to the LCD
	Decode algorithm:
	Get one byte, put it to the output file, and now it's the 'last' byte. 
	Loop 
	Get one byte 
	Is the current byte equal to last? 
	Yes 
		Now get another byte, this is 'counter' 
		Put current byte in the output file 
		Copy 'counter' times the 'last' byte to the output file 
		Put last copied byte in 'last' (or leave it alone) 
	No 
		Put current byte to the output file 
		Now 'last' is the current byte 
	Repeat. 

    BMP[0] contains width/8
    BMP[1] contains height
----------------------------------------------------------------------------*/
void bitmap(uint8_t x, uint8_t y, const uint8_t *BMP) {
    uint8_t *p;
	uint8_t data=0,count=0;
	uint8_t width,height;
    uint8_t const *b;
    width=pgm_read_byte(&BMP[0]);
    height=pgm_read_byte(&BMP[1])/8;
    b=&BMP[2];
    p= &Disp_send.DataAddress[(uint16_t)((-x)*18)+y];
  	for (uint8_t col=0; col < width; col++) {
		for (uint8_t row=0; row<height; row++) {
			if(count==0) {
				data = pgm_read_byte(b++);
				if(data==pgm_read_byte(b++)) {
					count = pgm_read_byte(b++);
				}
				else {
					count = 1;
					b--;
				}
			}
			count--;
            #ifdef INVERT_DISPLAY
            *p++=~data;
            #else
            *p++=data;
            #endif
		}
        p-=18+height;   // Next line
	}
}

/*----------------------------------------------------------------------------
Send a run length encoded image from program memory to the LCD
	Decode algorithm:
	Get one byte, put it to the output file, and now it's the 'last' byte. 
	Loop 
	Get one byte 
	Is the current byte equal to last? 
	Yes 
		Now get another byte, this is 'counter' 
		Put current byte in the output file 
		Copy 'counter' times the 'last' byte to the output file 
		Put last copied byte in 'last' (or leave it alone) 
	No 
		Put current byte to the output file 
		Now 'last' is the current byte 
	Repeat. 

    BMP[0] contains width/8
    BMP[1] contains height
----------------------------------------------------------------------------*/
void bitmap_safe(int8_t x, int8_t y, const uint8_t *BMP, uint8_t c) {
    uint8_t *p;
	uint8_t data=0,count=0;
    int16_t width,height;
	uint8_t const *b;
    width=pgm_read_byte(&BMP[0]);
    height=pgm_read_byte(&BMP[1])/8;
    b=&BMP[2];    
    if(BMP==0 || width<0 || y+height<=0) return;
    p= &Disp_send.DataAddress[(int16_t)((-x)*18)+y];
  	for (int16_t col=x ; col < x+width; col++) {
		for (int16_t row=y; row<y+height; row++) {
			if(count==0) {
				data = pgm_read_byte(b++);
				if(data==pgm_read_byte(b++)) {
					count = pgm_read_byte(b++);
				}
				else {
					count = 1;
					b--;
				}
			}
			count--;
            // Check if pixel stays within boundaries
            if(col>=0 && col<128 && row>=0 && row<16) {
                #ifdef INVERT_DISPLAY
                if(c==0)        *p &= ~data;
                else if(c==1)   *p |= ~data;
                else if(c==2)   *p &= data;
                else if(c==3)   *p |= data;
                else if(c==4)   *p ^= ~data;
                else            *p = ~data;                
                #else
                if(c==0)        *p &= data;
                else if(c==1)   *p |= data;
                else if(c==2)   *p &= ~data;
                else if(c==3)   *p |= ~data;
                else if(c==4)   *p ^= data;
                else            *p = data;
                #endif
            }
            p++;
		}
        p-=18+height;   // Next line
	}
}