#include <ncurses.h>

int main() {
    int ch; // váriavel pra guardar a tecla

    initscr(); // aloca memoria inicia terminal em modo curses...
    raw(); // a entrada do teclado é passada de forma indireta (sem nenhum tipo de sinal gerado)
    keypad(stdscr, TRUE); // garante que o terminal na memoria alocada receba ações do teclado
    noecho(); // Desliga o modo de "echo" da biblioteca ncurses, ou seja, impede que os caracteres digitados apareçam automaticamente na tela.

    printw("Pressione alguma tecla e a mesma será exibida em negrito.\n");
    ch = getch(); // getch vai receber a tecla pressionada

    if (ch == KEY_F(1)) {
        printw("A tecla F1 foi pressionada!"); // exemplo de como teclas F são coletadas
    } else {
        printw("A tecla que você pressionou foi: ");
        attron(A_BOLD); // abre negrito
        printw("%c", ch);
        attroff(A_BOLD); // fecha negrito
    }
    refresh();
    getch(); // pega a proxima tecla para então, fechar o programa
    endwin();

    return 0;
}
