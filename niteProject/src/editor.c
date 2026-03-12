/*
 *
 * Arquivo principal; funcionalidades relacionadas ao editor são tratadas aqui.
 *
 */

#include "../include/file_validation.h"
#include "../include/editor.h"
#include "../include/dialog.h"
#include "../include/syntax.h"
#include <ncurses.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static int current_line_len(EditorBuffer *buffer);
static void handle_system_clipboard(char *dest, int max_len);
static void set_system_clipboard(const char *text);
static void selection_bounds(const Selection *sel, int *sl, int *sc, int *el, int *ec);
static void copy_selection_to_clipboard(EditorBuffer *buffer, const Selection *sel, char *dest, size_t max_len);
static void delete_selection(EditorBuffer *buffer, Selection *sel);

char clipboard[MAX_CLIPBOARD_SIZE] = ""; // Buffer global para armazenar o conteúdo copiado/colado (suporta multilinha para blocos de código)

EditorBuffer* create_new_file() {

    EditorBuffer *buffer = malloc(sizeof(EditorBuffer)); // Alocar memória para o buffer do editor, que é a estrutura principal que armazena o conteúdo do arquivo, o nome do arquivo, o estado de modificação e a posição do cursor.
    buffer->lines = malloc(INITIAL_CAPACITY * sizeof(char*)); // Alocar array de linhas
    buffer->lines[0] = malloc(256);  // Primeira linha vazia
    buffer->lines[0][0] = '\0'; // Iniciar com string vazia
    buffer->num_lines = 1; // Iniciar com 1 linha (mesmo que vazia)
    buffer->capacity = INITIAL_CAPACITY; // Capacidade inicial
    buffer->filename = NULL; // Sem nome inicialmente
    buffer->modified = 0; // Ainda não modificado
    buffer->current_line = 0; // Cursor começa na primeira linha
    buffer->current_col = 0;  // Linha inicia vazia
    return buffer; // Retornar o buffer criado

}

void free_editor_buffer(EditorBuffer *buffer) { // Liberar memória alocada para o buffer

    if (buffer) { // Verificar se o buffer é válido antes de tentar liberar
        for (int i = 0; i < buffer->num_lines; i++) { // Liberar cada linha individualmente
            free(buffer->lines[i]);
        }
        free(buffer->lines); // Liberar o array de linhas
        free(buffer->filename); // Liberar o nome do arquivo, se existir

        //Liberar syntax
        if (buffer->syntax) { // Verificar se o contexto de syntax existe antes de tentar destruir
            syntax_destroy(buffer->syntax); // Destruir o contexto de syntax para liberar recursos relacionados
        }

        free(buffer); // Finalmente, liberar o próprio buffer
    }

}

int save_file(EditorBuffer *buffer, const char *filename) { // Função para salvar o conteúdo do buffer em um arquivo

    FILE *file = fopen(filename, "w"); // Tentar abrir o arquivo para escrita
    if (!file) { // Verificar se o arquivo foi aberto com sucesso
        return 0;  // Erro ao abrir arquivo
    }

    for (int i = 0; i < buffer->num_lines; i++) { // Escrever cada linha do buffer no arquivo, adicionando uma nova linha após cada uma
        fprintf(file, "%s\n", buffer->lines[i]); // Escrever a linha atual seguida de uma nova linha
    }

    fclose(file); // Fechar o arquivo após a escrita
    buffer->modified = 0; // Marcar o buffer como não modificado, já que as mudanças foram salvas
    return 1;  // Sucesso

}

// Retorna o comprimento da linha atual sem dereferenciar ponteiro nulo (evita SIGSEGV).
static int current_line_len(EditorBuffer *buffer) {

	if (!buffer || !buffer->lines || buffer->num_lines <= 0) return 0; // Se o buffer é nulo ou vazio, retorna 0.
	if (buffer->current_line < 0 || buffer->current_line >= buffer->num_lines) return 0; // Se a linha atual é inválida, retorna 0.
	char *line = buffer->lines[buffer->current_line]; // Obter a linha atual do buffer.

	return line ? (int)strlen(line) : 0; // Retornar o comprimento da linha atual, evitando SIGSEGV.

}

