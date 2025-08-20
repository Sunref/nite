#include <ncurses.h>
#include <string.h>

int main() {
    char mesg[] = "Alguma string"; // vatiáveis e string
    int row;
    int  col;

    initscr(); // inicianod curses
    getmaxyx(stdscr, row, col); // pegando dimenções da janela
    mvprintw(row / 2, (col - strlen(mesg)) / 2, "%s", mesg); // imprimindo a string mesg no meio da janela / print com coordenadas

    mvprintw(row - 2, 0, "Essa tela tem %d linhas e %d colunas\n", row, col); // print com coordenadas
    printw("Tente redimensionar sua janela (se possível) e execute este programa novamente."); // print normal. Detalhe: ele vai abaixo do ultimo print

    refresh();
    getch();
    endwin();

    return 0;
}
