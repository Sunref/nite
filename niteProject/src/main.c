// Projeto: nite
// Autor: Fernanda Martins da Silva
// Programa principal do editor de texto nite, que inicializa a tela ncurses,

#include "../include/command.h"
#include "../include/status.h"
#include "../include/menu.h"
#include <ncurses.h>

int main() {

    initscr();               // Inicializa a tela ncurses
    cbreak();                // Desativa o buffer da linha (modo raw)
    noecho();                // Não ecoa os caracteres digitados pelo usuário
    keypad(stdscr, TRUE);    // Habilita captura de teclas especiais (setas, F1, etc.)
    start_color();           // Inicia o uso de cores no terminal
    use_default_colors();    // Usa as cores padrão do terminal

    // Define pares de cores para uso posterior
    init_pair(1, COLOR_BLUE, -1);
    init_pair(2, COLOR_YELLOW, -1);
    init_pair(3, COLOR_RED, -1);
    init_pair(4, COLOR_WHITE, -1);

    int row;                 // Variáveis para armazenar o tamanho da tela
    int col;
    getmaxyx(stdscr, row, col); // pegando tamanho da tela

    char status_msg[128] = ""; // Buffer para mensagem de status

    while (1) {

        draw_centered_screen(stdscr); // Desenha a interface centralizada

        // Mostra mensagem de status na linha acima do prompt
        show_status(stdscr, row, col, status_msg);

        // Prompt na última linha e lê input do usuário
        char input[128] = "";
        get_user_input(stdscr, row, col, input, sizeof(input));

        // Toda lógica de comando é tratada em command.c
        int cmd_result = process_command(input, status_msg, sizeof(status_msg), stdscr, row, col);
        if (cmd_result == 1) {
            break;
        }

    }

    endwin(); // Finaliza a tela ncurses
    return 0; // Finaliza programa

}
