#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <stdlib.h>
#include <string.h>
#include "main.h"
#include "utils.h"
#include "qix.h"

// Sound Tunes stored in Flash Memory
static const uint8_t TuneWallStart[] PROGMEM = { 10, NOTE_C6, 0 };
static const uint8_t TuneStuck[] PROGMEM = { 30, NOTE_G6, 0 };
static const uint8_t TuneSpawn[] PROGMEM = { 5, NOTE_C6, 5, NOTE_E6, 5, NOTE_G6, 5, NOTE_C7, 5, NOTE_E7, 5, NOTE_G7, 5, NOTE_C8, 0 };
static const uint8_t TuneDead[] PROGMEM = { 20, NOTE_C7, 10, NOTE_OFF, 20, NOTE_G6, 10, NOTE_OFF, 20, NOTE_C6, 0 };
static const uint8_t TuneLevelUp[] PROGMEM = { 20, NOTE_C6, 20, NOTE_E6, 20, NOTE_G6, 20, NOTE_C7, 0 };
static const uint8_t TuneFill[] PROGMEM = { 5, NOTE_C6, 5, NOTE_D6, 5, NOTE_E6, 5, NOTE_F6, 5, NOTE_G6, 5, NOTE_A6, 5, NOTE_B6, 5, NOTE_C7, 0 };
static const uint8_t TuneStyxClose[] PROGMEM = { 5, NOTE_C7, 5, NOTE_E7, 0 };
static const uint8_t TuneStyxFar[] PROGMEM = { 10, NOTE_C6, 0 };
    
static void QixEngine(void);
static void HandleInput(void);          // Checks Buttons
static void MovePlayer(void);           // Updates Man
static uint8_t PlayerInput(uint8_t motion, uint8_t x, uint8_t y);   // Find direction based on player's input
static void Perimeter_Movement(uint8_t *direction, uint8_t x, uint8_t y, uint8_t traversal); // Finds next direction along wall perimeter
static void SpawnPlayer(void);          // Spawn animation and variable update
static void KillPlayer(void);           // Death animation
static void InitTraps(uint8_t level);   // Initialize Traps
static void MoveTraps(void);            // Updates Traps positions
static void InitStyx(uint8_t level);    // Initialize Styx
static void NewStyxDirection(uint8_t n); // Change direction and line size
static uint8_t CheckStyxCollision(uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2);  // Check if Styx line hits wall or trail
static void MoveStyx(void);             // Updates Styx positions
static void DoFill(void);               // Flood fill algorithm (adapted from TD.C)
static uint8_t fright(uint8_t x, uint8_t y);  // Fill right into Layer_Styx
static uint8_t fleft(uint8_t x, uint8_t y);   // Fill left into Layer_Styx
static uint8_t scanne(uint8_t x, uint8_t y, uint8_t xmax);  // Scan for non-boundary pixel
static uint8_t scaneq(uint8_t x, uint8_t y, uint8_t xmax);  // Scan for boundary pixel
static uint8_t popcount8(uint8_t b);     // Count set bits in a byte
static void DrawGame(void);             // Render display

// Precomputed direction deltas (index 0 unused)
static const int8_t dx[5] = {0,  0,  0, -1,  1};
static const int8_t dy[5] = {0, -1,  1,  0,  0};

// Direction lookup tables (Index: 1=DIR_UP  2=DIR_DOWN  3=DIR_LEFT  4=DIR_RIGHT)
static const uint8_t dir_left[5]  = {0, DIR_LEFT,  DIR_RIGHT, DIR_DOWN, DIR_UP};
static const uint8_t dir_right[5] = {0, DIR_RIGHT, DIR_LEFT,  DIR_UP,   DIR_DOWN};
static const uint8_t dir_back[5]  = {0, DIR_DOWN,  DIR_UP,    DIR_RIGHT,DIR_LEFT};

// Buffer pointer offsets for display functions.
// The display is rotated 90 degrees, so the lower-level pixel/line functions 
// expect pointers at a specific offset within the buffer. This offset compensates
// for the rotation by pre-adjusting the base pointer position.
// Offset = 127 * DISPLAY_BYTES_IN_ROW positions the pointer correctly for the 
// rotated coordinate system used by set_line_buffer, toggle_line_buffer, etc.
#define Layer_Wall      (T.QIX.LayerWall+127*DISPLAY_BYTES_IN_ROW)
#define Layer_Filled    (T.QIX.LayerFilled+127*DISPLAY_BYTES_IN_ROW)
#define Layer_Styx      (T.QIX.LayerStyx+127*DISPLAY_BYTES_IN_ROW)

// Qix game menu
void Qix(void) {
    setbit(MStatus,update);
    clrbit(MStatus, goback);
    clr_display();
    do {
        if(testbit(MStatus, update)) {
            clrbit(MStatus, update);
            if(testbit(Misc,userinput)) {
                clrbit(Misc, userinput);
                //if(testbit(Buttons, K1)) 
                //if(testbit(Buttons, K2))
                if(testbit(Buttons, K3)) QixEngine();
                if(testbit(Buttons, KML)) setbit(MStatus, goback);
            }
        	lcd_goto(0,0);
            lcd_goto(55,7); print5x8(PSTR("Qix"));
            lcd_goto(95,15); print5x8(&STRS_optionmenu[1][16]);    // STRS_optionmenu[1][16] contains the word "Start"
        }
        dma_display();
        WaitDisplay();
        SLP();          // Sleep
    } while(!testbit(MStatus, goback));
    setbit(MStatus, update);
}

