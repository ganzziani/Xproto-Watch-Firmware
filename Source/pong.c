#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include "main.h"
#include "utils.h"
#include "games.h"

void InitPong(void);
void NewPoint(void);
void MovePaddle(PaddleStruct *Player);
void PongEngine(void);
void PongBoard(void);

void InitPong(void) {
    T.PONG.Player1.x1=34;
    T.PONG.Player1.x2=94;
    T.PONG.Player1.y=127;
    T.PONG.Player1.points=0;
    T.PONG.Player2.x1=34;
    T.PONG.Player2.x2=94;
    T.PONG.Player2.y=0;
    T.PONG.Player2.points=0;
}

// New point
void NewPoint(void) {
    T.PONG.ballx = int2fix(64);
    T.PONG.bally = int2fix(64);
    do {    // Minimum initial speed x
        T.PONG.speedx = (int)128-qrandom();
    } while(T.PONG.speedx<4 && T.PONG.speedx>-4);
    do {    // Minimum initial speed y
        T.PONG.speedy = (int)128-qrandom();
    } while(T.PONG.speedy<128 && T.PONG.speedy>-128);
}

void Pong(void) {
    uint8_t p1=HUMAN_PLAYER1, p2=HUMAN_PLAYER2;
    setbit(MStatus,update);
    clrbit(WSettings, goback);
    clr_display();
    do {
        if(testbit(MStatus, update)) {
            clrbit(MStatus, update);
            if(testbit(Misc,userinput)) {
                clrbit(Misc, userinput);
                if(testbit(Buttons, K1)) p1++; if(p1>CPU_PLAYER1) p1=HUMAN_PLAYER1;
                if(testbit(Buttons, K2)) p2++; if(p2>CPU_PLAYER2) p2=HUMAN_PLAYER2;
                if(testbit(Buttons, K3)) {
                    T.PONG.Player1.state = p1;
                    T.PONG.Player2.state = p2;
                    PongEngine();
                }                    
                if(testbit(Buttons, KML)) setbit(WSettings, goback);
            }
            lcd_goto(55,0); print5x8(PSTR("Pong"));
            lcd_goto(0,2); print5x8(STR_Player); print5x8(STR_P1);
            if(p1==HUMAN_PLAYER1) print5x8(STR_Human);
            else print5x8(STR_CPU);
            lcd_goto(0,3); print5x8(STR_Player); print5x8(STR_P2);
            if(p2==HUMAN_PLAYER2) print5x8(STR_Human);
            else print5x8(STR_CPU);
            lcd_goto(0,15); print5x8(STR_GameMenu);
        }
        dma_display();
        WaitDisplay();
        SLP();          // Sleep
    } while(!testbit(WSettings, goback));
    setbit(MStatus, update);
}

void MovePaddle(PaddleStruct *Player) {
    uint8_t x=(Player->x1+Player->x2)>>1;                       // Calculate center of the paddle
    PaddleStruct *OtherPlayer;
    if(Player==&T.PONG.Player1) OtherPlayer=&T.PONG.Player2;
    else OtherPlayer = &T.PONG.Player1;
    if(Player->state==CPU_PLAYER1 || Player->state==CPU_PLAYER2) {  // CPU AI
        uint8_t bx;
        if((Player->state==CPU_PLAYER1 && T.PONG.speedy>0) ||         // Ball approaching my paddle
           (Player->state==CPU_PLAYER2 && T.PONG.speedy<0)) bx=fix2int(T.PONG.ballx);
        else bx=64;                                                 // Otherwise, center paddle
        if(x>bx && Player->x1>0)   { Player->x1--; Player->x2--; }
        if(x<bx && Player->x2<127) { Player->x1++; Player->x2++; }
    }
    uint8_t bx=fix2int(T.PONG.ballx);
    if((Player==&T.PONG.Player2 && T.PONG.speedy<0 && T.PONG.bally<int2fix(4)) ||     // Top check
       (Player==&T.PONG.Player1 && T.PONG.speedy>0 && T.PONG.bally>int2fix(123))) {   // Bottom check
        if(bx>=Player->x1 && bx<=Player->x2) {              // Bounce
            T.PONG.speedy =-T.PONG.speedy;
            T.PONG.speedx = multfix(T.PONG.speedx,float2fix(1.25));
            T.PONG.speedy = multfix(T.PONG.speedy,float2fix(1.25));
            if(bx==Player->x1)  T.PONG.speedx -=256;        // Maximum paddle effect
            else if(bx==Player->x2)  T.PONG.speedx +=256;   // Maximum paddle effect
            else T.PONG.speedx += 2*((int16_t)bx-x);        // Paddle effect
        }
        else {
            OtherPlayer->points++;
            OtherPlayer->x1+=2;     // Reduce paddle
            OtherPlayer->x2-=2;
            NewPoint();
        }
    }
}

