/*
 *
 * Rotinas responsáveis pela criação e gerenciamento de diálogos (janelas).
 *
 */

#include "../include/file_validation.h"
#include "../include/dialog.h"
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h> // Manipula diretórios

// Cria uma estrutura Dialog, aloca uma nova janela centrada de tamanho (height x width) e desenha uma borda com título.
Dialog* create_dialog(int height, int width, const char *title) {
    Dialog *dialog = malloc(sizeof(Dialog));
    if (!dialog) return NULL;

    dialog->height = height;
    dialog->width = width;
    dialog->start_y = (LINES - height) / 2;
    dialog->start_x = (COLS - width) / 2;

    // Cria a janela centrada e habilita captura de teclas especiais
    dialog->win = newwin(height, width, dialog->start_y, dialog->start_x);
    keypad(dialog->win, TRUE);

    // Desenha borda e escreve o título na borda superior
    box(dialog->win, 0, 0);
    mvwprintw(dialog->win, 0, 2, " %s ", title);

    wrefresh(dialog->win);
    return dialog;
}

//Libera os recursos associados ao diálogo: deleta a janela e libera a struct.
void destroy_dialog(Dialog *dialog) {
    if (dialog) {
        delwin(dialog->win);
        free(dialog);
    }
}

// Exibe um diálogo que permite navegar entre diretórios E selecionar arquivos
char* file_browser_dialog(Dialog *dialog, const char *start_path) {
    char current_path[512];
    snprintf(current_path, sizeof(current_path), "%s", start_path);

    char entries[50][256];  // Nomes das entradas (pastas + arquivos)
    int is_dir[50];         // Flag: 1 = diretório, 0 = arquivo
    int num_entries = 0;
    int selected = 0;

    while (1) {
        num_entries = 0;

        // Adicionar ".." se não estiver na raiz
        if (strlen(current_path) > 1) {
            strcpy(entries[num_entries], "..");
            is_dir[num_entries] = 1;
            num_entries++;
        }

        // Ler diretório atual
        DIR *dir = opendir(current_path);
        if (dir) {
            struct dirent *entry;
            while ((entry = readdir(dir)) != NULL && num_entries < 49) {
                // Pular arquivos ocultos (exceto "..")
                if (entry->d_name[0] == '.' && strcmp(entry->d_name, "..") != 0) {
                    continue;
                }

                // Adicionar diretórios e arquivos
                if (entry->d_type == DT_DIR && strcmp(entry->d_name, ".") != 0) {
                    strncpy(entries[num_entries], entry->d_name, 255);
                    entries[num_entries][255] = '\0';
                    is_dir[num_entries] = 1;
                    num_entries++;
                } else if (entry->d_type == DT_REG) {
                    strncpy(entries[num_entries], entry->d_name, 255);
                    entries[num_entries][255] = '\0';
                    is_dir[num_entries] = 0;
                    num_entries++;
                }
            }
            closedir(dir);
        }

        // Limpar área interna
        for (int i = 1; i < dialog->height - 1; i++) {
            mvwhline(dialog->win, i, 1, ' ', dialog->width - 2);
        }

        // Mostrar caminho atual
        mvwprintw(dialog->win, 1, 2, "Current: %.*s", dialog->width - 12, current_path);
        mvwprintw(dialog->win, 3, 2, "Select file:");

        // Mostrar lista de entradas
        int max_display = dialog->height - 8;
        for (int i = 0; i < num_entries && i < max_display; i++) {
            if (i == selected) {
                wattron(dialog->win, A_REVERSE);
            }

            // Diretórios entre colchetes, arquivos sem
            if (is_dir[i]) {
                mvwprintw(dialog->win, 5 + i, 2, " [%s] ", entries[i]);
            } else {
                mvwprintw(dialog->win, 5 + i, 2, "  %s  ", entries[i]);
            }

            if (i == selected) {
                wattroff(dialog->win, A_REVERSE);
            }
        }

        // Instruções
        mvwprintw(dialog->win, dialog->height - 3, 2, "Arrows: Navigate  Enter: Open/Select");
        mvwprintw(dialog->win, dialog->height - 2, 2, "ESC: Cancel");

        wrefresh(dialog->win);

        int ch = wgetch(dialog->win);

        switch (ch) {
            case 27: // ESC
                return NULL;

            case 10: // Enter
                if (num_entries > 0) {
                    if (is_dir[selected]) {
                        // É um diretório - navegar
                        if (strcmp(entries[selected], "..") == 0) {
                            // Voltar um nível
                            char *last_slash = strrchr(current_path, '/');
                            if (last_slash && last_slash != current_path) {
                                *last_slash = '\0';
                            }
                        } else {
                            // Entrar no diretório
                            char new_path[512];
                            snprintf(new_path, sizeof(new_path), "%s/%s", current_path, entries[selected]);
                            strcpy(current_path, new_path);
                        }
                        selected = 0;
                    } else {
                        // É um arquivo - selecionar e retornar caminho completo
                        char full_path[512];
                        snprintf(full_path, sizeof(full_path), "%s/%s", current_path, entries[selected]);
                        return strdup(full_path);
                    }
                }
                break;

            case 259: // KEY_UP
                if (selected > 0) selected--;
                break;

            case 258: // KEY_DOWN
                if (selected < num_entries - 1) selected++;
                break;
        }
    }
}