// Game engine main loop
void QixEngine(void) {
    T.QIX.gameState = STATE_INIT;
    clrbit(MStatus, goback);
    setbit(MStatus,update);
    do {
        if(testbit(Buttons, KML)) T.QIX.gameState = STATE_GAMEOVER; // Exit to menu
        switch(T.QIX.gameState) {
            case STATE_INIT:    // Initialize Game
                T.QIX.level = 0;
                T.QIX.score = 0;
                T.QIX.Man.lives = 3;
                T.QIX.gameState = STATE_LEVELUP;
                T.QIX.Man.trailX[0] = DISPLAY_MAX_X / 2;   // Center of screen
                T.QIX.Man.trailY[0] = DISPLAY_MAX_Y-1;     // On the bottom border
            break;
            case STATE_LEVELUP:
                // Clear layers
                for(uint16_t i = 0; i < DISPLAY_DATA_SIZE; i++) {
                    T.QIX.LayerWall[i] = T.QIX.LayerFilled[i] = T.QIX.LayerStyx[i] = 0;
                }
                // Draw main borders (leave top area for UI)
                set_line_buffer(1, 8, DISPLAY_MAX_X-1, 8, Layer_Wall);
                set_line_buffer(1, DISPLAY_MAX_Y-1, DISPLAY_MAX_X-1, DISPLAY_MAX_Y-1, Layer_Wall);
                set_line_buffer(1, 8, 1, DISPLAY_MAX_Y-1, Layer_Wall);
                set_line_buffer(DISPLAY_MAX_X-1, 8, DISPLAY_MAX_X-1, DISPLAY_MAX_Y-1, Layer_Wall);
                // Outside of the main border will be considered filled, useful for functions that check boundaries
                set_line_buffer(0, 7, DISPLAY_MAX_X, 7, Layer_Filled);
                set_line_buffer(0, DISPLAY_MAX_Y, DISPLAY_MAX_X, DISPLAY_MAX_Y, Layer_Filled);
                set_line_buffer(0, 7, 0, DISPLAY_MAX_Y, Layer_Filled);
                set_line_buffer(DISPLAY_MAX_X, 7, DISPLAY_MAX_X, DISPLAY_MAX_Y, Layer_Filled);

                Sound(TuneLevelUp);
                T.QIX.level++;
                T.QIX.filled_pixels = 0;
                T.QIX.capturedPercent = 0;
                // Initialize score multiplier system
                T.QIX.gameState = STATE_SPAWN;
                InitTraps(T.QIX.level);
                InitStyx(T.QIX.level);
                break;
            case STATE_SPAWN:
                DrawGame();         // Render display
                SpawnPlayer();      // Spawn animation and variables update
                T.QIX.gameState = STATE_PLAYING;
            break;
            case STATE_PLAYING:
                HandleInput();      // Updates p.dx/dy/state
                MovePlayer();       // Updates p.x/y
                MoveStyx();         // Updates Styx positions
                MoveTraps();        // Update Traps positions
                DrawGame();         // Render display
            break;
            case STATE_DIED:
                KillPlayer();       // Death animation
                T.QIX.Man.lives--;
                if(T.QIX.Man.lives==0) T.QIX.gameState = STATE_GAMEOVER;
                else T.QIX.gameState = STATE_SPAWN;
            break;
            case STATE_GAMEOVER:
                if(testbit(MStatus, update)) {   // Wait for keypress
                    setbit(MStatus, goback);
                }                    
            break;
        }        
        WaitDisplay();
        dma_display();
        SwitchBuffers();
    } while(!testbit(MStatus, goback));
    clrbit(MStatus, goback);
    clrbit(Misc, userinput);
    setbit(MStatus, update);
}

// Updates Man state
static void HandleInput(void) {
    uint8_t direction = T.QIX.Man.direction;
    if(Buttons==0) T.QIX.Man.direction = DIR_NONE;
    if(testbit(MStatus, update)) {
        clrbit(MStatus, update);
        if(testbit(Buttons, K1)) T.QIX.Man.speed = float2fix(0.5);      // Slow
        if(testbit(Buttons, K2)) T.QIX.Man.speed = float2fix(0.75);     // Medium
        if(testbit(Buttons, K3)) T.QIX.Man.speed = float2fix(1.0);        // Fast
        if(T.QIX.Man.action==MAN_DRAWING) { // Drawing mode controls
            if (testbit(Buttons, KUL)) {    // Turn left
                direction = dir_left[T.QIX.Man.old_direction];
            }
            else if (testbit(Buttons, KUR)) { // Turn right
                direction = dir_right[T.QIX.Man.old_direction];
            }
            if(T.QIX.Man.direction) {   // Save last non-zero direction
                T.QIX.Man.old_direction = direction;
            }                
        } else {    // Walking mode controls
            uint8_t x = fix2int(T.QIX.Man.x);
            uint8_t y = fix2int(T.QIX.Man.y);
            // Start/stop drawing with top buttons
            if (testbit(Buttons, KUL) || testbit(Buttons, KUR)) {
                T.QIX.Man.action = MAN_DRAWING;
                Sound(TuneWallStart);
                // Find direction perpendicular to wall
                if(get_pixel_buffer(x,y-1,Layer_Filled)) {
                    direction = DIR_DOWN;
                } else if(get_pixel_buffer(x,y+1,Layer_Filled)) {
                    direction = DIR_UP;
                } else if(get_pixel_buffer(x+1,y,Layer_Filled)) {
                    direction = DIR_LEFT;
                } else if(get_pixel_buffer(x-1,y,Layer_Filled)) {
                    direction = DIR_RIGHT;
                }
                T.QIX.Man.old_direction = direction;
                T.QIX.Man.trailX[0] = x;
                T.QIX.Man.trailY[0] = y;
                T.QIX.Man.trail_len = 1;
                T.QIX.Man.idle = 0;
            } else {    // Perimeter movement
                uint8_t motion = 0;
                if (testbit(Buttons, KBL)) motion = DIR_CCW;
                else if (testbit(Buttons, KBR)) motion = DIR_CW;
                else return;
                T.QIX.Man.lastDir = motion;
                direction = PlayerInput(motion, x, y);
            }                    
        }
    }
    T.QIX.Man.direction = direction;
}

