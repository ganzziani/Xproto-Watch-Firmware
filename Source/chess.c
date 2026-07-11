/*****************************************************************************
 * Chess GUI — menu, board rendering, and game loop
 *
 * This file contains only the user-interface layer for the chess game:
 *   Chess()      — configuration menu (level and player selection)
 *   printboard() — draws the 8×8 board with piece sprites
 *   Cursor()     — draws/erases/toggles the selection rectangle
 *   PlayChess()  — main game loop, input handling, and move dispatch
 *
 * The chess engine (move generation and search) is in avrmax_48.c.
 *****************************************************************************/

#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include "main.h"
#include "display.h"
#include "utils.h"
#include "games.h"
#include "bitmaps.h"
#include "strings.h"

/* -------------------------------------------------------------------------
 * Engine globals declared in avrmax_48.c
 * ------------------------------------------------------------------------- */
extern uint16_t     node_count;
extern signed int   root_eval;
extern signed int   input_from;
extern signed int   search_result;
extern unsigned char input_to;
extern unsigned char ep_pass;
extern unsigned char side;
extern unsigned char board[129];
extern unsigned char material;
extern signed int   arg_alpha;
extern signed int   arg_beta;
extern signed int   arg_eval;
extern unsigned char arg_ep;
extern unsigned char arg_root;
extern unsigned char arg_depth;
extern unsigned char arg_state;
extern unsigned char no_moves;
extern uint16_t      pos_key;

/* Forward declarations */
void PlayChess(void);

/* Engine functions */
void ChessEngine(void);
void chess_init_board(void);

/* Infinity score — must match the value defined in avrmax_48.c */
#define INF 8000

/* -------------------------------------------------------------------------
 * printboard() — render the full board from the engine's board[] array
 * ------------------------------------------------------------------------- */
void printboard(void) {
    uint8_t c, i = 0, color = 0, piece, x, y;
    clr_display();

    /* Draw checkerboard background */
    for (x = 0; x < 120; x += 15) {
        for (y = 0; y < 128; y += 16) {
            #ifdef INVERT_DISPLAY
            if ( testbit(color, 0)) fillRectangle(x, y, x + 14, y + 15, PIXEL_SET);
            #else
            if (!testbit(color, 0)) fillRectangle(x, y, x + 14, y + 15, PIXEL_SET);
            #endif
            color++;
        }
        color++;
    }

    /* Place piece sprites on the board */
    x = 1; y = 0; color = 0;
    do {
        if (i & 8) {
            /* Skip the invalid (0x88) half of each rank row */
            i += 7; y += 2; x = 1; color++;
        } else {
            /* Decode piece type from board[].  The string maps nibble values to
             * sprite indices: '@'=0 (empty), 'A'=1...'L'=12 for piece types.   */
            piece = ("@@GHIJKL@A@BCDEF"[board[i] & 15]) - '@';
            if (piece) {
                c = testbit(color, 0);
                if (piece > 6) {
                    piece -= 6;
                    #ifdef INVERT_DISPLAY
                    c += 2;
                    #endif
                    if ( testbit(color, 0)) { piece += 6; }
                } else {
                    #ifndef INVERT_DISPLAY
                    c += 2;
                    #endif
                    if (!testbit(color, 0)) { piece += 6; }
                }
                bitmap_safe(x, y, (uint8_t *)pgm_read_word(ChessBMPs + piece), c);
            }
            x += 15;
            color++;
        }
    } while (++i < 128);
}

/* -------------------------------------------------------------------------
 * Cursor() — draw, erase, or toggle a selection rectangle on a board square
 *
 * x, y : board coordinates (0..7)
 * c    : 0 = erase, 1 = draw, 2 = toggle
 * ------------------------------------------------------------------------- */
void Cursor(uint8_t x, uint8_t y, uint8_t c) {
    uint8_t color;
    #ifdef INVERT_DISPLAY
    color = PIXEL_SET;
    if (c == 0) { if ((x + y + 1) & 0x01) color = PIXEL_CLR; }  /* Erase */
    if (c == 1) { if ((x + y)     & 0x01) color = PIXEL_CLR; }  /* Draw  */
    #else
    color = PIXEL_CLR;
    if (c == 0) { if ((x + y + 1) & 0x01) color = PIXEL_SET; }  /* Erase */
    if (c == 1) { if ((x + y)     & 0x01) color = PIXEL_SET; }  /* Draw  */
    #endif
    if (c == 2) { color = PIXEL_TGL; }                          /* Toggle */
    Rectangle(x * 15 + 1, y * 16 + 1, x * 15 + 13, y * 16 + 14, color);
}

