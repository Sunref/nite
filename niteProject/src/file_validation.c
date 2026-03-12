/*
 *
 * Trata das extenções invalidas no editor.
 *
 */

#include "../include/file_validation.h"
#include <string.h> // Biblioteca para strcmp e outras funções de string

bool is_forbidden_extension(const char *filename) { // Verifica se o arquivo possui uma extensão proibida

    // Lista de extensões proibidas (necessita verificação)
    const char *blocked_exts[] = {
        ".exe", ".dll", ".so", ".bin", ".out", ".app",
        ".png", ".jpg", ".jpeg", ".gif", ".bmp", ".tiff", ".webp",
        ".mp3", ".mp4", ".wav", ".avi", ".mkv", ".mov", ".flv",
        ".zip", ".rar", ".tar", ".gz", ".7z", ".bz2",
        ".doc", ".docx", ".pptx", ".xlsx", ".pdf", ".odt",
        ".iso", ".img", ".dmg",
        ".class", ".jar", ".pyc",
        ".deb", ".rpm", ".msi",
        NULL
    };

    const char *dot = strrchr(filename, '.'); // Pega a última ocorrência do caractere '.'
    if (!dot) return false; // Sem extensão, não bloqueia

    for (int i = 0; blocked_exts[i] != NULL; i++) { // Percorre a lista de extensões proibidas
        if (strcasecmp(dot, blocked_exts[i]) == 0) {
            return true; // Bloqueado
        }
    }
    return false;

}

char* process_filename(const char *input_filename) { // Processa o nome do arquivo, retornando uma cópia duplicada

    if (!input_filename) return NULL; // Retorna NULL se o nome do arquivo for nulo

    // Verificar se é uma extensão proibida
    if (is_forbidden_extension(input_filename)) {
        return NULL; // Retorna NULL para indicar erro
    }

    // Se passou na validação, apenas duplica o nome sem modificar
    return strdup(input_filename);

}