// Find next direction based on the player's input
static uint8_t PlayerInput(uint8_t motion, uint8_t x, uint8_t y) {
    uint8_t candidate[2];
    uint8_t count = 0;
    // Find the two wall-connected directions
    for (uint8_t d = 1; d <= 4; d++) {
        uint8_t nx = x + dx[d];
        uint8_t ny = y + dy[d];
        if (get_pixel_buffer(nx, ny, Layer_Wall)) {
            candidate[count++] = d;
            if (count == 2) break;
        }
    }
    // We assume valid perimeter ? exactly 2 neighbors
    uint8_t d0 = candidate[0];
    uint8_t d1 = candidate[1];
    // Determine which one satisfies motion rule
    // For a given direction, check which side has FILLED
    uint8_t test = d0;
    uint8_t side = (motion == DIR_CW)
        ? dir_right[test]
        : dir_left[test];
    uint8_t sx = x + dx[side];
    uint8_t sy = y + dy[side];
    if (get_pixel_buffer(sx, sy, Layer_Filled)) {
        return d0;
    }
    else {
        return d1;
    }
}

// Updates Man position and trail
static void MovePlayer(void) {
    // Get current position
    uint8_t x = fix2int(T.QIX.Man.x);
    uint8_t y = fix2int(T.QIX.Man.y);
    // If drawing, add point to trail
    if (T.QIX.Man.action == MAN_DRAWING) {
        switch(T.QIX.Man.direction) {
            case DIR_UP:    T.QIX.Man.y -= T.QIX.Man.speed; break;
            case DIR_DOWN:  T.QIX.Man.y += T.QIX.Man.speed; break;
            case DIR_LEFT:  T.QIX.Man.x -= T.QIX.Man.speed; break;
            case DIR_RIGHT: T.QIX.Man.x += T.QIX.Man.speed; break;
            default:        T.QIX.Man.direction = DIR_NONE;
        }
        // Check if idle for too long while drawing
        if(T.QIX.Man.direction==DIR_NONE) {
            if(T.QIX.Man.idle<255) T.QIX.Man.idle++;
            if(T.QIX.Man.idle>=MAX_IDLE_TIME) {
                T.QIX.gameState = STATE_DIED;
            }
        } else T.QIX.Man.idle = 0;

        // Check collision with Styx while drawing
        uint8_t new_x = fix2int(T.QIX.Man.x);
        uint8_t new_y = fix2int(T.QIX.Man.y);
        
        // New position is different than trail head
        if((x!=new_x) || (y!=new_y)) {
            // Check if hitting own trail while drawing
            for(uint8_t i=0; i<T.QIX.Man.trail_len; i++) {
                if(new_x==T.QIX.Man.trailX[i] && new_y==T.QIX.Man.trailY[i]) {
                    T.QIX.gameState = STATE_DIED;
                    return;
                }
            }
            // Add new pixel to the trail
            if(T.QIX.Man.trail_len < MAX_TRAIL) {
                T.QIX.Man.trailX[T.QIX.Man.trail_len] = new_x;
                T.QIX.Man.trailY[T.QIX.Man.trail_len] = new_y;
                T.QIX.Man.trail_len++;
            } else { // If trail is too long, kill player
                T.QIX.gameState = STATE_DIED;
                return;
            }
            
            // Check if hit existing wall (completed area)
            if(get_pixel_buffer(new_x, new_y, Layer_Wall)) {
                // Transfer trail to the Wall and Filled buffers
                for(uint8_t i=0; i<T.QIX.Man.trail_len; i++) {
                    set_pixel_buffer(T.QIX.Man.trailX[i], T.QIX.Man.trailY[i], Layer_Wall);
                    set_pixel_buffer(T.QIX.Man.trailX[i], T.QIX.Man.trailY[i], Layer_Filled);
                }
                DoFill();
                T.QIX.Man.idle = 0;
                T.QIX.Man.trail_len = 0;
                T.QIX.Man.action = MAN_WALKING;
                T.QIX.Man.direction = DIR_NONE;
            }                
        }
    } else { // Man walking, speed is fixed
        // Update position
        if(T.QIX.Man.direction != DIR_NONE) {
            Perimeter_Movement(&T.QIX.Man.direction, x, y, T.QIX.Man.lastDir);
            uint8_t dir = T.QIX.Man.direction;
            x+=dx[dir];
            y+=dy[dir];
            T.QIX.Man.x = int2fix(x);
            T.QIX.Man.y = int2fix(y);
        }            
        // Check collision with Traps while walking
        for(uint8_t i=0; i<T.QIX.traps_active; i++) {
            if(!T.QIX.Traps[i].active) continue;
            for(uint8_t j=0; j<T.QIX.Traps[i].length; j++) {
                if(x==T.QIX.Traps[i].x[j] && y==T.QIX.Traps[i].y[j]) {
                    T.QIX.gameState = STATE_DIED;
                }
            }                                    
        }
    }
}

// Finds next direction when moving along wall perimeter
// This function is used by the Man when not drawing, and by Traps
// traversal: DIR_CW or DIR_CCW determines which side the wall is on
// Scan order: inner side -> ahead -> outer side -> back
// The first direction that has a wall pixel is chosen
static void Perimeter_Movement(uint8_t *direction, uint8_t x, uint8_t y, uint8_t traversal) {
    uint8_t dir = *direction;
    if (dir < 1 || dir > 4) return;

    // Scan order depends on traversal direction:
    // CW:  wall is on the left, scan: left -> ahead -> right -> back
    // CCW: wall is on the right, scan: right -> ahead -> left -> back
    uint8_t scan[4];
    if (traversal == DIR_CW) {
        scan[0] = dir_left[dir];
        scan[1] = dir;
        scan[2] = dir_right[dir];
        scan[3] = dir_back[dir];
    } else {
        scan[0] = dir_right[dir];
        scan[1] = dir;
        scan[2] = dir_left[dir];
        scan[3] = dir_back[dir];
    }

    for (uint8_t i = 0; i < 4; i++) {
        uint8_t d = scan[i];
        if (get_pixel_buffer(x + dx[d], y + dy[d], Layer_Wall)) {
            *direction = d;
            return;
        }
    }
}

