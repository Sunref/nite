#ifndef HANDLE_H
#define HANDLE_H

#include <ncurses.h>
#include "editor.h"

#define MAX_CLIPBOARD_SIZE (256 * 1024) // Tamanho máximo do clipboard para colar blocos multilinha (ex.: código)
#define MAX_UNDO 50 // Número máximo de ações de "undo" armazenadas

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

void handle_backspace(EditorBuffer *buffer);
void handle_delete(EditorBuffer *buffer, Selection *sel);
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