/* -------------------------------------------------------------------------
 * Chess() — configuration menu
 *
 * Lets the user choose White/Black as Human or CPU, set the difficulty
 * level (0–15), and start the game.
 * ------------------------------------------------------------------------- */
void Chess(void) {
    uint8_t p = 2;          /* Player-type selector: bit0=White, bit1=Black CPU */
    setbit(MStatus, update);
    clrbit(MStatus, goback);
    T.CHESS.level = 1;
    clr_display();
    do {
        if (testbit(MStatus, update)) {
            clrbit(MStatus, update);
            if (testbit(Misc, userinput)) {
                clrbit(Misc, userinput);
                if (testbit(Buttons, K1)) { p++; if (p > 3) p = 0; }
                if (testbit(Buttons, K2)) { if (++T.CHESS.level > 9) T.CHESS.level = 1; }
                if (testbit(Buttons, K3)) { PlayChess(); clr_display(); }
                if (testbit(Buttons, KML)) setbit(MStatus, goback);
            }

            /* Draw menu */
            lcd_goto(50, 0); print5x8(&STRS_optionmenu[2][14]);   /* "Chess" */
            lcd_goto(0,  2); print5x8(STR_White);
            if (p & 0x01) {  print5x8(STR_CPU);    T.CHESS.Player1 = CPU_PLAYER1;   }
            else          {  print5x8(STR_Human);  T.CHESS.Player1 = HUMAN_PLAYER1; }
            lcd_goto(0,  3); print5x8(STR_Black);
            if (p & 0x02) {  print5x8(STR_CPU);    T.CHESS.Player2 = CPU_PLAYER1;   }
            else          {  print5x8(STR_Human);  T.CHESS.Player2 = HUMAN_PLAYER1; }
            lcd_goto(0,  4); print5x8(STR_Level);  printN_5x8(T.CHESS.level);
            lcd_goto(0, 15); print5x8(STR_GameMenu2);
        }
        dma_display();
        WaitDisplay();
        SLP();
    } while (!testbit(MStatus, goback));
    setbit(MStatus, update);
}

/* -------------------------------------------------------------------------
 * CallEngine() — invoke the engine on the current position
 *
 * Set input_from beforehand: a square (human move to validate & commit),
 * INF (CPU: search and commit the best move), or -1 (probe only: report
 * no_moves/score without committing anything).
 * ------------------------------------------------------------------------- */
static void CallEngine(void) {
    no_moves  = 0;
    arg_alpha = -INF;
    arg_beta  =  INF;
    arg_eval  = root_eval;
    arg_ep    = ep_pass;
    arg_root  = 1;
    arg_depth = 3;
    arg_state = 0;
    ChessEngine();
}

/* -------------------------------------------------------------------------
 * PlayChess() — main game loop
 *
 * Alternates between human input and engine search.  Renders the board
 * after each move.  Exits when checkmate, stalemate, or a threefold-
 * repetition draw is detected (after showing the result) or when the
 * user presses the back button.
 * ------------------------------------------------------------------------- */