void enter_editor_mode(EditorBuffer *buffer, WINDOW *win, int row, int col) { // Função principal para entrar no modo de edição, onde o usuário pode interagir com o conteúdo do buffer e realizar ações como salvar ou sair

	keypad(win, TRUE); // Habilitar captura de teclas especiais (setas, backspace, etc.) na janela do editor, permitindo que o usuário navegue e edite o conteúdo usando essas teclas. Isso é essencial para a funcionalidade básica de um editor de texto, onde a navegação e edição são realizadas principalmente através do teclado.

    int ch; // Variável para armazenar a tecla pressionada pelo usuário
    int editor_active = 1; // Flag para controlar o loop principal do editor, indicando se o editor está ativo ou deve ser encerrado

    // Modo de comando
    int command_line_active = 0; // Flag para indicar se o modo de comando está ativo, permitindo ao usuário digitar comandos específicos para ações como salvar ou sair
    char cmdbuf[512]; // Buffer para armazenar o comando digitado pelo usuário no modo de comando
    int cmdlen = 0; // Comprimento atual do comando digitado, usado para controlar a posição do cursor e a edição do comando
    EditorHistory *history = history_create(); // Criar o histórico de "undo/redo" para o editor, permitindo que o usuário possa desfazer ou refazer ações realizadas durante a edição. O histórico é inicializado vazio e será preenchido com snapshots do estado do buffer à medida que o usuário realiza ações de edição.
    history_push(history, buffer); // Adicionar o estado inicial do buffer ao histórico, permitindo que o usuário possa desfazer até o estado inicial se desejar. Isso garante que o histórico tenha um ponto de partida para as operações de undo/redo, mesmo antes de qualquer ação de edição ser realizada.
    Selection sel = {0, 0, 0, 0, 0}; // Seleção inicializada como inativa, com as posições de início e fim definidas como 0.

    // Inicializar cores se disponível
    if (has_colors()) {
        start_color();
        use_default_colors(); // Usar cores padrão do terminal
        init_pair(1, COLOR_RED, -1);     // Vermelho
        init_pair(2, COLOR_YELLOW, -1);  // Amarelo
        init_pair(3, COLOR_WHITE, -1);   // Branco
    }

    // Inicializar cores para syntax highlighting
    if (has_colors()) {
        init_pair(5, COLOR_MAGENTA, -1);   // Keywords
        init_pair(6, COLOR_CYAN, -1);      // Functions
        init_pair(7, COLOR_GREEN, -1);     // Strings
        init_pair(8, COLOR_RED, -1);    // Numbers
        init_pair(9, COLOR_BLUE, -1);      // Comments
        init_pair(10, COLOR_BLUE, -1);     // Types
    }

    buffer->syntax = syntax_create(buffer->filename); // Tentar criar contexto de syntax baseado no nome do arquivo (extensão)
    if (buffer->syntax) { // Se o contexto de syntax foi criado com sucesso, atualizar o parsing inicial para aplicar highlights
        syntax_update(buffer->syntax, buffer->lines, buffer->num_lines);
    }

    while (editor_active) {
        // Garantir invariantes do buffer (evita SIGSEGV após paste/undo/redo ou estado inconsistente)
        if (buffer->num_lines == 0 && buffer->lines) {
            buffer->lines[0] = strdup("");
            buffer->num_lines = 1;
            buffer->current_line = 0;
            buffer->current_col = 0;
        }
        if (buffer->current_line < 0) buffer->current_line = 0;
        if (buffer->num_lines > 0 && buffer->current_line >= buffer->num_lines)
            buffer->current_line = buffer->num_lines - 1;
        if (buffer->lines && buffer->num_lines > 0 && buffer->current_line >= 0 && buffer->current_line < buffer->num_lines &&
            !buffer->lines[buffer->current_line]) {
            buffer->lines[buffer->current_line] = strdup("");
        }
        int cur_len = current_line_len(buffer);
        if (buffer->current_col > cur_len) buffer->current_col = cur_len;

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
                mvwprintw(win, row - 1, 0, ">_%s", cmdbuf); // Sem cores, apenas mostrar o prompt e o comando digitado
            }

            wmove(win, row - 1, 2 + cmdlen); // Posicionar cursor após o comando digitado
            curs_set(1); // Mostrar cursor no modo de comando
            wrefresh(win); // Atualizar a janela para mostrar o prompt e o comando

            // Ler tecla no modo de comando
            ch = wgetch(win);

            // Detectar Alt+Espaço para sair do modo comando
            if (ch == 0) {
                command_line_active = !command_line_active;
                cmdlen = 0;
                cmdbuf[0] = '\0';
                continue;
            }

            if (ch == 27) {
                nodelay(win, TRUE); // Ativar modo não bloqueante para capturar a próxima tecla sem esperar
                int next = wgetch(win); // Capturar a próxima tecla após ESC
                nodelay(win, FALSE); // Voltar ao modo bloqueante
                if (next == ERR) {
                    // Desativar modo
                    command_line_active = 0; // Desativar o modo de comando, voltando ao modo normal do editor
                    cmdlen = 0; // Resetar o comprimento do comando para 0, limpando o buffer de comando
                    cmdbuf[0] = '\0'; // Limpar o buffer de comando, definindo o primeiro caractere como nulo para indicar string vazia
                    continue;
                } else if (next != -1) { // Se a próxima tecla não for 'c' ou 'C' e for uma tecla válida, colocar de volta na fila de entrada para ser processada normalmente
                    ungetch(next); // Colocar a tecla de volta na fila de entrada para que seja processada normalmente pelo editor, permitindo que outras ações sejam realizadas mesmo após pressionar ESC
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
                            // Verificar se já tem filename (arquivo aberto existente)
                            if (buffer->filename != NULL) {
                                // Arquivo já existe, apenas salvar
                                if (save_file(buffer, buffer->filename)) {
                                    endwin();
                                    printf("File saved successfully: %s\nNite editor closed.\n", buffer->filename);
                                } else {
                                    endwin();
                                    printf("Error saving file. Nite editor closed.\n");
                                }
                                exit(0);
                            }

                            // Arquivo novo: perguntar onde salvar
                            char *final_filename = NULL; // Variável para armazenar o nome final do arquivo após validação
                            char *final_path = NULL; // Variável para armazenar o caminho final do diretório escolhido pelo usuário

                            // Loop para solicitar nome válido
                            while (!final_filename) { // Enquanto o nome do arquivo final não for válido, continuar solicitando ao usuário
                                Dialog *name_dialog = create_dialog(10, 60, "Save File - Enter Name");
                                char *filename = filename_dialog(name_dialog, buffer->filename); // Dialog para solicitar o nome do arquivo, passando o nome atual como sugestão
                                destroy_dialog(name_dialog); // Destruir o dialog após obter a entrada do usuário

                                if (!filename) {
                                    // Se cancelou, encerra o programa
                                    endwin();
                                    printf("Save cancelled. Nite editor closed.\n");
                                    exit(0);
                                }

                                final_filename = process_filename(filename); // Validar o nome do arquivo usando o sistema de validação, que verifica a extensão e retorna um nome válido ou NULL se for inválido
                                free(filename);

                                if (!final_filename) {
                                    // Se falhou na validação, mostra erro e tenta novamente
                                    Dialog *error_dialog = create_dialog(8, 60, "Error");
                                    mvwprintw(error_dialog->win, 3, 2, "Unsupported file type!");
                                    wrefresh(error_dialog->win); // Atualizar o dialog para mostrar a mensagem de erro
                                    wgetch(error_dialog->win); // Aguardar o usuário pressionar uma tecla para continuar, permitindo que ele leia a mensagem de erro antes de tentar novamente
                                    destroy_dialog(error_dialog); // Destruir o dialog de erro antes de solicitar o nome novamente
                                }
                            }

                            // Dialog para diretório
                            Dialog *dir_dialog = create_dialog(20, 70, "Save File - Choose Directory");
                            final_path = directory_dialog(dir_dialog, "."); // Dialog para escolher o diretório onde salvar o arquivo, começando no diretório atual (".")
                            destroy_dialog(dir_dialog);

                            if (final_path) { // Se um diretório foi escolhido, criar o caminho completo para o arquivo
                                char full_path[512]; // Buffer para armazenar o caminho completo do arquivo, combinando o diretório escolhido e o nome do arquivo validado
                                snprintf(full_path, sizeof(full_path), "%s/%s", final_path, final_filename); // Criar o caminho completo do arquivo usando snprintf para garantir que não haja estouro de buffer, formatando a string como "diretório/nome_do_arquivo"

                                // Verificar se arquivo já existe
                                FILE *check_file = fopen(full_path, "r"); // Tentar abrir o arquivo para leitura para verificar se ele já existe no caminho especificado
                                int file_exists = (check_file != NULL); // Se o arquivo foi aberto com sucesso, significa que ele existe, então a variável file_exists é definida como 1 (verdadeiro); caso contrário, é definida como 0 (falso)
                                if (check_file) fclose(check_file); // Se o arquivo foi aberto, fechá-lo imediatamente, pois a verificação é apenas para existência

                                int should_save = 1; // Flag para controlar se o arquivo deve ser salvo, inicialmente definida como 1 (verdadeiro), mas pode ser alterada para 0 (falso) se o usuário decidir não sobrescrever um arquivo existente
                                if (file_exists) {
                                    // Arquivo existe, perguntar se quer sobrescrever
                                    Dialog *overwrite_dialog = create_dialog(8, 50, "File Exists");
                                    int overwrite = confirm_dialog(overwrite_dialog, "File exists. Overwrite?");
                                    destroy_dialog(overwrite_dialog); // Dialog para confirmar se o usuário deseja sobrescrever o arquivo existente, passando a mensagem "File exists. Overwrite?" e armazenando a resposta do usuário na variável overwrite (1 para sim, 0 para não)
                                    should_save = overwrite; // Se o usuário escolher sobrescrever (overwrite == 1), a variável should_save permanece como 1 (verdadeiro), permitindo que o arquivo seja salvo; caso contrário, se o usuário escolher não sobrescrever (overwrite == 0), a variável should_save é definida como 0 (falso), indicando que o arquivo não deve ser salvo e o processo de salvamento será cancelado
                                }

                                if (should_save) { // Se o usuário confirmou que deseja salvar (seja porque o arquivo não existia ou porque ele escolheu sobrescrever), tentar salvar o arquivo usando a função save_file, passando o buffer do editor e o caminho completo do arquivo como argumentos. Se o salvamento for bem-sucedido, mostrar uma mensagem de sucesso e fechar o editor; caso contrário, mostrar uma mensagem de erro e fechar o editor.
                                    if (save_file(buffer, full_path)) {
                                        endwin();
                                        printf("File saved successfully: %s\nNite editor closed.\n", full_path);
                                    } else {
                                        endwin();
                                        printf("Error saving file. Nite editor closed.\n");
                                    }
                                } else {
                                    endwin();
                                    printf("Save cancelled. Nite editor closed.\n");
                                }
                                free(final_path); // Liberar a memória alocada para o caminho do diretório escolhido, já que não é mais necessário após o processo de salvamento
                            } else {
                                endwin();
                                printf("Save cancelled. Nite editor closed.\n");
                            }

                            free(final_filename); // Liberar a memória alocada para o nome do arquivo validado, já que não é mais necessário após o processo de salvamento
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

                            command_line_active = 0; // Desativar modo comando após salvar
                            cmdlen = 0; // Resetar comprimento do comando para 0, limpando o buffer de comando para a próxima vez que o usuário entrar no modo de comando
                            cmdbuf[0] = '\0'; // Limpar o buffer de comando, definindo o primeiro caractere como nulo para indicar string vazia, garantindo que o próximo comando digitado comece com um buffer limpo
                        } else {
                            // Erro ao salvar
                            Dialog *error_dialog = create_dialog(8, 60, "Error");
                            mvwprintw(error_dialog->win, 3, 2, "Error saving file!");
                            mvwprintw(error_dialog->win, 4, 2, "Press any key to continue...");
                            wrefresh(error_dialog->win); // Atualizar o dialog para mostrar a mensagem de erro
                            wgetch(error_dialog->win); // Aguardar o usuário pressionar uma tecla para continuar, permitindo que ele leia a mensagem de erro antes de fechar o dialog
                            destroy_dialog(error_dialog);

                            command_line_active = 0;
                            cmdlen = 0;
                            cmdbuf[0] = '\0';
                        }
                    } else {
                        // Arquivo novo = perguntar onde salvar
                        char *final_filename = NULL; // Variável para armazenar o nome final do arquivo após validação
                        char *final_path = NULL; // Variável para armazenar o caminho final do diretório escolhido pelo usuário

                        // Loop para solicitar nome válido
                        while (!final_filename) {
                            Dialog *name_dialog = create_dialog(10, 60, "Save File - Enter Name");
                            char *filename = filename_dialog(name_dialog, buffer->filename); // Dialog para solicitar o nome do arquivo, passando o nome atual como sugestão (que pode ser NULL se for um novo arquivo sem nome)
                            destroy_dialog(name_dialog);

                            if (!filename) {
                                // Voltar ao modo normal sem salvar
                                command_line_active = 0;
                                cmdlen = 0;
                                cmdbuf[0] = '\0';
                                break;
                            }

                            final_filename = process_filename(filename); // Validar o nome do arquivo usando o sistema de validação, que verifica a extensão e retorna um nome válido ou NULL se for inválido
                            free(filename); // Liberar a memória alocada para o nome do arquivo digitado, já que não é mais necessário após a validação

                            if (!final_filename) {
                                // Se falhou na validação, mostra erro e tenta novamente
                                Dialog *error_dialog = create_dialog(8, 60, "Error");
                                mvwprintw(error_dialog->win, 3, 2, "Unsupported file type!");
                                mvwprintw(error_dialog->win, 4, 2, "Press any key to try again...");
                                wrefresh(error_dialog->win); // Atualizar o dialog para mostrar a mensagem de erro
                                wgetch(error_dialog->win); // Aguardar o usuário pressionar uma tecla para continuar, permitindo que ele leia a mensagem de erro antes de tentar novamente
                                destroy_dialog(error_dialog);
                            }
                        }

                        if (!final_filename) continue; // Se cancelou no loop

                        // Dialog para escolher diretório
                        Dialog *dir_dialog = create_dialog(20, 70, "Save File - Choose Directory");
                        final_path = directory_dialog(dir_dialog, "."); // Dialog para escolher o diretório onde salvar o arquivo, começando no diretório atual (".")
                        destroy_dialog(dir_dialog);

                        if (!final_path) { // Se cancelou a escolha do diretório, voltar ao modo normal sem salvar
                            free(final_filename);
                            command_line_active = 0;
                            cmdlen = 0;
                            cmdbuf[0] = '\0';
                            continue;
                        }

                        // Criar caminho completo
                        char full_path[512]; // Buffer para armazenar o caminho completo do arquivo, combinando o diretório escolhido e o nome do arquivo validado
                        snprintf(full_path, sizeof(full_path), "%s/%s", final_path, final_filename); // Criar o caminho completo do arquivo usando snprintf para garantir que não haja estouro de buffer, formatando a string como "diretório/nome_do_arquivo"

                        // Verificar se arquivo já existe
                        FILE *check_file = fopen(full_path, "r");
                        int file_exists = (check_file != NULL);
                        if (check_file) fclose(check_file);

                        int should_save = 1;
                        if (file_exists) {
                            // Arquivo existe, perguntar se quer sobrescrever
                            Dialog *overwrite_dialog = create_dialog(8, 50, "File Exists");
                            int overwrite = confirm_dialog(overwrite_dialog, "File exists. Overwrite?");
                            destroy_dialog(overwrite_dialog);
                            should_save = overwrite;
                        }

                        // Salvar arquivo se confirmado
                        if (should_save) {
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
                        } else {
                            // Salvamento cancelado
                            mvwhline(win, row - 1, 0, ' ', col);
                            mvwprintw(win, row - 1, 0, "Save cancelled.");
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

        // MODO NORMAL DO EDITOR
        // Limpar tela e mostrar conteúdo do arquivo
        wclear(win);

        mvwprintw(win, 0, 0, "Editing: %s", buffer->filename ? buffer->filename : "New File"); // Mostrar o nome do arquivo sendo editado, ou "New File" se for um arquivo novo sem nome
        mvwprintw(win, 1, 0, "CTRL+Space: Command Mode | !s: Save | !q: Quit"); // Mostrar instruções de atalho para o usuário, indicando como entrar no modo de comando (ALT+Espaço) e os comandos para salvar (!s) e sair (!q)

        // Mostrar linhas do arquivo numeradas e com quebra de linha
        int display_start = (buffer->current_line > row - 10) ? buffer->current_line - (row - 10) : 0;

        // Calcular largura necessária para números de linha (max 4 dígitos)
        int line_num_width = 4;

        // Calcular largura disponível para o conteúdo
        int content_width = col - line_num_width - 2;
        if (content_width <= 0) content_width = 1;

        int screen_line = 3;  // Linha inicial
        int max_screen_line = row - 5;  // Linha máxima disponível

        for (int i = display_start; i < buffer->num_lines && screen_line < max_screen_line; i++) { // Iterar sobre as linhas do buffer a partir da linha de exibição calculada, garantindo que não ultrapasse o número máximo de linhas que podem ser exibidas na tela
            int line_number = i + 1;
            int is_current_line = (i == buffer->current_line); // Verificar se a linha atual do loop é a linha onde o cursor está posicionado, para aplicar destaque visual

            char *line_content = buffer->lines[i]; // Obter o conteúdo da linha atual do buffer para renderizar na tela
            int line_len = strlen(line_content); // Calcular o comprimento da linha para determinar quantas linhas visuais ela ocupará quando renderizada com quebra de linha, considerando a largura disponível para o conteúdo

            int visual_lines = (line_len + content_width - 1) / content_width; // Calcular o número de linhas visuais necessárias para renderizar a linha atual, dividindo o comprimento da linha pelo conteúdo disponível por linha e arredondando para cima. Isso determina quantas vezes a linha precisa ser quebrada para caber na largura da tela.
            if (visual_lines == 0) visual_lines = 1;  // Linhas vazias ocupam 1 linha visual

            for (int wrap_line = 0; wrap_line < visual_lines && screen_line < max_screen_line; wrap_line++) { // Iterar sobre cada linha visual resultante da quebra da linha atual, garantindo que não ultrapasse o número máximo de linhas que podem ser exibidas na tela. A variável wrap_line indica qual parte da linha original está sendo renderizada (0 para a primeira parte, 1 para a segunda, etc.).
                // Desenhar número da linha apenas na primeira linha visual em caso de quebra
                if (wrap_line == 0) {
                    if (has_colors()) {
                        if (is_current_line) {
                            wattron(win, COLOR_PAIR(2) | A_BOLD); // Amarelo + negrito
                        } else {
                            wattron(win, COLOR_PAIR(3)); // Branco normal
                        }
                    } else if (is_current_line) {
                        wattron(win, A_BOLD); // Se não houver suporte a cores, usar negrito para destacar a linha atual
                    }

                    mvwprintw(win, screen_line, 0, "%3d ", line_number); // Imprimir o número da linha formatado com largura fixa de 3 dígitos, seguido por um espaço, para criar uma coluna de números de linha alinhada à direita. O número da linha é calculado como i + 1 para começar a contagem a partir de 1 em vez de 0.

                    if (has_colors()) { // Desligar atributos após imprimir o número da linha para evitar que o destaque se estenda ao conteúdo da linha. Se a linha atual estiver destacada, desligar o par de cores e o negrito; caso contrário, apenas desligar o par de cores.
                        if (is_current_line) { // Se a linha atual estiver destacada, desligar o par de cores e o negrito para evitar que o destaque se estenda ao conteúdo da linha. Caso contrário, apenas desligar o par de cores para as linhas normais.
                            wattroff(win, COLOR_PAIR(2) | A_BOLD);
                        } else {
                            wattroff(win, COLOR_PAIR(3));
                        }
                    } else if (is_current_line) { // Se não houver suporte a cores e a linha atual estiver destacada, desligar o negrito após imprimir o número da linha para evitar que o destaque se estenda ao conteúdo da linha.
                        wattroff(win, A_BOLD);
                    }
                } else {
                    // Linhas de quebra: imprimir espaços para alinhar com a coluna de números de linha, garantindo que o conteúdo das linhas quebradas fique alinhado corretamente abaixo do número da linha.
                    mvwprintw(win, screen_line, 0, "    ");
                }

                int start_pos = wrap_line * content_width; // Calcular a posição inicial do segmento da linha original que deve ser renderizado nesta linha visual, multiplicando o número da linha visual (wrap_line) pela largura disponível para o conteúdo (content_width). Isso determina qual parte da linha original deve ser exibida em cada linha visual resultante da quebra.
                int chars_to_show = (line_len - start_pos > content_width) ? content_width : (line_len - start_pos); // Calcular quantos caracteres do conteúdo da linha original devem ser mostrados nesta linha visual, verificando se o número de caracteres restantes a partir da posição inicial (line_len - start_pos) é maior do que a largura disponível para o conteúdo (content_width). Se for maior, mostrar apenas content_width caracteres; caso contrário, mostrar todos os caracteres restantes. Isso garante que cada linha visual mostre apenas a parte da linha original que cabe na largura da tela, criando o efeito de quebra de linha.

                if (chars_to_show > 0) { // Se houver caracteres para mostrar nesta linha visual, extrair o segmento correspondente da linha original e renderizá-lo na tela. O segmento é extraído usando strncpy para copiar os caracteres da posição inicial calculada (start_pos) até o número de caracteres a mostrar (chars_to_show), garantindo que o segmento seja corretamente terminado com um caractere nulo ('\0') para formar uma string válida.
                    char segment[content_width + 1];
                    strncpy(segment, line_content + start_pos, chars_to_show); // Copiar os caracteres do segmento da linha original para a variável segment, começando na posição inicial calculada (line_content + start_pos) e copiando o número de caracteres a mostrar (chars_to_show). O tamanho do segmento é definido como content_width + 1 para garantir espaço suficiente para os caracteres e o caractere nulo de terminação.
                    segment[chars_to_show] = '\0';

                    // Renderizar com syntax highlighting
                    for (int char_idx = 0; char_idx < chars_to_show; char_idx++) {
                        char ch = segment[char_idx]; // Obter o caractere atual do segmento que está sendo renderizado para aplicar o destaque de syntax, verificando o tipo de destaque para cada caractere com base no contexto de syntax do buffer. O índice char_idx é usado para iterar sobre cada caractere do segmento, e a posição real do caractere na linha original é calculada como start_pos + char_idx para obter a posição correta para a verificação de destaque.

                        if (buffer->syntax && buffer->syntax->enabled) { // Se o contexto de syntax estiver habilitado, obter o tipo de destaque para o caractere atual usando a função syntax_get_highlight, passando a linha atual (i) e a posição do caractere na linha (col_in_line) como argumentos. O tipo de destaque retornado é usado para determinar qual cor aplicar ao caractere ao renderizá-lo na tela, permitindo que palavras-chave, funções, strings, números, comentários e tipos sejam destacados com cores diferentes para melhorar a legibilidade do código.
                            int col_in_line = start_pos + char_idx;
                            HighlightType hl = syntax_get_highlight(buffer->syntax, i, col_in_line); // Obter o tipo de destaque para o caractere atual usando a função syntax_get_highlight, passando o contexto de syntax do buffer, a linha atual (i) e a posição do caractere na linha (col_in_line) como argumentos. O tipo de destaque retornado é um valor do enum HighlightType que indica se o caractere é parte de uma palavra-chave, função, string, número, comentário, tipo ou nenhum destaque.

                            int color_pair = 0;
                            switch (hl) { // Determinar o par de cores a ser usado para renderizar o caractere com base no tipo de destaque retornado pela função syntax_get_highlight. Cada tipo de destaque é associado a um par de cores específico que foi definido anteriormente usando init_pair, permitindo que diferentes elementos do código sejam renderizados com cores distintas para melhorar a legibilidade.
                               	case HIGHLIGHT_KEYWORD:  color_pair = 5; break;
                                case HIGHLIGHT_FUNCTION: color_pair = 6; break;
                                case HIGHLIGHT_STRING:   color_pair = 7; break;
                                case HIGHLIGHT_NUMBER:   color_pair = 8; break;
                                case HIGHLIGHT_COMMENT:  color_pair = 9; break;
                                case HIGHLIGHT_TYPE:     color_pair = 10; break;
                                default: color_pair = 0;
                            }

                            // Calcular se o caractere atual está dentro da seleção ativa, para aplicar destaque de seleção (inversão de cores) ao renderizá-lo. As posições de início e fim da seleção são normalizadas para garantir que sl/sc representem sempre o início e el/ec o fim, independentemente da direção em que o usuário fez a seleção.
                            int abs_col = start_pos + char_idx;
                            int in_selection = 0;

                            if (sel.active) { // Se a seleção estiver ativa, calcular se o caractere atual está dentro da área selecionada para aplicar destaque de seleção (inversão de cores) ao renderizá-lo. As posições de início e fim da seleção são obtidas a partir da estrutura sel, e é garantido que sl/sc representem a posição inicial da seleção e el/ec representem a posição final, independentemente da direção em que o usuário fez a seleção.
                                int sl = sel.start_line;
                                int sc = sel.start_col;
                                int el = sel.end_line;
                                int ec = sel.end_col;

                                // Garantir que sl/sc seja o início e el/ec seja o fim da seleção, independentemente da direção em que o usuário fez a seleção. Isso é feito comparando as posições de início e fim da seleção e trocando-as se necessário para garantir que sl/sc sempre represente a posição inicial da seleção e el/ec represente a posição final, facilitando a lógica de verificação de quais caracteres estão dentro da seleção.
                                if (sl > el || (sl == el && sc > ec)) {
                                    int tmp;
                                    tmp = sl; sl = el; el = tmp;
                                    tmp = sc; sc = ec; ec = tmp;
                                }

                                // Verificar se o caractere atual está dentro da seleção, considerando as diferentes possibilidades de como a seleção pode ser feita (de cima para baixo, de baixo para cima, etc.). A lógica de verificação determina se o caractere atual (i, abs_col) está dentro da área selecionada definida por sl/sc e el/ec.
                                if (i > sl && i < el) in_selection = 1;
                                else if (i == sl && i == el) in_selection = (abs_col >= sc && abs_col < ec);
                                else if (i == sl) in_selection = (abs_col >= sc);
                                else if (i == el) in_selection = (abs_col < ec);
                            }

                            if (color_pair > 0 && has_colors()) { // Se um par de cores válido foi determinado para o tipo de destaque do caractere e o terminal suporta cores, ativar o par de cores correspondente usando wattron antes de renderizar o caractere na tela. Isso garante que o caractere seja renderizado com a cor correta de acordo com seu tipo de destaque, melhorando a legibilidade do código.
                                wattron(win, COLOR_PAIR(color_pair));
                            }
                            if (in_selection) wattron(win, A_REVERSE); // Aplicar inversão de cores ao caractere se ele estiver dentro da seleção ativa, destacando-o visualmente como parte da área selecionada pelo usuário.
                            mvwaddch(win, screen_line, line_num_width + 1 + char_idx, ch); // Renderizar o caractere na tela usando mvwaddch, posicionando-o na linha visual atual (screen_line) e na coluna calculada como line_num_width + 1 + char_idx para garantir que o conteúdo da linha fique alinhado corretamente após a coluna de números de linha. O caractere é renderizado com o destaque de syntax aplicado, se houver um par de cores válido para o tipo de destaque do caractere.
                            if (in_selection) wattroff(win, A_REVERSE); // Desativar a inversão de cores após renderizar o caractere para garantir que o destaque de seleção não se estenda aos caracteres seguintes.
                            if (color_pair > 0 && has_colors()) { // Se um par de cores válido foi usado para renderizar o caractere, desligar o par de cores após renderizá-lo usando wattroff para garantir que o destaque de syntax não se estenda aos caracteres seguintes, permitindo que cada caractere seja renderizado com a cor correta de acordo com seu tipo de destaque.
                                wattroff(win, COLOR_PAIR(color_pair));
                            }
                        } else { // Se o contexto de syntax não estiver habilitado, renderizar o caractere normalmente sem aplicar destaque de syntax, mas ainda verificando se ele está dentro da seleção ativa para aplicar o destaque de seleção (inversão de cores) se necessário.
                            int abs_col2 = start_pos + char_idx;
                            int in_sel2 = 0;

                            if (sel.active) { // Se a seleção estiver ativa, calcular se o caractere atual está dentro da área selecionada para aplicar destaque de seleção mesmo sem syntax highlighting habilitado.
                                int sl = sel.start_line;
                                int sc = sel.start_col;
                                int el = sel.end_line;
                                int ec = sel.end_col;

                                // Normalizar as posições de início e fim da seleção para garantir que sl/sc represente sempre o início e el/ec o fim, independentemente da direção da seleção feita pelo usuário.
                                if (sl > el || (sl == el && sc > ec)) {
                                    int tmp; tmp = sl; sl = el; el = tmp;
                                    tmp = sc; sc = ec; ec = tmp;
                                }

                                // Verificar se o caractere atual está dentro da área selecionada, usando a mesma lógica do ramo com syntax highlighting para garantir consistência no comportamento da seleção em ambos os casos.
                                if (i > sl && i < el) in_sel2 = 1;
                                else if (i == sl && i == el) in_sel2 = (abs_col2 >= sc && abs_col2 < ec);
                                else if (i == sl) in_sel2 = (abs_col2 >= sc);
                                else if (i == el) in_sel2 = (abs_col2 < ec);
                            }

                            if (in_sel2) wattron(win, A_REVERSE); // Aplicar inversão de cores ao caractere se ele estiver dentro da seleção ativa, destacando-o visualmente como parte da área selecionada pelo usuário, mesmo sem syntax highlighting habilitado.
                            mvwaddch(win, screen_line, line_num_width + 1 + char_idx, ch); // Renderizar o caractere na tela usando mvwaddch, posicionando-o na linha visual atual (screen_line) e na coluna calculada como line_num_width + 1 + char_idx para garantir que o conteúdo da linha fique alinhado corretamente após a coluna de números de linha.
                            if (in_sel2) wattroff(win, A_REVERSE); // Desativar a inversão de cores após renderizar o caractere para garantir que o destaque de seleção não se estenda aos caracteres seguintes.
                        }
                    }
                }
                screen_line++; // Incrementar a linha visual atual (screen_line) para passar para a próxima linha visual, garantindo que as linhas visuais resultantes da quebra sejam renderizadas uma abaixo da outra na tela. Isso permite que o conteúdo da linha original seja exibido corretamente mesmo quando ultrapassa a largura da tela, criando o efeito de quebra de linha. O loop continuará renderizando as linhas visuais até que o número máximo de linhas que podem ser exibidas na tela seja alcançado (max_screen_line), garantindo que o editor não tente renderizar mais linhas do que o espaço disponível na tela.
            }
        }

        // Posicionar cursor calculando linha inicial e considerando linhas com quebras;
        int cursor_visual_line = 3;  // Começar na linha 3

        for (int i = display_start; i < buffer->current_line && cursor_visual_line < max_screen_line; i++) { // Iterar sobre as linhas do buffer desde a linha de exibição inicial (display_start) até a linha onde o cursor está posicionado (buffer->current_line), garantindo que não ultrapasse o número máximo de linhas que podem ser exibidas na tela. Para cada linha, calcular quantas linhas visuais ela ocupa considerando a quebra de linha, e incrementar a posição visual do cursor (cursor_visual_line) de acordo com o número de linhas visuais ocupadas por cada linha. Isso garante que o cursor seja posicionado corretamente na tela, mesmo quando há linhas com quebra, permitindo que o usuário veja o cursor na posição correta dentro do conteúdo do arquivo.
            int line_len = strlen(buffer->lines[i]);
            int visual_lines = (line_len + content_width - 1) / content_width;
            if (visual_lines == 0) visual_lines = 1;
            cursor_visual_line += visual_lines;
        }

        // Adicionar offset para colunas que ultrapassam a largura do conteúdo, garantindo que o cursor seja posicionado corretamente mesmo quando a linha atual do cursor tem mais caracteres do que a largura disponível para o conteúdo, criando um efeito de "scroll horizontal" onde o cursor se move para a direita à medida que o usuário digita ou navega para além da largura da tela. O offset é calculado dividindo a coluna atual do cursor (buffer->current_col) pela largura disponível para o conteúdo (content_width), e esse valor é adicionado à posição visual do cursor (cursor_visual_line) para ajustar sua posição na tela de acordo com o número de "linhas visuais" que foram "puladas" devido à quebra de linha.
        int wrap_offset = buffer->current_col / content_width;
        cursor_visual_line += wrap_offset;

        // Calcular a posição horizontal do cursor considerando a quebra de linha, garantindo que o cursor seja posicionado corretamente na coluna dentro da linha visual atual, mesmo quando a linha tem quebra. A posição horizontal do cursor é calculada usando o operador módulo para obter a posição dentro da linha visual (cursor_x_in_wrap) e adicionando o offset da coluna de números de linha (line_num_width + 1) para garantir que o cursor fique alinhado corretamente após a coluna de números de linha.
        int cursor_x_in_wrap = buffer->current_col % content_width;
        int cursor_x_on_screen = cursor_x_in_wrap + line_num_width + 1;

        wmove(win, cursor_visual_line, cursor_x_on_screen); // Mover o cursor para a posição calculada na tela, usando wmove para posicionar o cursor na linha visual atual (cursor_visual_line) e na coluna calculada (cursor_x_on_screen) para garantir que o cursor fique na posição correta dentro do conteúdo do arquivo, mesmo quando há linhas com quebra. Isso permite que o usuário veja o cursor na posição correta enquanto navega ou edita o arquivo, proporcionando uma experiência de edição mais intuitiva e visualmente consistente.
        curs_set(1); // Garantir que o cursor esteja visível, usando curs_set(1) para mostrar o cursor na tela. Isso é importante para que o usuário possa ver onde o cursor está posicionado enquanto edita o arquivo, especialmente em um editor de texto onde a posição do cursor é fundamental para a navegação e edição do conteúdo. Se o cursor não estiver visível, o usuário pode ter dificuldade em saber onde está editando, tornando a experiência de edição confusa e frustrante.
        wrefresh(win);

        ch = wgetch(win); // Aguardar a entrada do usuário para processar as ações de edição, navegação ou comandos. O programa ficará bloqueado nesta linha até que o usuário pressione uma tecla, permitindo que o editor responda às ações do usuário de forma interativa. A variável ch armazenará o código da tecla pressionada, que será usado para determinar qual ação executar (como inserir um caractere, mover o cursor, entrar no modo de comando, etc.) com base no código da tecla e no estado atual do editor.

        // Detectar Alt+Espaço no modo normal
        if (ch == 0) {
            command_line_active = 1;
            cmdlen = 0;
            cmdbuf[0] = '\0';
            continue;
        }

        switch (ch) {
            case 10: // Enter = Nova linha
                insert_new_line(buffer);
                break;

            case 8: case 127: case 263: // Backspace
                handle_backspace(buffer);
                break;

            case 259: // KEY_UP
                if (buffer->current_line > 0) {
                    buffer->current_line--;
                    buffer->current_col = current_line_len(buffer);
                }
                break;

            case 258: // KEY_DOWN
                if (buffer->current_line < buffer->num_lines - 1) {
                    buffer->current_line++;
                    buffer->current_col = current_line_len(buffer);
                }
                break;

            case 260: // KEY_LEFT
                if (buffer->current_col > 0) {
                    buffer->current_col--;
                }
                break;

            case 261: // KEY_RIGHT
            	if (buffer->current_col < current_line_len(buffer)) {
                    buffer->current_col++;
                }
                break;

            case 9: // TAB = Inserir 4 espaços
                for (int i = 0; i < 4; i++) {
                    insert_character(buffer, ' ');
                }
                break;

            case 1:  // Ctrl+A = Selecionar tudo
                sel.active = 1;
                sel.start_line = 0; // Linha inicial da seleção.
                sel.start_col = 0; // Coluna inicial da seleção.
                sel.end_line = buffer->num_lines > 0 ? buffer->num_lines - 1 : 0; // Linha final da seleção.
                sel.end_col = (buffer->num_lines > 0 && buffer->lines[buffer->num_lines - 1]) ? (int)strlen(buffer->lines[buffer->num_lines - 1]) : 0; // Coluna final da seleção.
                buffer->current_line = 0; // Linha atual do cursor.
                buffer->current_col = 0; // Coluna atual do cursor.
                break;

            case 3:  // Ctrl+C = Copiar
                handle_copy(buffer, &sel);
                break;

            case 22: // Ctrl+V = Colar
                handle_paste(buffer);
                break;

            case 24: // Ctrl+X = Recortar
                history_push(history, buffer);
                handle_cut(buffer, &sel);
                break;

			case 26: // Ctrl+Z = Undo
			    history_undo(history, buffer);
			    if (buffer->syntax)
			        syntax_update(buffer->syntax, buffer->lines, buffer->num_lines);
			    break;

			case 25: // Ctrl+Y = Redo
			    history_redo(history, buffer);
			    if (buffer->syntax)
			        syntax_update(buffer->syntax, buffer->lines, buffer->num_lines);
			    break;

			case KEY_SRIGHT: // Shift+Direita
			    if (!sel.active) {
			        sel.active = 1;
			        sel.start_line = buffer->current_line;
			        sel.start_col  = buffer->current_col;
			    }
			if (buffer->current_col < current_line_len(buffer))
			        buffer->current_col++;
			    sel.end_line = buffer->current_line;
			    sel.end_col  = buffer->current_col;
			    break;

			case KEY_SLEFT: // Shift+Esquerda
			    if (!sel.active) {
			        sel.active = 1;
			        sel.start_line = buffer->current_line;
			        sel.start_col  = buffer->current_col;
			    }
			    if (buffer->current_col > 0)
			        buffer->current_col--;
			    sel.end_line = buffer->current_line;
			    sel.end_col  = buffer->current_col;
			    break;

			case KEY_SF: // Shift+Baixo
			    if (!sel.active) {
			        sel.active = 1;
			        sel.start_line = buffer->current_line;
			        sel.start_col  = buffer->current_col;
			    }
			if (buffer->current_line < buffer->num_lines - 1) {
			        buffer->current_line++;
			        buffer->current_col = current_line_len(buffer);
			    }
			    sel.end_line = buffer->current_line;
			    sel.end_col  = buffer->current_col;
			    break;

			case KEY_SR: // Shift+Cima
			    if (!sel.active) {
			        sel.active = 1;
			        sel.start_line = buffer->current_line;
			        sel.start_col  = buffer->current_col;
			    }
			    if (buffer->current_line > 0) {
			        buffer->current_line--;
			        buffer->current_col = current_line_len(buffer);
			    }
			    sel.end_line = buffer->current_line;
			    sel.end_col  = buffer->current_col;
			    break;

			default:
			    if (ch >= 32 && ch <= 126) {
			        history_push(history, buffer);
			        insert_character(buffer, ch);
			    } else {
			        mvwprintw(win, row - 1, 0, "Unknown key code: %d", ch);
			        wrefresh(win);
			        napms(1500);
			    }
			    curs_set(1);
			    break;
        }
    }

}

void read_only(EditorBuffer *buffer,  WINDOW *win, int row, int col){ // Função para exibir o conteúdo de um arquivo em modo somente leitura, permitindo que o usuário navegue pelas linhas do arquivo usando as setas do teclado, mas sem permitir edição. O usuário também pode ativar um modo de comando para executar comandos como sair do visualizador. A função renderiza o conteúdo do arquivo na janela fornecida, mostrando os números das linhas e aplicando quebra de linha quando necessário para garantir que o conteúdo caiba na largura da tela. O usuário pode navegar usando as setas para cima/baixo e Page Up/Page Down, e pode entrar no modo de comando pressionando ALT+Espaço para executar comandos como !q para sair.

	int ch; // Variável para armazenar o código da tecla pressionada pelo usuário, usada para processar a navegação e os comandos no modo somente leitura. O programa ficará bloqueado aguardando a entrada do usuário nesta variável, permitindo que o usuário interaja com o visualizador de arquivo usando o teclado.
	int display_start = 0; // Variável para controlar a linha do buffer que está sendo exibida no topo da janela, usada para implementar a funcionalidade de rolagem. À medida que o usuário navega para baixo ou para cima, essa variável é ajustada para mostrar a parte correta do arquivo na janela, garantindo que o conteúdo seja renderizado corretamente mesmo quando o arquivo tem mais linhas do que a altura da janela.
	int line_num_width = 4; // Largura reservada para os números das linhas.
	int content_width = col - line_num_width - 2; // Largura disponível para o conteúdo do arquivo.
	if (content_width <= 0) content_width = 1;
	int max_screen_line = row - 3; // Número máximo de linhas visuais que podem ser exibidas na janelal.
	int command_line_active = 0; // Flag para indicar se o modo de comando está ativo.
	char cmdbuf[512]; // Buffer para armazenar o comando digitado pelo usuário no modo de comando.
	int cmdlen = 0; // Variável para controlar o comprimento do comando digitado.

	render: // Rótulo para facilitar a re-renderização da tela após processar a entrada do usuário, permitindo que o programa volte para este ponto e atualize a exibição do conteúdo do arquivo com base na nova posição de exibição (display_start) ou no estado do modo de comando (command_line_active) após o usuário navegar ou executar um comando.
		wclear(win);
		mvwprintw(win, 0, 0, "Reading %s [READ ONLY]", buffer->filename ? buffer->filename : "No file");
		mvwprintw(win, 1, 0, "Arrows: Scroll | CTRL+Space: Command line | !q: quit");

		int screen_line = 3; // Linha inicial para renderizar o conteúdo do arquivo.

		for(int i = display_start; i < buffer->num_lines && screen_line < max_screen_line; i++){ // Iterar sobre as linhas do buffer a partir da linha de exibição calculada (display_start), garantindo que não ultrapasse o número máximo de linhas que podem ser exibidas na tela (max_screen_line). Para cada linha, calcular quantas linhas visuais ela ocupa considerando a quebra de linha, e renderizar cada linha visual na tela com os números das linhas alinhados à direita. Isso permite que o usuário navegue pelo conteúdo do arquivo mesmo quando há mais linhas do que a altura da janela, criando um efeito de rolagem vertical.
			char *line_content = buffer->lines[i];
			int line_len = strlen(line_content);
			int visual_lines = (line_len + content_width - 1) / content_width;
			if(visual_lines == 0) visual_lines = 1;

			for(int wrap = 0; wrap < visual_lines && screen_line < max_screen_line; wrap++){ // Iterar sobre cada linha visual resultante da quebra da linha atual, garantindo que não ultrapasse o número máximo de linhas que podem ser exibidas na tela (max_screen_line). A variável wrap indica qual parte da linha original está sendo renderizada (0 para a primeira parte, 1 para a segunda, etc.). Para cada linha visual, renderizar o número da linha apenas na primeira linha visual (wrap == 0) e imprimir espaços para as linhas de quebra subsequentes para manter o alinhamento correto do conteúdo. Em seguida, extrair o segmento correspondente da linha original para a linha visual atual e renderizá-lo na tela, garantindo que o conteúdo seja exibido corretamente mesmo quando ultrapassa a largura da tela.
				if(wrap == 0){
					mvwprintw(win, screen_line, 0, "%3d ", i + 1);
				}else{
					mvwprintw(win, screen_line, 0, "    ");
				}

				int start_pos = wrap * content_width; // Posição inicial do segmento a mostrar na linha visual.
				int chars_to_show = line_len - start_pos; // Número de caracteres a mostrar nesta linha visual.
				if(chars_to_show > content_width) chars_to_show = content_width; // Limitar o número de caracteres a mostrar para não ultrapassar a largura da tela.

				if(chars_to_show > 0){ // Se houver caracteres para mostrar nesta linha visual, extrair o segmento correspondente da linha original e renderizá-lo na tela. O segmento é extraído usando strncpy para copiar os caracteres da posição inicial calculada (start_pos) até o número de caracteres a mostrar (chars_to_show), garantindo que o segmento seja corretamente terminado com um caractere nulo ('\0') para formar uma string válida.
					char segment[content_width + 1];
					strncpy(segment, line_content + start_pos, chars_to_show);
					segment[chars_to_show] = '\0';
					mvwprintw(win, screen_line, line_num_width + 1, "%s", segment);
				}

				screen_line++; // Incrementar a linha visual atual (screen_line) para passar para a próxima linha visual, garantindo que as linhas visuais resultantes da quebra sejam renderizadas uma abaixo da outra na tela.
			}
		}

		mvwprintw(win, row - 1, 0, "Line %d/%d", display_start + 1, buffer->num_lines); // Mostrar a linha atual de exibição (display_start + 1 para mostrar a linha em formato 1-based) e o total de linhas no buffer, fornecendo ao usuário uma indicação de onde ele está no arquivo enquanto navega. Isso é especialmente útil em arquivos grandes, permitindo que o usuário saiba sua posição relativa dentro do conteúdo do arquivo.

		if(command_line_active){ // Se o modo de comando estiver ativo, renderizar a linha de comando na parte inferior da tela, mostrando um prompt ">_ " seguido do comando digitado pelo usuário (cmdbuf). A linha de comando é renderizada com um destaque visual usando um par de cores e negrito para indicar que o usuário está no modo de comando, e o cursor é posicionado após o texto do comando para permitir que o usuário continue digitando. Se o modo de comando não estiver ativo, garantir que o cursor esteja oculto para evitar distrações enquanto o usuário navega pelo conteúdo do arquivo.
			mvwhline(win, row - 1, 0, ' ', col);
	        wattron(win, COLOR_PAIR(1) | A_BOLD);
	        mvwprintw(win, row - 1, 0, ">_ ");
	        wattroff(win, COLOR_PAIR(1) | A_BOLD);
	        mvwprintw(win, row - 1, 2, "%s", cmdbuf);
	        wmove(win, row - 1, 2 + cmdlen);
	        curs_set(1);
	    }else{
	        curs_set(0); // Cursor invisível no modo normal para evitar distrações enquanto navega pelo conteúdo do arquivo.
	    }

		wrefresh(win);
		ch = wgetch(win);

		// Ativar/desativar modo comando
		if (ch == 0) {
		   command_line_active = 1;
		    cmdlen = 0;
		    cmdbuf[0] = '\0';
	        goto render; // Re-renderizar para atualizar a linha de comando ou ocultá-la
	    }

	    // Modo comando ativo
	    if (command_line_active) {
	        if (ch == KEY_BACKSPACE || ch == 127 || ch == 8) {
	            if (cmdlen > 0) cmdbuf[--cmdlen] = '\0';
	        } else if (ch == 10) { // Enter
	            if (strcmp(cmdbuf, "!q") == 0) {
	                return; // Sair
	            }
	            // Comando desconhecido: limpa
	            cmdlen = 0;
	            cmdbuf[0] = '\0';
	        } else if (ch >= 32 && ch <= 126) { // Caracteres imprimíveis
	            if (cmdlen < (int)sizeof(cmdbuf) - 1)
	                cmdbuf[cmdlen++] = (char)ch;
	            cmdbuf[cmdlen] = '\0';
	        }
	        goto render;
	    }

	    // Navegação normal
	    switch (ch) {
	        case 259:
				if (display_start > 0) display_start--;
				goto render;
	        case 258:
				 if (display_start < buffer->num_lines - (max_screen_line - 3)) display_start++;
				goto render;
	        case 339:
				 display_start -= (max_screen_line - 3); if (display_start < 0) display_start = 0;
				goto render;
	        case 338:
				 display_start += (max_screen_line - 3); if (display_start >= buffer->num_lines) display_start = buffer->num_lines - 1;
				 goto render;
	        default:
				 goto render;
	    }

}

void insert_character(EditorBuffer *buffer, char ch) { // Função para inserir um caractere no buffer do editor na posição atual do cursor, realocando a linha se necessário e movendo os caracteres existentes para a direita para abrir espaço para o novo caractere. A função também atualiza o contexto de syntax se estiver habilitado, garantindo que o destaque de syntax seja atualizado corretamente após a inserção do caractere. Após inserir o caractere, a função marca o buffer como modificado para indicar que houve uma alteração no conteúdo do arquivo.

    char *line = buffer->lines[buffer->current_line]; // Obter a linha atual do buffer onde o caractere será inserido, usando a posição do cursor (buffer->current_line) para acessar a linha correta no array de linhas do buffer. Essa linha é a string onde o novo caractere será inserido, e os caracteres existentes serão movidos para a direita para abrir espaço para o novo caractere.
    int line_len = strlen(line); // Calcular o comprimento da linha atual para determinar quantos caracteres existem antes da inserção do novo caractere. Isso é necessário para realocar a linha se necessário e para mover os caracteres existentes para a direita, garantindo que o novo caractere seja inserido na posição correta sem sobrescrever os caracteres existentes.

    // Realocar se necessário
    line = realloc(line, line_len + 2); // Realocar a linha atual para ter espaço suficiente para o novo caractere e o caractere nulo de terminação ('\0'). O tamanho da nova alocação é o comprimento atual da linha (line_len) mais 2 bytes: um para o novo caractere que será inserido e outro para o caractere nulo que termina a string. Isso garante que a linha possa acomodar o novo caractere sem causar estouro de buffer, mantendo a integridade da string.
    buffer->lines[buffer->current_line] = line; // Atualizar o ponteiro da linha no buffer para a nova alocação.

    // Mover caracteres para a direita para abrir espaço para o novo caractere.
    for (int i = line_len; i > buffer->current_col; i--) {
        line[i] = line[i-1];
    }

    // Atualizar syntax após mover os caracteres para a direita.
    if (buffer->syntax) {
    	syntax_update(buffer->syntax, buffer->lines, buffer->num_lines);
    }

    line[buffer->current_col] = ch; // Inserir o novo caractere na posição do cursor (buffer->current_col) na linha atual. O loop anterior já moveu os caracteres existentes para a direita, então agora podemos simplesmente atribuir o novo caractere à posição do cursor, garantindo que ele seja inserido corretamente sem sobrescrever os caracteres existentes.
    line[line_len + 1] = '\0';

    buffer->current_col++; // Mover o cursor para a direita após a inserção do caractere.
    buffer->modified = 1; // Marcar o buffer como modificado para indicar que houve uma alteração no conteúdo do arquivo.
}

void insert_new_line(EditorBuffer *buffer) { // Função para inserir uma nova linha no buffer do editor na posição atual do cursor, dividindo a linha atual em duas partes: a parte antes do cursor permanece na linha atual, e a parte após o cursor é movida para a nova linha. A função também realoca o array de linhas do buffer se necessário para acomodar a nova linha, e atualiza o contexto de syntax se estiver habilitado para garantir que o destaque de syntax seja atualizado corretamente após a inserção da nova linha. Após inserir a nova linha, a função marca o buffer como modificado para indicar que houve uma alteração no conteúdo do arquivo.

    // Verificar se precisa expandir a capacidade do array de linhas do buffer para acomodar a nova linha. Se o número atual de linhas (buffer->num_lines) for igual ou maior do que a capacidade atual do array (buffer->capacity), é necessário realocar o array para aumentar sua capacidade, geralmente dobrando o tamanho para evitar realocações frequentes no futuro. Isso garante que o buffer possa acomodar a nova linha sem causar estouro de memória, mantendo a integridade do buffer.
    if (buffer->num_lines >= buffer->capacity) {
        buffer->capacity *= 2;
        buffer->lines = realloc(buffer->lines, buffer->capacity * sizeof(char*));
    }

    char *current_line = buffer->lines[buffer->current_line]; // Obter a linha atual do buffer onde a nova linha será inserida, usando a posição do cursor (buffer->current_line) para acessar a linha correta no array de linhas do buffer. Essa linha é a string que será dividida em duas partes: a parte antes do cursor permanecerá na linha atual, e a parte após o cursor será movida para a nova linha.
    char *new_line = strdup(current_line + buffer->current_col); // Criar a nova linha duplicando a parte da linha atual que está após a posição do cursor (buffer->current_col). A função strdup é usada para alocar uma nova string e copiar o conteúdo da parte da linha atual que deve ser movida para a nova linha, garantindo que a nova linha seja uma string separada e independente da linha original.
    current_line[buffer->current_col] = '\0';

    // Move as linhas para baixo para abrir espaço para a nova linha. O loop começa da última linha do buffer (buffer->num_lines) e vai até a linha logo após a linha atual (buffer->current_line + 1), movendo cada linha para baixo para criar um espaço para a nova linha. Isso garante que a nova linha seja inserida na posição correta no array de linhas do buffer, mantendo a ordem das linhas e evitando sobrescrever as linhas existentes.
    for (int i = buffer->num_lines; i > buffer->current_line + 1; i--) {
        buffer->lines[i] = buffer->lines[i-1];
    }

    // Atualizar syntax após mover as linhas para baixo.
    if (buffer->syntax) {
    	syntax_update(buffer->syntax, buffer->lines, buffer->num_lines);
    }

    buffer->lines[buffer->current_line + 1] = new_line; // Inserir a nova linha no array de linhas do buffer na posição logo após a linha atual (buffer->current_line + 1), garantindo que a nova linha seja adicionada corretamente ao buffer.
    buffer->num_lines++; // Incrementar o número de linhas do buffer para refletir a adição da nova linha.
    buffer->current_line++; // Mover o cursor para a nova linha.
    buffer->current_col = 0;  // Cursor vai para o início da nova linha (como é vazia, seria o "final" dela)
    buffer->modified = 1; // Marcar o buffer como modificado para indicar que houve uma alteração no conteúdo do arquivo devido à inserção da nova linha.
}

void handle_backspace(EditorBuffer *buffer) { // Função para lidar com a ação de backspace no editor, permitindo que o usuário apague caracteres ou junte linhas quando o cursor estiver no início de uma linha. Se o cursor estiver em uma posição maior que 0 na linha atual, a função remove o caractere anterior e move os caracteres seguintes para a esquerda. Se o cursor estiver no início da linha (posição 0) e não for a primeira linha do buffer, a função junta a linha atual com a linha anterior, movendo o conteúdo da linha atual para o final da linha anterior e removendo a linha atual do buffer. A função também atualiza o contexto de syntax se estiver habilitado para garantir que o destaque de syntax seja atualizado corretamente após a remoção do caractere ou a junção das linhas. Após realizar a ação de backspace, a função marca o buffer como modificado para indicar que houve uma alteração no conteúdo do arquivo.

    if (buffer->current_col > 0) { // Se o cursor estiver em uma posição maior que 0 na linha atual, remover o caractere anterior e mover os caracteres seguintes para a esquerda para preencher o espaço deixado pelo caractere removido.
        char *line = buffer->lines[buffer->current_line];
        int line_len = strlen(line);

        for (int i = buffer->current_col - 1; i < line_len; i++) { // Mover os caracteres seguintes para a esquerda, começando da posição do cursor menos um (buffer->current_col - 1) até o final da linha (line_len), para sobrescrever o caractere que está sendo removido e manter a integridade da string. Isso garante que o caractere anterior seja efetivamente removido da linha, e os caracteres seguintes sejam ajustados para preencher o espaço deixado pelo caractere removido.
            line[i] = line[i + 1];
        }

        // Atualizar syntax
        if (buffer->syntax) {
        	syntax_update(buffer->syntax, buffer->lines, buffer->num_lines);
        }

        buffer->current_col--; // Mover o cursor para a esquerda após remover o caractere.
        buffer->modified = 1; // Marcar o buffer como modificado para indicar que houve uma alteração no conteúdo do arquivo devido à remoção do caractere.

    } else if (buffer->current_line > 0) { // Se o cursor estiver no início da linha (posição 0) e não for a primeira linha do buffer, juntar a linha atual com a linha anterior.
        // Juntar a linha atual com a linha anterior, movendo o conteúdo da linha atual para o final da linha anterior e removendo a linha atual do buffer. O conteúdo da linha atual é concatenado à linha anterior usando realloc para garantir que haja espaço suficiente para a nova string resultante da junção, e depois a linha atual é removida do buffer movendo as linhas seguintes para cima e decrementando o número de linhas do buffer.
        char *prev_line = buffer->lines[buffer->current_line - 1];
        char *curr_line = buffer->lines[buffer->current_line];

        int prev_len = strlen(prev_line); // Calcular o comprimento da linha anterior para determinar onde a linha atual deve ser concatenada.
        prev_line = realloc(prev_line, prev_len + strlen(curr_line) + 1); // Realocar a linha anterior para ter espaço suficiente para concatenar a linha atual.
        strcat(prev_line, curr_line); // Concatenar a linha atual ao final da linha anterior, garantindo que o conteúdo da linha atual seja adicionado corretamente à linha anterior sem sobrescrever o conteúdo existente.

        buffer->lines[buffer->current_line - 1] = prev_line; // Atualizar o ponteiro da linha anterior no buffer para a nova alocação resultante da concatenação.
        free(curr_line); // Liberar a memória da linha atual, já que seu conteúdo foi movido para a linha anterior e ela será removida do buffer.

        // Mover as linhas seguintes para cima para preencher o espaço deixado pela linha removida, começando da linha logo após a linha atual (buffer->current_line) até a última linha do buffer (buffer->num_lines - 1), movendo cada linha para cima para criar um espaço para a nova linha. Isso garante que o buffer mantenha a ordem correta das linhas e evite deixar um espaço vazio onde a linha atual estava anteriormente.
        for (int i = buffer->current_line; i < buffer->num_lines - 1; i++) {
            buffer->lines[i] = buffer->lines[i + 1];
        }

        // Atualizar syntax
        if (buffer->syntax) {
            syntax_update(buffer->syntax, buffer->lines, buffer->num_lines);
        }

        buffer->num_lines--; // Decrementar o número de linhas do buffer para refletir a remoção da linha atual.
        buffer->current_line--; // Mover o cursor para a linha anterior, que agora contém o conteúdo combinado das duas linhas.
        buffer->current_col = prev_len; // Posicionar o cursor no final da linha anterior, que agora é o final do conteúdo combinado das duas linhas, garantindo que o cursor fique na posição correta após a junção das linhas.
        buffer->modified = 1; // Marcar o buffer como modificado para indicar que houve uma alteração no conteúdo do arquivo devido à junção das linhas e remoção da linha atual.
    }

}

// Normaliza seleção: (sl,sc) = início, (el,ec) = fim em ordem do documento.
static void selection_bounds(const Selection *sel, int *sl, int *sc, int *el, int *ec) {

    if (sel->start_line < sel->end_line || (sel->start_line == sel->end_line && sel->start_col <= sel->end_col)) { // Se a seleção é crescente, (start_line, start_col) é o início e (end_line, end_col) é o fim.
        *sl = sel->start_line; *sc = sel->start_col; *el = sel->end_line; *ec = sel->end_col;
    } else { // Se a seleção é decrescente, (end_line, end_col) é o início e (start_line, start_col) é o fim.
        *sl = sel->end_line; *sc = sel->end_col; *el = sel->start_line; *ec = sel->start_col;
    }

}

// Copia o texto da seleção (ou linha atual) para dest, com \n entre linhas. Não excede max_len.
static void copy_selection_to_clipboard(EditorBuffer *buffer, const Selection *sel, char *dest, size_t max_len) {

    if (!buffer->lines || buffer->num_lines <= 0) { dest[0] = '\0'; return; } // Se o buffer estiver vazio, retorna uma string vazia.

    if (!sel->active) { // Se a seleção não estiver ativa, copia a linha atual.
        const char *line = buffer->lines[buffer->current_line]; // Obtém a linha atual.
        if (line) { // Se a linha existir, copia para o destino.
            size_t n = strlen(line);
            if (n >= max_len) n = max_len - 1; // Se a linha for maior que o limite, corta para max_len - 1.
            memcpy(dest, line, n + 1); // Copia a linha para o destino, incluindo o terminador nulo.
            dest[n] = '\0';
        } else dest[0] = '\0'; // Se a linha não existir, retorna uma string vazia.
        return;
    }

    int sl; // Linha inicial da seleção.
    int sc; // Coluna inicial da seleção.
    int el; // Linha final da seleção.
    int ec; // Coluna final da seleção.
    selection_bounds(sel, &sl, &sc, &el, &ec); // Obtém os limites da seleção.
    size_t i = 0;

    for (int L = sl; L <= el && i < max_len - 1; L++) { // Percorre as linhas da seleção.
        if (L >= buffer->num_lines) break; // Se a linha não existir, sai do loop.

        char *line = buffer->lines[L]; // Obtém a linha atual.

        if (!line) { if (L < el) dest[i++] = '\n'; continue; } // Se a linha não existir, adiciona uma quebra de linha e continua.

        int len = (int)strlen(line); // Obtém o comprimento da linha atual.
        int start_off = (L == sl) ? sc : 0; // Obtém o offset inicial da seleção na linha atual.
        int end_off   = (L == el) ? ec : len; // Obtém o offset final da seleção na linha atual.

        if (start_off > len) start_off = len; // Se o offset inicial for maior que o comprimento da linha, ajusta para o final da linha.
        if (end_off > len) end_off = len; // Se o offset final for maior que o comprimento da linha, ajusta para o final da linha.
        if (start_off >= end_off) { if (L < el && i < max_len - 1) dest[i++] = '\n'; continue; } // Se o offset inicial for maior ou igual ao offset final, pula para a próxima linha.

        for (int j = start_off; j < end_off && i < max_len - 1; j++) dest[i++] = line[j]; // Copia os caracteres da seleção para o buffer de destino.

        if (L < el && i < max_len - 1) dest[i++] = '\n'; // Se não é a última linha da seleção, adiciona uma quebra de linha.
    }
    dest[i] = '\0'; // Termina a string no buffer de destino.

}

// Remove a região selecionada do buffer e coloca o cursor no início da região. Limpa sel->active.
static void delete_selection(EditorBuffer *buffer, Selection *sel) {

    if (!sel->active || !buffer->lines || buffer->num_lines <= 0) return; // Se a seleção não estiver ativa ou o buffer estiver vazio, retornar imediatamente.

    int sl; // Flag para indicar a linha inicial da seleção.
    int sc; // Flag para indicar a coluna inicial da seleção.
    int el; // Flag para indicar a linha final da seleção.
    int ec; // Flag para indicar a coluna final da seleção.

    selection_bounds(sel, &sl, &sc, &el, &ec); // Obter os limites da seleção.

    if (sl == el) { // Se a seleção estiver em uma única linha.
        char *line = buffer->lines[sl]; // Obter a linha da seleção.
        int len = (int)strlen(line); // Obter o comprimento da linha.
        if (sc > len) sc = len; // Se a coluna inicial estiver além do final da linha, ajustar para o final.
        if (ec > len) ec = len; // Se a coluna final estiver além do final da linha, ajustar para o final.
        if (sc >= ec) { sel->active = 0; return; } // Se a seleção for inválida (coluna inicial maior ou igual à coluna final), desativar a seleção e retornar.

        size_t tail = (size_t)(len - ec); // Obter o comprimento da parte da linha após a seleção.
        memmove(line + sc, line + ec, tail + 1); // Mover a parte da linha após a seleção para a posição da coluna inicial.
        buffer->current_line = sl; // Atualizar a linha atual do cursor para a linha da seleção.
        buffer->current_col = sc; // Atualizar a coluna atual do cursor para a coluna inicial da seleção.
    } else { // Se a seleção abranger mais de uma linha.
        char *line_sl = buffer->lines[sl]; // Obter a linha inicial da seleção.
        char *line_el = buffer->lines[el]; // Obter a linha final da seleção.
        int len_sl = line_sl ? (int)strlen(line_sl) : 0; // Obter o comprimento da linha inicial da seleção.
        int len_el = line_el ? (int)strlen(line_el) : 0; // Obter o comprimento da linha final da seleção.

        if (sc > len_sl) sc = len_sl; // Se a coluna inicial da seleção estiver além do comprimento da linha inicial, ajustar para o final da linha.
        if (ec > len_el) ec = len_el; // Se a coluna final da seleção estiver além do comprimento da linha final, ajustar para o final da linha.

        size_t left_len = (size_t)sc; // Obter o comprimento do segmento da linha inicial antes da coluna inicial da seleção.
        size_t right_len = (size_t)(len_el - ec); // Obter o comprimento do segmento da linha final após a coluna final da seleção.
        char *new_line = malloc(left_len + right_len + 1); // Alocar memória para a nova linha.

        if (!new_line) return; // Se a alocação falhar, retornar sem fazer nada.
        if (left_len) memcpy(new_line, line_sl, left_len); // Copiar o segmento da linha inicial antes da coluna inicial da seleção.
        if (right_len) memcpy(new_line + left_len, line_el + ec, right_len); // Copiar o segmento da linha final após a coluna final da seleção.

        new_line[left_len + right_len] = '\0'; // Terminar a string com nulo.
        free(line_sl); // Liberar a linha inicial antiga.

        for (int i = sl + 1; i <= el; i++) free(buffer->lines[i]); // Liberar as linhas do buffer que estão dentro da seleção.

        buffer->lines[sl] = new_line; // Substituir a linha inicial com a nova linha.
        int n_remove = el - sl; // Número de linhas a serem removidas.

        for (int i = sl + 1; i < buffer->num_lines - n_remove; i++){ // Mover as linhas restantes para preencher o espaço vazio.
            buffer->lines[i] = buffer->lines[i + n_remove]; // Copiar a linha seguinte para a posição atual.
        }

        buffer->num_lines -= n_remove; // Atualizar o número de linhas no buffer.
        buffer->current_line = sl; // Atualizar a linha atual para a linha inicial da seleção.
        buffer->current_col = sc; // Atualizar a coluna atual para a coluna inicial da seleção.
    }
    buffer->modified = 1; // Atualizar o flag de modificação do buffer.
    sel->active = 0; // Limpar a seleção ativa.

}

void handle_copy(EditorBuffer *buffer, const Selection *sel) { // Função para copiar o conteúdo selecionado para o clipboard do sistema.

    copy_selection_to_clipboard(buffer, sel, clipboard, MAX_CLIPBOARD_SIZE); // Copia o conteúdo selecionado para o buffer local clipboard.
    set_system_clipboard(clipboard); // Envia o conteúdo do buffer clipboard para o clipboard do sistema.

}

void handle_cut(EditorBuffer *buffer, Selection *sel) { // Função para cortar o conteúdo selecionado do editor e colocá-lo no clipboard do sistema.

    if (sel->active) { // Se a seleção está ativa, copia o conteúdo selecionado para o clipboard e remove-o do buffer.
        copy_selection_to_clipboard(buffer, sel, clipboard, MAX_CLIPBOARD_SIZE);
        delete_selection(buffer, sel);
    } else { // Se a seleção não está ativa, copia a linha atual para o clipboard e remove-a do buffer.
        if (!buffer->lines || buffer->num_lines <= 0) return; // Se o buffer estiver vazio, não faz nada.

        copy_selection_to_clipboard(buffer, sel, clipboard, MAX_CLIPBOARD_SIZE); // Copia a linha atual para o clipboard.
        int cur = buffer->current_line; // Obtém a linha atual.
        free(buffer->lines[cur]); // Libera a memória da linha atual.

        for (int i = cur; i < buffer->num_lines - 1; i++){ // Desloca todas as linhas após a linha atual uma posição para cima.
            buffer->lines[i] = buffer->lines[i + 1];
        }

        buffer->num_lines--; // Decrementa o número de linhas no buffer.
        if (buffer->current_line >= buffer->num_lines) { // Se a linha atual ultrapassar o número de linhas, ajusta para a última linha disponível.
            buffer->current_line = buffer->num_lines > 0 ? buffer->num_lines - 1 : 0;
        }

        buffer->current_col = 0;
        buffer->modified = 1;
    }
    set_system_clipboard(clipboard); // Envia o conteúdo do clipboard interno para o clipboard do sistema.

}

void handle_paste(EditorBuffer *buffer) { // Função para lidar com a ação de colar no editor, permitindo que o usuário insira o conteúdo do clipboard interno na posição atual do cursor.

	if (!buffer) return; // Se o buffer for nulo, retorna imediatamente.
	if (!buffer->lines) return; // Se as linhas do buffer forem nulas, retorna imediatamente.

	if (buffer->num_lines == 0) { // Se o buffer estiver vazio, cria uma linha vazia antes de colar.
	    buffer->lines[0] = strdup("");
	    buffer->num_lines = 1;
	}

	if (buffer->num_lines == 0) return; // Se o buffer ainda estiver vazio após a criação, retorna imediatamente.
	if (buffer->current_line >= buffer->num_lines) return; // Se o cursor estiver além do número de linhas, retorna imediatamente.

	// Ler estado da linha atual ANTES de chamar handle_system_clipboard (popen/vfork pode alterar memória em alguns ambientes)
	char *current_line = buffer->lines[buffer->current_line]; // Ler a linha atual do buffer.
	if (!current_line) return; // Se a linha atual for nula, retorna imediatamente.
	int line_len = (int)strlen(current_line); // Obter o comprimento da linha atual.
	if (buffer->current_col > line_len) buffer->current_col = line_len; // Se o cursor estiver além do comprimento da linha, ajusta para o final da linha.

	handle_system_clipboard(clipboard, MAX_CLIPBOARD_SIZE); // Ler o conteúdo atualizado do clipboard do sistema (multilinha) para o buffer interno antes de colar.

    int clipboard_len = (int)strlen(clipboard); // Obter o comprimento do conteúdo do clipboard.
    if (clipboard_len == 0) return; // Se o clipboard estiver vazio, não há nada para colar, então retornar imediatamente.

    char *after_cursor = strdup(current_line + buffer->current_col); // Salvar a parte da linha atual que está após a posição do cursor (buffer->current_col) em uma nova string (after_cursor) para ser reinserida no final da última linha colada. A função strdup é usada para alocar uma nova string e copiar o conteúdo da parte da linha atual que deve ser preservada, garantindo que o texto à direita do cursor seja mantido e possa ser corretamente posicionado após o conteúdo colado. Se a alocação falhar, retornar imediatamente para evitar erros de memória.
    if (!after_cursor) after_cursor = strdup("");

    current_line[buffer->current_col] = '\0'; // Truncar a linha atual no cursor, separando a parte esquerda (que receberá o primeiro segmento do clipboard) da parte direita (que será reinserida após o último segmento colado).

    // Duplicar o conteúdo do clipboard para poder modificá-lo durante o processo de divisão por '\n' sem alterar o buffer original do clipboard, garantindo que operações futuras de colagem continuem funcionando corretamente.
    char *clip_copy = strdup(clipboard);
    if (!clip_copy) { // Se a alocação falhar, retornar imediatamente para evitar erros de memória.
        free(after_cursor);
        return;
    }
    char *token = clip_copy; // Ponteiro para o início do segmento atual sendo processado, avançando pelo conteúdo do clipboard à medida que cada segmento é inserido no buffer.

    int first = 1; // Flag para indicar se estamos processando o primeiro segmento do clipboard, que é tratado de forma diferente dos demais por ser inserido na linha atual em vez de criar uma nova linha.

    while (token != NULL) { // Iterar sobre cada segmento do clipboard separado por '\n', inserindo cada um no buffer na posição correta para reconstruir a estrutura multilinha do conteúdo original.

        char *next_newline = strchr(token, '\n'); // Encontrar o próximo '\n' no segmento atual para determinar onde termina o segmento e começa o próximo, permitindo dividir o clipboard em linhas individuais.
        if (next_newline) *next_newline = '\0'; // Substituir o '\n' por '\0' para terminar o segmento atual como string, facilitando a inserção do segmento no buffer sem incluir o caractere de nova linha.

        int token_len = strlen(token); // Calcular o comprimento do segmento atual para determinar quanto espaço é necessário ao inserir o segmento no buffer.

        if (first) { // Primeiro segmento: inserir diretamente na linha atual após a posição do cursor, concatenando ao conteúdo já existente à esquerda do cursor.
            int cur_len = strlen(buffer->lines[buffer->current_line]);

            char *tmp = realloc(buffer->lines[buffer->current_line], cur_len + token_len + 1);
            if (!tmp) {
                free(after_cursor);
                free(clip_copy);
                return;
            }

            buffer->lines[buffer->current_line] = tmp; // Realocar a linha atual para acomodar o segmento a ser inserido, garantindo espaço suficiente para o conteúdo existente mais o novo segmento mais o terminador nulo.

            strcat(buffer->lines[buffer->current_line], token); // Concatenar o primeiro segmento do clipboard ao final da parte esquerda da linha atual, inserindo o conteúdo na posição correta após o cursor.
            buffer->current_col += token_len; // Mover o cursor para o final do segmento inserido, posicionando-o corretamente após o conteúdo colado.
            first = 0; // Marcar que o primeiro segmento já foi processado, para que os próximos segmentos sejam tratados como novas linhas.
        } else { // Segmentos subsequentes: criar uma nova linha no buffer para cada segmento, inserindo o conteúdo do segmento na nova linha e movendo o cursor para ela.

            // Expandir o array de linhas do buffer se necessário para acomodar a nova linha.
            if (buffer->num_lines >= buffer->capacity) {
                buffer->capacity *= 2;
                char **tmp = realloc(buffer->lines, buffer->capacity * sizeof(char*));
                if (!tmp) { // Se a alocação falhar, retornar imediatamente para evitar erros de memória.
                    free(after_cursor);
                    free(clip_copy);
                    return;
                }
                buffer->lines = tmp;
            }

            // Mover as linhas existentes após a linha atual para baixo para abrir espaço para a nova linha, garantindo que a ordem das linhas seja preservada e que a nova linha seja inserida na posição correta no array de linhas do buffer.
            memmove(
                &buffer->lines[buffer->current_line + 2],
                &buffer->lines[buffer->current_line + 1],
                (buffer->num_lines - buffer->current_line - 1) * sizeof(char *)
            );

            char *new_line = strdup(token); // Criar a nova linha com o conteúdo do segmento atual do clipboard, duplicando a string do segmento para garantir que a nova linha tenha sua própria cópia independente do conteúdo.
            if (!new_line) {
                free(after_cursor);
                free(clip_copy);
                return;
            }

            buffer->lines[buffer->current_line + 1] = new_line;

            buffer->num_lines++; // Incrementar o número de linhas do buffer para refletir a adição da nova linha.
            buffer->current_line++; // Mover o cursor para a nova linha inserida para que o próximo segmento seja inserido na posição correta.
            buffer->current_col = token_len; // Posicionar o cursor no final do conteúdo inserido na nova linha.
        }

        token = next_newline ? next_newline + 1 : NULL; // Avançar para o próximo segmento do clipboard, posicionando o ponteiro token logo após o '\n' encontrado, ou definindo como NULL se não houver mais segmentos para processar.
    }

    // Reinserir o conteúdo que estava à direita do cursor na linha original ao final da última linha inserida, garantindo que o texto que estava após o cursor não seja perdido e seja corretamente posicionado após todo o conteúdo colado.
    int cur_len = strlen(buffer->lines[buffer->current_line]);
    int after_len = strlen(after_cursor);

    char *tmp = realloc(buffer->lines[buffer->current_line], cur_len + after_len + 1); // Realocar a última linha inserida para ter espaço suficiente para o conteúdo reinserido após o cursor.
    if (!tmp) { // Se a alocação falhar, retornar imediatamente para evitar erros de memória.
        free(after_cursor);
        free(clip_copy);
        return;
    }
    buffer->lines[buffer->current_line] = tmp; // Realocar a última linha inserida para ter espaço suficiente para o conteúdo reinserido após o cursor.

    strcat(buffer->lines[buffer->current_line], after_cursor); // Concatenar o conteúdo que estava após o cursor ao final da última linha inserida, restaurando o texto que existia à direita do cursor antes da operação de colagem.

    // Atualizar o syntax highlighting após a operação de colagem para garantir que o destaque seja aplicado corretamente ao novo conteúdo inserido no buffer.
    if (buffer->syntax) {
        syntax_update(buffer->syntax, buffer->lines, buffer->num_lines);
    }

    free(after_cursor); // Liberar a memória alocada para a cópia do conteúdo após o cursor, já que não é mais necessária após a reinserção.
    free(clip_copy); // Liberar a memória alocada para a cópia do clipboard, já que não é mais necessária após o processamento de todos os segmentos.
    buffer->modified = 1; // Marcar o buffer como modificado para indicar que houve uma alteração no conteúdo do arquivo devido à operação de colagem.

}

static void set_system_clipboard(const char *text) { // Função para enviar o conteúdo copiado/recortado para o clipboard do sistema, permitindo que o usuário cole o conteúdo em outros aplicativos fora do editor. A função usa popen para executar um comando de shell que tenta escrever no clipboard usando xclip ou xsel, dependendo de qual estiver disponível no sistema.

    FILE *pipe = popen(
        "xclip -selection clipboard 2>/dev/null || xsel --clipboard --input 2>/dev/null",
        "w" // Modo escrita: envia dados PARA o clipboard, ao contrário de handle_system_clipboard que usa "r" para ler.
    );
    if (!pipe) return;
    fputs(text, pipe); // Escrever o conteúdo no clipboard do sistema através do pipe aberto para o comando de shell.
    pclose(pipe);

}

static void handle_system_clipboard(char *dest, int max_len) { // Função para enviar o conteúdo copiado/recortado para o clipboard do sistema, permitindo que o usuário cole o conteúdo em outros aplicativos fora do editor. A função usa popen para executar um comando de shell que tenta escrever no clipboard usando xclip ou xsel, dependendo de qual estiver disponível no sistema.

	FILE *pipe = popen( // Abrir um pipe para ler o conteúdo do clipboard usando xclip ou xsel.
        "xclip -selection clipboard -o 2>/dev/null || xsel --clipboard --output 2>/dev/null",
        "r"
    );
    if (!pipe) return; // Se falhar, retornar imediatamente.

    int i = 0;
    int ch;
    int prev_was_cr = 0;

    while (i < max_len - 1 && (ch = fgetc(pipe)) != EOF) { // Ler caracteres do pipe até o limite máximo ou até o fim do arquivo.
        if (ch == '\r') { // Converter \r para \n.
            dest[i++] = '\n';
            prev_was_cr = 1;
        } else if (ch == '\n') { // Converter \n para \n (mantendo o \n original).
            if (!prev_was_cr) dest[i++] = '\n'; /* \r\n já gerou \n no \r */
            prev_was_cr = 0;
        } else { // Copiar caracteres normais para o destino.
            dest[i++] = (char)ch;
            prev_was_cr = 0;
        }
    }
    dest[i] = '\0';
    pclose(pipe);

}

EditorHistory* history_create(){

	EditorHistory *h = calloc(1, sizeof(EditorHistory)); // Alocar memória para a estrutura de histórico do editor, inicializando todos os campos com zero usando calloc. Isso garante que as contagens de undo e redo.
    h->undo_top = -1; // Inicializar o topo da pilha de undo como -1 para indicar que a pilha está vazia.
    h->undo_count = 0; // Inicializar a contagem de undo como 0 para indicar que não há ações para desfazer.
    h->redo_top = -1; // Inicializar o topo da pilha de redo como -1 para indicar que a pilha está vazia.
    h->redo_count = 0; // Inicializar a contagem de redo como 0 para indicar que não há ações para refazer.
    return h;

}

void history_destroy(EditorHistory *h) { // Função para destruir a estrutura de histórico do editor, liberando toda a memória alocada para os snapshots de undo e redo. A função itera sobre cada snapshot na pilha de undo e redo, liberando a memória alocada para as linhas de cada snapshot e, em seguida, libera a memória da estrutura de histórico em si. Isso garante que não haja vazamentos de memória ao destruir o histórico do editor.

    for (int i = 0; i < MAX_UNDO; i++) { // Iterar sobre cada snapshot na pilha de undo (h->undo_stack) e redo (h->redo_stack) para liberar a memória alocada para as linhas de cada snapshot. O loop percorre cada snapshot em ambas as pilhas, verificando se há linhas alocadas e, em caso afirmativo, liberando a memória de cada linha individualmente antes de liberar a memória do array de linhas. Isso garante que toda a memória alocada para os snapshots de undo e redo seja corretamente liberada, evitando vazamentos de memória ao destruir o histórico do editor.
        if (h->undo_stack[i].lines) {
            for (int j = 0; j < h->undo_stack[i].num_lines; j++)
                free(h->undo_stack[i].lines[j]);
            free(h->undo_stack[i].lines);
        }
        // Limpar a pilha de redo, pois qualquer nova ação invalida a possibilidade de refazer ações anteriores. O loop itera sobre cada snapshot na pilha de redo (h->redo_stack) e libera a memória alocada para as linhas de cada snapshot, garantindo que não haja vazamentos de memória ao limpar a pilha de redo. Após liberar a memória das linhas, o ponteiro para as linhas é definido como NULL para evitar acessos inválidos no futuro.
        if (h->redo_stack[i].lines) {
            for (int j = 0; j < h->redo_stack[i].num_lines; j++)
                free(h->redo_stack[i].lines[j]);
            free(h->redo_stack[i].lines);
        }
    }
    free(h); // Liberar a memória da estrutura de histórico do editor em si após liberar toda a memória alocada para os snapshots de undo e redo, garantindo que não haja vazamentos de memória ao destruir o histórico do editor.

}

void history_push(EditorHistory *h, EditorBuffer *buffer) { // Função para salvar o estado atual do buffer do editor na pilha de undo, permitindo que o usuário possa desfazer as ações realizadas posteriormente. A função também limpa a pilha de redo, pois qualquer nova ação invalida a possibilidade de refazer ações anteriores. O estado salvo inclui o número de linhas, a posição do cursor (linha e coluna) e o conteúdo de cada linha do buffer, garantindo que o estado completo do editor seja armazenado para que possa ser restaurado corretamente durante uma operação de undo.

    for (int i = 0; i < MAX_UNDO; i++) { // Limpar a pilha de redo, pois qualquer nova ação invalida a possibilidade de refazer ações anteriores. O loop itera sobre cada snapshot na pilha de redo (h->redo_stack) e libera a memória alocada para as linhas de cada snapshot, garantindo que não haja vazamentos de memória ao limpar a pilha de redo. Após liberar a memória das linhas, o ponteiro para as linhas é definido como NULL para evitar acessos inválidos no futuro.
        if (h->redo_stack[i].lines) {
            for (int j = 0; j < h->redo_stack[i].num_lines; j++)
                free(h->redo_stack[i].lines[j]);
            free(h->redo_stack[i].lines);
            h->redo_stack[i].lines = NULL;
        }
    }

    h->redo_top = -1; // Reiniciar o topo da pilha de redo para -1 para indicar que a pilha está vazia após limpar os snapshots de redo.
    h->redo_count = 0; // Reiniciar a contagem de redo para 0 para indicar que não há ações para refazer após limpar a pilha de redo.
    h->undo_top = (h->undo_top + 1) % MAX_UNDO; // Atualizar o topo da pilha de undo para apontar para o próximo snapshot de undo.
    if (h->undo_count < MAX_UNDO) h->undo_count++; // Incrementar a contagem de undo para refletir que uma nova ação foi realizada, indicando que há uma ação a mais para desfazer.

    EditorSnapshot *snap = &h->undo_stack[h->undo_top]; // Obter o snapshot de undo que será salvo, usando o topo da pilha de undo (h->undo_top) para acessar o snapshot correspondente.
    if (snap->lines) {
        for (int i = 0; i < snap->num_lines; i++) free(snap->lines[i]);
        free(snap->lines);
    }

    snap->num_lines    = buffer->num_lines; // Salvar o número de linhas do buffer no snapshot de undo para que ele possa ser restaurado posteriormente se o usuário decidir desfazer a ação.
    snap->current_line = buffer->current_line; // Salvar a posição da linha do cursor no snapshot de undo para que ele possa ser restaurado posteriormente se o usuário decidir desfazer a ação.
    snap->current_col  = buffer->current_col; // Salvar a posição da coluna do cursor no snapshot de undo para que ele possa ser restaurado posteriormente se o usuário decidir desfazer a ação.
    snap->lines = malloc(buffer->num_lines * sizeof(char*)); // Alocar memória para as linhas do snapshot de undo com base no número de linhas do buffer, garantindo que haja espaço suficiente para armazenar o estado atual do buffer para que possa ser restaurado posteriormente se o usuário decidir desfazer a ação.

    for (int i = 0; i < buffer->num_lines; i++){ // Salvar o conteúdo de cada linha do buffer no snapshot de undo, iterando sobre cada linha do buffer (buffer->lines) e duplicando a string para a linha correspondente no snapshot de undo (snap->lines).
        snap->lines[i] = strdup(buffer->lines[i]);
    }

}

void history_undo(EditorHistory *h, EditorBuffer *buffer) { // Função para desfazer a última ação realizada no editor, permitindo que o usuário volte ao estado anterior do buffer.

    if (h->undo_count <= 0) return; // Verificar se há ações para desfazer, verificando se a contagem de undo (h->undo_count) é menor ou igual a 0. Se não houver ações para desfazer, a função retorna imediatamente, evitando qualquer alteração no buffer.

    // Salvar estado atual no redo antes de restaurar o undo
    h->redo_top = (h->redo_top + 1) % MAX_UNDO;
    if (h->redo_count < MAX_UNDO) h->redo_count++;

    EditorSnapshot *redo_snap = &h->redo_stack[h->redo_top]; // Obter o snapshot do redo que será salvo, usando o topo da pilha de redo (h->redo_top) para acessar o snapshot correspondente. Este snapshot armazenará o estado atual do buffer antes de restaurar o estado anterior do undo, permitindo que o usuário refaça a ação posteriormente se desejar.
    if (redo_snap->lines) {
        for (int i = 0; i < redo_snap->num_lines; i++) free(redo_snap->lines[i]); // Liberar a memória das linhas do snapshot de redo atual para evitar vazamentos de memória, iterando sobre cada linha do snapshot de redo (redo_snap->lines) e liberando a memória alocada para cada linha.
        free(redo_snap->lines);
    }

    redo_snap->num_lines    = buffer->num_lines; // Salvar o número de linhas do buffer no snapshot de redo para que ele possa ser restaurado posteriormente se o usuário decidir refazer a ação.
    redo_snap->current_line = buffer->current_line; // Salvar a posição da linha do cursor no snapshot de redo para que ele possa ser restaurado posteriormente se o usuário decidir refazer a ação.
    redo_snap->current_col  = buffer->current_col; // Salvar a posição da coluna do cursor no snapshot de redo para que ele possa ser restaurado posteriormente se o usuário decidir refazer a ação.
    redo_snap->lines = malloc(buffer->num_lines * sizeof(char*)); // Alocar memória para as linhas do snapshot de redo com base no número de linhas do buffer, garantindo que haja espaço suficiente para armazenar o estado atual do buffer antes de restaurar o estado anterior do undo.

    for (int i = 0; i < buffer->num_lines; i++){ // Salvar o conteúdo de cada linha do buffer no snapshot de redo, iterando sobre cada linha do buffer (buffer->lines) e duplicando a string para a linha correspondente no snapshot de redo (redo_snap->lines), garantindo que o estado atual do buffer seja armazenado corretamente no snapshot de redo para que possa ser restaurado posteriormente se o usuário decidir refazer a ação.
        redo_snap->lines[i] = strdup(buffer->lines[i]);
    }

    // Restaurar snapshot do undo
    EditorSnapshot *snap = &h->undo_stack[h->undo_top];
    for (int i = 0; i < buffer->num_lines; i++) free(buffer->lines[i]);

    buffer->num_lines    = snap->num_lines; // Restaurar o número de linhas do buffer para o valor armazenado no snapshot do undo, garantindo que o buffer tenha a quantidade correta de linhas após a restauração.
    buffer->current_line = snap->current_line; // Restaurar a posição da linha do cursor para o valor armazenado no snapshot do undo, garantindo que o cursor fique na posição correta após a restauração.
    buffer->current_col  = snap->current_col; // Restaurar a posição da coluna do cursor para o valor armazenado no snapshot do undo, garantindo que o cursor fique na posição correta após a restauração.

    for (int i = 0; i < snap->num_lines; i++){ // Restaurar o conteúdo das linhas do buffer para os valores armazenados no snapshot do undo, iterando sobre cada linha do snapshot de undo e duplicando a string para a linha correspondente no buffer, garantindo que o estado anterior do buffer seja restaurado corretamente após a ação de desfazer (undo).
        buffer->lines[i] = strdup(snap->lines[i]);
    }

    h->undo_top = (h->undo_top - 1 + MAX_UNDO) % MAX_UNDO; // Atualizar o topo da pilha de undo para apontar para o próximo snapshot de undo.
    h->undo_count--; // Decrementar a contagem de undo para refletir que uma ação de desfazer (undo) foi realizada, indicando que há uma ação a menos para desfazer.
    buffer->modified = 1; // Marcar o buffer como modificado para indicar que houve uma alteração no conteúdo do arquivo devido à restauração do estado anterior, garantindo que o usuário saiba que o buffer foi alterado e precisa ser salvo se desejar manter as alterações.

}

void history_redo(EditorHistory *h, EditorBuffer *buffer) { // Função para refazer a última ação desfeita no editor, permitindo que o usuário recupere o estado anterior do buffer após ter usado a função de desfazer (undo).

    if (h->redo_count <= 0) return; // Verificar se há ações para refazer, verificando se a contagem de redo (h->redo_count) é menor ou igual a 0. Se não houver ações para refazer, a função retorna imediatamente, evitando qualquer alteração no buffer.

    EditorSnapshot *snap = &h->redo_stack[h->redo_top]; // Obter o snapshot do redo que será restaurado, usando o topo da pilha de redo (h->redo_top) para acessar o snapshot correspondente. Este snapshot contém o estado do buffer antes da última ação desfeita, e será usado para restaurar o buffer ao estado anterior.
    for (int i = 0; i < buffer->num_lines; i++) free(buffer->lines[i]); // Liberar a memória das linhas atuais do buffer para evitar vazamentos de memória, iterando sobre cada linha do buffer (buffer->lines) e liberando a memória alocada para cada linha.

    buffer->num_lines    = snap->num_lines; // Restaurar o número de linhas do buffer para o valor armazenado no snapshot do redo, garantindo que o buffer tenha a quantidade correta de linhas após a restauração.
    buffer->current_line = snap->current_line; // Restaurar a posição da linha do cursor para o valor armazenado no snapshot do redo, garantindo que o cursor fique na posição correta após a restauração.
    buffer->current_col  = snap->current_col; // Restaurar a posição da coluna do cursor para o valor armazenado no snapshot do redo, garantindo que o cursor fique na posição correta após a restauração.
    for (int i = 0; i < snap->num_lines; i++){ // Restaurar o conteúdo das linhas do buffer para os valores armazenados no snapshot do redo, iterando sobre cada linha do snapshot e duplicando a string para a linha correspondente no buffer.
    	buffer->lines[i] = strdup(snap->lines[i]);
    }

    h->redo_top = (h->redo_top - 1 + MAX_UNDO) % MAX_UNDO; // Atualizar o topo da pilha de redo para apontar para o próximo snapshot de redo, decrementando o topo (h->redo_top) e usando a operação de módulo para garantir que ele fique dentro dos limites da pilha circular de redo.
    h->redo_count--; // Decrementar a contagem de redo para refletir que uma ação de redo foi realizada, indicando que há uma ação a menos para refazer.
    buffer->modified = 1; // Marcar o buffer como modificado para indicar que houve uma alteração no conteúdo do arquivo devido à restauração do estado anterior, garantindo que o usuário saiba que o buffer foi alterado e precisa ser salvo se desejar manter as alterações.

}