#ifndef EDITOR_H
#define EDITOR_H

#include <ncurses.h>

#define INITIAL_CAPACITY 100 // Capacidade inicial para o número de linhas
#define MAX_LINE_LENGTH 4096 // Comprimento máximo de uma linha, usado para alocar buffers temporários para operações de cópia e colagem

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

#endif