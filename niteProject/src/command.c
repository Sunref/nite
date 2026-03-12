/*
 *
 * Processa comandos digitados pelo usuário.
 *
 */

#define _POSIX_C_SOURCE 200809L

#include "../include/file_validation.h"
#include "../include/command.h"
#include "../include/config.h"
#include "../include/editor.h"
#include "../include/dialog.h"
#include <ncurses.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <dirent.h> // Biblioteca para manipulação de diretórios
#include <sys/stat.h> // Biblioteca para manipulação de arquivos
#include <libgen.h> // Biblioteca para manipulação de caminhos de arquivo
#include <unistd.h> // Biblioteca para manipulação de processos e sistema
#include <limits.h> // Biblioteca para manipulação de limites de tipos

// ! FUNÇÃO POR CURSOR
// Retorna o buffer do help.txt: caminho do make, cwd, ../ e relativo ao executável (Linux).
static EditorBuffer *load_help_file(void) {

    EditorBuffer *buf;
#ifdef HELP_FILE
    if (HELP_FILE[0] != '\0') {
        buf = load_file(HELP_FILE);
        if (buf) return buf;
    }
#endif
    static const char *rel[] = { "help.txt", "../help.txt", "../../help.txt" };
    for (size_t i = 0; i < sizeof(rel) / sizeof(rel[0]); i++) {
        buf = load_file(rel[i]);
        if (buf) return buf;
    }
#if defined(__linux__)
    char exe[PATH_MAX];
    ssize_t n = readlink("/proc/self/exe", exe, sizeof(exe) - 1);
    if (n > 0) {
        exe[n] = '\0';
        char *dir = dirname(exe);
        char path[PATH_MAX];
        snprintf(path, sizeof(path), "%s/../help.txt", dir);
        buf = load_file(path);
        if (buf) return buf;
        snprintf(path, sizeof(path), "%s/help.txt", dir);
        buf = load_file(path);
        if (buf) return buf;
    }
#endif
    return NULL;

}
// ! FIM DA FUNÇÃO

EditorBuffer* load_file(const char *filepath) { // Função auxiliar para carregar arquivo existente

    FILE *file = fopen(filepath, "r");
    if (!file) {
        return NULL; // Não conseguiu abrir o arquivo
    }

    EditorBuffer *buffer = malloc(sizeof(EditorBuffer)); // Aloca memória para o buffer
    buffer->capacity = 100; // Capacidade inicial
    buffer->lines = malloc(buffer->capacity * sizeof(char*)); // Aloca memória para as linhas
    buffer->num_lines = 0; // Número de linhas atual
    buffer->filename = strdup(filepath); // Nome do arquivo
    buffer->modified = 0; // Flag se o arquivo foi modificado
    buffer->current_line = 0; // Linha atual do cursor
    buffer->current_col = 0; // Coluna atual do cursor
    buffer->syntax = NULL; // Contexto de syntax para o buffer

    char line_buf[1024]; // Buffer para ler cada linha do arquivo
    while (fgets(line_buf, sizeof(line_buf), file)) { // Lê cada linha do arquivo
        // Remover newline se existir
        size_t len = strlen(line_buf);
        if (len > 0 && line_buf[len - 1] == '\n') {
            line_buf[len - 1] = '\0';
        }

        // Expandir capacidade se necessário
        if (buffer->num_lines >= buffer->capacity) {
            buffer->capacity *= 2;
            buffer->lines = realloc(buffer->lines, buffer->capacity * sizeof(char*));
        }

        buffer->lines[buffer->num_lines] = strdup(line_buf); // Copia a linha para o buffer
        buffer->num_lines++; // Incrementa o número de linhas
    }

    fclose(file);

    // Se arquivo estava vazio, adicionar uma linha vazia
    if (buffer->num_lines == 0) {
        buffer->lines[0] = malloc(256); // Aloca memória para a linha vazia
        buffer->lines[0][0] = '\0'; // Inicializa a linha vazia
        buffer->num_lines = 1; // Incrementa o número de linhas
        buffer->current_col = 0;  // Linha vazia, cursor no início
    } else {
        // Cursor vai para o final da primeira linha
        buffer->current_col = strlen(buffer->lines[0]);
    }

    return buffer; // Retorna o buffer preenchido

}

