#ifndef STATUS_H
#define STATUS_H

#include <ncurses.h>
#include <stddef.h>

void show_status(WINDOW *win, int row, int col, const char *msg);
void get_user_input(WINDOW *win, int row, int col, char *input, size_t input_size);

#endif
