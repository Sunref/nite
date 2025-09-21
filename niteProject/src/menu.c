// Desenha a tela principal centralizada na janela fornecida

#include "../include/menu.h"
#include "../include/config.h"
#include <ncurses.h>
#include <string.h>

void draw_centered_screen(WINDOW *win) {

    int row;
    int col;
    getmaxyx(win, row, col);    // Obtém o tamanho atual da janela (linhas e colunas)

    wclear(win);    // Limpa a janela antes de desenhar
    use_default_colors(); // Usa as cores padrão do terminal

    // Título e subtítulo, versão e autor  centralizados
    wattron(win, COLOR_PAIR(4));
    mvwprintw(win, row/2 - 4, (col - strlen(TITLE)) / 2, "%s", TITLE);
    mvwprintw(win, row/2 - 2, (col - strlen(SUBTITLE)) / 2, "%s", SUBTITLE);
    mvwprintw(win, row/2, (col - strlen(VERSION)) / 2, "%s", VERSION);
    mvwprintw(win, row/2 + 1, (col - strlen(AUTHOR)) / 2, "%s", AUTHOR);
    wattroff(win, COLOR_PAIR(4));

    int cmd_y = row/2 + 4; // Linha onde os comandos serão exibidos

    // Digite !o para abrir um arquivo
    mvwprintw(win, cmd_y, (col - 50) / 2, "Enter ");
    wattron(win, COLOR_PAIR(1)); // azul
    wprintw(win, "!o");
    wattroff(win, COLOR_PAIR(1));
    wprintw(win, " to open a file");

    // Digite !n para criar novo arquivo
    mvwprintw(win, cmd_y + 1, (col - 50) / 2, "Enter ");
    wattron(win, COLOR_PAIR(2)); // amarelo
    wprintw(win, "!n");
    wattroff(win, COLOR_PAIR(2));
    wprintw(win, " to create a new file");

    // Digite !q para sair do editor
    mvwprintw(win, cmd_y + 2, (col - 50) / 2, "Enter ");
    wattron(win, COLOR_PAIR(3)); // vermelho
    wprintw(win, "!q");
    wattroff(win, COLOR_PAIR(3));
    wprintw(win, " to exit the editor");

    wrefresh(win);      // Atualiza a janela para mostrar as mudanças

}
