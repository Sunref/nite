#ifndef STATUS_H
#define STATUS_H

#include <ncurses.h>
#include <stddef.h>

// Exibe a mensagem de status na linha acima do prompt
void show_status(WINDOW *win, int row, int col, const char *msg);

// Lê o input do usuário na última linha, após o prompt "> "
void get_user_input(WINDOW *win, int row, int col, char *input, size_t input_size);

#endif
