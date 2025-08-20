// Exibe a mensagem de status na linha acima do prompt de comando e
// Lê o input do usuário na última linha, após o prompt "> "

#include "../include/config.h"
#include <ncurses.h>
#include <string.h>

void show_status(WINDOW *win, int row, int col, const char *msg) {

    mvwprintw(win, row - 2, 0, "%s", msg);   // Imprime a mensagem na linha row - 2, coluna 0
    wclrtoeol(win);                         // Limpa o resto da linha
    wrefresh(win);                         // Atualiza a janela para mostrar a

}

void get_user_input(WINDOW *win, int row, int col, char *input, size_t input_size) {

    // Desenha prompt
    mvwprintw(win, row - 1, 0, ">_ ");                           // Desenha o prompt
    wclrtoeol(win);                                              // Limpa o resto da linha
    mvwprintw(win, row - 1, col - strlen(CMD_HELP), CMD_HELP);   // Exibe o comando de ajuda no final da linha
    wrefresh(win);                                               // Atualiza a janela para mostrar o prompt

    echo(); // Ativa o eco para mostrar o que o usuário digita

    // Move cursor após o prompt
    wmove(win, row - 1, 2);
    wgetnstr(win, input, input_size - 1); // Lê a entrada do usuário, limitando ao tamanho do buffer
    noecho(); // Desativa o eco após a leitura

}