void PlayChess(void) {
    uint8_t newx = 0, newy = 0, oldx = 0, oldy = 0;
    uint8_t selx = 255, sely = 255;     /* Currently selected (from) square; 255 = none */
    uint8_t lastx = 255, lasty = 255;   /* Destination of the previous move              */
    uint8_t *player;
    const char *result = PSTR("Checkmate");

    player = &T.CHESS.Player1;
    T.CHESS.MP = T.CHESS.SA + U;        /* Initialize engine stack pointer (empty stack) */
    clrbit(MStatus, goback);

    /* Reset engine state */
    side = 16;
    search_result = ep_pass = root_eval = material = 0;

    /* Set up starting position (clears board[] and places pieces) */
    chess_init_board();

    do {
        while (search_result > -INF + 1 && !testbit(MStatus, goback)) {
            printboard();

            /* Draw side-to-move indicator squares in the margin */
            if (player == &T.CHESS.Player1) {
                Rectangle(122,   2, 127,   7, PIXEL_CLR);
                Rectangle(122, 120, 127, 125, PIXEL_SET);
            } else {
                Rectangle(122,   2, 127,   7, PIXEL_SET);
                Rectangle(122, 120, 127, 125, PIXEL_CLR);
            }

            /* Highlight the destination square of the last move */
            if (lastx < 8 && lasty < 8) Cursor(lastx, lasty, 1);

            if (testbit(Buttons, KML)) setbit(MStatus, goback);
            if (testbit(MStatus, goback)) break;

            if (*player == HUMAN_PLAYER1) {
                /* --------------------------------------------------------
                 * Human player: poll input until a complete (from→to) move
                 * is entered.  K2 selects/deselects squares.
                 * -------------------------------------------------------- */
                clrbit(WatchBits, enter);   /* Reuse 'enter' bit as "move ready" flag */

                do {
                    if (testbit(Misc, userinput)) {
                        clrbit(Misc, userinput);
                        if (testbit(Buttons, KML)) setbit(MStatus, goback);
                        if (testbit(Buttons, K1))  newx--;
                        if (testbit(Buttons, K3))  newx++;
                        if (testbit(Buttons, KUR)) newy--;
                        if (testbit(Buttons, KBR)) newy++;

                        if (testbit(Buttons, K2)) {
                            if (newx == selx && newy == sely) {
                                /* Deselect */
                                Cursor(selx, sely, 0);
                                selx = 255; sely = 255;
                            } else {
                                if (selx == 255 || sely == 255) {
                                    /* First press: choose the piece to move */
                                    input_from = newx + newy * 16;
                                    selx = newx; sely = newy;
                                } else {
                                    /* Second press: choose the destination  */
                                    input_to = newx + newy * 16;
                                    selx = 255; sely = 255;
                                    setbit(WatchBits, enter); /* Signal: move ready */
                                }
                                Cursor(selx, sely, 1);
                            }
                        }
                    }

                    /* Wrap cursor at board edges */
                    if (newx == 255) newx = 7; else if (newx > 7) newx = 0;
                    if (newy == 255) newy = 7; else if (newy > 7) newy = 0;

                    /* Erase old cursor position; leave it drawn if it is selected */
                    if (oldx != newx || oldy != newy) {
                        uint8_t color = 0;
                        if (oldx == selx && oldy == sely) color = 1;
                        Cursor(oldx, oldy, color);
                        oldx = newx; oldy = newy;
                    }

                    Cursor(newx, newy, 2);  /* Blink cursor at current position */
                    dma_display();
                    WaitDisplay();
                    OFFRED();
                    SLP();
                } while (!(testbit(MStatus, goback) || testbit(WatchBits, enter)));

            } else {
                /* --------------------------------------------------------
                 * CPU player: set input_from = INF as the "computer move"
                 * signal; the engine writes the chosen move to input_to.
                 * -------------------------------------------------------- */
                input_from = INF;
                node_count = 127U << T.CHESS.level; /* Thinking time (exponential, max 65024) */
                node_count += qrandom();            /* Add slight randomness       */
                dma_display();
            }

            CallEngine();

            if (search_result == INF) {         /* Move committed: pass the turn */
                if (player == &T.CHESS.Player1) player = &T.CHESS.Player2;
                else                            player = &T.CHESS.Player1;
                /* Record the destination square to highlight on the next frame */
                lastx = input_to & 0x07;
                lasty = input_to >> 4;

                /* Threefold repetition: the engine recorded the new position;
                 * count exact occurrences among the stored positions.         */
                {
                    uint8_t i, reps = 0, n = T.CHESS.hist_n;
                    if (n > 16) n = 16;
                    for (i = 0; i < n; i++) {
                        if (T.CHESS.histk[i] == pos_key) {
                            uint8_t *hb = T.CHESS.histb[i];
                            uint8_t a = 0;
                            reps++;
                            do {    /* Verify with a full board comparison       */
                                if (!(a & 8) && *hb++ != board[a]) {
                                    reps--;
                                    break;
                                }
                            } while (++a < 120);
                        }
                    }
                    if (reps >= 3) { result = PSTR("   Draw  "); break; }
                }

                /* Probe: can the new side to move reply at all?  Announces
                 * mate/stalemate right after the deciding move.               */
                input_from = -1;
                CallEngine();
                if (no_moves) {                 /* Game over                    */
                    if (search_result > -INF + 1) result = PSTR("Stalemate");
                    break;
                }
            } else if (*player == HUMAN_PLAYER1) {
                ONRED();        /* Illegal move: flash red LED */
            }   /* else: CPU search aborted without a move; retry or exit       */
        }

        /* Game over (unless the user quit): show result, wait for back button  */
        if (!testbit(MStatus, goback)) {
            printboard();
            lcd_goto(37, 7);
            print5x8(result);
            do {
                dma_display();
                WaitDisplay();
                SLP();
            } while (!testbit(Buttons, KML));
            while (testbit(Buttons, KML)) SLP();    /* Wait for release so the   */
            break;                                  /* chess menu ignores the key */
        }
    } while (!testbit(MStatus, goback));
}