// Spawn player: animation and variables update
static void SpawnPlayer(void) {
    Sound(TuneSpawn);
    // Locate player on last good position
    T.QIX.Man.x = int2fix(T.QIX.Man.trailX[0]);
    T.QIX.Man.y = int2fix(T.QIX.Man.trailY[0]);
    T.QIX.Man.direction = DIR_NONE;
    T.QIX.Man.action = MAN_WALKING;
    T.QIX.Man.trail_len = 0;
    T.QIX.Man.idle = 0;
    T.QIX.Man.speed = float2fix(1.0);
    // Perform Spawn animation: circle with decreasing radius on the Man's position
    // Get current position
    uint8_t x = fix2int(T.QIX.Man.x);
    uint8_t y = fix2int(T.QIX.Man.y);
    for(uint8_t i=120; i>0; i-=4) {
        for(uint8_t j=0; j<2; j++) {
            lcd_circle(x, y, i, PIXEL_TGL);
            WaitDisplay();
            dma_display();
        }
    }
}

// Handle player death
static void KillPlayer(void) {
    Sound(TuneDead);
    // Perform Kill animation: circle with increasing radius on the Man's position
    // Get current position
    uint8_t x = fix2int(T.QIX.Man.x);
    uint8_t y = fix2int(T.QIX.Man.y);
    for(uint8_t i=0; i<120; i+=4) {
        for(uint8_t j=0; j<2; j++) {
            lcd_circle(x, y, i, PIXEL_TGL);
            WaitDisplay();
            dma_display();
        }
    }
}

// Initialize Traps
static void InitTraps(uint8_t level) {
    if(level < 2) {
        T.QIX.traps_active = 0;
        } else if(level < 4) {
        T.QIX.traps_active = 1;
        } else {
        T.QIX.traps_active = 2;
    }
    for(uint8_t i = 0; i < T.QIX.traps_active; i++) {
        T.QIX.Traps[i].length = (level/4)+1;
        uint8_t r = prandom();
        if(testbit(r,7)) {
            T.QIX.Traps[i].head_idx = DIR_CW;
            T.QIX.Traps[i].direction = DIR_RIGHT;
        } else {
            T.QIX.Traps[i].head_idx = DIR_CCW;
            T.QIX.Traps[i].direction = DIR_LEFT;
        }
        for(uint8_t j=0; j<T.QIX.Traps[i].length; j++) {
            T.QIX.Traps[i].x[j] = 32 + (r&0x3F);    // Random x location between 32 and 95
            T.QIX.Traps[i].y[j] = 8;                // y location: top wall
        }
    }
}

// Updates Traps positions
// Worms crawl along the edges of filled areas, leaving a trail of dots
static void MoveTraps(void) {
    for (uint8_t i = 0; i < T.QIX.traps_active; i++) {
        if(!T.QIX.Traps[i].active) continue;
        
        uint8_t old_x = T.QIX.Traps[i].x[0];
        uint8_t old_y = T.QIX.Traps[i].y[0];
        
        // Move trap head along edges of filled areas using perimeter movement
        Perimeter_Movement(&T.QIX.Traps[i].direction, old_x, old_y, T.QIX.Traps[i].head_idx);
        
        // Get new head position after perimeter movement calculation
        uint8_t dir = T.QIX.Traps[i].direction;
        uint8_t new_x = old_x + dx[dir];
        uint8_t new_y = old_y + dy[dir];
        
        // Check if trap is stuck (can't find valid move along filled edge)
        uint8_t ahead_filled = get_pixel_buffer(new_x, new_y, Layer_Filled);
        if(!ahead_filled) {
            T.QIX.Traps[i].stuck_count++;
            if(T.QIX.Traps[i].stuck_count > 50) {
                // Trap is stuck - relocate to a random position on filled edge
                T.QIX.Traps[i].stuck_count = 0;
                // Find a new starting point on the filled area boundary
                for(uint8_t attempt = 0; attempt < 20; attempt++) {
                    uint8_t rx = prandom() >> 1;    // rx: [0, 127]
                    uint8_t ry = prandom() >> 1;    // ry: [0, 127]
                    if(get_pixel_buffer(rx, ry, Layer_Filled)) {
                        // Check if adjacent to empty space (boundary)
                        if(!get_pixel_buffer(rx+1, ry, Layer_Filled) ||
                           !get_pixel_buffer(rx-1, ry, Layer_Filled) ||
                           !get_pixel_buffer(rx, ry+1, Layer_Filled) ||
                           !get_pixel_buffer(rx, ry-1, Layer_Filled)) {
                            new_x = rx;
                            new_y = ry;
                            break;
                        }
                    }
                }
            }
        } else {
            T.QIX.Traps[i].stuck_count = 0;  // Reset stuck counter on successful move
        }
        
        // Update head position
        T.QIX.Traps[i].x[0] = (uint8_t)new_x;
        T.QIX.Traps[i].y[0] = (uint8_t)new_y;
        
        // Shift body segments to follow head (each segment follows the one ahead)
        // Process in reverse order so each segment takes the position of the one before it
        for(uint8_t j = T.QIX.Traps[i].length - 1; j > 0; j--) {
            T.QIX.Traps[i].x[j] = T.QIX.Traps[i].x[j-1];
            T.QIX.Traps[i].y[j] = T.QIX.Traps[i].y[j-1];
        }
        
        // Grow trap at higher levels (from original Styx TB.C)
        // Simplified: grow if length < 8, or randomly based on level
        uint8_t level = T.QIX.level;
        uint8_t shouldGrow = 0;
        
        // Always grow until length reaches 8
        if(T.QIX.Traps[i].length < 8) {
            shouldGrow = 1;
        } else if(level > 3 && prandom() > 200) {
            // At higher levels, occasional random growth (approx 22% chance per frame when level > 3)
            shouldGrow = 1;
        }
        
        if(shouldGrow && T.QIX.Traps[i].length < DOT_MAX - 1) {
            T.QIX.Traps[i].length++;
        }
      
        // Check collision with player while walking
        uint8_t px = fix2int(T.QIX.Man.x);
        uint8_t py = fix2int(T.QIX.Man.y);
        for(uint8_t j = 0; j < T.QIX.Traps[i].length && j < DOT_MAX; j++) {
            if(px == T.QIX.Traps[i].x[j] && py == T.QIX.Traps[i].y[j]) {
                T.QIX.gameState = STATE_DIED;
            }
        }
    }        
}    

