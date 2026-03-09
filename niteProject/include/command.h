#ifndef COMMAND_H
#define COMMAND_H

#include <ncurses.h>

#define _POSIX_C_SOURCE 200809L // Para strdup no Linux

// Processa o comando digitado pelo usuário, incluindo janela e posição
int process_command(const char *cmd, char *status_msg, size_t msg_size, WINDOW *win, int row, int col);

#endif