int process_command(const char *cmd, char *status_msg, size_t msg_size, WINDOW *win, int row, int col) { // Processa o comando digitado pelo usuário

    if (strcmp(cmd, CMD_HELP) == 0) {
        // Texto inicia na linha `row - 2`, coluna 0
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
        // Limpa o resto da linha atual na janela
        wclrtoeol(win);
        // Força atualização da janela para mostrar o texto ao usuário
        wrefresh(win);
        // Limpa a mensagem de status
        strcpy(status_msg, "");

        EditorBuffer *help_buffer = load_help_file(); // Carrega o arquivo de ajuda

        if (!help_buffer) { // Se não foi possível abrir o arquivo de ajuda
            snprintf(status_msg, msg_size, "Could not open help.txt");
            return 0;
        }

        read_only(help_buffer, win, row, col); // Exibe o conteúdo do arquivo de ajuda em modo somente leitura
        clear(); // Limpa a tela antes de exibir o conteúdo do arquivo
        refresh(); // Atualiza a tela para mostrar o conteúdo do arquivo
        free_editor_buffer(help_buffer); // Libera a memória alocada para o buffer de ajuda
        strcpy(status_msg, ""); // Limpa a mensagem de status
        return 0;

    }
    // Comando de saída
    if (strcmp(cmd, CMD_EXIT) == 0) {
        snprintf(status_msg, msg_size, "Exiting the editor...");
        return 1;
    }
    // Comando de abrir arquivo
    if (strcmp(cmd, CMD_OPEN) == 0) {
        char *final_path = NULL;

        // Dialog para navegar e selecionar arquivo
        Dialog *file_dialog = create_dialog(20, 70, "Open File - Browse and Select");
        final_path = file_browser_dialog(file_dialog, ".");
        destroy_dialog(file_dialog);

        if (!final_path) {
            snprintf(status_msg, msg_size, "Open cancelled.");
            return 0;
        }

        // Validar extensão do arquivo
        char *processed_filename = process_filename(final_path);
        if (!processed_filename) {
            Dialog *error_dialog = create_dialog(8, 60, "Error"); // Cria um diálogo de erro
            mvwprintw(error_dialog->win, 3, 2, "Unsupported file type!"); // Informa que o tipo de arquivo não é suportado
            wrefresh(error_dialog->win); // Atualiza a tela com a mensagem de erro
            destroy_dialog(error_dialog); // Destroi o diálogo de erro
            snprintf(status_msg, msg_size, "Invalid file type."); // Informa que o tipo de arquivo é inválido
            free(final_path); // Libera a memória alocada para o caminho do arquivo
            return 0;
        }
        free(processed_filename); // Libera a memória alocada para o nome do arquivo processado

        // Tentar carregar o arquivo
        EditorBuffer *buffer = load_file(final_path);

        if (!buffer) { // Mesmo processo de erro da extensão de arquivo inválida
            Dialog *error_dialog = create_dialog(8, 60, "Error");
            mvwprintw(error_dialog->win, 3, 2, "Could not load file: %s", final_path);
            wrefresh(error_dialog->win);
            destroy_dialog(error_dialog);
            snprintf(status_msg, msg_size, "Error loading file!");
            free(final_path);
            return 0;
        }

        Dialog *mode_dlg = create_dialog(8, 45, "Open File"); // Cria um diálogo para perguntar se o arquivo deve ser aberto em modo de edição ou somente leitura
        bool edit = mode_dialog(mode_dlg, "Open in edit mode?"); // Padrão: modo edição
        destroy_dialog(mode_dlg); // Libera a memória alocada para o diálogo

        if (edit) { // Se o usuário escolheu modo edição, entra no modo de edição
            enter_editor_mode(buffer, win, row, col);
        } else { // Se o usuário escolheu modo somente leitura, entra no modo somente leitura
            read_only(buffer, win, row, col);
        }

        clear(); // Limpa a tela antes de exibir o próximo conteúdo
        refresh(); // Atualiza a tela com o conteúdo limpo

        free_editor_buffer(buffer); // Libera a memória alocada para o buffer
        free(final_path); // Libera a memória alocada para o caminho do arquivo
        snprintf(status_msg, msg_size, "File closed.");
        return 0;
    }
    // Comando de criar novo arquivo
    if (strcmp(cmd, CMD_NEW) == 0) {
        // `create_new_file()` cria um `EditorBuffer` (detalhes na implementacao de editor.h)
        EditorBuffer *buffer = create_new_file();
        // Define nome do arquivo somente na hora de salvar
        buffer->filename = NULL;
        enter_editor_mode(buffer, win, row, col);
        clear();
        refresh();
        free_editor_buffer(buffer);
        snprintf(status_msg, msg_size, "File closed.");
        return 0;
    }

    // Qualquer outro comando inválido
    snprintf(status_msg, msg_size, "Invalid command: %s", cmd);
    return 0;

}