#include <ncurses.h>

int main() {
    initscr();              // Inicializa a tela
    printw("Hello, World!"); // Escreve na tela
    refresh();              // Atualiza a tela
    getch();                // Espera tecla
    endwin();               // Finaliza ncurses
    return 0;
}
