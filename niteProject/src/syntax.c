#include "../include/syntax.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <dlfcn.h>

// Log para arquivo
static FILE *debug_log = NULL;

static void log_debug(const char *format, ...) {
    if (!debug_log) {
        debug_log = fopen("/tmp/nite_debug.log", "a");
        if (!debug_log) return;
    }

    va_list args;
    va_start(args, format);
    vfprintf(debug_log, format, args);
    va_end(args);
    fflush(debug_log);
}

// Forward declarations
static void apply_highlights_recursive(SyntaxContext *ctx, TSNode node, char **lines, const char *source);

// Detectar linguagem pela extensão
const char* detect_language_from_extension(const char *filename) {
    if (!filename) return NULL;

    const char *dot = strrchr(filename, '.');
    if (!dot) return NULL;

    if (strcmp(dot, ".py") == 0) return "python";
    if (strcmp(dot, ".c") == 0 || strcmp(dot, ".h") == 0) return "c";

    return NULL;
}

// Carregar linguagem dinamicamente
static TSLanguage* load_language(const char *lang_name) {
    char so_path[512];

    // Tentar primeiro no diretório do executável (./grammars/)
    snprintf(so_path, sizeof(so_path), "./grammars/%s.so", lang_name);
    log_debug( "[DEBUG] Trying path: %s\n", so_path);

    void *handle = dlopen(so_path, RTLD_NOW);

    // Se não encontrar, tentar caminho relativo ao source
    if (!handle) {
        log_debug( "[DEBUG] Failed: %s\n", dlerror());
        snprintf(so_path, sizeof(so_path), "../grammars/%s.so", lang_name);
        log_debug( "[DEBUG] Trying path: %s\n", so_path);
        handle = dlopen(so_path, RTLD_NOW);
    }

    // Se ainda não encontrar, tentar caminho do projeto
    if (!handle) {
        log_debug( "[DEBUG] Failed: %s\n", dlerror());
        snprintf(so_path, sizeof(so_path), "grammars/%s.so", lang_name);
        log_debug( "[DEBUG] Trying path: %s\n", so_path);
        handle = dlopen(so_path, RTLD_NOW);
    }

    // Tentar caminho de instalação do sistema (/usr/local/lib/nite/grammars/)
    if (!handle) {
        log_debug( "[DEBUG] Failed: %s\n", dlerror());
        snprintf(so_path, sizeof(so_path), "/usr/local/lib/nite/grammars/%s.so", lang_name);
        log_debug( "[DEBUG] Trying system path: %s\n", so_path);
        handle = dlopen(so_path, RTLD_NOW);
    }

    // Tentar variável de ambiente NITE_GRAMMAR_PATH
    if (!handle) {
        const char *env_path = getenv("NITE_GRAMMAR_PATH");
        if (env_path) {
            log_debug( "[DEBUG] Failed: %s\n", dlerror());
            snprintf(so_path, sizeof(so_path), "%s/%s.so", env_path, lang_name);
            log_debug( "[DEBUG] Trying env path: %s\n", so_path);
            handle = dlopen(so_path, RTLD_NOW);
        }
    }

    if (!handle) {
        log_debug( "[DEBUG] All paths failed. Last error: %s\n", dlerror());
        return NULL;
    }

    log_debug( "[DEBUG] Successfully loaded: %s\n", so_path);

    char symbol[128];
    snprintf(symbol, sizeof(symbol), "tree_sitter_%s", lang_name);

    TSLanguage *(*lang_fn)(void) = dlsym(handle, symbol);
    if (!lang_fn) {
        log_debug("Failed to find symbol %s: %s\n", symbol, dlerror());
        dlclose(handle);
        return NULL;
    }

    TSLanguage *language = lang_fn();
    uint32_t version = ts_language_abi_version(language);
    log_debug("[DEBUG] Grammar ABI version: %u, Expected: %u\n", version, TREE_SITTER_LANGUAGE_VERSION);

    return language;
}

// Criar contexto de syntax
SyntaxContext* syntax_create(const char *filename) {
    const char *lang_name = detect_language_from_extension(filename);
    if (!lang_name) {
        log_debug( "[DEBUG] No language detected for: %s\n", filename);
        return NULL;
    }

    log_debug( "[DEBUG] Detected language: %s\n", lang_name);

    TSLanguage *language = load_language(lang_name);
    if (!language) {
        log_debug( "[DEBUG] Failed to load language: %s\n", lang_name);
        return NULL;
    }

    log_debug( "[DEBUG] Language loaded successfully: %s\n", lang_name);

    SyntaxContext *ctx = calloc(1, sizeof(SyntaxContext));
    if (!ctx) return NULL;

    ctx->parser = ts_parser_new();
    if (!ctx->parser) {
        free(ctx);
        return NULL;
    }

    if (!ts_parser_set_language(ctx->parser, language)) {
        ts_parser_delete(ctx->parser);
        free(ctx);
        log_debug( "[DEBUG] Failed to set language\n");
        return NULL;
    }

    log_debug( "[DEBUG] Syntax context created successfully!\n");

    ctx->language = language;
    ctx->tree = NULL;
    ctx->line_highlights = NULL;
    ctx->num_lines = 0;
    ctx->language_name = strdup(lang_name);
    ctx->enabled = true;

    return ctx;
}

