/*
 *
 * Rotinas responsáveis pela criação e gerenciamento de diálogos (janelas).
 *
 */

#include "../include/file_validation.h"
#include "../include/dialog.h"
#include <stdlib.h>
#include <string.h>
#include <unistd.h> // Biblioteca para terminal e processos
#include <dirent.h> // Biblioteca para manipulação de diretórios

Dialog* create_dialog(int height, int width, const char *title) { // Cria uma estrutura Dialog, aloca uma nova janela centrada de tamanho (height x width) e desenha uma borda com título.

    Dialog *dialog = malloc(sizeof(Dialog)); // Aloca memória para a estrutura Dialog
    if (!dialog) return NULL; // Retorna NULL se não foi possível alocar memória

    dialog->height = height; // Define a altura do diálogo
    dialog->width = width; // Define a largura do diálogo
    dialog->start_y = (LINES - height) / 2; // Define a posição vertical centralizada
    dialog->start_x = (COLS - width) / 2; // Define a posição horizontal centralizada

    // Cria a janela centrada e habilita captura de teclas especiais
    dialog->win = newwin(height, width, dialog->start_y, dialog->start_x);
    keypad(dialog->win, TRUE); // Habilita captura de teclas especiais

    // Desenha borda e escreve o título na borda superior
    box(dialog->win, 0, 0);
    mvwprintw(dialog->win, 0, 2, " %s ", title); // Escreve o título no centro da borda superior

    wrefresh(dialog->win); // Atualiza a tela com as alterações
    return dialog; // Retorna o ponteiro para o diálogo criado

}

//Libera os recursos associados ao diálogo: deleta a janela e libera a struct.
void destroy_dialog(Dialog *dialog) {

    if (dialog) { // Verifica se o ponteiro não é NULL antes de liberar
        delwin(dialog->win); // Deleta a janela associada ao diálogo
        free(dialog); // Libera a memória alocada para a estrutura Dialog
    }

}

