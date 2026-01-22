// Processa comandos digitados pelo usuário
// Recebe o comando, um buffer para mensagem de status, janela e posição para saída

// Explicação geral:
// - A função `process_command` compara a string `cmd` com comandos conhecidos (definidos em `command.h`).
// - Dependendo do comando, atualiza a janela ncurses (`win`) e/ou `status_msg`.
// - Retorno: 0 significa "comando tratado; continuar execução", 1 significa "sinalizar saída".

#include "../include/file_validation.h"
#include "../include/command.h"
#include "../include/config.h"
#include "../include/editor.h"
#include <ncurses.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

int process_command(const char *cmd, char *status_msg, size_t msg_size, WINDOW *win, int row, int col) {

    if (strcmp(cmd, CMD_HELP) == 0) {

        // Texto padrão (inicia na linha `row - 2`, coluna 0)
        mvwprintw(win, row - 2, 0, "Available commands: ");

        // !o em azul
        wattron(win, COLOR_PAIR(3));
        wprintw(win, "%s ", CMD_OPEN);
        wattroff(win, COLOR_PAIR(3));

        // !n em amarelo
        wattron(win, COLOR_PAIR(2));
        wprintw(win, "%s ", CMD_NEW);
        wattroff(win, COLOR_PAIR(2));

        // !q em vermelho
        wattron(win, COLOR_PAIR(1));
        wprintw(win, "%s ", CMD_EXIT);
        wattroff(win, COLOR_PAIR(1));

        // !help sem cor (padrão do terminal)
        wprintw(win, "%s", CMD_HELP);

        // Limpa o resto da linha atual na janela (útil se havia texto sobrando)
        wclrtoeol(win);
        // Força atualização da janela para mostrar o texto ao usuário
        wrefresh(win);

        // Limpa a mensagem de status
        strcpy(status_msg, "");
        return 0;

    }

    // Comando de saída
    if (strcmp(cmd, CMD_EXIT) == 0) {
        snprintf(status_msg, msg_size, "Exiting the editor...");
        return 1;
    }

    // Comando de abrir arquivo
    if (strcmp(cmd, CMD_OPEN) == 0) {
        snprintf(status_msg, msg_size, "Open file not yet implemented!");
        return 0;
    }

    // Comando de criar novo arquivo
    if (strcmp(cmd, CMD_NEW) == 0) {
        // Solicitando nome do arquivo
        char filename[256];

        // Texto (inicia na linha `row - 2`, coluna 0)
        mvwprintw(win, row - 2, 0, "Enter filename: ");

        // Limpa o resto da linha atual na janela (útil se havia texto sobrando)
        wclrtoeol(win);
        // Força atualização da janela para mostrar o texto ao usuário
        wrefresh(win);

        echo(); // Essencial para o usuário conseguir ver o que está sendo digitado

        // Move o cursor para depois do texto "Enter filename: " (col 16)
        wmove(win, row - 2, 16);
        // Lê a string do usuário na janela `win`, com tamanho máximo (sizeof-1)
        wgetnstr(win, filename, sizeof(filename) - 1);
        noecho();

        // Valida/normaliza o nome do arquivo usando o módulo de validação `process_filename` deve retornar um `char*` alocado (malloc) contendo o nome processado ou NULL se inválido.
        char *processed_filename = process_filename(filename);
        if (!processed_filename) {
            if (is_forbidden_extension(filename)) {
                snprintf(status_msg, msg_size, "Error: File extension '%s' is not supported!", filename);
            } else {
                snprintf(status_msg, msg_size, "Error: Invalid filename!");
            }
            return 0;
        }

        // `create_new_file()` cria um `EditorBuffer` (detalhes na implementacao de editor.h)
        EditorBuffer *buffer = create_new_file();
        buffer->filename = processed_filename; // Atribui o nome do arquivo ao buffer.

        enter_editor_mode(buffer, win, row, col);

        free_editor_buffer(buffer);
        snprintf(status_msg, msg_size, "File created and saved successfully!");
        return 0;
    }

    // Qualquer outro comando inválido
    snprintf(status_msg, msg_size, "Invalid command: %s", cmd);
    return 0;
}