void PongBoard(void) {
    uint8_t x = fix2int(T.PONG.ballx);
    uint8_t y = fix2int(T.PONG.bally);
    lcd_goto(0,7); print5x8(PSTR("Player 1:"));
    lcd_goto(10,8); printN_5x8(T.PONG.Player1.points);
    lcd_goto(76,7); print5x8(PSTR("Player 2:"));
    lcd_goto(96,8); printN_5x8(T.PONG.Player2.points);
    lcd_hline(T.PONG.Player1.x1, T.PONG.Player1.x2, T.PONG.Player1.y,PIXEL_SET);
    lcd_hline(T.PONG.Player2.x1, T.PONG.Player2.x2, T.PONG.Player2.y,PIXEL_SET);
    set_pixel(x-1,y-3); set_pixel(x,y-3  ); set_pixel(x+1,y-3);
    set_pixel(x-2,y-2); set_pixel(x+2,y-2);
    set_pixel(x-3,y-1); set_pixel(x+3,y-1);
    set_pixel(x-3,y  ); set_pixel(x+3,y  );
    set_pixel(x-3,y+1); set_pixel(x+3,y+1);
    set_pixel(x-2,y+2); set_pixel(x+2,y+2);
    set_pixel(x-1,y+3); set_pixel(x,y+3  ); set_pixel(x+1,y+3);
}

void PongEngine(void) {
    InitPong();
    NewPoint();
    clrbit(WSettings, goback);
    setbit(MStatus,update);
    do {
        uint8_t in = PORTF.IN;              // Read buttons
        if(testbit(in, KML)) setbit(WSettings, goback);
        if(testbit(in, KBL)) {
            if(T.PONG.Player1.x1) {
                T.PONG.Player1.x1--;
                T.PONG.Player1.x2--;
            }                
        }
        if(testbit(in, KBR)) {
            if(T.PONG.Player1.x2<127) {
                T.PONG.Player1.x1++;
                T.PONG.Player1.x2++;
            }
        }
        if(testbit(in, KUL)) {
            if(T.PONG.Player2.x1) {
                T.PONG.Player2.x1--;
                T.PONG.Player2.x2--;
            }
        }
        if(testbit(in, KUR)) {
            if(T.PONG.Player2.x2<127) {
                T.PONG.Player2.x1++;
                T.PONG.Player2.x2++;
            }
        }
        if(T.PONG.speedy>int2fix(4)) T.PONG.speedy=float2fix(3.9);         // Limit speed
        if(T.PONG.speedy<int2fix(-4)) T.PONG.speedy=float2fix(-3.9);
        if(T.PONG.speedx>int2fix(4)) T.PONG.speedx=float2fix(3.9);         // Limit speed
        if(T.PONG.speedx<int2fix(-4)) T.PONG.speedx=float2fix(-3.9);
        T.PONG.ballx+=T.PONG.speedx;
        T.PONG.bally+=T.PONG.speedy;
        if((T.PONG.speedx>0 && T.PONG.ballx>int2fix(123)) ||
           (T.PONG.speedx<0 && T.PONG.ballx<int2fix(4))) { // Side wall bounce
            T.PONG.speedx=-T.PONG.speedx;
        }
        clr_display();
        MovePaddle(&T.PONG.Player1);
        MovePaddle(&T.PONG.Player2);
        PongBoard();
        lcd_goto(27,13);
        if(T.PONG.Player1.points>=10) {
            setbit(WSettings, goback);
            print5x8(PSTR("Player 1 Wins"));
        }
        if(T.PONG.Player2.points>=10) {
            setbit(WSettings, goback);
            print5x8(PSTR("Player 2 Wins"));
        }
        WaitDisplay();
        dma_display();
        SwitchBuffers();
    } while(!testbit(WSettings, goback));
    clrbit(WSettings, goback);
    SwitchBuffers();    // Keep last buffer used
    clrbit(Misc, userinput);
    setbit(MStatus, update);
}