// Destruir contexto
void syntax_destroy(SyntaxContext *ctx) {
    if (!ctx) return;

    if (ctx->tree) {
        ts_tree_delete(ctx->tree);
    }

    if (ctx->parser) {
        ts_parser_delete(ctx->parser);
    }

    if (ctx->line_highlights) {
        for (size_t i = 0; i < ctx->num_lines; i++) {
            free(ctx->line_highlights[i].types);
        }
        free(ctx->line_highlights);
    }

    if (ctx->language_name) {
        free(ctx->language_name);
    }

    free(ctx);
}

// Converter buffer de linhas para string única
static char* buffer_to_string(char **lines, size_t num_lines) {
    if (!lines || num_lines == 0) return strdup("");

    size_t total_length = 0;
    for (size_t i = 0; i < num_lines; i++) {
        total_length += strlen(lines[i]) + 1; // +1 para \n
    }

    char *source = malloc(total_length + 1);
    if (!source) return NULL;

    char *ptr = source;
    for (size_t i = 0; i < num_lines; i++) {
        size_t len = strlen(lines[i]);
        memcpy(ptr, lines[i], len);
        ptr += len;
        *ptr++ = '\n';
    }
    *ptr = '\0';

    return source;
}

// Atualizar parsing
void syntax_update(SyntaxContext *ctx, char **lines, size_t num_lines) {
    if (!ctx || !ctx->enabled) return;

    // Converter buffer para string
    char *source = buffer_to_string(lines, num_lines);
    if (!source) return;

    // Parse incremental
    TSTree *new_tree = ts_parser_parse_string(
        ctx->parser,
        ctx->tree,
        source,
        strlen(source)
    );

    // Liberar árvore antiga
    if (ctx->tree) {
        ts_tree_delete(ctx->tree);
    }
    ctx->tree = new_tree;

    // Realocar arrays de highlight
    if (ctx->line_highlights) {
        for (size_t i = 0; i < ctx->num_lines; i++) {
            free(ctx->line_highlights[i].types);
        }
        free(ctx->line_highlights);
    }

    ctx->num_lines = num_lines;
    ctx->line_highlights = calloc(num_lines, sizeof(LineHighlight));

    // Inicializar cada linha
    for (size_t i = 0; i < num_lines; i++) {
        size_t line_len = strlen(lines[i]);
        ctx->line_highlights[i].length = line_len;
        ctx->line_highlights[i].types = calloc(line_len, sizeof(HighlightType));
        // Inicializar tudo como NORMAL
        for (size_t j = 0; j < line_len; j++) {
            ctx->line_highlights[i].types[j] = HIGHLIGHT_NORMAL;
        }
    }

    // Aplicar highlights baseado na árvore
    if (ctx->tree) {
        TSNode root = ts_tree_root_node(ctx->tree);
        apply_highlights_recursive(ctx, root, lines, source);
    }

    free(source);
}

