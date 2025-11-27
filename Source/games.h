#ifndef _GAMES_H
#define _GAMES_H

// Snake definitions

#define GO_UP     1
#define GO_DOWN   2
#define GO_LEFT   4
#define GO_RIGHT  8

#define EMPTY           0
#define FRUIT           1
#define OBSTACLE        2
#define NO_PLAYER1      3
#define HUMAN_PLAYER1   4
#define CPU_PLAYER1     5
#define NO_PLAYER2      6
#define HUMAN_PLAYER2   7
#define CPU_PLAYER2     8

typedef struct {
    uint8_t x[32*32];
    uint8_t y[32*32];
    uint8_t direction;
    uint8_t state;
    uint16_t size;
} SnakeStruct;

typedef struct {
    uint8_t points;
    uint8_t x1;
    uint8_t x2;
    uint8_t y;
    uint8_t state;
} PaddleStruct;
        
void Snake(void);
void Pong(void);
void Chess(void);

#endif