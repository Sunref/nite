#include "../include/file_validation.h"
#include <strings.h>
#include <string.h>

bool is_forbidden_extension(const char *filename) {
    // Lista de extensões proibidas
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

    const char *dot = strrchr(filename, '.');
    if (!dot) return false; // sem extensão, não bloqueia

    for (int i = 0; blocked_exts[i] != NULL; i++) {
        if (strcasecmp(dot, blocked_exts[i]) == 0) {
            return true; // bloqueado
        }
    }
    return false;
}

char* process_filename(const char *input_filename) {
    if (!input_filename) return NULL;

    // Verificar se é uma extensão proibida
    if (is_forbidden_extension(input_filename)) {
        return NULL; // retorna NULL para indicar erro
    }

    // Se passou na validação, apenas duplica o nome sem modificar
    return strdup(input_filename);
}