// Mapear tipo de node para HighlightType
static HighlightType node_type_to_highlight(const char *type) {
    // Keywords do C
    if (strcmp(type, "if") == 0 || strcmp(type, "else") == 0 ||
        strcmp(type, "for") == 0 || strcmp(type, "while") == 0 ||
        strcmp(type, "do") == 0 || strcmp(type, "switch") == 0 ||
        strcmp(type, "case") == 0 || strcmp(type, "default") == 0 ||
        strcmp(type, "return") == 0 || strcmp(type, "break") == 0 ||
        strcmp(type, "continue") == 0 || strcmp(type, "goto") == 0 ||
        strcmp(type, "sizeof") == 0 || strcmp(type, "typedef") == 0 ||
        strcmp(type, "struct") == 0 || strcmp(type, "union") == 0 ||
        strcmp(type, "enum") == 0 || strcmp(type, "const") == 0 ||
        strcmp(type, "static") == 0 || strcmp(type, "extern") == 0 ||
        strcmp(type, "register") == 0 || strcmp(type, "volatile") == 0 ||
        strcmp(type, "auto") == 0 || strcmp(type, "inline") == 0 ||
        strcmp(type, "restrict") == 0) {
        return HIGHLIGHT_KEYWORD;
    }

    // Tipos primitivos do C
    if (strcmp(type, "primitive_type") == 0 ||
        strcmp(type, "type_identifier") == 0 ||
        strcmp(type, "sized_type_specifier") == 0) {
        return HIGHLIGHT_TYPE;
    }

    // Strings e caracteres
    if (strcmp(type, "string_literal") == 0 ||
        strcmp(type, "char_literal") == 0 ||
        strcmp(type, "system_lib_string") == 0) {
        return HIGHLIGHT_STRING;
    }

    // Números
    if (strcmp(type, "number_literal") == 0) {
        return HIGHLIGHT_NUMBER;
    }

    // Comentários
    if (strcmp(type, "comment") == 0) {
        return HIGHLIGHT_COMMENT;
    }

    // Funções e identificadores de função
    if (strcmp(type, "function_declarator") == 0 ||
        strcmp(type, "call_expression") == 0) {
        return HIGHLIGHT_FUNCTION;
    }

    // Preprocessor
    if (strcmp(type, "preproc_include") == 0 ||
        strcmp(type, "preproc_def") == 0 ||
        strcmp(type, "preproc_function_def") == 0 ||
        strcmp(type, "preproc_call") == 0 ||
        strcmp(type, "preproc_if") == 0 ||
        strcmp(type, "preproc_ifdef") == 0 ||
        strcmp(type, "preproc_directive") == 0) {
        return HIGHLIGHT_KEYWORD;
    }

    // Keywords do Python (manter para compatibilidade)
    if (strcmp(type, "def") == 0 || strcmp(type, "class") == 0 ||
        strcmp(type, "import") == 0 || strcmp(type, "from") == 0 ||
        strcmp(type, "as") == 0 || strcmp(type, "in") == 0 ||
        strcmp(type, "and") == 0 || strcmp(type, "or") == 0 ||
        strcmp(type, "not") == 0 || strcmp(type, "pass") == 0 ||
        strcmp(type, "with") == 0 || strcmp(type, "try") == 0 ||
        strcmp(type, "except") == 0 || strcmp(type, "finally") == 0 ||
        strcmp(type, "raise") == 0 || strcmp(type, "async") == 0 ||
        strcmp(type, "await") == 0 || strcmp(type, "yield") == 0) {
        return HIGHLIGHT_KEYWORD;
    }

    // Strings do Python
    if (strcmp(type, "string") == 0 || strcmp(type, "string_content") == 0) {
        return HIGHLIGHT_STRING;
    }

    // Números do Python
    if (strcmp(type, "integer") == 0 || strcmp(type, "float") == 0) {
        return HIGHLIGHT_NUMBER;
    }

    return HIGHLIGHT_NORMAL;
}

// Aplicar highlights recursivamente
static void apply_highlights_recursive(SyntaxContext *ctx, TSNode node, char **lines, const char *source) {
    const char *node_type = ts_node_type(node);

    // DEBUG: Log todos os node types encontrados
    if (!ts_node_is_named(node)) {
        // Skip anonymous nodes (like punctuation)
    } else {
        TSPoint start = ts_node_start_point(node);
        log_debug("[NODE] Type: '%s' at line %d\n", node_type, start.row + 1);
    }

    HighlightType hl_type = node_type_to_highlight(node_type);

    // Se este node tem um highlight, aplicar
    if (hl_type != HIGHLIGHT_NORMAL) {
        TSPoint start_point = ts_node_start_point(node);
        TSPoint end_point = ts_node_end_point(node);

        // Aplicar highlight em cada linha do range
        for (uint32_t line = start_point.row; line <= end_point.row && line < ctx->num_lines; line++) {
            uint32_t start_col = (line == start_point.row) ? start_point.column : 0;
            uint32_t end_col = (line == end_point.row) ? end_point.column : ctx->line_highlights[line].length;

            for (uint32_t col = start_col; col < end_col && col < ctx->line_highlights[line].length; col++) {
                ctx->line_highlights[line].types[col] = hl_type;
            }
        }
    }

    // Recursão nos filhos
    uint32_t child_count = ts_node_child_count(node);
    for (uint32_t i = 0; i < child_count; i++) {
        TSNode child = ts_node_child(node, i);
        apply_highlights_recursive(ctx, child, lines, source);
    }
}

// Obter highlight de uma posição
HighlightType syntax_get_highlight(SyntaxContext *ctx, size_t line, size_t col) {
    if (!ctx || !ctx->enabled || !ctx->line_highlights) return HIGHLIGHT_NORMAL;
    if (line >= ctx->num_lines) return HIGHLIGHT_NORMAL;
    if (col >= ctx->line_highlights[line].length) return HIGHLIGHT_NORMAL;

    return ctx->line_highlights[line].types[col];
}