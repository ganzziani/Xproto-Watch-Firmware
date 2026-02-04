#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include "main.h"
#include "utils.h"
#include "games.h"

static void InitSnake(void);
static void NewFruit(void);
static void MoveSnake(SnakeStruct *Player);
static void SnakeEngine(void);

void InitSnake(void) {
    uint16_t k=0;
    for(uint8_t i=0; i<32; i++) {
        for(uint8_t j=0; j<32; j++) {
            T.SNAKE.board[i][j]=0;
            T.SNAKE.Player1.x[k]=0;
            T.SNAKE.Player1.y[k]=0;
            T.SNAKE.Player2.x[k]=0;
            T.SNAKE.Player2.y[k]=0;
            k++;
        }
    }
    T.SNAKE.Player1.x[0]=T.SNAKE.Player1.x[1]=2;
    T.SNAKE.Player1.y[0]=T.SNAKE.Player1.y[1]=2;
    T.SNAKE.Player1.size=2;
    T.SNAKE.Player1.direction = GO_RIGHT;
    T.SNAKE.Player2.x[0]=T.SNAKE.Player2.x[1]=29;
    T.SNAKE.Player2.y[0]=T.SNAKE.Player2.y[1]=29;
    T.SNAKE.Player2.size=2;
    T.SNAKE.Player2.direction = GO_LEFT;
    T.SNAKE.Fruit = 0;
}

// Spawn new fruit
void NewFruit(void) {
    // Linear Pixel Shuffling for Image Processing
    #define G1 41
    #define G2 60
    #define Sincrement (G2 - G1)
    do {    /* determine next pixel */
        T.SNAKE.Fruitx = ( T.SNAKE.Fruitx + Sincrement ) % G1;
        T.SNAKE.Fruity = ( T.SNAKE.Fruity + Sincrement ) % G2;
    } while ((T.SNAKE.Fruitx >=32) || (T.SNAKE.Fruity >= 32) ||   // Out of bounds
      (T.SNAKE.board[T.SNAKE.Fruitx][T.SNAKE.Fruity]!=EMPTY)); // Occupied
    T.SNAKE.board[T.SNAKE.Fruitx][T.SNAKE.Fruity] = FRUIT;       // New head position
    if(TCC0.PER>250) TCC0.PERBUF=multfix(TCC0.PER, float2fix(0.9));   // Speed up
    T.SNAKE.Fruit++;
}

void Snake(void) {
    uint8_t p1=HUMAN_PLAYER1, p2=NO_PLAYER2;
    T.SNAKE.Fruitx = prandom()>>3;
    T.SNAKE.Fruity = prandom()>>3;
    setbit(MStatus,update);
    clrbit(MStatus, goback);
    clr_display();
    do {
        if(testbit(MStatus, update)) {
            clrbit(MStatus, update);
            if(testbit(Misc,userinput)) {
                clrbit(Misc, userinput);
                if(testbit(Buttons, K1)) p1++;
                if(p1>CPU_PLAYER1) p1=NO_PLAYER1;
                if(testbit(Buttons, K2)) p2++;
                if(p2>CPU_PLAYER2) p2=NO_PLAYER2;
                if(testbit(Buttons, K3)) {
                    T.SNAKE.Player1.state = p1;
                    T.SNAKE.Player2.state = p2;
                    SnakeEngine();
                }                    
                if(testbit(Buttons, KML)) setbit(MStatus, goback);
            }
            lcd_goto(49,0); print5x8(PSTR("Snake"));
            lcd_goto(0,2); print5x8(STR_Player); print5x8(STR_P1);
            if(p1==NO_PLAYER1) print5x8(&STRS_mainmenu[2][9]);  // STRS_mainmenu[2][9] contains the text "-  "
            else if(p1==HUMAN_PLAYER1) print5x8(STR_Human);
            if(p1==CPU_PLAYER1) print5x8(STR_CPU);
            lcd_goto(0,3); print5x8(STR_Player); print5x8(STR_P2);
            if(p2==NO_PLAYER2) print5x8(&STRS_mainmenu[2][9]);
            else if(p2==HUMAN_PLAYER2) print5x8(STR_Human);
            if(p2==CPU_PLAYER2) print5x8(STR_CPU);
            lcd_goto(0,15); print5x8(STR_GameMenu);
        }
        dma_display();
        WaitDisplay();
        SLP();          // Sleep
    } while(!testbit(MStatus, goback));
    setbit(MStatus, update);
}

