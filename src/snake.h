#ifndef SNAKE_H
#define SNAKE_H

#define DIR_UP    0
#define DIR_RIGHT 1
#define DIR_DOWN  2
#define DIR_LEFT  3

void game_init(unsigned int seed);
void game_tick(void);
void game_set_direction(int dir);
int  game_is_over(void);

#endif
