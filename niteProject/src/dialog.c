#include "../include/file_validation.h"
#include "../include/dialog.h"
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>

Dialog* create_dialog(int height, int width, const char *title) {
    Dialog *dialog = malloc(sizeof(Dialog));

    dialog->height = height;
    dialog->width = width;
    dialog->start_y = (LINES - height) / 2;
    dialog->start_x = (COLS - width) / 2;

    dialog->win = newwin(height, width, dialog->start_y, dialog->start_x);
    keypad(dialog->win, TRUE);  // Habilitar teclas especiais

    // Desenhar borda e título
    box(dialog->win, 0, 0);
    mvwprintw(dialog->win, 0, 2, " %s ", title);

    wrefresh(dialog->win);
    return dialog;
}

void destroy_dialog(Dialog *dialog) {
    if (dialog) {
        delwin(dialog->win);
        free(dialog);
    }
}

char* filename_dialog(Dialog *dialog, const char *current_name) {
    static char filename[256];

    // Limpar área interna
    for (int i = 1; i < dialog->height - 1; i++) {
        mvwhline(dialog->win, i, 1, ' ', dialog->width - 2);
    }

    // Mostrar prompt
    mvwprintw(dialog->win, 2, 2, "Filename:");
    mvwprintw(dialog->win, 4, 2, "%s", current_name ? current_name : "");

    // Instruções
    mvwprintw(dialog->win, dialog->height - 3, 2, "Enter: Confirm  ESC: Cancel");

    // Posicionar cursor no campo de input
    wmove(dialog->win, 4, 2);
    wrefresh(dialog->win);

    // Capturar input
    echo();
    curs_set(1); // Mostrar cursor

    int ch;
    int pos = current_name ? strlen(current_name) : 0;

    if (current_name) {
        strcpy(filename, current_name);
        wmove(dialog->win, 4, 2 + pos);
    } else {
        filename[0] = '\0';
        pos = 0;
    }

    while ((ch = wgetch(dialog->win)) != 10) { // Enter
        if (ch == 27) { // ESC
            noecho();
            curs_set(0);
            return NULL;
        } else if (ch == KEY_BACKSPACE || ch == 127 || ch == 8) {
            if (pos > 0) {
                pos--;
                filename[pos] = '\0';
                mvwprintw(dialog->win, 4, 2, "%-50s", filename);
                wmove(dialog->win, 4, 2 + pos);
                wrefresh(dialog->win);
            }
        } else if (ch >= 32 && ch <= 126 && pos < 250) {
            filename[pos] = ch;
            filename[pos + 1] = '\0';
            pos++;
            mvwprintw(dialog->win, 4, 2, "%s", filename);
            wmove(dialog->win, 4, 2 + pos);
            wrefresh(dialog->win);
        }
    }

    noecho();
    keypad(dialog->win, FALSE); // Desabilitar teclas especiais

    if (strlen(filename) > 0) {
        // Validar usando o novo sistema de validação
        char *processed = process_filename(filename);
        if (!processed) {
            // Mostrar erro na janela
            mvwprintw(dialog->win, dialog->height - 5, 2, "Error: Unsupported file type!");
            wrefresh(dialog->win);
            napms(2000);
            curs_set(0); // Esconder cursor após erro
            return NULL;
        }
        curs_set(0); // Esconder cursor após sucesso
        return processed;
    }

    curs_set(0); // Esconder cursor se cancelou
    return NULL;
}

static void reload_dirs(char dirs[50][256], int *num_dirs, int *selected, char *selected_path) {
    *num_dirs = 0;

    // Adicionar ".." para voltar (exceto se já estiver na raiz)
    if (strlen(selected_path) > 1) {
        strcpy(dirs[*num_dirs], "..");
        (*num_dirs)++;
    }

    DIR *dir = opendir(".");
    if (dir) {
        struct dirent *entry;
        while ((entry = readdir(dir)) != NULL && *num_dirs < 49) {
            if (entry->d_type == DT_DIR && entry->d_name[0] != '.') {
                strcpy(dirs[*num_dirs], entry->d_name);
                (*num_dirs)++;
            }
        }
        closedir(dir);
    }
    *selected = 0;
}

char* directory_dialog(Dialog *dialog, const char *current_path) {
    static char selected_path[512];
    char dirs[50][256];
    int num_dirs = 0;
    int selected = 0;

    // Obter diretório atual
    getcwd(selected_path, sizeof(selected_path));

    reload_dirs(dirs, &num_dirs, &selected, selected_path);

    while (1) {
        // Limpar área interna
        for (int i = 1; i < dialog->height - 1; i++) {
            mvwhline(dialog->win, i, 1, ' ', dialog->width - 2);
        }

        // Mostrar diretório atual
        mvwprintw(dialog->win, 1, 2, "Current: %.*s", dialog->width - 12, selected_path);

        // Mostrar lista de diretórios
        mvwprintw(dialog->win, 3, 2, "Select directory:");

        int max_display = dialog->height - 8;
        for (int i = 0; i < num_dirs && i < max_display; i++) {
            if (i == selected) {
                wattron(dialog->win, A_REVERSE);
                mvwprintw(dialog->win, 5 + i, 2, " %-*s ", dialog->width - 6, dirs[i]);
                wattroff(dialog->win, A_REVERSE);
            } else {
                mvwprintw(dialog->win, 5 + i, 2, " %-*s ", dialog->width - 6, dirs[i]);
            }
        }

        // Instruções
        mvwprintw(dialog->win, dialog->height - 3, 2, "Arrows: Navigate  Enter: Open");
        mvwprintw(dialog->win, dialog->height - 2, 2, "S: Save on current path  ESC: Cancel");

        wrefresh(dialog->win);

        int ch = wgetch(dialog->win);

        switch (ch) {
            case 27: // ESC
                return NULL;

            case 10: // Enter
                if (num_dirs > 0) {
                    if (strcmp(dirs[selected], "..") == 0) {
                        // Voltar um diretório
                        chdir("..");
                        getcwd(selected_path, sizeof(selected_path));
                    } else {
                        // Entrar no diretório
                        if (chdir(dirs[selected]) == 0) {
                            getcwd(selected_path, sizeof(selected_path));
                        }
                    }
                    reload_dirs(dirs, &num_dirs, &selected, selected_path);
                }
                break;

            case 259: // KEY_UP (seta para cima)
                if (selected > 0) selected--;
                break;

            case 258: // KEY_DOWN (seta para baixo)
                if (selected < num_dirs - 1) selected++;
                break;

            case 's': case 'S': // Selecionar diretório atual
                return strdup(selected_path);
        }
    }
}

int confirm_dialog(Dialog *dialog, const char *message) {
    // Limpar área interna
    for (int i = 1; i < dialog->height - 1; i++) {
        mvwhline(dialog->win, i, 1, ' ', dialog->width - 2);
    }

    // Mostrar mensagem
    mvwprintw(dialog->win, 3, 2, "%s", message);

    // Botões
    mvwprintw(dialog->win, dialog->height - 3, 2, "[Y] Yes  [N] No");

    wrefresh(dialog->win);

    int ch;
    while ((ch = wgetch(dialog->win)) != 27) { // ESC
        if (ch == 'y' || ch == 'Y') {
            return 1; // Yes
        } else if (ch == 'n' || ch == 'N') {
            return 0; // No
        }
    }

    return 0; // Default: No
}