// Arrays to select only one direction, if more than one direction was selected
// Select rightmost bit
const uint8_t Direction1[16] PROGMEM = {
    GO_UP,          // 0000
    GO_UP,          // 0001
    GO_DOWN,        // 0010
    GO_UP,          // 0011
    GO_LEFT,        // 0100
    GO_UP,          // 0101
    GO_DOWN,        // 0110
    GO_UP,          // 0111
    GO_RIGHT,       // 1000
    GO_UP,          // 1001
    GO_DOWN,        // 1010
    GO_UP,          // 1011
    GO_LEFT,        // 1100
    GO_UP,          // 1101
    GO_DOWN,        // 1110
    GO_UP,          // 1111
};

// Arrays to select only one direction, if more than one direction was selected
// Select lefttmost bit
const uint8_t Direction2[16] PROGMEM = {
    GO_RIGHT,       // 0000
    GO_UP,          // 0001
    GO_DOWN,        // 0010
    GO_DOWN,        // 0011
    GO_LEFT,        // 0100
    GO_LEFT,        // 0101
    GO_LEFT,        // 0110
    GO_LEFT,        // 0111
    GO_RIGHT,       // 1000
    GO_RIGHT,       // 1001
    GO_RIGHT,       // 1010
    GO_RIGHT,       // 1011
    GO_RIGHT,       // 1100
    GO_RIGHT,       // 1101
    GO_RIGHT,       // 1110
    GO_RIGHT,       // 1111
};

void MoveSnake(SnakeStruct *Player) {
    if(Player->state==CPU_PLAYER1 || Player->state==CPU_PLAYER2) {                          // CPU AI
        uint8_t FruitDirection=0;
        uint8_t GoodDirection=0x0F;
        if(Player->y[0]>T.SNAKE.Fruity) FruitDirection|=GO_UP;                           // Find fruit
        if(Player->y[0]<T.SNAKE.Fruity) FruitDirection|=GO_DOWN;
        if(Player->x[0]>T.SNAKE.Fruitx) FruitDirection|=GO_LEFT;
        if(Player->x[0]<T.SNAKE.Fruitx) FruitDirection|=GO_RIGHT;
        if(T.SNAKE.board[Player->x[0]][Player->y[0]-1]>FRUIT || Player->y[0]==0)  GoodDirection&=~GO_UP;     // Avoid obstacles and out of bounds
        if(T.SNAKE.board[Player->x[0]][Player->y[0]+1]>FRUIT || Player->y[0]==31) GoodDirection&=~GO_DOWN;
        if(T.SNAKE.board[Player->x[0]-1][Player->y[0]]>FRUIT || Player->x[0]==0)  GoodDirection&=~GO_LEFT;
        if(T.SNAKE.board[Player->x[0]+1][Player->y[0]]>FRUIT || Player->x[0]==31) GoodDirection&=~GO_RIGHT;
        FruitDirection &= GoodDirection;                                                    // Don't follow fruit thru obstacles
        if(FruitDirection) {                                                                // A good path to fruit exists
            if((FruitDirection & Player->direction)==0) {                                   // Snake not following fruit already
                Player->direction=FruitDirection;
            }
        } else Player->direction=GoodDirection;                                             // Otherwise, just avoid walls
        // Make sure only one direction is selected
        if(prandom()&0x01) Player->direction = pgm_read_byte(Direction1+Player->direction);
        else Player->direction = pgm_read_byte(Direction2+Player->direction);
    }
    if(Player->state) { // Snake alive
        T.SNAKE.board[Player->x[Player->size-1]][Player->y[Player->size-1]] = 0;             // Erase tail from board
        for(uint16_t i=Player->size-1; i>0; i--) {                                           // Drag body
            Player->x[i]=Player->x[i-1];
            Player->y[i]=Player->y[i-1];
        }
        T.SNAKE.board[Player->x[Player->size-1]][Player->y[Player->size-1]] = Player->state;     // New tail position
        switch(Player->direction) {                                         // Move head
            case GO_UP:    Player->y[0]--; break;
            case GO_DOWN:  Player->y[0]++; break;
            case GO_LEFT:  Player->x[0]--; break;
            case GO_RIGHT: Player->x[0]++; break;
        }
        if(T.SNAKE.board[Player->x[0]][Player->y[0]]>=OBSTACLE ||        // New position is an obstacle or a body
            Player->x[0]>=32 || Player->y[0]>=32) {                         // Out of boundaries
            Player->state = 0;                                              // Died
            return;
        }            
        T.SNAKE.board[Player->x[0]][Player->y[0]] = Player->state;       // New head position
        if(T.SNAKE.Fruitx==Player->x[0] && T.SNAKE.Fruity==Player->y[0]) {  // Got Fruit!
            if(Player->size<=(32*32)) {
                for(uint8_t i=0; i<T.SNAKE.Fruit; i++) {
                    Player->x[Player->size+i]=Player->x[Player->size-1];          // Extend tail
                    Player->y[Player->size+i]=Player->y[Player->size-1];
                }                    
                Player->size+=T.SNAKE.Fruit;
                NewFruit();
            }                
        }
    } else {
        if(Player->size>3) {    // Shrink if dead
            T.SNAKE.board[Player->x[Player->size-1]][Player->y[Player->size-1]] = 0; // Erase tail from board
            Player->size--;
        }            
    }        
}

