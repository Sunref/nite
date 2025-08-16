#include "../include/config.h"
#include "../include/menu.h"
#include <string.h>

void draw_centered_screen(WINDOW *win) {

    int row;                    // variaveis para armazenar o tamanho da janela
    int col;
    getmaxyx(win, row, col);    // pegando o tamanho da janela
    clear();                    // limpa a tela

    mvprintw(row/2 - 4, (col - strlen(TITLE)) / 2, "%s", TITLE);        // titulo
    mvprintw(row/2 - 2, (col - strlen(SUBTITLE)) / 2, "%s", SUBTITLE);  // subtitulo
    mvprintw(row/2, (col - strlen(VERSION)) / 2, "%s", VERSION);        // versao
    mvprintw(row/2 + 1, (col - strlen(AUTHOR)) / 2, "%s", AUTHOR);      // autor

    int cmd_y = row/2 + 4; // posicao y para os comandos

    mvprintw(cmd_y, (col - 40) / 2, "Digite ");
    attron(COLOR_PAIR(1)); printw("%c", CMD_OPEN); attroff(COLOR_PAIR(1));  // "o" para abrir (na cor azul)
    printw(" para abrir um arquivo");

    mvprintw(cmd_y + 1, (col - 40) / 2, "Digite ");
    attron(COLOR_PAIR(2)); printw("%c", CMD_NEW); attroff(COLOR_PAIR(2));  // "n" para criar novo arquivo (na cor amarelo)
    printw(" para criar novo arquivo");

    mvprintw(cmd_y + 2, (col - 40) / 2, "Digite ");
    attron(COLOR_PAIR(3)); printw("%c", CMD_EXIT); attroff(COLOR_PAIR(3));  // "q" para sair (na cor vermelho)
    printw(" para sair do editor");

    refresh(); // atualiza a tela

}