// Mostra um diálogo simples para o usuário digitar um nome de arquivo. Faz limpeza da área interna, exibe prompt e lê input caractere a caractere.
char* filename_dialog(Dialog *dialog, const char *current_name) {
    static char filename[256];

    // Limpa a área interna do diálogo (entre as bordas)
    for (int i = 1; i < dialog->height - 1; i++) {
        mvwhline(dialog->win, i, 1, ' ', dialog->width - 2);
    }

    // Exibir prompt e valor atual (se houver)
    mvwprintw(dialog->win, 2, 2, "Filename:");
    mvwprintw(dialog->win, 4, 2, "%s", current_name ? current_name : "");
    mvwprintw(dialog->win, dialog->height - 3, 2, "Enter: Confirm  ESC: Cancel");

    // Posicionar cursor no campo de entrada
    wmove(dialog->win, 4, 2);
    wrefresh(dialog->win);

    // Prepara para capturar o input com eco local e cursor visível
    echo();
    curs_set(1);

    int ch;
    int pos = current_name ? (int)strlen(current_name) : 0;

    if (current_name) {
        // Copia o nome atual para o buffer e posiciona o cursor no fim
        strncpy(filename, current_name, sizeof(filename) - 1);
        filename[sizeof(filename) - 1] = '\0';
        wmove(dialog->win, 4, 2 + pos);
    } else {
        filename[0] = '\0';
        pos = 0;
    }

    // Loop de leitura caractere a caractere até Enter
    while ((ch = wgetch(dialog->win)) != 10) { // 10 == Enter
        if (ch == 27) { // ESC
            // Cancela o diálogo e retorna NULL (sem alterar estado externo)
            noecho();
            curs_set(0);
            return NULL;
        } else if (ch == KEY_BACKSPACE || ch == 127 || ch == 8) {
            // Processa backspace localmente
            if (pos > 0) {
                pos--;
                filename[pos] = '\0';
                // Limpa e reescreve a linha de input para manter visual consistente
                mvwprintw(dialog->win, 4, 2, "%-50s", filename);
                wmove(dialog->win, 4, 2 + pos);
                wrefresh(dialog->win);
            }
        } else if (ch >= 32 && ch <= 126 && pos < (int)sizeof(filename) - 2) {
            // Aceita apenas caracteres imprimíveis e respeita o limite do buffer
            filename[pos] = (char)ch;
            filename[pos + 1] = '\0';
            pos++;
            mvwprintw(dialog->win, 4, 2, "%s", filename);
            wmove(dialog->win, 4, 2 + pos);
            wrefresh(dialog->win);
        } else {
            // Teclas não tratadas são ignoradas (talvez implementar um "beep" pra aviso??)
        }
    }

    // Finaliza modo de entrada
    noecho();
    keypad(dialog->win, FALSE);
    curs_set(0);

    // Se o usuário digitou algo, valida via process_filename
    if (strlen(filename) > 0) {
        char *processed = process_filename(filename);
        if (!processed) {
            // Mostra erro no próprio diálogo por breves instantes
            mvwprintw(dialog->win, dialog->height - 5, 2, "Error: Unsupported file type!");
            wrefresh(dialog->win);
            napms(1000); // Espera 1 segundo para mostrar erro (talvez 1s seja tempo demais, analizar depois nos testes (anteriormente 2s... muita espera))
            curs_set(0);
            return NULL;
        }
        // Sucesso: retorna a string processada (que tipicamente é alocada)
        curs_set(0);
        return processed;
    }

    // Se vazio, considera como cancelado
    curs_set(0);
    return NULL;
}