void SnakeBoard(void) {
    uint8_t x,y;
    for(uint8_t i=0; i<32; i++) {
        for(uint8_t j=0; j<32; j++) {
            x=i*4; y=j*4;
            switch(T.SNAKE.board[i][j]) {
                case  EMPTY:
                break;
                case FRUIT:
                                      set_pixel(x+1,y  ); set_pixel(x+2,y  ); 
                    set_pixel(x,y+1);                                         set_pixel(x+3,y+1);
                    set_pixel(x,y+2);                                         set_pixel(x+3,y+2);
                                      set_pixel(x+1,y+3); set_pixel(x+2,y+3); 
                break;
                case HUMAN_PLAYER1:
                case CPU_PLAYER1:
                    set_pixel(x,y  ); set_pixel(x+1,y  ); set_pixel(x+2,y  ); set_pixel(x+3,y  );
                    set_pixel(x,y+1);                                         set_pixel(x+3,y+1);
                    set_pixel(x,y+2);                                         set_pixel(x+3,y+2);
                    set_pixel(x,y+3); set_pixel(x+1,y+3); set_pixel(x+2,y+3); set_pixel(x+3,y+3);
                break;
                case HUMAN_PLAYER2:
                case CPU_PLAYER2:
                    set_pixel(x,y  ); set_pixel(x+1,y  ); set_pixel(x+2,y  ); set_pixel(x+3,y  );
                    set_pixel(x,y+1); set_pixel(x+1,y+1); set_pixel(x+2,y+1); set_pixel(x+3,y+1);
                    set_pixel(x,y+2); set_pixel(x+1,y+2); set_pixel(x+2,y+2); set_pixel(x+3,y+2);
                    set_pixel(x,y+3); set_pixel(x+1,y+3); set_pixel(x+2,y+3); set_pixel(x+3,y+3);
                break;
                default:
                                      set_pixel(x+1,y  );                     set_pixel(x+3,y  );
                    set_pixel(x,y+1);                     set_pixel(x+2,y+1); 
                                      set_pixel(x+1,y+2);                     set_pixel(x+3,y+2);
                    set_pixel(x,y+3);                     set_pixel(x+2,y+3);
                break;
            }
        }
    }
}

