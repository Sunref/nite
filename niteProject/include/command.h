#ifndef COMMAND_H
#define COMMAND_H

#include <ncurses.h>
#include "editor.h"

#define _POSIX_C_SOURCE 200809L // Para strdup no Linux

EditorBuffer *load_file(const char *filepath);
int process_command(const char *cmd, char *status_msg, size_t msg_size, WINDOW *win, int row, int col);

#endif
