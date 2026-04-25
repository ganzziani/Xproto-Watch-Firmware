#ifndef _QIX_H
#define _QIX_H

#include <stdint.h>
#include "mygccdef.h"
#include "LS013B7DH03.h"

// Game states
#define STATE_MENU      0
#define STATE_INIT      1
#define STATE_INITLEVEL 3
#define STATE_LEVELUP   4
#define STATE_SPAWN     5
#define STATE_PLAYING   6
#define STATE_DIED      7
#define STATE_GAMEOVER  8

// Game constants
#define STYX_MAX        2       // Maximum Styx creatures
#define STYX_LINES      8       // Line segments per Styx
#define TRAP_MAX        2       // Maximum traps
#define DOT_MAX         60      // Maximum trap dots
#define MAX_TRAIL       255     // Maximum trail length
// Capture completion: playfield interior is 124 x 117 = 14508 fillable pixels.
// 80 % of 14508 ≈ 11607 pixels required to advance level.
#define TOTAL_FILLABLE       14508      // (DISPLAY_MAX_X-3) * (DISPLAY_MAX_Y - UI_TOP_MARGIN - 2)
#define CAPTURE_GOAL_PIXELS  11606      // ~80 % of TOTAL_FILLABLE
// captureProgress is on a 0–127 scale so that scaling to any bar width is a shift.
#define STACK_MAX       960     // Flood fill stack size (increased for complex fills)
#define MAX_IDLE_TIME   250     // Max idle time before death

// Styx movement constants
#define STYX_RETRY_COUNT        3       // Number of retries to find valid line position
#define STYX_BOUNDARY_MARGIN    3       // Minimum distance from screen edge for Styx
#define STYX_MAX_DEFAULT        20      // Maximum Styx line vector length (multiplier; larger = longer lines)
#define STYX_SPEED              30      // Velocity divisor base: dx = int2fix(Sin(dir))/(STYX_SPEED-rand)
                                        // rand is 0-15, so actual divisor range is (STYX_SPEED-15)..STYX_SPEED
                                        // Higher value = slower Styx movement

// Trap growth constants  
#define TRAP_INITIAL_GROWTH     8       // Initial trap length before random growth
#define TRAP_RANDOM_GROWTH_LEVEL 3      // Level at which random trap growth begins
#define TRAP_RANDOM_GROWTH_CHANCE 200   // Random threshold for trap growth (0-255)

// Fill algorithm constants
#define FILL_STACK_OVERFLOW_CHECK 2     // Stack entries to reserve as safety margin

// UI/constants from display
#define UI_TOP_MARGIN           8       // Top margin for UI elements
#define UI_BOTTOM_MARGIN        7       // Bottom margin for playfield

// Directions
#define DIR_NONE        0
#define DIR_UP          1
#define DIR_DOWN        2
#define DIR_LEFT        3
#define DIR_RIGHT       4
#define DIR_CW          5       // Clockwise (Perimeter)
#define DIR_CCW         6       // Counter-Clockwise (Perimeter)

// Man Action
#define MAN_WALKING     1
#define MAN_DRAWING     2

// Man structure
typedef struct {
    uint8_t lives;
    fixed x;                // Fixed-point position
    fixed y;
    uint8_t direction;      // Current movement direction
    uint8_t old_direction;  // Previous movement direction
    uint8_t action;         // Walking or drawing
    fixed speed;            // Movement speed (fixed point)
    uint8_t idle;           // Idle counter
    uint8_t trailX[MAX_TRAIL];
    uint8_t trailY[MAX_TRAIL];
    uint8_t trail_len;
    uint8_t lastDir;        // Last direction for collision avoidance
} ManStruct;

// Styx structure
typedef struct {
    uint8_t active;         // Styx is active
    fixed x;                // Base position X
    fixed y;                // Base position Y
    fixed dx;               // Base velocity X
    fixed dy;               // Base velocity Y
    uint8_t vect_dir;       // Direction of the line vector (0-255, 256=cycle)
    uint8_t vect_length;    // Length of the line vector (0-255, 256=cycle)
    int8_t  vect_dirinc;    // Vector rotation increment
    uint8_t max_vec_length; // Maximum line vector length
    int8_t  vect_inc;       // Rotation speed of the line
    uint8_t line_len;       // Length of the current line segment
    uint8_t line_idx;       // Current line segment index in history
    // Styx line history: x1, y1, x2, y2 for each segment
    uint8_t line_x1[STYX_LINES];
    uint8_t line_y1[STYX_LINES];
    uint8_t line_x2[STYX_LINES];
    uint8_t line_y2[STYX_LINES];
} StyxStruct;

// Trap structure
typedef struct {
    uint8_t active;
    uint8_t direction;
    uint8_t length;
    uint8_t x[DOT_MAX];
    uint8_t y[DOT_MAX];
    uint8_t head_idx;
    uint8_t tail_idx;
    uint8_t stuck_count;
} TrapStruct;

typedef struct {
    uint16_t score;
    uint8_t level;
    uint8_t styx_active;
    uint8_t traps_active;
    uint8_t gameState;
    uint16_t filled_pixels;
    uint8_t captureProgress;      // 0–127 (127 ≈ 100 % captured)
    uint8_t score_multiplier;
    uint8_t multiplier_timer;
    uint8_t StyxSpeed;
    ManStruct Man;
    StyxStruct Styx[STYX_MAX];
    TrapStruct Traps[TRAP_MAX];
    // Layer_Filled holds every "solid" pixel: perimeter walls, drawn walls, and
    // captured-interior pixels. A pixel is a "wall" (walkable boundary) iff it
    // is filled AND has at least one empty 4-neighbor (see is_wall() in qix.c).
    uint8_t LayerFilled[DISPLAY_DATA_SIZE];
    uint8_t LayerStyx[DISPLAY_DATA_SIZE];
    uint8_t FillStack[2][STACK_MAX];
} structQIX;

void Qix(void);

#endif