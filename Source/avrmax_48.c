/*****************************************************************************

 Xmegalab / XMultiKit Chess application, ported from:

 ****************************************************************************
 *                               AVR-Max,                                   *
 * A chess program smaller than 2KB (of non-blank source), by H.G. Muller   *
 * AVR ATMega88V port, iterative Negamax, by Andre Adrian                   *
 ************************************************************************** *
 * Features:                                                                 *
 * - Iterative negamax search with alpha-beta pruning                        *
 * - All-capture MVV/LVA quiescence search                                   *
 * - Internal iterative deepening with best-move-first sorting               *
 * - Hash table storing score and best move                                  *
 * - Futility pruning and R=2 null-move pruning                              *
 * - King safety (magnetic frozen king in middle-game)                       *
 * - Repetition-draw detection                                               *
 * - LMR (Late Move Reduction) for non-pawn, non-capture moves               *
 * - Full FIDE rules (except under-promotion)                                *
 *****************************************************************************/

#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include "main.h"
#include "utils.h"

/* -------------------------------------------------------------------------
 * Board and search constants
 * ------------------------------------------------------------------------- */
#define OFF_BOARD   0x88    /* Mask for invalid bits in 0x88 board representation */
#define SIGN_BIT    128     /* Sign bit of unsigned char                           */
#define INF         8000    /* Infinity score (represents checkmate)               */

/* -------------------------------------------------------------------------
 * Piece tables (stored in flash)
 *
 * piece_values[]: relative piece values indexed by piece type (0..7)
 *   0=empty, 1=pawn, 2=knight, 3=bishop, 4=rook, 5=queen, 6=king
 *
 * move_vectors[]: step-vector lists for each piece, followed by two lookup tables:
 *   [0..16]  : step vectors (ray directions), terminated by 0
 *   [17..23] : first index in move_vectors[] for each piece type
 *   [24..31] : initial piece placement for back rank (used in board setup)
 * ------------------------------------------------------------------------- */
const signed char piece_values[] PROGMEM = {
    0, 2, 2, 7, -1, 8, 12, 23
};

const signed char move_vectors[] PROGMEM = {
    -16, -15, -17,  0,                      /* Rook/Queen directions (orthogonal/diagonal) */
      1,  16,   0,                           /* continued                                   */
      1,  16,  15,  17,  0,                  /* continued                                   */
     14,  18,  31,  33,  0,                  /* Knight jump offsets                         */
      7,  -1,  11,   6,   8,   3,   6,       /* First direction index per piece type        */
      6,   3,   5,   7,   4,   5,   3,   6   /* Initial back-rank piece layout              */
};

/* Flash-read accessors for the two tables */
#define piece_val(idx)  ((signed char)pgm_read_byte(piece_values + (idx)))
#define move_vec(idx)   ((signed char)pgm_read_byte(move_vectors + (idx)))

/* -------------------------------------------------------------------------
 * Engine global state
 *
 * These are shared with chess.c (GUI layer) via extern declarations there.
 * All single-letter originals renamed for clarity.
 * ------------------------------------------------------------------------- */
long     node_count;    /* Node counter; decremented each node, expires = time limit      */
signed int
         root_eval,     /* Eval score updated after each root move (passed back to GUI)   */
         input_from,    /* Human: origin square; CPU: set to INF as "computer move" flag  */
         cap_value,     /* Temporary: value of the piece being captured                   */
         lmr_score,     /* Score after Late Move Reduction evaluation                     */
         arg_alpha,     /* D() argument: alpha (lower bound) of search window             */
         arg_beta,      /* D() argument: beta (upper bound) of search window              */
         arg_eval,      /* D() argument: current eval score passed to child node          */
         search_result; /* D() return value: best score found by the search               */

unsigned char
         input_to,      /* Human: destination square; CPU: written by engine after search */
         ep_pass,       /* En passant square passed between root calls                    */
         side,          /* Side to move: 8 = white, 16 = black                            */
         board[129],    /* 0x88 board: 128 active squares + 1 dummy; pieces in low nibble */
         material,      /* Captured non-pawn material (tracks game phase)                 */
         tmp,           /* Scratch variable (board setup)                                 */
         arg_ep,        /* D() argument: en passant square                                */
         arg_root,      /* D() argument: non-zero at root node (level-1 flag)             */
         arg_depth,     /* D() argument: remaining search depth                           */
         arg_state;     /* D() argument: which label to resume at when returning          */

