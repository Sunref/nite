#include <ncurses.h>

WINDOW *create_newwin(int height, int width, int starty, int startx);
void destroy_win(WINDOW *local_win);

int main(int argc, char *argv[]) {
    WINDOW *my_win;
    int startx, starty, width, height;
    int ch;

    initscr();            /* Inicia o modo curses */
    cbreak();             /* Buffer de linha desativado, passe tudo para mim  */
    keypad(stdscr, TRUE); /* Eu preciso daquele F1 bacana	*/
    noecho();
    curs_set(false);

    height = 3;
    width = 10;
    starty = (LINES - height) / 2; /* Calculando para um posicionamento central */
    startx = (COLS - width) / 2;   /* da janela */
    printw("Pressione F1 para sair");
    refresh();
    my_win = create_newwin(height, width, starty, startx);

    while ((ch = getch()) != KEY_F(1)) {
        switch (ch) {
        case KEY_LEFT:
        destroy_win(my_win);
        my_win = create_newwin(height, width, starty, --startx);
        break;
        case KEY_RIGHT:
        destroy_win(my_win);
        my_win = create_newwin(height, width, starty, ++startx);
        break;
        case KEY_UP:
        destroy_win(my_win);
        my_win = create_newwin(height, width, --starty, startx);
        break;
        case KEY_DOWN:
        destroy_win(my_win);
        my_win = create_newwin(height, width, ++starty, startx);
        break;
    }
}

  endwin(); /* Termina o modo curses */
  return 0;
}

WINDOW *create_newwin(int height, int width, int starty, int startx) {
    WINDOW *local_win;

    local_win = newwin(height, width, starty, startx);
    box(local_win, 0, 0); /* 0, 0 dá caracteres padrão para as linhas verticais  and horizontais	*/
    wrefresh(local_win);  /* Mostra aquela caixa 	*/

    return local_win;
}

void destroy_win(WINDOW *local_win) {
    /* box (local_win, '', ''); : Isso não produzirá o resultado
    *  desejado de apagar a janela. Vai deixar seus quatro cantos,
    * e uma lembrança feia da janela.
    */
    wborder(local_win, ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ');
    /* Os parâmetros usados são
    * 1. win: a janela na qual operar
    * 2. ls: caractere a ser usado para o lado esquerdo da janela
    * 3. rs: caractere a ser usado para o lado direito da janela
    * 4. ts: caractere a ser usado na parte superior da janela
    * 5. bs: caractere a ser usado na parte inferior da janela
    * 6. tl: caractere a ser usado para o canto superior esquerdo da janela
    * 7. tr: caractere a ser usado no canto superior direito da janela
    * 8. bl: caractere a ser usado no canto inferior esquerdo da janela
    * 9. br: caractere a ser usado no canto inferior direito da janela
    */
    wrefresh(local_win);
    delwin(local_win);
}
