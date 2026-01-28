/*
 *
 * Processa comandos digitados pelo usuário. Recebe o comando, um buffer para mensagem de status, janela e posição para saída
 *
 */
#include "../include/file_validation.h"
#include "../include/command.h"
#include "../include/config.h"
#include "../include/editor.h"
#include "../include/dialog.h"
#include <ncurses.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <sys/stat.h>

// Função auxiliar para carregar arquivo existente
EditorBuffer* load_file(const char *filepath) {
    FILE *file = fopen(filepath, "r");
    if (!file) {
        return NULL; // Não conseguiu abrir o arquivo
    }

    EditorBuffer *buffer = malloc(sizeof(EditorBuffer));
    buffer->capacity = 100;
    buffer->lines = malloc(buffer->capacity * sizeof(char*));
    buffer->num_lines = 0;
    buffer->filename = strdup(filepath);
    buffer->modified = 0;
    buffer->current_line = 0;
    buffer->current_col = 0;

    char line_buf[1024];
    while (fgets(line_buf, sizeof(line_buf), file)) {
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

        buffer->lines[buffer->num_lines] = strdup(line_buf);
        buffer->num_lines++;
    }

    fclose(file);

    // Se arquivo estava vazio, adicionar uma linha vazia
    if (buffer->num_lines == 0) {
        buffer->lines[0] = malloc(256);
        buffer->lines[0][0] = '\0';
        buffer->num_lines = 1;
    }

    return buffer;
}

int process_command(const char *cmd, char *status_msg, size_t msg_size, WINDOW *win, int row, int col) {
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
            Dialog *error_dialog = create_dialog(8, 60, "Error");
            mvwprintw(error_dialog->win, 3, 2, "Unsupported file type!");
            mvwprintw(error_dialog->win, 4, 2, "Press any key to continue...");
            wrefresh(error_dialog->win);
            wgetch(error_dialog->win);
            destroy_dialog(error_dialog);
            snprintf(status_msg, msg_size, "Invalid file type.");
            free(final_path);
            return 0;
        }
        free(processed_filename);

        // Tentar carregar o arquivo
        EditorBuffer *buffer = load_file(final_path);

        if (!buffer) {
            Dialog *error_dialog = create_dialog(8, 60, "Error");
            mvwprintw(error_dialog->win, 3, 2, "Could not load file: %s", final_path);
            mvwprintw(error_dialog->win, 4, 2, "Press any key to continue...");
            wrefresh(error_dialog->win);
            wgetch(error_dialog->win);
            destroy_dialog(error_dialog);
            snprintf(status_msg, msg_size, "Error loading file!");
            free(final_path);
            return 0;
        }

        // Entrar no modo de edição
        enter_editor_mode(buffer, win, row, col);
        free_editor_buffer(buffer);

        free(final_path);
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
        free_editor_buffer(buffer);
        snprintf(status_msg, msg_size, "File closed.");
        return 0;
    }
    // Qualquer outro comando inválido
    snprintf(status_msg, msg_size, "Invalid command: %s", cmd);
    return 0;
}