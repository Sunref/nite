#ifndef SYNTAX_H
#define SYNTAX_H

#include <ncurses.h>

// Linguagens suportadas
typedef enum {
    LANG_C,
    LANG_JAVA,
    LANG_PYTHON,
    LANG_HTML,
    LANG_UNKNOWN
} Language;

// Tipos de tokens reconhecidos
typedef enum {
    SYNTAX_KEYWORD,
    SYNTAX_STRING,
    SYNTAX_COMMENT,
    SYNTAX_NUMBER,
    SYNTAX_OPERATOR,
    SYNTAX_FUNCTION,
    SYNTAX_VARIABLE,
    SYNTAX_TYPE,
    SYNTAX_PUNCTUATION,
    SYNTAX_COUNT
} SyntaxGroup;

// Inicializa as cores do tema
void init_syntax_colors();

// Detecta linguagem pelo nome do arquivo (.c/.java/.py/.html etc...)
Language detect_language(const char *filename);

// Desenha uma linha com highlight de sintaxe
void highlight_line(int y, const char *line, Language lang);

#endif
