// Processa comandos digitados pelo usuário
// Recebe o comando, um buffer para mensagem de status, janela e posição para saída

#include "../include/file_validation.h"
#include "../include/command.h"
#include "../include/config.h"
#include "../include/editor.h"
#include <ncurses.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

int process_command(const char *cmd, char *status_msg, size_t msg_size, WINDOW *win, int row, int col) {

    if (strcmp(cmd, CMD_HELP) == 0) {

        // Texto padrão
        mvwprintw(win, row - 2, 0, "Available commands: ");

        // !o em azul
        wattron(win, COLOR_PAIR(3));
        wprintw(win, "%s ", CMD_OPEN);
        wattroff(win, COLOR_PAIR(3));

        // !n em amarelo
        wattron(win, COLOR_PAIR(2));
        wprintw(win, "%s ", CMD_NEW);
        wattroff(win, COLOR_PAIR(2));

        // !q em vermelho
        wattron(win, COLOR_PAIR(1));
        wprintw(win, "%s ", CMD_EXIT);
        wattroff(win, COLOR_PAIR(1));

        // !help sem cor (padrão do terminal)
        wprintw(win, "%s", CMD_HELP);

        // Limpa a linha e atualiza a janela
        wclrtoeol(win);
        wrefresh(win);

        // Limpa a mensagem de status
        strcpy(status_msg, "");
        return 0;

    }

    // Comando de saída
    if (strcmp(cmd, CMD_EXIT) == 0) {
        snprintf(status_msg, msg_size, "Exiting the editor...");
        return 1;
    }

    // Comando de abrir arquivo
    if (strcmp(cmd, CMD_OPEN) == 0) {
        snprintf(status_msg, msg_size, "Open file not yet implemented!");
        return 0;
    }

    // Comando de criar novo arquivo
    if (strcmp(cmd, CMD_NEW) == 0) {
        // Solicitando nome do arquivo
        char filename[256];
        mvwprintw(win, row - 2, 0, "Enter filename: ");
        wclrtoeol(win);
        wrefresh(win);

        echo();
        wmove(win, row - 2, 16);
        wgetnstr(win, filename, sizeof(filename) - 1);
        noecho();

        // Validar usando o novo sistema de validação
        char *processed_filename = process_filename(filename);
        if (!processed_filename) {
            if (is_forbidden_extension(filename)) {
                snprintf(status_msg, msg_size, "Error: File extension '%s' is not supported!", filename);
            } else {
                snprintf(status_msg, msg_size, "Error: Invalid filename!");
            }
            return 0;
        }

        // Criar buffer do editor e entrar no modo de edição
        EditorBuffer *buffer = create_new_file();
        buffer->filename = processed_filename; // já é malloc'ed pelo process_filename

        enter_editor_mode(buffer, win, row, col);

        free_editor_buffer(buffer);
        snprintf(status_msg, msg_size, "File created and saved successfully!");
        return 0;
    }

    // Qualquer outro comando inválido
    snprintf(status_msg, msg_size, "Invalid command: %s", cmd);
    return 0;
}