// Preenche a matriz `dirs` com nomes de subdiretórios do diretório atual. Insere ".." como primeira entrada salvo quando já estivermos na raiz ("/").
static void reload_dirs(char dirs[50][256], int *num_dirs, int *selected, char *selected_path) {
    *num_dirs = 0;

    // Se não estivermos na raiz, adiciona opção de voltar ("..")
    if (strlen(selected_path) > 1) {
        strcpy(dirs[*num_dirs], "..");
        (*num_dirs)++;
    }

    DIR *dir = opendir(".");
    if (dir) {
        struct dirent *entry;
        while ((entry = readdir(dir)) != NULL && *num_dirs < 49) {
            // Considera apenas entradas do tipo diretório e que não comecem com '.' (analizar depois se é ideal mostrar diretórios ocultos)
            if (entry->d_type == DT_DIR && entry->d_name[0] != '.') {
                strncpy(dirs[*num_dirs], entry->d_name, 255);
                dirs[*num_dirs][255] = '\0';
                (*num_dirs)++;
            }
        }
        closedir(dir);
    }
    *selected = 0;
}

// Exibe um diálogo que permite navegar entre diretórios, selecionar o diretório atual ou cancelar.
char* directory_dialog(Dialog *dialog, const char *current_path) {
    static char selected_path[512];
    char dirs[50][256];
    int num_dirs = 0;
    int selected = 0;

    // Inicialmente é obtido o diretório atual de trabalho
    getcwd(selected_path, sizeof(selected_path));

    // Carrega os subdiretórios na lista
    reload_dirs(dirs, &num_dirs, &selected, selected_path);

    while (1) {
        // Limpa a área interna do diálogo
        for (int i = 1; i < dialog->height - 1; i++) {
            mvwhline(dialog->win, i, 1, ' ', dialog->width - 2);
        }

        // Mostra diretório atual (truncado se necessário para caber)
        mvwprintw(dialog->win, 1, 2, "Current: %.*s", dialog->width - 12, selected_path);

        // Título da lista
        mvwprintw(dialog->win, 3, 2, "Select directory:");

        int max_display = dialog->height - 8; // espaço útil para listar entradas
        for (int i = 0; i < num_dirs && i < max_display; i++) {
        if (i == selected) {
            // Destaque visual para a linha selecionada
            wattron(dialog->win, A_REVERSE);
            mvwprintw(dialog->win, 5 + i, 2, " [%s] ", dirs[i]);
            wattroff(dialog->win, A_REVERSE);
	        } else {
	            mvwprintw(dialog->win, 5 + i, 2, " [%s] ", dirs[i]);
	        }
        }

        // Instruções de uso na parte inferior do diálogo
        mvwprintw(dialog->win, dialog->height - 3, 2, "Arrows: Navigate  Enter: Open");
        mvwprintw(dialog->win, dialog->height - 2, 2, "S: Save on current path  ESC: Cancel");

        wrefresh(dialog->win);

        int ch = wgetch(dialog->win);

        switch (ch) {
            case 27: // ESC — cancelar
                return NULL;

            case 10: // Enter — entrar no diretório selecionado (ou voltar caso "..")
                if (num_dirs > 0) {
                    if (strcmp(dirs[selected], "..") == 0) {
                        chdir("..");
                        getcwd(selected_path, sizeof(selected_path));
                    } else {
                        if (chdir(dirs[selected]) == 0) {
                            getcwd(selected_path, sizeof(selected_path));
                        }
                    }
                    // Recarrega a lista após mudança de diretório
                    reload_dirs(dirs, &num_dirs, &selected, selected_path);
                }
                break;

            case 259: // KEY_UP
                if (selected > 0) selected--;
                break;

            case 258: // KEY_DOWN
                if (selected < num_dirs - 1) selected++;
                break;

            case 's': case 'S': // Selecionar diretório atual e retornar seu caminho
                return strdup(selected_path);

            default:
                // Ignora outras teclas
                break;
        }
    }
}

// Exibe uma caixa de confirmação simples contendo uma mensagem e permitindo que o usuário escolha Yes (y/Y) ou No (n/N). ESC retorna (n/N).
int confirm_dialog(Dialog *dialog, const char *message) {
    // Limpa a área interna do diálogo
    for (int i = 1; i < dialog->height - 1; i++) {
        mvwhline(dialog->win, i, 1, ' ', dialog->width - 2);
    }

    // Mostra a mensagem e as opções (Enter também confirma)
    mvwprintw(dialog->win, 3, 2, "%s", message);
    mvwprintw(dialog->win, dialog->height - 3, 2, "[Y/Enter] Yes  [N] No");

    wrefresh(dialog->win);

    int ch;
    while ((ch = wgetch(dialog->win)) != 27) { // ESC
        if (ch == 'y' || ch == 'Y' || ch == 10 || ch == KEY_ENTER) {
            return 1; // Yes (Enter também confirma)
        } else if (ch == 'n' || ch == 'N') {
            return 0; // No
        }
    }

    // Se pressionou ESC, considera como No
    return 0;
}