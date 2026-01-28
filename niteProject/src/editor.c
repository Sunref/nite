#include "../include/file_validation.h"
#include "../include/editor.h"
#include "../include/dialog.h"
#include <stdlib.h>
#include <string.h>
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

    // Modo de comando
    int command_line_active = 0;
    char cmdbuf[512];
    int cmdlen = 0;

    // Inicializar cores se disponível
    if (has_colors()) {
        start_color();
        use_default_colors(); // Usar cores padrão do terminal
        init_pair(1, COLOR_RED, -1);     // Vermelho
        init_pair(2, COLOR_YELLOW, -1);  // Amarelo
        init_pair(3, COLOR_WHITE, -1);   // Branco
    }

    while (editor_active) {
        // Se modo está ativo, renderizar prompt e capturar entrada
        if (command_line_active) {
            // Mostrar buffer de comando na última linha
            mvwhline(win, row - 1, 0, ' ', col);

            // ">_" em vermelho
            if (has_colors()) {
                wattron(win, COLOR_PAIR(1) | A_BOLD);
                mvwprintw(win, row - 1, 0, ">_");
                wattroff(win, COLOR_PAIR(1) | A_BOLD);
                mvwprintw(win, row - 1, 2, "%s", cmdbuf);
            } else {
                mvwprintw(win, row - 1, 0, ">_%s", cmdbuf);
            }

            wmove(win, row - 1, 2 + cmdlen);
            curs_set(1);
            wrefresh(win);

            // Ler tecla no modo de comando
            ch = wgetch(win);

            // Detectar Alt+C para sair do modo comando
            if (ch == 27) {
                nodelay(win, TRUE);
                int next = wgetch(win);
                nodelay(win, FALSE);
                if (next == 'c' || next == 'C') {
                    // Desativar modo
                    command_line_active = 0;
                    cmdlen = 0;
                    cmdbuf[0] = '\0';
                    continue;
                } else if (next != -1) {
                    ungetch(next);
                }
                continue;
            }

            // Backspace no modo de comando
            if (ch == KEY_BACKSPACE || ch == 127 || ch == 8) {
                if (cmdlen > 0) {
                    cmdlen--;
                    cmdbuf[cmdlen] = '\0';
                }
            }
            // Enter no modo de comando (executar comando)
            else if (ch == 10 || ch == '\r') {
                // Verificar qual comando foi digitado
                if (strcmp(cmdbuf, "!q") == 0) {
                    // Comando !q (sair))
                    if (buffer->modified) {
                        Dialog *confirm = create_dialog(8, 50, "Confirm Exit");
                        int save_choice = confirm_dialog(confirm, "Save changes before exit?");
                        destroy_dialog(confirm);

                        if (save_choice) {
                            // Se escolheu salvar, fazer o mesmo processo do !s
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
                }
                else if (strcmp(cmdbuf, "!s") == 0) {
                    // Comando !s (salvar)

                    // Verificar se o arquivo já tem um caminho (foi aberto ou já foi salvo antes)
                    if (buffer->filename != NULL) {
                        // Arquivo já existe = salvar diretamente
                        if (save_file(buffer, buffer->filename)) {
                            // Caso sucesso: mostrar mensagem na linha de status
                            mvwhline(win, row - 1, 0, ' ', col);
                            mvwprintw(win, row - 1, 0, "File saved: %s", buffer->filename);
                            wrefresh(win);
                            napms(1500); // Mostrar por 1s e meio

                            command_line_active = 0;
                            cmdlen = 0;
                            cmdbuf[0] = '\0';
                        } else {
                            // Erro ao salvar
                            Dialog *error_dialog = create_dialog(8, 60, "Error");
                            mvwprintw(error_dialog->win, 3, 2, "Error saving file!");
                            mvwprintw(error_dialog->win, 4, 2, "Press any key to continue...");
                            wrefresh(error_dialog->win);
                            wgetch(error_dialog->win);
                            destroy_dialog(error_dialog);

                            command_line_active = 0;
                            cmdlen = 0;
                            cmdbuf[0] = '\0';
                        }
                    } else {
                        // Arquivo novo = perguntar onde salvar
                        char *final_filename = NULL;
                        char *final_path = NULL;

                        // Loop para solicitar nome válido
                        while (!final_filename) {
                            Dialog *name_dialog = create_dialog(10, 60, "Save File - Enter Name");
                            char *filename = filename_dialog(name_dialog, buffer->filename);
                            destroy_dialog(name_dialog);

                            if (!filename) {
                                // Voltar ao modo normal sem salvar
                                command_line_active = 0;
                                cmdlen = 0;
                                cmdbuf[0] = '\0';
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

                        if (!final_filename) continue; // Se cancelou no loop

                        // Dialog para escolher diretório
                        Dialog *dir_dialog = create_dialog(20, 70, "Save File - Choose Directory");
                        final_path = directory_dialog(dir_dialog, ".");
                        destroy_dialog(dir_dialog);

                        if (!final_path) {
                            free(final_filename);
                            command_line_active = 0;
                            cmdlen = 0;
                            cmdbuf[0] = '\0';
                            continue;
                        }

                        // Criar caminho completo
                        char full_path[512];
                        snprintf(full_path, sizeof(full_path), "%s/%s", final_path, final_filename);

                        // Salvar arquivo
                        if (save_file(buffer, full_path)) {
                            if (buffer->filename) free(buffer->filename);
                            buffer->filename = strdup(full_path);

                            // Mostrar mensagem de sucesso
                            mvwhline(win, row - 1, 0, ' ', col);
                            mvwprintw(win, row - 1, 0, "File saved: %s", full_path);
                            wrefresh(win);
                            napms(1500); // Mostrar por 1s e meio
                        } else {
                            // Mostrar mensagem de erro
                            mvwhline(win, row - 1, 0, ' ', col);
                            mvwprintw(win, row - 1, 0, "Error saving file!");
                            wrefresh(win);
                            napms(1500);
                        }

                        free(final_filename);
                        free(final_path);

                        // Desativar modo comando e limpar buffer
                        command_line_active = 0;
                        cmdlen = 0;
                        cmdbuf[0] = '\0';
                    }
                }
                else {
                    // Comando desconhecido = limpar e continuar
                    cmdlen = 0;
                    cmdbuf[0] = '\0';
                }
                continue;
            }
            // Caracteres imprimíveis
            else if (ch >= 32 && ch <= 126) {
                if (cmdlen < (int)sizeof(cmdbuf) - 1) {
                    cmdbuf[cmdlen++] = (char)ch;
                    cmdbuf[cmdlen] = '\0';
                }
            }

            continue; // Pular renderização normal do editor
        }

        // --- MODO NORMAL DO EDITOR ---
        // Limpar tela e mostrar conteúdo do arquivo
        wclear(win);

        // Mostrar título do arquivo
        mvwprintw(win, 0, 0, "Editing: %s", buffer->filename ? buffer->filename : "New File");
        mvwprintw(win, 1, 0, "ALT+C: Command Mode | !s: Save | !q: Quit");

        // Mostrar linhas do arquivo numeradas
        int display_start = (buffer->current_line > row - 10) ? buffer->current_line - (row - 10) : 0;

        // Calcular largura necessária para números de linha (max 4 dígitos)
        int line_num_width = 4;

        for (int i = 0; i < row - 5 && (display_start + i) < buffer->num_lines; i++) {
            int line_number = display_start + i + 1; // Linhas começam em 1
            int screen_line = i + 3;

            // Verificar se é a linha atual
            int is_current_line = (display_start + i == buffer->current_line);

            // Desenhar número da linha com destaque se for a linha atual
            if (has_colors()) {
                if (is_current_line) {
                    wattron(win, COLOR_PAIR(2) | A_BOLD); // Amarelo + negrito
                } else {
                    wattron(win, COLOR_PAIR(3)); // Branco normal
                }
            } else if (is_current_line) {
                wattron(win, A_BOLD); // Apenas negrito se não tiver cores
            }

            mvwprintw(win, screen_line, 0, "%3d ", line_number);

            if (has_colors()) {
                if (is_current_line) {
                    wattroff(win, COLOR_PAIR(2) | A_BOLD);
                } else {
                    wattroff(win, COLOR_PAIR(3));
                }
            } else if (is_current_line) {
                wattroff(win, A_BOLD);
            }

            // Desenhar conteúdo da linha (deslocado para dar espaço aos números)
            mvwprintw(win, screen_line, line_num_width + 1, "%s", buffer->lines[display_start + i]);
        }

        // Posicionar cursor e garantir que está visível
        // Ajustar posição X do cursor para considerar os números de linha
        int display_line = buffer->current_line - display_start + 3;
        int cursor_x_on_screen = buffer->current_col + line_num_width + 1;

        wmove(win, display_line, cursor_x_on_screen);
        curs_set(1);  // Garantir cursor visível sempre
        wrefresh(win);

        ch = wgetch(win);

        // Detectar Alt+C no modo normal
        if (ch == 27) {
            nodelay(win, TRUE);
            int next = wgetch(win);
            nodelay(win, FALSE);
            if (next == 'c' || next == 'C') {
                // Ativar modo de comando
                command_line_active = 1;
                cmdlen = 0;
                cmdbuf[0] = '\0';
                continue;
            } else if (next != -1) {
                ungetch(next);
            }
            continue;
        }

        switch (ch) {
            case 10: // Enter = Nova linha
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

            case 9: // TAB = Inserir 4 espaços
                for (int i = 0; i < 4; i++) {
                    insert_character(buffer, ' ');
                }
                break;

            default:
                if (ch >= 32 && ch <= 126) { // Caracteres imprimíveis
                    insert_character(buffer, ch);
                } else {
                    // Debug: mostrar códigos de tecla não reconhecidos (alterar posteriormente para não mostrar o código da tecla)
                    mvwprintw(win, row - 1, 0, "Unknown key code: %d", ch);
                    wrefresh(win);
                    napms(1500);
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