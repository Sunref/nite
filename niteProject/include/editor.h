#ifndef EDITOR_H
#define EDITOR_H

#include <ncurses.h>

typedef struct {
    char **lines;           // Array de strings (linhas do arquivo)
    int num_lines;          // Número de linhas atual
    int capacity;           // Capacidade máxima de linhas alocadas
    char *filename;         // Nome do arquivo
    int modified;           // Flag se o arquivo foi modificado
    int current_line;       // Linha atual do cursor
    int current_col;        // Coluna atual do cursor
} EditorBuffer;

// Funções principais do editor
EditorBuffer* create_new_file();
void free_editor_buffer(EditorBuffer *buffer);
int save_file(EditorBuffer *buffer, const char *filename);
void enter_editor_mode(EditorBuffer *buffer, WINDOW *win, int row, int col);

// Funções auxiliares de edição
void insert_character(EditorBuffer *buffer, char ch);
void insert_new_line(EditorBuffer *buffer);
void handle_backspace(EditorBuffer *buffer);

#endif