#include "../include/syntax.h"
#include <string.h>
#include <ctype.h>

// -------------------------------
// Palavras-chave por linguagem
// -------------------------------
const char *keywords_c[] = {
    "int", "char", "float", "double", "return", "if", "else",
    "for", "while", "do", "switch", "case", "struct", "typedef",
    "void", "break", "continue", "static", "const", "unsigned",
    "signed", "sizeof", "enum", "long", "short", "extern", "volatile", "register", NULL
};

const char *keywords_java[] = {
    "class", "public", "private", "protected", "static", "final", "abstract",
    "void", "int", "char", "float", "double", "boolean", "if", "else", "for",
    "while", "do", "switch", "case", "break", "continue", "return", "try",
    "catch", "finally", "throw", "throws", "import", "package", "extends",
    "implements", "this", "super", "new", "null", "true", "false", NULL
};

const char *keywords_python[] = {
    "def", "class", "import", "from", "as", "return", "if", "elif", "else",
    "for", "while", "break", "continue", "try", "except", "finally", "raise",
    "with", "lambda", "global", "nonlocal", "pass", "yield", "True", "False", "None",
    "and", "or", "not", "is", "in", "assert", NULL
};

const char *keywords_html[] = {
    "html", "head", "body", "title", "meta", "link", "script", "style", "div",
    "span", "a", "p", "h1", "h2", "h3", "h4", "h5", "h6", "ul", "ol", "li",
    "table", "tr", "td", "th", "form", "input", "button", "select", "option",
    "textarea", "img", "br", "hr", "footer", "header", "section", "article",
    "nav", "main", NULL
};

const char *keywords_css[] = {
    "color", "background", "background-color", "margin", "padding", "border",
    "width", "height", "font", "font-size", "font-weight", "display", "position",
    "absolute", "relative", "fixed", "flex", "grid", "inline", "block", "none",
    "float", "clear", "z-index", "overflow", "text-align", "align-items",
    "justify-content", "opacity", "transform", "transition", "animation", NULL
};

// -------------------------------
// Inicialização de cores (Sunref Yue → 256 cores)
// -------------------------------
void init_syntax_colors() {
    start_color();

    init_pair(SYNTAX_KEYWORD,     219, -1); // #f9BEF0 → rosa
    init_pair(SYNTAX_STRING,      229, -1); // #FFF5C2 → amarelo
    init_pair(SYNTAX_COMMENT,     243, -1); // #7B7B7B → cinza
    init_pair(SYNTAX_NUMBER,      183, -1); // #CCB3FF → lilás
    init_pair(SYNTAX_OPERATOR,    216, -1); // #FFCAB3 → laranja
    init_pair(SYNTAX_FUNCTION,    193, -1); // #E2FFCC → verde claro
    init_pair(SYNTAX_VARIABLE,    252, -1); // #DFDFDF → quase branco
    init_pair(SYNTAX_TYPE,        159, -1); // #CEFFFF → azul claro
    init_pair(SYNTAX_PUNCTUATION, 218, -1); // #FFB7E0 → rosa forte
}

// -------------------------------
// Detectar linguagem pelo sufixo
// -------------------------------
Language detect_language(const char *filename) {
    const char *dot = strrchr(filename, '.');
    if (!dot) return LANG_UNKNOWN;

    if (strcmp(dot, ".c") == 0 || strcmp(dot, ".h") == 0)
        return LANG_C;
    if (strcmp(dot, ".java") == 0)
        return LANG_JAVA;
    if (strcmp(dot, ".py") == 0)
        return LANG_PYTHON;
    if (strcmp(dot, ".html") == 0 || strcmp(dot, ".htm") == 0)
        return LANG_HTML;
    if (strcmp(dot, ".css") == 0)
        return LANG_HTML; // CSS tratado como extensão do HTML

    return LANG_UNKNOWN;
}

// -------------------------------
// Utilitários
// -------------------------------
int is_keyword(const char *word, Language lang) {
    const char **kw = NULL;
    switch (lang) {
        case LANG_C: kw = keywords_c; break;
        case LANG_JAVA: kw = keywords_java; break;
        case LANG_PYTHON: kw = keywords_python; break;
        case LANG_HTML: kw = keywords_html; break;
        default: return 0;
    }
    for (int i = 0; kw && kw[i]; i++) {
        if (strcmp(word, kw[i]) == 0)
            return 1;
    }
    return 0;
}

// -------------------------------
// Highlight de sintaxe
// -------------------------------
void highlight_line(int y, const char *line, Language lang) {
    int x = 0;
    const char *p = line;

    while (*p) {
        // Comentários
        if (lang == LANG_C || lang == LANG_JAVA) {
            if (*p == '/' && *(p+1) == '/') {
                attron(COLOR_PAIR(SYNTAX_COMMENT));
                mvprintw(y, x, "%s", p);
                attroff(COLOR_PAIR(SYNTAX_COMMENT));
                break;
            }
        }
        if (lang == LANG_PYTHON) {
            if (*p == '#') {
                attron(COLOR_PAIR(SYNTAX_COMMENT));
                mvprintw(y, x, "%s", p);
                attroff(COLOR_PAIR(SYNTAX_COMMENT));
                break;
            }
        }
        if (lang == LANG_HTML) {
            if (*p == '<' && *(p+1) == '!' && strncmp(p, "<!--", 4) == 0) {
                attron(COLOR_PAIR(SYNTAX_COMMENT));
                mvprintw(y, x, "%s", p);
                attroff(COLOR_PAIR(SYNTAX_COMMENT));
                break;
            }
        }

        // Strings
        if (*p == '"' || *p == '\'') {
            char quote = *p;
            attron(COLOR_PAIR(SYNTAX_STRING));
            addch(*p++);
            x++;
            while (*p && *p != quote) {
                addch(*p++);
                x++;
            }
            if (*p == quote) {
                addch(*p++);
                x++;
            }
            attroff(COLOR_PAIR(SYNTAX_STRING));
            continue;
        }

        // Número
        if (isdigit(*p)) {
            attron(COLOR_PAIR(SYNTAX_NUMBER));
            while (isdigit(*p)) {
                mvaddch(y, x++, *p++);
            }
            attroff(COLOR_PAIR(SYNTAX_NUMBER));
            continue;
        }

        // Palavra (keyword, tag, etc.)
        if (isalpha(*p) || *p == '_') {
            char word[128];
            int len = 0;
            int start_x = x;

            while (isalnum(*p) || *p == '_' || *p == '-') {
                word[len++] = *p++;
                x++;
            }
            word[len] = '\0';

            if (is_keyword(word, lang)) {
                attron(COLOR_PAIR(SYNTAX_KEYWORD));
                mvprintw(y, start_x, "%s", word);
                attroff(COLOR_PAIR(SYNTAX_KEYWORD));
            } else {
                mvprintw(y, start_x, "%s", word);
            }
            continue;
        }

        // Operadores
        if (strchr("+-*/=%<>!&|^", *p)) {
            attron(COLOR_PAIR(SYNTAX_OPERATOR));
            mvaddch(y, x++, *p++);
            attroff(COLOR_PAIR(SYNTAX_OPERATOR));
            continue;
        }

        // Pontuação
        if (ispunct(*p)) {
            attron(COLOR_PAIR(SYNTAX_PUNCTUATION));
            mvaddch(y, x++, *p++);
            attroff(COLOR_PAIR(SYNTAX_PUNCTUATION));
            continue;
        }

        // Default
        mvaddch(y, x++, *p++);
    }
}
