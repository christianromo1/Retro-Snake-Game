#include "snake.h"
#include "graphics.h"
#include <stdint.h>

#define CELL_SIZE   20
#define BORDER      20
#define GRID_COLS   ((640 - 2*BORDER) / CELL_SIZE)   // 30
#define GRID_ROWS   ((480 - 2*BORDER) / CELL_SIZE)   // 22
#define MAX_SNAKE   (GRID_COLS * GRID_ROWS)

typedef struct { int col; int row; } Segment;

static Segment      snake[MAX_SNAKE];
static int          snake_len;
static int          direction;
static int          next_direction;
static int          food_col, food_row;
static int          score;
static int          game_over;
static unsigned int rand_seed;

// ---- Random ----
static unsigned int rand_next(void) {
    rand_seed = rand_seed * 1664525 + 1013904223;
    return rand_seed;
}
static int rand_range(int min, int max) {
    return min + (int)(rand_next() % (unsigned int)(max - min));
}

// ---- Grid helpers ----
static int cell_x(int col) { return BORDER + col * CELL_SIZE; }
static int cell_y(int row) { return BORDER + row * CELL_SIZE; }

// ---- Food placement ----
static void place_food(void) {
    int ok;
    do {
        food_col = rand_range(0, GRID_COLS);
        food_row = rand_range(0, GRID_ROWS);
        ok = 1;
        for (int i = 0; i < snake_len; i++) {
            if (snake[i].col == food_col && snake[i].row == food_row) {
                ok = 0; break;
            }
        }
    } while (!ok);
}

// ---- 3x5 pixel font for score digits ----
static const unsigned char digit_font[10][5] = {
    {0x7,0x5,0x5,0x5,0x7}, // 0
    {0x2,0x6,0x2,0x2,0x7}, // 1
    {0x7,0x1,0x7,0x4,0x7}, // 2
    {0x7,0x1,0x7,0x1,0x7}, // 3
    {0x5,0x5,0x7,0x1,0x1}, // 4
    {0x7,0x4,0x7,0x1,0x7}, // 5
    {0x7,0x4,0x7,0x5,0x7}, // 6
    {0x7,0x1,0x1,0x1,0x1}, // 7
    {0x7,0x5,0x7,0x5,0x7}, // 8
    {0x7,0x5,0x7,0x1,0x7}, // 9
};

static void draw_digit(int d, int px, int py, uint32_t color) {
    for (int row = 0; row < 5; row++)
        for (int col = 0; col < 3; col++)
            if (digit_font[d][row] & (1 << (2 - col)))
                draw_rect(px + col*2, py + row*2, 2, 2, color);
}

static void draw_score(void) {
    int s = score;
    int px = 30, py = 5;
    int hundreds = s / 100;
    int tens     = (s / 10) % 10;
    int ones     = s % 10;
    if (hundreds > 0) { draw_digit(hundreds, px, py, COLOR_WHITE); px += 8; }
    if (hundreds > 0 || tens > 0) { draw_digit(tens, px, py, COLOR_WHITE); px += 8; }
    draw_digit(ones, px, py, COLOR_WHITE);
}

// ---- 3x5 pixel letter font ----
// Characters: space=0,A=1,C=2,E=3,G=4,L=5,M=6,O=7,P=8,R=9,S=10,T=11,V=12,Y=13,colon=14
static const unsigned char letter_font[15][5] = {
    {0x0,0x0,0x0,0x0,0x0}, // space
    {0x2,0x5,0x7,0x5,0x5}, // A
    {0x3,0x4,0x4,0x4,0x3}, // C
    {0x7,0x4,0x6,0x4,0x7}, // E
    {0x3,0x4,0x6,0x5,0x3}, // G
    {0x4,0x4,0x4,0x4,0x7}, // L
    {0x5,0x7,0x5,0x5,0x5}, // M
    {0x7,0x5,0x5,0x5,0x7}, // O
    {0x6,0x5,0x6,0x4,0x4}, // P
    {0x6,0x5,0x6,0x5,0x5}, // R
    {0x7,0x4,0x7,0x1,0x7}, // S
    {0x7,0x2,0x2,0x2,0x2}, // T
    {0x5,0x5,0x5,0x2,0x2}, // V
    {0x5,0x5,0x2,0x2,0x2}, // Y
    {0x0,0x2,0x0,0x2,0x0}, // colon
};

static void draw_char(int idx, int px, int py, uint32_t color) {
    for (int row = 0; row < 5; row++)
        for (int col = 0; col < 3; col++)
            if (letter_font[idx][row] & (1 << (2 - col)))
                draw_rect(px + col*3, py + row*3, 3, 3, color);
}

#define L_SPC 0
#define L_A   1
#define L_C   2
#define L_E   3
#define L_G   4
#define L_L   5
#define L_M   6
#define L_O   7
#define L_P   8
#define L_R   9
#define L_S   10
#define L_T   11
#define L_V   12
#define L_Y   13
#define L_COL 14

static void draw_string(const int *chars, int len, int px, int py, uint32_t color) {
    for (int i = 0; i < len; i++)
        draw_char(chars[i], px + i * 13, py, color);
}

