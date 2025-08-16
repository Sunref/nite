#include <ncurses.h>
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv) {

    int ch; // caracter atual
    int prev; // caracter anterior
    int row; // linhas
    int col; // colunas
    prev = EOF; // inicializa prev como "nada lido"
    FILE *fp; /* ponteiro para abrir o arquivo  */
    int y; // pos Y do cursor em linhas
    int x; // pos X do cursor em colunas

    if (argc != 2) {
        printf("Use um nome de arquivo: %s\n", argv[0]); // caso a forma de abrir o arquivo esteja errada (um argumento só))
        exit(1);
    }

    // tenta abrir o arquivo no modo leitura
    fp = fopen(argv[1], "r");
    if (fp == NULL) {
        perror("Não foi possível abrir o arquivo"); // exibe erro se não conseguir abrir
        exit(1);
    }

    initscr(); // inicia a ncurses
    getmaxyx(stdscr, row, col); // pegando proporções da janela

    // lê todo o arquivo (caracter por caracter) até o fim
    while ((ch = fgetc(fp)) != EOF) {
        getyx(stdscr, y, x); // pega a posição atual do cursor
        if (y == (row - 1)) { // se ultima linha da tela:
            printw("<-Pressione qualquer tecla->");
            getch();     // aguarda entrada do usuário
            clear();     // limpa a tela
            move(0, 0);  // volta o cursor para o início
        }

        if (prev == '/' && ch == '*') { // se detectar um comentário
            attron(A_BOLD);             // ativa o negrito
            getyx(stdscr, y, x);
            move(y, x - 1);             // move o cursor uma posição antes para imprimir /
            printw("%c%c", '/', ch);
        } else {
            printw("%c", ch); // se não for comentário, imprime o caracter normalmente
        }

        refresh();

        if (prev == '*' && ch == '/') { // se detectar fim de comentário
            attroff(A_BOLD); // desliga o negrito
        }

        prev = ch; // guarda o caractere atual como "anterior" para a próxima iteração
    }

    endwin(); // termina ncurses
    fclose(fp); // fecha o arquivo
    return 0; // termina a execução do programa
}