/* Shortcut to the working-state struct (T.CHESS._ is saved/restored on the
 * simulated call stack T.CHESS.SA, so it must remain a struct member).     */
#define _ T.CHESS._

/* -------------------------------------------------------------------------
 * chess_init_board() — set up the starting position in board[]
 *
 * Called by PlayChess() in chess.c to avoid exposing move_vectors[] there.
 * ------------------------------------------------------------------------- */
void chess_init_board(void) {
    uint8_t i;
    for (i = 0; i < sizeof board; ++i) board[i] = 0;
    tmp = 8;
    while (tmp--) {
        board[tmp]       = (board[tmp + 112] = move_vec(tmp + 24) + 8) + 8;
        board[tmp + 16]  = 18;
        board[tmp + 96]  = 9;
        input_to = 8;
        while (input_to--)
            board[16 * input_to + tmp + 8] =
                (tmp - 4) * (tmp - 4) +
                (input_to + input_to - 7) * (input_to + input_to - 7) / 4;
    }
}

/* -------------------------------------------------------------------------
 * D() — Iterative negamax search with simulated call stack
 *
 * Because the AVR has very limited hardware stack space, D() simulates
 * recursion with an explicit stack in T.CHESS.SA[].  A "recursive call" is
 * implemented by saving the working state to *T.CHESS.MP, decrementing MP,
 * loading the new arguments, then jumping to CALL:.  A "return" increments
 * MP and jumps to RETURN:, which dispatches to the saved resume label via
 * the callee's arg_state value (0 = null-move return, 1 = move-eval return).
 *
 * Arguments are passed in arg_* globals; the return value is search_result.
 * ------------------------------------------------------------------------- */
