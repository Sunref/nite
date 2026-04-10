/*
 *
 * Arquivo auxiliar para lidar com as ações do usuário no editor, como backspace, copiar/colar, seleção e exclusão de texto.
 *
 */

#include "../include/editor.h"
#include "../include/handle.h"
#include "../include/syntax.h"
#include <ncurses.h>
#include <string.h>

static int current_line_len(EditorBuffer *buffer);
static void handle_system_clipboard(char *dest, int max_len);
static void set_system_clipboard(const char *text);
static void selection_bounds(const Selection *sel, int *sl, int *sc, int *el, int *ec);
static void copy_selection_to_clipboard(EditorBuffer *buffer, const Selection *sel, char *dest, size_t max_len);
static void delete_selection(EditorBuffer *buffer, Selection *sel);

char clipboard[MAX_CLIPBOARD_SIZE] = ""; // Buffer global para armazenar o conteúdo copiado/colado (suporta multilinha para blocos de código)

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