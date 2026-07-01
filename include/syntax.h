#ifndef SYNTAX_H
#define SYNTAX_H

#include <tree_sitter/api.h>

// Tipos de highlight
typedef enum {
    HIGHLIGHT_NORMAL,
    HIGHLIGHT_KEYWORD,
    HIGHLIGHT_FUNCTION,
    HIGHLIGHT_STRING,
    HIGHLIGHT_NUMBER,
    HIGHLIGHT_COMMENT,
    HIGHLIGHT_TYPE,
    HIGHLIGHT_OPERATOR,
    HIGHLIGHT_VARIABLE
} HighlightType;

// Highlight de uma linha
typedef struct {
    HighlightType *types;  // Array de tipos (um por caractere)
    size_t length;         // Tamanho do array
} LineHighlight;

// Forward declaration - declaração antecipada
typedef struct SyntaxContext SyntaxContext;

// Estrutura completa
struct SyntaxContext {
    TSParser *parser;
    TSTree *tree;
    const TSLanguage *language;
    LineHighlight *line_highlights;  // Array paralelo ao buffer->lines
    size_t num_lines;
    char *language_name;
    bool enabled;
};

// Funções públicas
SyntaxContext* syntax_create(const char *filename);
void syntax_destroy(SyntaxContext *ctx);
void syntax_update(SyntaxContext *ctx, char **lines, size_t num_lines);
HighlightType syntax_get_highlight(SyntaxContext *ctx, size_t line, size_t col);
const char* detect_language_from_extension(const char *filename);

#endif // SYNTAX_H