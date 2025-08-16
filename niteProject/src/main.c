#include "../include/config.h"
#include "../include/menu.h"

int main() {

    initscr();              // inicia o modo ncurses
    keypad(stdscr, TRUE);   // habilita o uso de teclas especiais
    noecho();               // não mostra os caracteres digitados
    curs_set(0);            // esconde o cursor

    start_color();                  // inicia o modo de cores
    use_default_colors();           // permite usar a cor padrão do terminal
    init_pair(1, COLOR_O, -1);      // define o par de cores 1 (azul)
    init_pair(2, COLOR_Q, -1);      // define o par de cores 2 (vermelho)

    while (1) {

        draw_centered_screen(stdscr);   // desenha a tela central

        int ch = getch();
        if (ch == CMD_EXIT)             // sai do programa se "q" for pressionado
            break;
        if (ch == KEY_RESIZE)
            continue;                   // redesenha a tela se a janela for redimensionada
    }

    endwin();       // finaliza o modo ncurses
    return 0;       // finaliza o programa

}
