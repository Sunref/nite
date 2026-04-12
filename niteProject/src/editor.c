/*
 *
 * Arquivo principal; funcionalidades relacionadas ao editor são tratadas aqui.
 *
 */

#include "../include/file_validation.h"
#include "../include/editor.h"
#include "../include/handle.h"
#include "../include/dialog.h"
#include "../include/syntax.h"
#include "../include/config.h"
#include "../include/command.h"
#include <ncurses.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

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
    int scroll_top = 0; // Primeira linha lógica visível no topo da janela; persiste entre frames para scroll suave

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
                if (strcmp(cmdbuf, CMD_EXIT) == 0) {
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
                else if (strcmp(cmdbuf, CMD_SAVE) == 0) {
                    // Comando !s (salvar)

                    // Verificar se o arquivo já tem um caminho (foi aberto ou já foi salvo antes)
                    if (buffer->filename != NULL) {
                        // Arquivo já existe = salvar diretamente
                        if (save_file(buffer, buffer->filename)) {
                            // Caso sucesso: mostrar mensagem na linha de status
                            mvwhline(win, row - 1, 0, ' ', col);
                            mvwprintw(win, row - 1, 0, "File saved: %s", buffer->filename);
                            wrefresh(win);
                            napms(600); // Mostrar por 0.6s

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
                                napms(600); // Mostrar por 0.6s
                            } else {
                                // Mostrar mensagem de erro
                                mvwhline(win, row - 1, 0, ' ', col);
                                mvwprintw(win, row - 1, 0, "Error saving file!");
                                wrefresh(win);
                                napms(600);
                            }
                        } else {
                            // Salvamento cancelado
                            mvwhline(win, row - 1, 0, ' ', col);
                            mvwprintw(win, row - 1, 0, "Save cancelled.");
                            wrefresh(win);
                            napms(600);
                        }

                        free(final_filename);
                        free(final_path);

                        // Desativar modo comando e limpar buffer
                        command_line_active = 0;
                        cmdlen = 0;
                        cmdbuf[0] = '\0';
                    }
                }
                else if (strcmp(cmdbuf, CMD_HELP) == 0) {
                    // Abrir help.txt em janela sobreposta (read_only); ao sair, volta ao editor
                    command_line_active = 0;
                    cmdlen = 0;
                    cmdbuf[0] = '\0';
                    EditorBuffer *help_buf = load_help_file();
                    if (help_buf) {
                        read_only(help_buf, win, row, col);
                        free_editor_buffer(help_buf);
                    } else {
                        mvwhline(win, row - 1, 0, ' ', col);
                        mvwprintw(win, row - 1, 0, "Could not open help.txt");
                        wrefresh(win);
                        napms(800);
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
        mvwprintw(win, 1, 0, "CTRL+Space: Command Mode | %s: Save | %s: Quit", CMD_SAVE, CMD_EXIT); // Mostrar instruções de atalho para o usuário, indicando como entrar no modo de comando (ALT+Espaço) e os comandos para salvar e sair

        // Scroll inteligente: manter cursor visível com margem de contexto (scrolloff)
        int scrolloff = 5; // Linhas de contexto acima/abaixo do cursor
        int visible_lines = (row - 5) - 3; // Altura útil da área de conteúdo em linhas de tela
        if (visible_lines < 1) visible_lines = 1;

        // Scroll para cima: cursor acima da margem superior
        if (buffer->current_line < scroll_top + scrolloff) {
            scroll_top = buffer->current_line - scrolloff;
            if (scroll_top < 0) scroll_top = 0;
        }
        // Scroll para baixo: cursor abaixo da margem inferior
        if (buffer->current_line > scroll_top + visible_lines - 1 - scrolloff) {
            scroll_top = buffer->current_line - (visible_lines - 1 - scrolloff);
            if (scroll_top < 0) scroll_top = 0;
            if (scroll_top >= buffer->num_lines) scroll_top = buffer->num_lines - 1;
        }

        // Mostrar linhas do arquivo numeradas e com quebra de linha

        // Calcular largura necessária para números de linha (max 4 dígitos)
        int line_num_width = 4;

        // Calcular largura disponível para o conteúdo
        int content_width = col - line_num_width - 2;
        if (content_width <= 0) content_width = 1;

        int screen_line = 3;  // Linha inicial
        int max_screen_line = row - 5;  // Linha máxima disponível

        for (int i = scroll_top; i < buffer->num_lines && screen_line < max_screen_line; i++) { // Iterar sobre as linhas do buffer a partir da linha de exibição calculada, garantindo que não ultrapasse o número máximo de linhas que podem ser exibidas na tela
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

        for (int i = scroll_top; i < buffer->current_line && cursor_visual_line < max_screen_line; i++) { // Iterar sobre as linhas do buffer desde scroll_top até a linha do cursor, calculando quantas linhas visuais cada linha ocupa para posicionar o cursor corretamente na tela considerando quebras de linha.
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
                sel.active = 0;
                insert_new_line(buffer);
                break;

            case 8: case 127: case 263: // Backspace
                sel.active = 0;
                handle_backspace(buffer);
                break;

            case KEY_DC: // Delete = Apagar caractere sob o cursor (ou seleção ativa)
                history_push(history, buffer);
                handle_delete(buffer, &sel);
                break;

            case 259: // KEY_UP
                sel.active = 0;
                if (buffer->current_line > 0) {
                    buffer->current_line--;
                    buffer->current_col = current_line_len(buffer);
                }
                break;

            case 258: // KEY_DOWN
                sel.active = 0;
                if (buffer->current_line < buffer->num_lines - 1) {
                    buffer->current_line++;
                    buffer->current_col = current_line_len(buffer);
                }
                break;

            case 260: // KEY_LEFT
                sel.active = 0;
                if (buffer->current_col > 0) {
                    buffer->current_col--;
                }
                break;

            case 261: // KEY_RIGHT
                sel.active = 0;
            	if (buffer->current_col < current_line_len(buffer)) {
                    buffer->current_col++;
                }
                break;

            case KEY_HOME: // HOME = Ir para o início da linha
            case 560:      // Ctrl+← = Ir para o início da linha
                sel.active = 0;
                buffer->current_col = 0;
                break;

            case KEY_END:  // END = Ir para o fim da linha
            case 575:      // Ctrl+→ = Ir para o fim da linha
                sel.active = 0;
                buffer->current_col = current_line_len(buffer);
                break;

            case 581: { // Ctrl+↑ = Ir para o início da palavra
                sel.active = 0;
                char *line = buffer->lines[buffer->current_line];
                int col = buffer->current_col;
                // Pula caracteres não-palavra para trás
                while (col > 0 && !isalnum((unsigned char)line[col - 1]) && line[col - 1] != '_')
                    col--;
                // Volta até o início da palavra
                while (col > 0 && (isalnum((unsigned char)line[col - 1]) || line[col - 1] == '_'))
                    col--;
                buffer->current_col = col;
                break;
            }

            case 540: { // Ctrl+↓ = Ir para o fim da palavra
                sel.active = 0;
                char *line = buffer->lines[buffer->current_line];
                int len = current_line_len(buffer);
                int col = buffer->current_col;
                // Pula caracteres não-palavra para frente
                while (col < len && !isalnum((unsigned char)line[col]) && line[col] != '_')
                    col++;
                // Avança até o fim da palavra
                while (col < len && (isalnum((unsigned char)line[col]) || line[col] == '_'))
                    col++;
                buffer->current_col = col;
                break;
            }

            case 9: // TAB = Inserir 4 espaços
                sel.active = 0;
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
                sel.active = 0;
                handle_paste(buffer);
                break;

            case 24: // Ctrl+X = Recortar
                history_push(history, buffer);
                handle_cut(buffer, &sel);
                break;

			case 26: // Ctrl+Z = Undo
			    sel.active = 0;
			    history_undo(history, buffer);
			    if (buffer->syntax)
			        syntax_update(buffer->syntax, buffer->lines, buffer->num_lines);
			    break;

			case 25: // Ctrl+Y = Redo
			    sel.active = 0;
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
			        sel.active = 0;
			        history_push(history, buffer);
			        insert_character(buffer, ch);
			    } else {
			        mvwprintw(win, row - 1, 0, "Unknown key code: %d", ch);
			        wrefresh(win);
			        napms(600);
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
		mvwprintw(win, 1, 0, "Arrows: Scroll | CTRL+Space: Command line | %s: quit", CMD_EXIT);

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

		// Ativar/desativar modo comando com CTRL+Space (toggle)
		if (ch == 0) {
		    command_line_active = !command_line_active;
		    cmdlen = 0;
		    cmdbuf[0] = '\0';
	        goto render;
	    }

	    // Modo comando ativo
	    if (command_line_active) {
	        if (ch == 27) { // ESC = fechar linha de comando sem executar
	            command_line_active = 0;
	            cmdlen = 0;
	            cmdbuf[0] = '\0';
	            goto render;
	        }
	        if (ch == KEY_BACKSPACE || ch == 127 || ch == 8) {
	            if (cmdlen > 0) cmdbuf[--cmdlen] = '\0';
	        } else if (ch == 10) { // Enter
	            if (strcmp(cmdbuf, CMD_EXIT) == 0) {
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