static void InitStyx(uint8_t level) {
    // Initialize Styx creatures
    T.QIX.styx_active = (T.QIX.level < 3) ? 1 : 2;
    for(uint8_t i=0; i<T.QIX.styx_active; i++) {
        T.QIX.Styx[i].active = 1;
        // Start in upper center
        T.QIX.Styx[i].x = int2fix(DISPLAY_MAX_X/2);
        T.QIX.Styx[i].y = int2fix(DISPLAY_MAX_Y/3);
        T.QIX.Styx[i].vect_dir = 0;
        T.QIX.Styx[i].vect_length = 10;
        T.QIX.Styx[i].max_vec_length = 10;
        T.QIX.Styx[i].line_idx = 0;
        uint8_t x = fix2int(T.QIX.Styx[i].x);
        uint8_t y = fix2int(T.QIX.Styx[i].y);
        // Initialize line segments
        for(uint8_t j = 0; j<STYX_LINES; j++) {
            T.QIX.Styx[i].line_x1[j] = x;
            T.QIX.Styx[i].line_x2[j] = x;
            T.QIX.Styx[i].line_y1[j] = y;
            T.QIX.Styx[i].line_y2[j] = y;
        }
    }
}

static void NewStyxDirection(uint8_t n) {
    // Target line length: full STYX_MAX_DEFAULT for one Styx, 2/3 for two
    // (original used a divisor of 3-5, which in the divisor model gave long lines;
    //  here vect_length is a multiplier so we target STYX_MAX_DEFAULT directly)
    if(T.QIX.styx_active >= 2) {
        T.QIX.Styx[n].max_vec_length = STYX_MAX_DEFAULT * 2 / 3;
    } else {
        T.QIX.Styx[n].max_vec_length = STYX_MAX_DEFAULT;
    }
    uint8_t dir = qrandom();
    uint8_t rand15_1 = prandom();
    uint8_t rand15_2 = rand15_1 & 0x0F;
    rand15_1 >>= 4;
    T.QIX.Styx[n].dx = int2fix(Sin(dir))/(STYX_SPEED-rand15_1);
    T.QIX.Styx[n].dy = int2fix(Cos(dir))/(STYX_SPEED-rand15_2);
    uint8_t rand = prandom();
    int inc = (int8_t)(rand & 0x0F);
    if(testbit(rand, 7)) inc=-inc;
    T.QIX.Styx[n].vect_dirinc = inc;
}

// Check if Styx line collides with walls or filled areas
// Returns: 1 if collision, 0 if clear
static uint8_t CheckStyxCollision(uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2) {
    // Simple Bresenham-like check along the line
    int8_t sdx = (int8_t)x2 - (int8_t)x1;
    int8_t sdy = (int8_t)y2 - (int8_t)y1;
    uint8_t adx = (sdx < 0) ? -sdx : sdx;
    uint8_t ady = (sdy < 0) ? -sdy : sdy;
    uint8_t steps = (adx > ady) ? adx : ady;
    
    if(steps == 0) {
        return get_pixel_buffer(x1, y1, Layer_Filled);
    }
    
    int8_t x_inc = (sdx > 0) ? 1 : (sdx < 0) ? -1 : 0;
    int8_t y_inc = (sdy > 0) ? 1 : (sdy < 0) ? -1 : 0;
    
    int16_t x = x1 * 256;  // Use fixed point for smooth stepping
    int16_t y = y1 * 256;
    sdx = x_inc * (256 / steps);
    sdy = y_inc * (256 / steps);
    
    for(uint8_t i = 0; i <= steps; i++) {
        uint8_t cx = x >> 8;
        uint8_t cy = y >> 8;
        if(get_pixel_buffer(cx, cy, Layer_Filled)) {
            return 1;
        }
        x += sdx;
        y += sdy;
    }
    return 0;
}

