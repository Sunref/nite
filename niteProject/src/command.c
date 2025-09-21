// Processa comandos digitados pelo usuário
// Recebe o comando, um buffer para mensagem de status, janela e posição para saída

#include "../include/command.h"
#include "../include/config.h"
#include <string.h>
#include <stdio.h>

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
        snprintf(status_msg, msg_size, "Create new file not yet implemented!");
        return 0;
    }

    // Comando de ajuda
    if(strcmp(cmd, CMD_HELP) == 0){
        snprintf(status_msg, msg_size, "Command map not yet implemented!");
        return 0;
    }

    // Qualquer outro comando inválido
    snprintf(status_msg, msg_size, "Invalid command: %s", cmd);
    return 0;
}
