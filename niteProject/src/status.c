/*
 *
 * Rotinas responsáveis por exibir a mensagem de status e ler o input do usuário
 * na interface de linha de comando do editor.
 *
 */

#include "../include/config.h"
#include <ncurses.h>
#include <string.h>
#include <stddef.h>

// Escreve a mensagem de status.
void show_status(WINDOW *win, int row, int col, const char *msg) {

    // Proteção básica: se a janela for menor que 2 linhas, não escreve
    if (row < 2 || !win) {
        return;
    }

    // mvwprintw posiciona o cursor e escreve a string formatada na janela indicada.
    mvwprintw(win, row - 2, 0, "%s", msg ? msg : "");

    // wclrtoeol limpa o restante da linha (útil quando nova mensagem é menor que a anterior).
    wclrtoeol(win);

    // Força ncurses a atualizar a área afetada na tela.
    wrefresh(win);
}

// Desenha o prompt no final da janela e lê a string digitada pelo usuário.
void get_user_input(WINDOW *win, int row, int col, char *input, size_t input_size) {

    // Valida parâmetros básicos para evitar comportamento indefinido
    if (!win || !input || input_size == 0 || row < 1) {
        if (input && input_size > 0) input[0] = '\0';
        return;
    }

    // Calcula posição segura para colocar a dica de ajuda à direita
    size_t help_len = strlen(CMD_HELP);
    int help_x = col - (int)help_len;
    if (help_x < 0) help_x = 0; // evita coordenada negativa

    // Desenha o prompt na última linha (row - 1) e limpa o resto da linha
    mvwprintw(win, row - 1, 0, ">_ ");
    wclrtoeol(win);

    // Exibe a dica de ajuda no final da linha (pode sobrescrever se espaço insuficiente, ajustar)
    mvwprintw(win, row - 1, help_x, CMD_HELP);

    // Atualiza a janela para que o prompt e a dica apareçam antes da leitura
    wrefresh(win);

    // Habilita eco localmente para que o usuário veja o que digita
    echo();

    // Move o cursor para logo após o prompt (coluna 2) e lê a string protegida
    wmove(win, row - 1, 2);

    /*
     * wgetnstr lê até input_size - 1 caracteres, garantindo terminação NULL.
     * Em caso de erro a função retorna ERR;
     */
    wgetnstr(win, input, (int)input_size - 1);

    // Desliga o eco de volta ao estado padrão do programa
    noecho();
}