// Exibe um diálogo que permite navegar entre diretórios E selecionar arquivos
char* file_browser_dialog(Dialog *dialog, const char *start_path) {

    char current_path[512]; // Armazena o caminho atual do diretório
    snprintf(current_path, sizeof(current_path), "%s", start_path); // Inicializa com o caminho de início

    char entries[50][256];  // Nomes das entradas (pastas + arquivos)
    int is_dir[50];         // Flag: 1 = diretório, 0 = arquivo
    int num_entries = 0; // Número de entradas encontradas no diretório atual
    int selected = 0; // Índice da entrada selecionada pelo usuário

    while (1) { // Loop principal do diálogo
        num_entries = 0; // Reinicia o número de entradas a cada iteração

        // Adicionar ".." se não estiver na raiz
        if (strlen(current_path) > 1) {
            strcpy(entries[num_entries], ".."); // Adiciona ".." ao início das entradas
            is_dir[num_entries] = 1; // Indica que ".." é um diretório
            num_entries++;
        }

        // Ler diretório atual
        DIR *dir = opendir(current_path); // Abre o diretório atual
        if (dir) { // Se o diretório foi aberto com sucesso
            struct dirent *entry; // Pega a próxima entrada do diretório
            while ((entry = readdir(dir)) != NULL && num_entries < 49) { // Enquanto houver entradas e não ultrapassar o limite
                // Pular arquivos ocultos (exceto "..")
                if (entry->d_name[0] == '.' && strcmp(entry->d_name, "..") != 0) { // Se for um arquivo oculto e não for ".."
                    continue;
                }

                // Adicionar diretórios e arquivos
                if (entry->d_type == DT_DIR && strcmp(entry->d_name, ".") != 0) { // Se for um diretório e não for "."
                    strncpy(entries[num_entries], entry->d_name, 255); // Copia o nome da entrada para o array de nomes
                    entries[num_entries][255] = '\0'; // Termina a string com '\0'
                    is_dir[num_entries] = 1; // Marca como diretório
                    num_entries++; // Incrementa o número de entradas
                } else if (entry->d_type == DT_REG) { // Se for um arquivo
                    strncpy(entries[num_entries], entry->d_name, 255); // Copia o nome da entrada para o array de nomes
                    entries[num_entries][255] = '\0'; // Termina a string com '\0'
                    is_dir[num_entries] = 0; // Marca como arquivo
                    num_entries++; // Incrementa o número de entradas
                }
            }
            closedir(dir); // Fecha o diretório
        }

        // Limpar área interna
        for (int i = 1; i < dialog->height - 1; i++) {
            mvwhline(dialog->win, i, 1, ' ', dialog->width - 2); // limpa linha por linha
        }

        // Mostrar caminho atual
        mvwprintw(dialog->win, 1, 2, "Current: %.*s", dialog->width - 12, current_path);
        mvwprintw(dialog->win, 3, 2, "Select file:");

        // Mostrar lista de entradas
        int max_display = dialog->height - 8; // Número máximo de linhas para exibir entradas
        for (int i = 0; i < num_entries && i < max_display; i++) { // Itera pelas entradas até o máximo de linhas
            if (i == selected) { // Se for a entrada selecionada, inverte a cor
                wattron(dialog->win, A_REVERSE);
            }

            // Diretórios entre colchetes, arquivos sem
            if (is_dir[i]) { // Se for diretório, exibe entre colchetes
                mvwprintw(dialog->win, 5 + i, 2, " [%s] ", entries[i]);
            } else {
                mvwprintw(dialog->win, 5 + i, 2, "  %s  ", entries[i]); // Se for arquivo, exibe normalmente
            }

            if (i == selected) { // Se for a entrada selecionada, desinverte a cor
                wattroff(dialog->win, A_REVERSE);
            }
        }

        // Instruções
        mvwprintw(dialog->win, dialog->height - 3, 2, "Arrows: Navigate  Enter: Open/Select");
        mvwprintw(dialog->win, dialog->height - 2, 2, "ESC: Cancel");

        wrefresh(dialog->win); // Atualiza a tela

        int ch = wgetch(dialog->win); // Aguarda o próximo caractere

        switch (ch) {
            case 27: // ESC
                return NULL;

            case 10: // Enter
                if (num_entries > 0) {
                    if (is_dir[selected]) {
                        // É um diretório - navegar
                        if (strcmp(entries[selected], "..") == 0) {
                            // Voltar um nível
                            char *last_slash = strrchr(current_path, '/'); // Pega o último segmento do caminho
                            if (last_slash && last_slash != current_path) { // Se não for a raiz
                                *last_slash = '\0'; // Remove o último segmento do caminho
                            }
                        } else {
                            // Entrar no diretório
                            char new_path[512]; // Caminho completo do novo diretório
                            snprintf(new_path, sizeof(new_path), "%s/%s", current_path, entries[selected]); // Constrói o caminho completo do novo diretório
                            strcpy(current_path, new_path); // Copia o novo caminho para o caminho atual
                        }
                        selected = 0; // Reinicia a seleção para o início da lista
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


char* filename_dialog(Dialog *dialog, const char *current_name) { // Mostra um diálogo simples para o usuário digitar um nome de arquivo. Faz limpeza da área interna, exibe prompt e lê input caractere a caractere.

    static char filename[256]; // Buffer para armazenar o nome do arquivo

    // Limpa a área interna do diálogo (entre as bordas)
    for (int i = 1; i < dialog->height - 1; i++) {
        mvwhline(dialog->win, i, 1, ' ', dialog->width - 2); // Preenche a linha com espaços vazios
    }

    // Exibir prompt e valor atual (se houver)
    mvwprintw(dialog->win, 2, 2, "Filename:");
    mvwprintw(dialog->win, 4, 2, "%s", current_name ? current_name : ""); // Exibe o valor atual do nome do arquivo, se houver
    mvwprintw(dialog->win, dialog->height - 3, 2, "Enter: Confirm  ESC: Cancel");

    // Posicionar cursor no campo de entrada
    wmove(dialog->win, 4, 2); //
    wrefresh(dialog->win); // Atualiza a tela com o prompt e o valor atual (se houver)

    // Prepara para capturar o input com eco local e cursor visível
    echo();
    curs_set(1);

    int ch; // Armazena o caractere lido do teclado
    int pos = current_name ? (int)strlen(current_name) : 0; // Posição do cursor no campo de entrada

    if (current_name) {
        // Copia o nome atual para o buffer e posiciona o cursor no fim
        strncpy(filename, current_name, sizeof(filename) - 1);
        filename[sizeof(filename) - 1] = '\0';
        wmove(dialog->win, 4, 2 + pos);
    } else { // Se não há nome atual, limpa o buffer e posiciona o cursor no início
        filename[0] = '\0';
        pos = 0;
    }

    // Loop de leitura caractere a caractere até Enter
    while ((ch = wgetch(dialog->win)) != 10) { // 10 == Enter
        if (ch == 27) { // ESC
            // Cancela o diálogo e retorna NULL (sem alterar estado externo)
            noecho(); // Desabilita eco de caracteres no terminal
            curs_set(0); // Oculta o cursor no terminal
            return NULL; // Retorna NULL para indicar cancelamento
        } else if (ch == KEY_BACKSPACE || ch == 127 || ch == 8) { // Backspace
            // Processa backspace localmente
            if (pos > 0) {
                pos--; // Decrementa a posição do cursor
                filename[pos] = '\0'; // Limpa o caractere apagado
                // Limpa e reescreve a linha de input para manter visual consistente
                mvwprintw(dialog->win, 4, 2, "%-50s", filename); // Reescreve a linha com o nome atualizado
                wmove(dialog->win, 4, 2 + pos); // Move o cursor para a posição atualizada
                wrefresh(dialog->win); // Atualiza a tela
            }
        } else if (ch >= 32 && ch <= 126 && pos < (int)sizeof(filename) - 2) { // Aceita apenas caracteres imprimíveis e respeita o limite do buffer
            filename[pos] = (char)ch; // Armazena o caractere no buffer
            filename[pos + 1] = '\0'; // Termina a string no próximo caractere
            pos++; // Incrementa a posição do cursor
            mvwprintw(dialog->win, 4, 2, "%s", filename); // Reescreve a linha com o nome atualizado
            wmove(dialog->win, 4, 2 + pos);
            wrefresh(dialog->win);
        } else {
            // Teclas não tratadas são ignoradas e um beep é emitido como aviso
            beep();
        }
    }


    noecho(); // Finaliza modo de entrada
    keypad(dialog->win, FALSE); // Desativa teclas especiais da janela (setas, enter, etc.)
    curs_set(0); // Oculta o cursor

    // Se o usuário digitou algo, valida via process_filename
    if (strlen(filename) > 0) {
        char *processed = process_filename(filename); // Processa o nome do arquivo
        if (!processed) { // Se o nome não é válido, mostra erro
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


static void reload_dirs(char dirs[50][256], int *num_dirs, int *selected, char *selected_path) { // Preenche a matriz `dirs` com nomes de subdiretórios do diretório atual. Insere ".." como primeira entrada salvo quando já estivermos na raiz ("/").

    *num_dirs = 0; // Inicializa o número de diretórios encontrados

    // Se não estivermos na raiz, adiciona opção de voltar ("..")
    if (strlen(selected_path) > 1) {
        strcpy(dirs[*num_dirs], "..");
        (*num_dirs)++;
    }

    DIR *dir = opendir("."); // Abre o diretório atual
    if (dir) { // Se o diretório foi aberto com sucesso
        struct dirent *entry;
        while ((entry = readdir(dir)) != NULL && *num_dirs < 49) { // Enquanto houver entradas e não ultrapassarmos o limite
            // Considera apenas entradas do tipo diretório e que não comecem com '.' (analizar depois se é ideal mostrar diretórios ocultos)
            if (entry->d_type == DT_DIR && entry->d_name[0] != '.') {
                strncpy(dirs[*num_dirs], entry->d_name, 255); // Copia o nome do diretório para a matriz `dirs`
                dirs[*num_dirs][255] = '\0'; // Define o caractere nulo no final da string
                (*num_dirs)++;
            }
        }
        closedir(dir); // Fecha o diretório após ler todas as entradas
    }
    *selected = 0; // Define o índice selecionado como 0 (primeira entrada)

}

char* directory_dialog(Dialog *dialog, const char *current_path) { // Exibe um diálogo que permite navegar entre diretórios, selecionar o diretório atual ou cancelar.

    static char selected_path[512]; // Armazena o caminho do diretório selecionado
    char dirs[50][256]; // Armazena os nomes dos diretórios disponíveis
    int num_dirs = 0; // Contador de diretórios disponíveis
    int selected = 0; // Índice do diretório selecionado

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
        for (int i = 0; i < num_dirs && i < max_display; i++) { // Percorre os diretórios disponíveis
		    if (i == selected) {
		        // Destaque visual para a linha selecionada
		        wattron(dialog->win, A_REVERSE);
		        mvwprintw(dialog->win, 5 + i, 2, " [%s] ", dirs[i]);
		        wattroff(dialog->win, A_REVERSE);
		    } else { // Diretório não selecionado
		        mvwprintw(dialog->win, 5 + i, 2, " [%s] ", dirs[i]);
		    }
        }

        // Instruções de uso na parte inferior do diálogo
        mvwprintw(dialog->win, dialog->height - 3, 2, "Arrows: Navigate  Enter: Open");
        mvwprintw(dialog->win, dialog->height - 2, 2, "S: Save on current path  ESC: Cancel");

        wrefresh(dialog->win); // Atualiza a tela com as alterações

        int ch = wgetch(dialog->win); // Captura a tecla pressionada pelo usuário

        switch (ch) {
            case 27: // ESC — cancelar
                return NULL;

            case 10: // Enter — entrar no diretório selecionado (ou voltar caso "..")
                if (num_dirs > 0) {
                    if (strcmp(dirs[selected], "..") == 0) {
                        chdir("..");
                        getcwd(selected_path, sizeof(selected_path)); // Obtém o novo caminho após voltar
                    } else { // Diretório selecionado, entra no diretório
                        if (chdir(dirs[selected]) == 0) {
                            getcwd(selected_path, sizeof(selected_path)); // Obtém o novo caminho após entrar no diretório
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

int confirm_dialog(Dialog *dialog, const char *message) { // Exibe uma caixa de confirmação simples contendo uma mensagem e permitindo que o usuário escolha Yes (y/Y) ou No (n/N). ESC retorna (n/N).

    // Limpa a área interna do diálogo
    for (int i = 1; i < dialog->height - 1; i++) {
        mvwhline(dialog->win, i, 1, ' ', dialog->width - 2);
    }

    // Mostra a mensagem e as opções (Enter também confirma)
    mvwprintw(dialog->win, 3, 2, "%s", message);
    mvwprintw(dialog->win, dialog->height - 3, 2, "[Y/Enter] Yes  [N/n] No");

    wrefresh(dialog->win); // Atualiza a tela com as novas informações

    int ch; // Armazena a tecla pressionada pelo usuário
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

bool mode_dialog(Dialog *dialog, const char *message) { // Exibe uma caixa de diálogo de modo (Enable/Disable) e retorna true/false com base na escolha do usuário.

    // Limpa área interna (igual às outras funções)
    for (int i = 1; i < dialog->height - 1; i++) {
        mvwhline(dialog->win, i, 1, ' ', dialog->width - 2);
    }

    mvwprintw(dialog->win, 3, 2, "%s", message);
    mvwprintw(dialog->win, dialog->height - 3, 2, "[Y/Enter] Enable  [N/n] Disable");

    wrefresh(dialog->win); // Atualiza a tela com as novas informações

    int ch; // Armazena a tecla pressionada pelo usuário
    while ((ch = wgetch(dialog->win)) != 27) { // !ESC
        if (ch == 'y' || ch == 'Y' || ch == 10 || ch == KEY_ENTER){ // Enter também confirma
            return true;
        }
        if (ch == 'n' || ch == 'N'){
            return false;
        }
    }

    return false; // ESC = desativa

}