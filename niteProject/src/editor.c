#include "../include/file_validation.h"
#include "../include/editor.h"
#include "../include/dialog.h"
#include <stdlib.h>
#include <string.h>
#include <ncurses.h>
#include <stdio.h>

#define INITIAL_CAPACITY 100

EditorBuffer* create_new_file() {
    EditorBuffer *buffer = malloc(sizeof(EditorBuffer));
    buffer->lines = malloc(INITIAL_CAPACITY * sizeof(char*));
    buffer->lines[0] = malloc(256);  // Primeira linha vazia
    buffer->lines[0][0] = '\0';
    buffer->num_lines = 1;
    buffer->capacity = INITIAL_CAPACITY;
    buffer->filename = NULL;
    buffer->modified = 0;
    buffer->current_line = 0;
    buffer->current_col = 0;
    return buffer;
}

void free_editor_buffer(EditorBuffer *buffer) {
    if (buffer) {
        for (int i = 0; i < buffer->num_lines; i++) {
            free(buffer->lines[i]);
        }
        free(buffer->lines);
        free(buffer->filename);
        free(buffer);
    }
}

int save_file(EditorBuffer *buffer, const char *filename) {
    FILE *file = fopen(filename, "w");
    if (!file) {
        return 0;  // Erro ao abrir arquivo
    }

    for (int i = 0; i < buffer->num_lines; i++) {
        fprintf(file, "%s\n", buffer->lines[i]);
    }

    fclose(file);
    buffer->modified = 0;
    return 1;  // Sucesso
}

