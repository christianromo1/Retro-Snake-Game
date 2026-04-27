#include "snake.h"
#include "graphics.h"
#include <stdint.h>

// Grid configuration
#define CELL_SIZE       20
#define BORDER          20
#define GRID_COLS       ((640 - 2*BORDER) / CELL_SIZE)   // 30
#define GRID_ROWS       ((480 - 2*BORDER) / CELL_SIZE)   // 22
#define MAX_SNAKE       (GRID_COLS * GRID_ROWS)

// Game-specific colors
#define COLOR_GRID_LIGHT  0x004CAF50
#define COLOR_GRID_DARK   0x00388E3C
#define COLOR_BORDER_BG   0x002E7D32
#define COLOR_SNAKE_HEAD  0x004499EE   // bright blue
#define COLOR_SNAKE_BODY  0x00224488   // dark blue


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

static uint32_t cell_bg_color(int col, int row) {
    return ((col + row) % 2 == 0) ? COLOR_GRID_LIGHT : COLOR_GRID_DARK;
}

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

// ============================================================
// FONTS
// ============================================================

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

// 2px scale — top border
static void draw_digit_sm(int d, int px, int py, uint32_t color) {
    for (int r = 0; r < 5; r++)
        for (int c = 0; c < 3; c++)
            if (digit_font[d][r] & (1 << (2 - c)))
                draw_rect(px + c*2, py + r*2, 2, 2, color);
}

// 3px scale — game over screen, matches letter font size
static void draw_digit_lg(int d, int px, int py, uint32_t color) {
    for (int r = 0; r < 5; r++)
        for (int c = 0; c < 3; c++)
            if (digit_font[d][r] & (1 << (2 - c)))
                draw_rect(px + c*3, py + r*3, 3, 3, color);
}

static void draw_top_score(void) {
    draw_rect(BORDER, 2, 80, 16, COLOR_BORDER_BG);
    int s = score, px = BORDER + 4, py = 4;
    if (s >= 100) { draw_digit_sm(s/100,     px, py, COLOR_WHITE); px += 8; }
    if (s >= 10)  { draw_digit_sm((s/10)%10, px, py, COLOR_WHITE); px += 8; }
    draw_digit_sm(s%10, px, py, COLOR_WHITE);
}

// Letter font — 3px scale
// 0=spc 1=A 2=C 3=E 4=G 5=K 6=L 7=M 8=O 9=P 10=R 11=S 12=T 13=U 14=V 15=W 16=Y 17=colon
static const unsigned char letter_font[18][5] = {
    {0x0,0x0,0x0,0x0,0x0}, // space
    {0x2,0x5,0x7,0x5,0x5}, // A
    {0x3,0x4,0x4,0x4,0x3}, // C
    {0x7,0x4,0x6,0x4,0x7}, // E
    {0x3,0x4,0x6,0x5,0x3}, // G
    {0x5,0x6,0x4,0x6,0x5}, // K
    {0x4,0x4,0x4,0x4,0x7}, // L
    {0x5,0x7,0x5,0x5,0x5}, // M
    {0x7,0x5,0x5,0x5,0x7}, // O
    {0x6,0x5,0x6,0x4,0x4}, // P
    {0x6,0x5,0x6,0x5,0x5}, // R
    {0x7,0x4,0x7,0x1,0x7}, // S
    {0x7,0x2,0x2,0x2,0x2}, // T
    {0x5,0x5,0x5,0x5,0x7}, // U
    {0x5,0x5,0x5,0x2,0x2}, // V
    {0x5,0x5,0x7,0x7,0x5}, // W
    {0x5,0x5,0x2,0x2,0x2}, // Y
    {0x0,0x2,0x0,0x2,0x0}, // colon
};

#define L_SPC 0
#define L_A   1
#define L_C   2
#define L_E   3
#define L_G   4
#define L_K   5
#define L_L   6
#define L_M   7
#define L_O   8
#define L_P   9
#define L_R   10
#define L_S   11
#define L_T   12
#define L_U   13
#define L_V   14
#define L_W   15
#define L_Y   16
#define L_COL 17

static void draw_char(int idx, int px, int py, uint32_t color) {
    for (int r = 0; r < 5; r++)
        for (int c = 0; c < 3; c++)
            if (letter_font[idx][r] & (1 << (2 - c)))
                draw_rect(px + c*3, py + r*3, 3, 3, color);
}

static void draw_string(const int *chars, int len, int px, int py, uint32_t color) {
    for (int i = 0; i < len; i++)
        draw_char(chars[i], px + i*13, py, color);
}

// ============================================================
// GAME OVER SCREEN
// ============================================================

static void draw_game_over_screen(int final_score) {
    draw_rect(150, 145, 340, 190, COLOR_GRAY);
    draw_rect(158, 153, 324, 174, COLOR_BLACK);

    // "GAME OVER" — 9*13=117px wide, center at x=320 → start at 262
    {
        int msg[] = {L_G, L_A, L_M, L_E, L_SPC, L_O, L_V, L_E, L_R};
        draw_string(msg, 9, 262, 165, COLOR_RED);
    }

    // "SCORE:" + digit(s) at same 3px scale for alignment
    {
        int msg[] = {L_S, L_C, L_O, L_R, L_E, L_COL};
        int lx = 248, ly = 200;
        draw_string(msg, 6, lx, ly, COLOR_WHITE);
        int s = final_score, px = lx + 6*13 + 4;
        if (s >= 100) { draw_digit_lg(s/100,     px, ly, COLOR_YELLOW); px += 13; }
        if (s >= 10)  { draw_digit_lg((s/10)%10, px, ly, COLOR_YELLOW); px += 13; }
                        draw_digit_lg(s%10,       px, ly, COLOR_YELLOW);
    }

    // "ARROW KEY TO PLAY" — 17*13=221px wide, center at x=320 → start at 210
    {
        int msg[] = {L_A,L_R,L_R,L_O,L_W,L_SPC,
                     L_K,L_E,L_Y,L_SPC,
                     L_T,L_O,L_SPC,
                     L_P,L_L,L_A,L_Y};
        draw_string(msg, 17, 210, 270, COLOR_GREEN);
    }
}

