#ifndef DIALOG_H
#define DIALOG_H

#include <ncurses.h>

// Estrutura para janelas de diálogo
typedef struct {
    WINDOW *win;
    int height;
    int width;
    int start_y;
    int start_x;
} Dialog;

// Funções de diálogo
Dialog* create_dialog(int height, int width, const char *title);
void destroy_dialog(Dialog *dialog);
char* filename_dialog(Dialog *dialog, const char *current_name);
char* directory_dialog(Dialog *dialog, const char *current_path);
int confirm_dialog(Dialog *dialog, const char *message);

#endif