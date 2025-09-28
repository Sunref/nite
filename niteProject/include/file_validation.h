#ifndef FILE_VALIDATION_H
#define FILE_VALIDATION_H

#include <stdbool.h>

// Função para verificar se uma extensão é proibida
bool is_forbidden_extension(const char *filename);

// Função para processar e validar nome de arquivo
char* process_filename(const char *input_filename);



#endif