void D(void)
{
CALL:
    if (--T.CHESS.MP < T.CHESS.SA) {        /* Stack underrun: search too deep  */
        ++T.CHESS.MP;
        search_result = -arg_beta;           /* Return failure score             */
        goto RETURN;
    }

    /* Load call arguments into the working-state struct */
    _.q = arg_alpha;
    _.l = arg_beta;
    _.e = arg_eval;
    _.E = arg_ep;
    _.z = arg_root;
    _.n = arg_depth;
    _.a = arg_state;

    _.q--;              /* Shrink alpha by 1: implements the delayed-loss bonus    */
    side ^= 24;         /* Flip side to move (8 XOR 24 = 16, 16 XOR 24 = 8)       */
    _.d = _.Y = 0;      /* Reset iterative-deepening depth and best destination    */
    _.X = 0;            /* Start board scan from square 0                          */

    /* -----------------------------------------------------------------------
     * Iterative deepening loop.
     * Deepen until _.n plies, or (at root with CPU move) until node_count < 0.
     * When time runs out, commit the best move found and exit with _.d = 3.
     * ----------------------------------------------------------------------- */
    while (_.d++ < _.n || _.d < 3 ||
           (_.z & input_from == INF &&
            (node_count >= 0 & _.d < 98 ||
             (input_from = _.X, input_to = _.Y & ~OFF_BOARD, _.d = 3))))
    {
        _.x = _.B = _.X;        /* Start scan at the previous best-move origin     */
        _.h = _.Y & SIGN_BIT;   /* Flag: try stored best move before normal order  */

        if (_.d < 3) {
            /* Too shallow for null-move pruning; use INF as an "unset" sentinel */
            _.P = INF;
        } else {
            /* Null-move pruning: let the opponent "pass"; if they still can't   *
             * beat beta then our position is already strong enough to cut off.  */
            *T.CHESS.MP = _;        /* Save working state to simulated stack       */
            arg_alpha = -_.l;
            arg_beta  = 1 - _.l;
            arg_eval  = -_.e;
            arg_ep    = SIGN_BIT;   /* SIGN_BIT signals null move (no e.p. square) */
            arg_root  = 0;
            arg_depth = _.d - 3;
            arg_state = 0;          /* Resume at NULL_MOVE_RETURN after returning  */
            goto CALL;

NULL_MOVE_RETURN:
            _ = *T.CHESS.MP;        /* Restore working state from stack            */
            _.P = search_result;    /* Store null-move score for pruning decisions  */
        }

        /* Stand-pat / futility: if the null-move score already beats beta,  *
         * or if we are in the endgame, we can prune without a full search.  *
         * Otherwise use the negated null-move score as a lower bound.       */
        _.m = (-_.P < _.l | material > 35) ? (_.d > 2 ? -INF : _.e) : -_.P;
        --node_count;               /* Count this node toward the time budget      */

        /* -------------------------------------------------------------------
         * Board scan: iterate over all squares looking for own pieces.
         * The 0x88 trick (x+9 & ~OFF_BOARD) visits only valid squares.
         * ------------------------------------------------------------------- */
        do {
            _.u = board[_.x];
            if (_.u & side) {                   /* Square holds one of our pieces  */
                _.r = _.p = _.u & 7;            /* Low 3 bits = piece type (r > 0) */
                _.j = move_vec(_.p + 16);       /* First direction index for piece  */

                /* Loop over all move directions for this piece */
                while (_.r = _.p > 2 & _.r < 0 ? -_.r : -move_vec(++_.j))
                {
RESUME_BEST_MOVE:   /* Re-enter here to try the direction normally after the hash move */
                    _.y = _.x;
                    _.F = _.G = SIGN_BIT;       /* Init castling rook squares       */

                    /* -----------------------------------------------------------
                     * Ray traversal: slide along direction _.r until blocked.
                     * For jumpers (knights, king) the do-while body runs once.
                     * ----------------------------------------------------------- */
                    do {
                        /* If the hash/best-move flag is set, sneak it in first   */
                        _.H = _.y = _.h ? _.Y ^ _.h : _.y + _.r;

                        if (_.y & OFF_BOARD) break;     /* Hit edge of board       */

                        /* Disallow castling through an attacked square            */
                        _.m = (_.E - SIGN_BIT & board[_.E]) &&
                              _.y - _.E < 2 & _.E - _.y < 2 ? INF : _.m;

                        /* En passant: if pawn lands on the e.p. square, the      *
                         * captured pawn is one rank behind (shift capture sq H)  */
                        if (_.p < 3 & _.y == _.E) _.H ^= 16;

                        _.t = board[_.H];
                        /* Illegal: captures own piece, or pawn moves sideways    *
                         * without a capture, or pawn advances onto occupied sq   */
                        if (_.t & side | _.p < 3 & !(_.y - _.x & 7) - !_.t) break;

                        /* Value of captured piece (37× piece value + colour bits) */
                        cap_value = 37 * piece_val(_.t & 7) + (_.t & 192);
                        _.m = cap_value < 0 ? INF : _.m;    /* King capture flag   */
                        if (_.m >= _.l & _.d > 1) goto FAIL_HIGH; /* Beta cutoff   */

                        /* MVV/LVA base score: use capture value at quiescence,   *
                         * otherwise inherit the parent's eval for deeper search  */
                        _.v = _.d - 1 ? _.e : cap_value - _.p;

                        if (_.d - !_.t > 1) {   /* Sufficient depth for full eval  */

                            /* Positional adjustment by piece type and game phase  */
                            _.v = _.p - 4 | material > 29
                                ? (_.p - 7 | material > 7
                                    ? (_.p < 6
                                        ? board[_.x + 8] - board[_.y + 8] /* Center table */
                                        : 0)                               /* King/Queen: none */
                                    : -12)                                 /* Early queen penalty */
                                : -9;                                      /* Mid-game king penalty */

                            /* Execute move: clear origin/capture, place piece    */
                            board[_.G] = board[_.H] = board[_.x] = 0;
                            board[_.y] = _.u | 32;  /* Bit 5 = "piece has moved"   */

                            if (!(_.G & OFF_BOARD)) {
                                board[_.F] = side + 6;  /* Castling: place rook    */
                                _.v += 30;              /* Castling score bonus     */
                            }

                            if (_.p < 3) {  /* Pawn-specific evaluation            */
                                /* Penalise isolated/doubled pawns and clinging    *
                                 * to a non-virgin king; bonus for endgame pushes  */
                                _.v -= 9 * (
                                    (_.x - 2 & OFF_BOARD || board[_.x - 2] - _.u) +
                                    (_.x + 2 & OFF_BOARD || board[_.x + 2] - _.u) - 1
                                    + (board[_.x ^ 16] == side + 36)
                                ) - (material >> 2);

                                /* Promotion bonus, or 6th/7th rank advance bonus  */
                                _.V = _.y + _.r + 1 & SIGN_BIT
                                        ? 647 - _.p
                                        : 2 * (_.u & _.y + 16 & 32);
                                board[_.y] += _.V;
                                cap_value  += _.V;
                            }

                            _.v += _.e + cap_value;         /* Running eval total  */
                            _.V = _.m > _.q ? _.m : _.q;    /* New alpha           */

                            /* LMR: reduce depth for quiet, non-pawn, late moves   */
                            _.C = _.d - 1 - (_.d > 5 & _.p > 2 & !_.t & !_.h);
                            /* Extend one ply when in check                        */
                            _.C = material > 29 | _.d < 3 | _.P - INF ? _.C : _.d;

                            do {
                                if (_.C > 2 | _.v > _.V) {
                                    /* Recursive call: search opponent's replies   */
                                    *T.CHESS.MP = _;
                                    arg_alpha = -_.l;
                                    arg_beta  = -_.V;
                                    arg_eval  = -_.v;
                                    arg_ep    = _.F;
                                    arg_root  = 0;
                                    arg_depth = _.C;
                                    arg_state = 1;  /* Resume at RECURSIVE_RETURN */
                                    goto CALL;

RECURSIVE_RETURN:
                                    _ = *T.CHESS.MP;        /* Restore caller state */
                                    lmr_score = -search_result;
                                    ONGRN();    /* Blink green LED while thinking   */
                                } else {
                                    lmr_score = _.v;    /* Futile: use stand-pat    */
                                }
                            } while (lmr_score > _.q & ++_.C < _.d);

                            _.v = lmr_score;

                            /* Root node: check if the human's submitted move      *
                             * matches the current (from, to); if so, it's legal   */
                            if (_.z && input_from - INF && _.v + INF
                                    && _.x == input_from & _.y == input_to)
                            {
                                root_eval  = -_.e - cap_value;
                                ep_pass    = _.F;
                                material  += cap_value >> 7;  /* Track material    */
                                ++T.CHESS.MP;
                                search_result = _.l;          /* Signal: legal     */
                                goto RETURN;
                            }

                            /* Undo move */
                            board[_.G] = side + 6;
                            board[_.F] = board[_.y] = 0;
                            board[_.x] = _.u;
                            board[_.H] = _.t;
                        }

                        /* Update best move if this one scored higher */
                        if (_.v > _.m) {
                            _.m = _.v;
                            _.X = _.x;
                            _.Y = _.y | SIGN_BIT & _.F;  /* SIGN_BIT encodes double step */
                        }

                        /* Done with hash move: redo the direction without it     */
                        if (_.h) { _.h = 0; goto RESUME_BEST_MOVE; }

                        /* For sliders: continue ray. For jumpers/first-step: stop *
                         * by faking a capture (t != 0 exits the do-while).        *
                         * Also check castling eligibility here.                   */
                        if (_.x + _.r - _.y | _.u & 32 |
                            _.p > 2 & (_.p - 4 | _.j - 7 ||
                                board[_.G = _.x + 3 ^ _.r >> 1 & 7] - side - 6
                                || board[_.G ^ 1] | board[_.G ^ 2]))
                            _.t += _.p < 5;     /* Fake capture: stop non-slider  */
                        else
                            _.F = _.y;          /* Remember square for e.p.        */

                        OFFGRN();
                    } while (!_.t);     /* Slide until blocked (or jumper stops)   */
                }
            }
        } while ((_.x = _.x + 9 & ~OFF_BOARD) - _.B); /* Next valid square, wrap */

FAIL_HIGH:
        /* If best score is a mate score, no need to search deeper */
        if (_.m > INF - OFF_BOARD | _.m < OFF_BOARD - INF)
            _.d = 98;

        /* Stalemate / checkmate: if the best move still loses the king, score 0  */
        _.m = _.m + INF | _.P == INF ? _.m : 0;

    } /* end iterative deepening while */

    side ^= 24;                                 /* Restore side to move            */
    ++T.CHESS.MP;
    search_result = (_.m += _.m < _.e);         /* Delayed-loss bonus + return val  */

RETURN:
    /* Simulated function return: dispatch to caller's resume label using the
     * callee's arg_state (set before the goto CALL that created this frame).  */
    if (T.CHESS.MP != T.CHESS.SA + U) {
        switch (_.a) {
            case 0: goto NULL_MOVE_RETURN;
            case 1: goto RECURSIVE_RETURN;
        }
    }
    OFFGRN();
}