// ============================================================
// DRAWING
// ============================================================

static void draw_background(void) {
    clear_screen(COLOR_BORDER_BG);
    for (int row = 0; row < GRID_ROWS; row++)
        for (int col = 0; col < GRID_COLS; col++)
            draw_rect(cell_x(col), cell_y(row),
                      CELL_SIZE, CELL_SIZE, cell_bg_color(col, row));
}

static void draw_snake_head_with_eyes(int col, int row) {
    int bx = cell_x(col), by = cell_y(row);
    draw_rect(bx + 1, by + 1, CELL_SIZE - 2, CELL_SIZE - 2, COLOR_SNAKE_HEAD);

    int ex1, ey1, ex2, ey2;
    if (direction == DIR_RIGHT || direction == DIR_LEFT) {
        int ex = (direction == DIR_RIGHT) ? bx + CELL_SIZE - 7 : bx + 4;
        ex1 = ex; ey1 = by + 4;
        ex2 = ex; ey2 = by + CELL_SIZE - 7;
    } else {
        int ey = (direction == DIR_UP) ? by + 4 : by + CELL_SIZE - 7;
        ex1 = bx + 4;             ey1 = ey;
        ex2 = bx + CELL_SIZE - 7; ey2 = ey;
    }
    draw_rect(ex1, ey1, 3, 3, COLOR_WHITE);
    draw_rect(ex2, ey2, 3, 3, COLOR_WHITE);
    draw_rect(ex1+1, ey1+1, 1, 1, COLOR_BLACK);
    draw_rect(ex2+1, ey2+1, 1, 1, COLOR_BLACK);
}

static void draw_snake_segment(int i) {
    if (i == 0) {
        draw_snake_head_with_eyes(snake[i].col, snake[i].row);
    } else {
        draw_rect(cell_x(snake[i].col) + 2,
                  cell_y(snake[i].row) + 2,
                  CELL_SIZE - 4, CELL_SIZE - 4, COLOR_SNAKE_BODY);
    }
}

// Apple-shaped food using layered rects
static void draw_food(void) {
    int fx = cell_x(food_col), fy = cell_y(food_row);
    draw_rect(fx + 4, fy + 3,  12, 14, COLOR_RED);
    draw_rect(fx + 2, fy + 5,  16, 10, COLOR_RED);
    draw_rect(fx + 9, fy + 2,  3,  3,  COLOR_DKGREEN);
}

static void erase_cell(int col, int row) {
    draw_rect(cell_x(col), cell_y(row),
              CELL_SIZE, CELL_SIZE, cell_bg_color(col, row));
}

// ============================================================
// PUBLIC API
// ============================================================

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
    draw_background();
    for (int i = 0; i < snake_len; i++) draw_snake_segment(i);
    draw_food();
    draw_top_score();
}

void game_tick(void) {
    if (game_over) return;

    if ((next_direction == DIR_UP    && direction != DIR_DOWN)  ||
        (next_direction == DIR_DOWN  && direction != DIR_UP)    ||
        (next_direction == DIR_LEFT  && direction != DIR_RIGHT) ||
        (next_direction == DIR_RIGHT && direction != DIR_LEFT))
        direction = next_direction;

    int nc = snake[0].col, nr = snake[0].row;
    if (direction == DIR_UP)    nr--;
    if (direction == DIR_DOWN)  nr++;
    if (direction == DIR_LEFT)  nc--;
    if (direction == DIR_RIGHT) nc++;

    if (nc < 0 || nc >= GRID_COLS || nr < 0 || nr >= GRID_ROWS) {
        game_over = 1; draw_game_over_screen(score); return;
    }

    for (int i = 0; i < snake_len - 1; i++) {
        if (snake[i].col == nc && snake[i].row == nr) {
            game_over = 1; draw_game_over_screen(score); return;
        }
    }

    int ate = (nc == food_col && nr == food_row);

    if (!ate)
        erase_cell(snake[snake_len-1].col, snake[snake_len-1].row);

    if (ate && snake_len < MAX_SNAKE) snake_len++;

    for (int i = snake_len - 1; i > 0; i--)
        snake[i] = snake[i-1];
    snake[0].col = nc;
    snake[0].row = nr;

    if (snake_len > 1)
        draw_rect(cell_x(snake[1].col) + 2, cell_y(snake[1].row) + 2,
                  CELL_SIZE - 4, CELL_SIZE - 4, COLOR_SNAKE_BODY);
    draw_snake_head_with_eyes(snake[0].col, snake[0].row);

    if (ate) {
        score++;
        place_food();
        draw_food();
        draw_top_score();
    }
}

void game_set_direction(int dir) { next_direction = dir; }
int  game_is_over(void)          { return game_over;     }