void enter_editor_mode(EditorBuffer *buffer, WINDOW *win, int row, int col) {
    int ch;
    int editor_active = 1;

    while (editor_active) {
        // Limpar tela e mostrar conteúdo do arquivo
        wclear(win);

        // Mostrar título do arquivo
        mvwprintw(win, 0, 0, "Editing: %s", buffer->filename ? buffer->filename : "New File");
        mvwprintw(win, 1, 0, "F1: Save  F2: Exit");

        // Mostrar linhas do arquivo
        int display_start = (buffer->current_line > row - 10) ? buffer->current_line - (row - 10) : 0;

        for (int i = 0; i < row - 5 && (display_start + i) < buffer->num_lines; i++) {
            mvwprintw(win, i + 3, 0, "%s", buffer->lines[display_start + i]);
        }

        // Posicionar cursor e garantir que está visível
        int display_line = buffer->current_line - display_start + 3;
        wmove(win, display_line, buffer->current_col);
        curs_set(1);  // Garantir cursor visível sempre
        wrefresh(win);

        ch = wgetch(win);

        switch (ch) {
            case KEY_F(2): // F2 - Sair completamente
                if (buffer->modified) {
                    Dialog *confirm = create_dialog(8, 50, "Confirm Exit");
                    int save_choice = confirm_dialog(confirm, "Save changes before exit?");
                    destroy_dialog(confirm);

                    if (save_choice) {
                        // Se escolheu salvar, fazer o mesmo processo do F1
                        char *final_filename = NULL;
                        char *final_path = NULL;

                        // Loop para solicitar nome válido
                        while (!final_filename) {
                            Dialog *name_dialog = create_dialog(10, 60, "Save File - Enter Name");
                            char *filename = filename_dialog(name_dialog, buffer->filename);
                            destroy_dialog(name_dialog);

                            if (!filename) {
                                // Se cancelou, encerra o programa
                                endwin();
                                printf("Save cancelled. Nite editor closed.\n");
                                exit(0);
                            }

                            // Validar usando o novo sistema de validação
                            final_filename = process_filename(filename);
                            free(filename);

                            if (!final_filename) {
                                // Se falhou na validação, mostra erro e tenta novamente
                                Dialog *error_dialog = create_dialog(8, 60, "Error");
                                mvwprintw(error_dialog->win, 3, 2, "Unsupported file type!");
                                mvwprintw(error_dialog->win, 4, 2, "Press any key to try again...");
                                wrefresh(error_dialog->win);
                                wgetch(error_dialog->win);
                                destroy_dialog(error_dialog);
                            }
                        }

                        // Dialog para diretório
                        Dialog *dir_dialog = create_dialog(20, 70, "Save File - Choose Directory");
                        final_path = directory_dialog(dir_dialog, ".");
                        destroy_dialog(dir_dialog);

                        if (final_path) {
                            char full_path[512];
                            snprintf(full_path, sizeof(full_path), "%s/%s", final_path, final_filename);
                            if (save_file(buffer, full_path)) {
                                endwin();
                                printf("File saved successfully: %s\nNite editor closed.\n", full_path);
                            } else {
                                endwin();
                                printf("Error saving file. Nite editor closed.\n");
                            }
                            free(final_path);
                        } else {
                            endwin();
                            printf("Save cancelled. Nite editor closed.\n");
                        }

                        free(final_filename);
                    } else {
                        endwin();
                        printf("Changes discarded. Nite editor closed.\n");
                    }
                } else {
                    endwin();
                    printf("Nite editor closed.\n");
                }
                exit(0);

            case KEY_F(1): // F1 - Salvar
                {
                    char *final_filename = NULL;
                    char *final_path = NULL;

                    // Loop para solicitar nome válido
                    while (!final_filename) {
                        Dialog *name_dialog = create_dialog(10, 60, "Save File - Enter Name");
                        char *filename = filename_dialog(name_dialog, buffer->filename);
                        destroy_dialog(name_dialog);

                        if (!filename) {
                            mvwprintw(win, row - 1, 0, "Save cancelled.");
                            wrefresh(win);
                            napms(1000);
                            break;
                        }

                        // Validar usando o novo sistema de validação
                        final_filename = process_filename(filename);
                        free(filename);

                        if (!final_filename) {
                            // Se falhou na validação, mostra erro e tenta novamente
                            Dialog *error_dialog = create_dialog(8, 60, "Error");
                            mvwprintw(error_dialog->win, 3, 2, "Unsupported file type!");
                            mvwprintw(error_dialog->win, 4, 2, "Press any key to try again...");
                            wrefresh(error_dialog->win);
                            wgetch(error_dialog->win);
                            destroy_dialog(error_dialog);
                        }
                    }

                    if (!final_filename) break; // Se cancelou no loop

                    // 2. Dialog para escolher diretório
                    Dialog *dir_dialog = create_dialog(20, 70, "Save File - Choose Directory");
                    final_path = directory_dialog(dir_dialog, ".");
                    destroy_dialog(dir_dialog);

                    if (!final_path) {
                        mvwprintw(win, row - 1, 0, "Save cancelled.");
                        free(final_filename);
                        wrefresh(win);
                        napms(1000);
                        break;
                    }

                    // 3. Criar caminho completo
                    char full_path[512];
                    snprintf(full_path, sizeof(full_path), "%s/%s", final_path, final_filename);

                    // 4. Salvar arquivo
                    if (save_file(buffer, full_path)) {
                        if (buffer->filename) free(buffer->filename);
                        buffer->filename = strdup(full_path);
                        mvwprintw(win, row - 1, 0, "File saved: %s", full_path);
                    } else {
                        mvwprintw(win, row - 1, 0, "Error saving file!");
                    }

                    free(final_filename);
                    free(final_path);
                    wrefresh(win);
                    napms(2000);

                    // Restaurar cursor visível após salvar
                    curs_set(1);
                }
                break;

            case 10: // Enter - Nova linha
                insert_new_line(buffer);
                break;

            case 8: case 127: case 263: // Backspace
                handle_backspace(buffer);
                break;

            case 259: // KEY_UP (seta para cima)
                if (buffer->current_line > 0) {
                    buffer->current_line--;
                    if (buffer->current_col > strlen(buffer->lines[buffer->current_line])) {
                        buffer->current_col = strlen(buffer->lines[buffer->current_line]);
                    }
                }
                break;

            case 258: // KEY_DOWN (seta para baixo)
                if (buffer->current_line < buffer->num_lines - 1) {
                    buffer->current_line++;
                    if (buffer->current_col > strlen(buffer->lines[buffer->current_line])) {
                        buffer->current_col = strlen(buffer->lines[buffer->current_line]);
                    }
                }
                break;

            case 260: // KEY_LEFT (seta para esquerda)
                if (buffer->current_col > 0) {
                    buffer->current_col--;
                }
                break;

            case 261: // KEY_RIGHT (seta para direita)
                if (buffer->current_col < strlen(buffer->lines[buffer->current_line])) {
                    buffer->current_col++;
                }
                break;

            default:
                if (ch >= 32 && ch <= 126) { // Caracteres imprimíveis
                    insert_character(buffer, ch);
                } else {
                    // Debug: mostrar códigos de tecla não reconhecidos
                    mvwprintw(win, row - 1, 0, "Unknown key code: %d", ch);
                    wrefresh(win);
                    napms(1000);
                }
                // Garantir que cursor permanece visível após qualquer operação
                curs_set(1);
                break;
        }
    }
}

