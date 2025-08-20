#include <ncurses.h> // biblioteca de manipulação de terminal

int main(){
  initscr(); // inicializa o terminal em modo curses. Aloca memoria na tela atual (stdscr)
  printw("Hello World !!!"); // imprime a mensagem na memoria alocada (stdscr) nas cordenadas atuais (no caso, 0x e 0y)
  refresh();
  getch(); // aguarda que o susuário tecle algo para continuar (nesse caso não faz nada // fecha)
  endwin(); // termina a curses  (stdscr), liberando a memória alocada.

  return 0;
}
