#ifndef EDITOR_H
#define EDITOR_H

#include <ncurses.h>

#define INITIAL_CAPACITY 100 // Capacidade inicial para o número de linhas
#define MAX_LINE_LENGTH 4096 // Comprimento máximo de uma linha, usado para alocar buffers temporários para operações de cópia e colagem
#define MAX_CLIPBOARD_SIZE (256 * 1024) // Tamanho máximo do clipboard para colar blocos multilinha (ex.: código)
#define MAX_UNDO 50 // Número máximo de ações de "undo" armazenadas

typedef struct {
    char **lines;           		// Array de strings (linhas do arquivo)
    int num_lines;          		// Número de linhas atual
    int capacity;           		// Capacidade máxima de linhas alocadas
    char *filename;         		// Nome do arquivo
    int modified;           		// Flag se o arquivo foi modificado
    int current_line;       		// Linha atual do cursor
    int current_col;       			// Coluna atual do cursor
    struct SyntaxContext *syntax;  	// Contexto de syntax para o buffer
} EditorBuffer;

typedef struct {
    char **lines; // Array de strings representando as linhas do buffer no momento do snapshot, permitindo restaurar o estado do buffer durante operações de undo/redo.
    int num_lines; // Número de linhas no snapshot, usado para controlar a restauração do estado do buffer durante undo/redo.
    int current_line; // Linha atual do cursor no momento do snapshot, permitindo restaurar a posição do cursor durante operações de undo/redo.
    int current_col; // Coluna atual do cursor no momento do snapshot, permitindo restaurar a posição do cursor durante operações de undo/redo.
} EditorSnapshot;

typedef struct {
    EditorSnapshot undo_stack[MAX_UNDO]; // Pilha de snapshots para operações de undo, permitindo armazenar o estado do buffer antes de cada ação para que possa ser restaurado posteriormente.
    int undo_top; // Índice do topo da pilha de undo, usado para controlar onde o próximo snapshot será armazenado e para determinar se há ações de undo disponíveis.
    int undo_count; // Contador do número de ações de undo atualmente armazenadas na pilha, usado para limitar o número de snapshots e controlar a disponibilidade de undo.
    EditorSnapshot redo_stack[MAX_UNDO]; // Pilha de snapshots para operações de redo, permitindo armazenar o estado do buffer antes de cada ação de undo para que possa ser restaurado posteriormente se o usuário decidir refazer a ação.
    int redo_top; // Índice do topo da pilha de redo, usado para controlar onde o próximo snapshot de redo será armazenado e para determinar se há ações de redo disponíveis.
    int redo_count; // Contador do número de ações de redo atualmente armazenadas na pilha, usado para limitar o número de snapshots e controlar a disponibilidade de redo.
} EditorHistory;

typedef struct {
    int active; // Flag para indicar se a seleção está ativa, permitindo ao editor saber quando o usuário iniciou uma seleção de texto para operações como cópia, corte ou colagem.
    int start_line; // Linha inicial da seleção, usada para determinar o início do bloco de texto selecionado pelo usuário para operações de edição.
    int start_col; // Coluna inicial da seleção, usada para determinar o início do bloco de texto selecionado pelo usuário para operações de edição.
    int end_line; // Linha final da seleção, usada para determinar o fim do bloco de texto selecionado pelo usuário para operações de edição.
    int end_col; // Coluna final da seleção, usada para determinar o fim do bloco de texto selecionado pelo usuário para operações de edição.
} Selection;

// Funções principais do editor
EditorBuffer* create_new_file();
int save_file(EditorBuffer *buffer, const char *filename);
void free_editor_buffer(EditorBuffer *buffer);
void enter_editor_mode(EditorBuffer *buffer, WINDOW *win, int row, int col);
void read_only(EditorBuffer *buffer,  WINDOW *win, int row, int col);

// Funções auxiliares de edição
void insert_character(EditorBuffer *buffer, char ch);
void insert_new_line(EditorBuffer *buffer);
void handle_backspace(EditorBuffer *buffer);
void handle_copy(EditorBuffer *buffer, const Selection *sel);
void handle_cut(EditorBuffer *buffer, Selection *sel);
void handle_paste(EditorBuffer *buffer);

// Funções para gerenciamento de histórico de "undo/redo"
EditorHistory* history_create();
void history_destroy(EditorHistory *h);
void history_push(EditorHistory *h, EditorBuffer *buffer);
void history_undo(EditorHistory *h, EditorBuffer *buffer);
void history_redo(EditorHistory *h, EditorBuffer *buffer);

#endif