void insert_character(EditorBuffer *buffer, char ch) {
    char *line = buffer->lines[buffer->current_line];
    int line_len = strlen(line);

    // Realocar se necessário
    line = realloc(line, line_len + 2);
    buffer->lines[buffer->current_line] = line;

    // Mover caracteres para a direita
    for (int i = line_len; i > buffer->current_col; i--) {
        line[i] = line[i-1];
    }

    line[buffer->current_col] = ch;
    line[line_len + 1] = '\0';

    buffer->current_col++;
    buffer->modified = 1;
}

void insert_new_line(EditorBuffer *buffer) {
    // Verificar se precisa expandir a capacidade
    if (buffer->num_lines >= buffer->capacity) {
        buffer->capacity *= 2;
        buffer->lines = realloc(buffer->lines, buffer->capacity * sizeof(char*));
    }

    char *current_line = buffer->lines[buffer->current_line];
    char *new_line = strdup(current_line + buffer->current_col);
    current_line[buffer->current_col] = '\0';

    // Mover linhas para baixo
    for (int i = buffer->num_lines; i > buffer->current_line + 1; i--) {
        buffer->lines[i] = buffer->lines[i-1];
    }

    buffer->lines[buffer->current_line + 1] = new_line;
    buffer->num_lines++;
    buffer->current_line++;
    buffer->current_col = 0;
    buffer->modified = 1;
}

void handle_backspace(EditorBuffer *buffer) {
    if (buffer->current_col > 0) {
        // Backspace na linha atual
        char *line = buffer->lines[buffer->current_line];
        int line_len = strlen(line);

        for (int i = buffer->current_col - 1; i < line_len; i++) {
            line[i] = line[i + 1];
        }

        buffer->current_col--;
        buffer->modified = 1;
    } else if (buffer->current_line > 0) {
        // Juntar com a linha anterior
        char *prev_line = buffer->lines[buffer->current_line - 1];
        char *curr_line = buffer->lines[buffer->current_line];

        int prev_len = strlen(prev_line);
        prev_line = realloc(prev_line, prev_len + strlen(curr_line) + 1);
        strcat(prev_line, curr_line);

        buffer->lines[buffer->current_line - 1] = prev_line;
        free(curr_line);

        // Mover linhas para cima
        for (int i = buffer->current_line; i < buffer->num_lines - 1; i++) {
            buffer->lines[i] = buffer->lines[i + 1];
        }

        buffer->num_lines--;
        buffer->current_line--;
        buffer->current_col = prev_len;
        buffer->modified = 1;
    }
}