// Updates Styx positions and draw lines into the LayerStyx buffer
// Based on TA.C move_styx() - vector-based movement with rotation
static void MoveStyx(void) {
    // Rebuild Layer_Styx from scratch every frame. XOR-toggle erasing fails when
    // all retries collide (no erase step runs), leaving ghost lines on screen.
    // With clear+set_line_buffer the layer is always authoritative regardless of
    // whether the Styx moved this frame or not.
    memset(T.QIX.LayerStyx, 0, DISPLAY_DATA_SIZE);

    for (uint8_t i = 0; i < T.QIX.styx_active; i++) {
        if(!T.QIX.Styx[i].active) break;

        uint8_t retries = 3;  // Allow 3 tries to find a good line location

        while(retries > 0) {
            // Random direction change based on level (from TA.C: random(get_level()+30)>28)
            if(prandom() < (T.QIX.level + 30)) {
                NewStyxDirection(i);
            }

            // Increment vector rotation direction (wraps at 256 = full circle)
            T.QIX.Styx[i].vect_dir += T.QIX.Styx[i].vect_dirinc;

            // Calculate line endpoint offsets using sine/cosine of vect_dir
            // vect_length controls the length of each "stick" in the bundle
            uint8_t vlen = T.QIX.Styx[i].vect_length;
            if(vlen < 1) vlen = 1;

            // Get vector components (scaled by line length)
            int16_t x_offset = ((int32_t)Sin(T.QIX.Styx[i].vect_dir) * vlen) >> 1;
            int16_t y_offset = ((int32_t)Cos(T.QIX.Styx[i].vect_dir) * vlen) >> 1;

            // Save old position in case we need to rollback
            fixed old_x = T.QIX.Styx[i].x;
            fixed old_y = T.QIX.Styx[i].y;

            // Move base position
            T.QIX.Styx[i].x += T.QIX.Styx[i].dx;
            T.QIX.Styx[i].y += T.QIX.Styx[i].dy;

            // Calculate line endpoints (centered on base position)
            uint8_t bx = fix2int(T.QIX.Styx[i].x);  // Base x
            uint8_t by = fix2int(T.QIX.Styx[i].y);  // Base y

            int16_t x1 = bx - (x_offset >> 7);  // Line start
            int16_t y1 = by - (y_offset >> 7);
            int16_t x2 = bx + (x_offset >> 7);  // Line end
            int16_t y2 = by + (y_offset >> 7);

            // Clamp to valid range
            if(x1 < 0) x1 = 0;
            if(x1 > DISPLAY_MAX_X) x1 = DISPLAY_MAX_X;
            if(y1 < 0) y1 = 0;
            if(y1 > DISPLAY_MAX_Y) y1 = DISPLAY_MAX_Y;
            if(x2 < 0) x2 = 0;
            if(x2 > DISPLAY_MAX_X) x2 = DISPLAY_MAX_X;
            if(y2 < 0) y2 = 0;
            if(y2 > DISPLAY_MAX_Y) y2 = DISPLAY_MAX_Y;

            // Check for collision with walls or filled areas
            uint8_t collision = CheckStyxCollision((uint8_t)x1, (uint8_t)y1, (uint8_t)x2, (uint8_t)y2);

            // Also check boundary constraints (keep Styx away from edges by 3 pixels)
            if(bx < 3 || bx > DISPLAY_MAX_X-3 || by < 8+3 || by > DISPLAY_MAX_Y-3) {
                collision = 1;
            }

            if(collision) {
                // Line hit something - rollback and try new direction
                T.QIX.Styx[i].x = old_x;
                T.QIX.Styx[i].y = old_y;
                NewStyxDirection(i);
                retries--;
            } else {
                // Update the circular buffer slot with the new line position
                uint8_t line_idx = T.QIX.Styx[i].line_idx;
                T.QIX.Styx[i].line_x1[line_idx] = (uint8_t)x1;
                T.QIX.Styx[i].line_y1[line_idx] = (uint8_t)y1;
                T.QIX.Styx[i].line_x2[line_idx] = (uint8_t)x2;
                T.QIX.Styx[i].line_y2[line_idx] = (uint8_t)y2;

                // Advance to next line slot (circular buffer)
                T.QIX.Styx[i].line_idx++;
                if(T.QIX.Styx[i].line_idx >= STYX_LINES) {
                    T.QIX.Styx[i].line_idx = 0;
                }

                // Adjust line length toward maximum
                if(T.QIX.Styx[i].vect_length > STYX_MAX_DEFAULT) {
                    T.QIX.Styx[i].vect_length = STYX_MAX_DEFAULT;
                }
                if(T.QIX.Styx[i].vect_length < T.QIX.Styx[i].max_vec_length) {
                    T.QIX.Styx[i].vect_length++;
                } else if(T.QIX.Styx[i].vect_length > T.QIX.Styx[i].max_vec_length) {
                    T.QIX.Styx[i].vect_length--;
                }

                retries = 0;  // Success, exit retry loop
            }
        }

        // Redraw all STYX_LINES history slots for this creature into the now-clear layer.
        // Because the layer was zeroed above, set_line_buffer (OR) is correct here.
        // When the Styx is stuck (all retries failed), the buffer didn't advance so the
        // same lines are redrawn — the Styx freezes in place with no ghost residue.
        for(uint8_t j = 0; j < STYX_LINES; j++) {
            set_line_buffer(T.QIX.Styx[i].line_x1[j], T.QIX.Styx[i].line_y1[j],
                            T.QIX.Styx[i].line_x2[j], T.QIX.Styx[i].line_y2[j],
                            Layer_Styx);
        }
    }

    // Check player trail collision against the fully rebuilt Styx layer.
    // Done once after all creatures are drawn so both Styx hit the trail correctly.
    if(T.QIX.Man.action == MAN_DRAWING) {
        for(uint8_t t = 0; t < T.QIX.Man.trail_len; t++) {
            if(get_pixel_buffer(T.QIX.Man.trailX[t], T.QIX.Man.trailY[t], Layer_Styx)) {
                T.QIX.gameState = STATE_DIED;
                break;
            }
        }
    }
}

// Count set bits in a byte (for filled pixel counting)
static uint8_t popcount8(uint8_t b) {
    b = b - ((b >> 1) & 0x55);
    b = (b & 0x33) + ((b >> 2) & 0x33);
    return (b + (b >> 4)) & 0x0F;
}