// ---- Game over screen with score and restart prompt ----
static void draw_game_over_screen(int final_score) {
    // Dark overlay box
    draw_rect(160, 160, 320, 160, COLOR_GRAY);
    draw_rect(168, 168, 304, 144, COLOR_BLACK);

    // "GAME OVER"
    {
        int msg[] = {L_G, L_A, L_M, L_E, L_SPC, L_O, L_V, L_E, L_R};
        draw_string(msg, 9, 261, 185, COLOR_RED);
    }

    // "SCORE:" then digits
    {
        int msg[] = {L_S, L_C, L_O, L_R, L_E, L_COL};
        draw_string(msg, 6, 240, 215, COLOR_WHITE);
        int s = final_score;
        int px = 240 + 6*13 + 4;
        if (s >= 100) { draw_digit(s/100,     px, 215, COLOR_YELLOW); px += 8; }
        if (s >= 10)  { draw_digit((s/10)%10, px, 215, COLOR_YELLOW); px += 8; }
                        draw_digit(s%10,       px, 215, COLOR_YELLOW);
    }

    // "PRESS R TO PLAY"
    {
        int msg[] = {L_P, L_R, L_E, L_S, L_S, L_SPC, L_R, L_SPC, L_T, L_O, L_SPC, L_P, L_L, L_A, L_Y};
        draw_string(msg, 15, 220, 270, COLOR_GREEN);
    }
}

// ---- Drawing ----
static void draw_border(void) {
    draw_rect(0,          0,          640, BORDER, COLOR_GRAY);
    draw_rect(0,          480-BORDER, 640, BORDER, COLOR_GRAY);
    draw_rect(0,          0,          BORDER, 480,  COLOR_GRAY);
    draw_rect(640-BORDER, 0,          BORDER, 480,  COLOR_GRAY);
}

// ---- Sprite include for snake head ----
#include "snake_head.c"

static void draw_snake_head(int col, int row) {
    // Draw sprite scaled 1x in center of cell
    int cx = cell_x(col) + 2;
    int cy = cell_y(row) + 2;
    bitBlit(cx, cy, snake_head.pixel_data, 16, 16);
}

static void draw_snake_segment(int i) {
    if (i == 0) {
        draw_snake_head(snake[i].col, snake[i].row);
    } else {
        draw_rect(cell_x(snake[i].col) + 1,
                  cell_y(snake[i].row) + 1,
                  CELL_SIZE - 2, CELL_SIZE - 2, COLOR_DKGREEN);
    }
}

static void draw_food(void) {
    // Food stays as red rectangle (food.c sprite is too dark to see)
    draw_rect(cell_x(food_col) + 3,
              cell_y(food_row) + 3,
              CELL_SIZE - 6, CELL_SIZE - 6, COLOR_RED);
}

// ---- Public API ----
void game_init(unsigned int seed) {
    rand_seed      = seed;
    snake_len      = 3;
    direction      = DIR_RIGHT;
    next_direction = DIR_RIGHT;
    score          = 0;
    game_over      = 0;

    int sc = GRID_COLS / 2;
    int sr = GRID_ROWS / 2;
    for (int i = 0; i < snake_len; i++) {
        snake[i].col = sc - i;
        snake[i].row = sr;
    }

    place_food();

    clear_screen(COLOR_BLACK);
    draw_border();
    for (int i = 0; i < snake_len; i++)
        draw_snake_segment(i);
    draw_food();
    draw_score();
}

void game_tick(void) {
    if (game_over) return;

    // Apply direction (block 180-degree reversal)
    if ((next_direction == DIR_UP    && direction != DIR_DOWN)  ||
        (next_direction == DIR_DOWN  && direction != DIR_UP)    ||
        (next_direction == DIR_LEFT  && direction != DIR_RIGHT) ||
        (next_direction == DIR_RIGHT && direction != DIR_LEFT))
        direction = next_direction;

    // New head position
    int nc = snake[0].col;
    int nr = snake[0].row;
    if (direction == DIR_UP)    nr--;
    if (direction == DIR_DOWN)  nr++;
    if (direction == DIR_LEFT)  nc--;
    if (direction == DIR_RIGHT) nc++;

    // Wall collision
    if (nc < 0 || nc >= GRID_COLS || nr < 0 || nr >= GRID_ROWS) {
        game_over = 1; draw_game_over_screen(score); return;
    }

    // Self collision (ignore tail — it moves away)
    for (int i = 0; i < snake_len - 1; i++) {
        if (snake[i].col == nc && snake[i].row == nr) {
            game_over = 1; draw_game_over_screen(score); return;
        }
    }

    int ate = (nc == food_col && nr == food_row);

    // Erase tail cell if not growing
    if (!ate) {
        draw_rect(cell_x(snake[snake_len-1].col) + 1,
                  cell_y(snake[snake_len-1].row) + 1,
                  CELL_SIZE - 2, CELL_SIZE - 2, COLOR_BLACK);
    }

    // Shift body
    if (ate && snake_len < MAX_SNAKE) snake_len++;
    for (int i = snake_len - 1; i > 0; i--)
        snake[i] = snake[i-1];
    snake[0].col = nc;
    snake[0].row = nr;

    // Recolor old head position as body, draw new head
    if (snake_len > 1)
        draw_rect(cell_x(snake[1].col) + 1, cell_y(snake[1].row) + 1,
                  CELL_SIZE - 2, CELL_SIZE - 2, COLOR_DKGREEN);
    draw_snake_head(snake[0].col, snake[0].row);

    if (ate) {
        score++;
        place_food();
        draw_food();
        draw_rect(25, 4, 120, 12, COLOR_GRAY);  // clear old score area
        draw_score();
    }
}

void game_set_direction(int dir) {
    next_direction = dir;
}

int game_is_over(void) {
    return game_over;
}