void SnakeEngine(void) {
    PR.PRPC  &= 0b11111110;         // Enable TCC0 clock
    TCC0.PER = 15626;               // 1Hz
    TCC0.CTRLA = 5;                 // 31.25kHz clock
    InitSnake();
    NewFruit();
    clrbit(MStatus, goback);
    setbit(MStatus,update);
    do {
        if(testbit(Misc,userinput)) {
            clrbit(Misc, userinput);
            if(testbit(Buttons, KML)) setbit(MStatus, goback);
            if(testbit(Buttons, KBL)) {
                if(T.SNAKE.Player1.direction==GO_UP) T.SNAKE.Player1.direction=GO_LEFT;
                else if(T.SNAKE.Player1.direction==GO_DOWN) T.SNAKE.Player1.direction=GO_RIGHT;
                else if(T.SNAKE.Player1.direction==GO_LEFT) T.SNAKE.Player1.direction=GO_DOWN;
                else if(T.SNAKE.Player1.direction==GO_RIGHT) T.SNAKE.Player1.direction=GO_UP;
            }
            else if(testbit(Buttons, KBR)) {
                if(T.SNAKE.Player1.direction==GO_UP) T.SNAKE.Player1.direction=GO_RIGHT;
                else if(T.SNAKE.Player1.direction==GO_DOWN) T.SNAKE.Player1.direction=GO_LEFT;
                else if(T.SNAKE.Player1.direction==GO_LEFT) T.SNAKE.Player1.direction=GO_UP;
                else if(T.SNAKE.Player1.direction==GO_RIGHT) T.SNAKE.Player1.direction=GO_DOWN;
            }
            if(testbit(Buttons, KUL)) {
                if(T.SNAKE.Player2.direction==GO_UP) T.SNAKE.Player2.direction=GO_LEFT;
                else if(T.SNAKE.Player2.direction==GO_DOWN) T.SNAKE.Player2.direction=GO_RIGHT;
                else if(T.SNAKE.Player2.direction==GO_LEFT) T.SNAKE.Player2.direction=GO_DOWN;
                else if(T.SNAKE.Player2.direction==GO_RIGHT) T.SNAKE.Player2.direction=GO_UP;
            }
            else if(testbit(Buttons, KUR)) {
                if(T.SNAKE.Player2.direction==GO_UP) T.SNAKE.Player2.direction=GO_RIGHT;
                else if(T.SNAKE.Player2.direction==GO_DOWN) T.SNAKE.Player2.direction=GO_LEFT;
                else if(T.SNAKE.Player2.direction==GO_LEFT) T.SNAKE.Player2.direction=GO_UP;
                else if(T.SNAKE.Player2.direction==GO_RIGHT) T.SNAKE.Player2.direction=GO_DOWN;
            }
        }
        if(TCC0.INTFLAGS&0x01) {    // Limit speed
            TCC0.INTFLAGS=1;
            clr_display();
            if(T.SNAKE.Player1.state!=NO_PLAYER1) MoveSnake(&T.SNAKE.Player1);
            if(T.SNAKE.Player2.state!=NO_PLAYER2) MoveSnake(&T.SNAKE.Player2);
            SnakeBoard();
            WaitDisplay();    
            dma_display();
            SwitchBuffers();
            if((T.SNAKE.Player1.state==0 || T.SNAKE.Player1.state==NO_PLAYER1) &&
               (T.SNAKE.Player2.state==0 || T.SNAKE.Player2.state==NO_PLAYER2)) {
                setbit(MStatus, goback);
                lcd_goto(38,13); print5x8(PSTR("Game Over"));
            }
        }
    } while(!testbit(MStatus, goback));
    clrbit(MStatus, goback);
    setbit(MStatus, update);
    TCC0.CTRLA = 0;
    PR.PRPC |= 0b00000001;         // Disable TCC0 TCC1C clocks
}