// Scan-line fill helper: fill right into Layer_Styx, bounded by Layer_Filled
// Returns the boundary position (first pixel NOT filled)
static uint8_t fright(uint8_t x, uint8_t y) {
    while(x <= DISPLAY_MAX_X) {
        if(get_pixel_buffer(x, y, Layer_Filled) || get_pixel_buffer(x, y, Layer_Styx)) return x;
        set_pixel_buffer(x, y, Layer_Styx);
        x++;
    }
    return x;
}

// Scan-line fill helper: fill left into Layer_Styx, bounded by Layer_Filled
// Returns the boundary position (first pixel NOT filled)
static uint8_t fleft(uint8_t x, uint8_t y) {
    while(x > 0) {
        if(get_pixel_buffer(x, y, Layer_Filled) || get_pixel_buffer(x, y, Layer_Styx)) return x;
        set_pixel_buffer(x, y, Layer_Styx);
        x--;
    }
    // x==0: sentinel in Layer_Filled should stop us, but handle edge gracefully
    if(!get_pixel_buffer(0, y, Layer_Filled) && !get_pixel_buffer(0, y, Layer_Styx))
        set_pixel_buffer(0, y, Layer_Styx);
    return 0;
}

// Scan-line fill helper: scan right for first non-boundary pixel (start of gap)
static uint8_t scanne(uint8_t x, uint8_t y, uint8_t xmax) {
    while(x <= xmax) {
        if(!get_pixel_buffer(x, y, Layer_Filled) && !get_pixel_buffer(x, y, Layer_Styx)) return x;
        x++;
    }
    return x;
}

// Scan-line fill helper: scan right for first boundary pixel (end of gap)
static uint8_t scaneq(uint8_t x, uint8_t y, uint8_t xmax) {
    while(x <= xmax) {
        if(get_pixel_buffer(x, y, Layer_Filled) || get_pixel_buffer(x, y, Layer_Styx)) return x;
        x++;
    }
    return x;
}

// Fill algorithm adapted from TD.C (Alvy Ray Smith's TINT FILL) for monochrome display
// Two-pass approach matching the original Styx claim_screen() in TC.C:
//   Pass 1: Flood fill from Styx[0] position into Layer_Styx (temporary mask),
//           bounded by Layer_Filled. This marks the region that must NOT be filled.
//   Pass 2: Byte-level sweep sets every unfilled, non-Styx pixel in Layer_Filled.
// Layer_Styx is used as a disposable temp; MoveStyx() rebuilds it next frame.
static void DoFill(void) {
    Sound(TuneFill);

    // --- Pass 1: Mark the Styx's region in Layer_Styx ---
    memset(T.QIX.LayerStyx, 0, DISPLAY_DATA_SIZE);

    uint8_t seedX = fix2int(T.QIX.Styx[0].x);
    uint8_t seedY = fix2int(T.QIX.Styx[0].y);

    if(!get_pixel_buffer(seedX, seedY, Layer_Filled)) {
        uint16_t sp = 0;
        uint8_t yref = seedY, lxref = seedX, rxref = seedX;

        T.QIX.FillStack[0][sp] = seedX;
        T.QIX.FillStack[1][sp] = seedY;
        sp++;

        while(sp > 0) {
            sp--;
            uint8_t fx = T.QIX.FillStack[0][sp];
            uint8_t fy = T.QIX.FillStack[1][sp];

            // Skip if already marked or is a boundary
            if(get_pixel_buffer(fx, fy, Layer_Filled) || get_pixel_buffer(fx, fy, Layer_Styx))
                continue;

            // Fill current scan line (filline from TD.C)
            uint8_t saved_x = fx;
            uint8_t rx = fright(fx, fy);    // fill right, returns boundary pos
            rx--;                            // rx = rightmost filled pixel
            if(saved_x > 0) fx = saved_x - 1;
            uint8_t lx = fleft(fx, fy);     // fill left, returns boundary pos
            lx++;                            // lx = leftmost filled pixel

            // hineighbor / loneighbor optimization (TD.C)
            // Only scan the new direction when the current line fits within
            // the reference line; otherwise scan both directions.
            uint8_t scan_hi = 1, scan_lo = 1;
            if(fy == (uint8_t)(yref + 1) && lx >= lxref && rx <= rxref) {
                scan_lo = 0;   // came from below, fits within -> only scan hi
            } else if((uint8_t)(fy + 1) == yref && lx >= lxref && rx <= rxref) {
                scan_hi = 0;   // came from above, fits within -> only scan lo
            }

            // Scan hi (y + 1)
            if(scan_hi && fy < DISPLAY_MAX_Y) {
                uint8_t ny = fy + 1;
                uint8_t sx = lx;
                while(sx <= rx) {
                    sx = scanne(sx, ny, rx);
                    if(sx > rx) break;
                    if(sp < STACK_MAX) {
                        T.QIX.FillStack[0][sp] = sx;
                        T.QIX.FillStack[1][sp] = ny;
                        sp++;
                    }
                    sx = scaneq(sx, ny, rx);
                }
            }

            // Scan lo (y - 1)
            if(scan_lo && fy > 0) {
                uint8_t ny = fy - 1;
                uint8_t sx = lx;
                while(sx <= rx) {
                    sx = scanne(sx, ny, rx);
                    if(sx > rx) break;
                    if(sp < STACK_MAX) {
                        T.QIX.FillStack[0][sp] = sx;
                        T.QIX.FillStack[1][sp] = ny;
                        sp++;
                    }
                    sx = scaneq(sx, ny, rx);
                }
            }

            yref = fy; lxref = lx; rxref = rx;
        }
    }

    // --- Pass 2: Fill all non-Styx unfilled pixels (byte-level) ---
    // Playfield interior: x=[2, DISPLAY_MAX_X-2], y=[9, DISPLAY_MAX_Y-2]
    // For each byte: new_bits = mask & ~filled & ~styx; filled |= new_bits
    uint16_t filled_pixels = 0;
    uint8_t first_byte = (UI_TOP_MARGIN + 1) >> 3;                 // byte containing first playfield y
    uint8_t last_byte  = (DISPLAY_MAX_Y - 2) >> 3;                 // byte containing last playfield y
    uint8_t first_mask = 0xFF >> ((UI_TOP_MARGIN + 1) & 7);        // skip wall row bits
    uint8_t last_mask  = 0xFF << (7 - ((DISPLAY_MAX_Y - 2) & 7));  // skip wall/sentinel bits

    for(uint8_t x = 2; x <= DISPLAY_MAX_X - 2; x++) {
        uint16_t row = (uint16_t)(DISPLAY_MAX_X - x) * DISPLAY_BYTES_IN_ROW;
        for(uint8_t bi = first_byte; bi <= last_byte; bi++) {
            uint8_t mask;
            if(bi == first_byte)      mask = first_mask;
            else if(bi == last_byte)  mask = last_mask;
            else                      mask = 0xFF;

            uint8_t filled = T.QIX.LayerFilled[row + bi];
            uint8_t styx   = T.QIX.LayerStyx[row + bi];
            uint8_t to_fill = mask & ~filled & ~styx;
            T.QIX.LayerFilled[row + bi] = filled | to_fill;
            filled_pixels += popcount8(to_fill);
        }
    }

    // --- Check if Styx[1] got captured in the filled area ---
    // Matches check_styx() in TA.C: if Styx is now inside filled space, deactivate it
    if(T.QIX.styx_active >= 2) {
        uint8_t sx = fix2int(T.QIX.Styx[1].x);
        uint8_t sy = fix2int(T.QIX.Styx[1].y);
        if(get_pixel_buffer(sx, sy, Layer_Filled)) {
            T.QIX.Styx[1].active = 0;
            T.QIX.styx_active = 1;
        }
    }

    // --- Update score and percentage ---
    T.QIX.filled_pixels += filled_pixels;
    // Total fillable pixels: interior playfield area
    uint16_t total_pixels = (uint16_t)(DISPLAY_MAX_X - 3) * (DISPLAY_MAX_Y - UI_TOP_MARGIN - 3);
    T.QIX.capturedPercent = (uint8_t)((uint32_t)T.QIX.filled_pixels * 100 / total_pixels);

    if(T.QIX.capturedPercent >= CAPTURE_GOAL) {
        T.QIX.gameState = STATE_LEVELUP;
    }

    // Award points: filled_pixels x speed_multiplier x score_multiplier
    uint8_t speed_mult;
    if(T.QIX.Man.speed <= float2fix(0.5)) speed_mult = 3;      // Slow = 3x
    else if(T.QIX.Man.speed <= float2fix(0.75)) speed_mult = 2; // Medium = 2x
    else speed_mult = 1;                                        // Fast = 1x

    uint16_t points = (uint16_t)filled_pixels * speed_mult * T.QIX.score_multiplier;
    T.QIX.score += points;
}

// Render display
// Display refresh rate: ~60Hz (every 16.67ms)
static void DrawGame(void) {
    // Update score multiplier timer
    T.QIX.multiplier_timer--;
    if(T.QIX.multiplier_timer == 0) {
        uint8_t multiplier = T.QIX.score_multiplier;
        multiplier++;
        if(multiplier>3) {
            multiplier=1;
            // Higher multiplier factor is active for less time:
            // 3:  85 frames -> ~ 1.4 seconds
            // 2: 170 frames -> ~ 2.8 seconds
            // 1: 255 frames -> ~ 4.2 seconds
             T.QIX.multiplier_timer = 85 * (4-multiplier);
        }
        T.QIX.score_multiplier = multiplier;
    }
    
    clr_display();  // Clear active buffer
    // OR game layers into the active display buffer
    OR_display(T.QIX.LayerWall);
    OR_display(T.QIX.LayerFilled);
    OR_display(T.QIX.LayerStyx);
    // Draw player
    uint8_t x = fix2int(T.QIX.Man.x);
    uint8_t y = fix2int(T.QIX.Man.y);    
    clr_pixel(x,y);
    set_pixel(x-1,y);
    set_pixel(x+1,y);
    set_pixel(x,y-1);
    set_pixel(x,y+1);
    // Draw the drawing trail
    for(uint8_t i=0; i<T.QIX.Man.trail_len; i++) {
        set_pixel(T.QIX.Man.trailX[i], T.QIX.Man.trailY[i]);
    }
    // Draw traps body with alternating on/off pattern (from TB.C dot_type concept)
    for (uint8_t i = 0; i < T.QIX.traps_active; i++) {
        if(!T.QIX.Traps[i].active) continue;
        for(uint8_t j = 0; j < T.QIX.Traps[i].length && j < DOT_MAX; j++) {
            uint8_t tx = T.QIX.Traps[i].x[j];
            uint8_t ty = T.QIX.Traps[i].y[j];
            // Alternate dots on/off for crawling effect
            if(j & 0x01) {
                set_pixel(tx, ty);  // Draw odd-indexed segments
            } else {
                clr_pixel(tx, ty);  // Clear even-indexed segments
            }
        } 
    }
    // Draw UI elements
    lcd_goto(0, 0);
    print5x8(PSTR("Lvl:"));
    printN5x8(T.QIX.level);
    print5x8(PSTR(" x"));
    putchar5x8('0' + T.QIX.score_multiplier);
    print5x8(PSTR(" Score:"));
    print16_5x8(T.QIX.score);
    // Draw lives as pixels at the right edge
    for(uint8_t y = 0; y < T.QIX.Man.lives; y++) {
        set_pixel(DISPLAY_MAX_X, y<<1);
    }

 }