#!/bin/bash
# Limpeza.
# Desenvolvido por David Buzatto

echo "Iniciando limpeza de arquivos temporários..."

# Lista de extensões para remover
extensions=(
    "aux" "bak" "bbl" "blg" "brf" "def" "equ" "exp" 
    "gz" "idx" "ilg" "lis" "listing" "lof" "log" "loq" 
    "lot" "nav" "out" "pdf" "pyg" "sav" "sigla" "siglax" 
    "snm" "stf" "symbols" "symbolsx" "synctex" "toc" "xwm"
)

# Contador de arquivos removidos
count=0

# Remove arquivos com cada extensão
for ext in "${extensions[@]}"; do
    for file in *."$ext"; do
        if [[ -f "$file" ]]; then
            rm "$file"
            echo "Removido: $file"
            ((count++))
        fi
    done
done

# Remove arquivos específicos do LaTeX que podem ter nomes compostos
rm -f *.gz\(busy\) 2>/dev/null
rm -f *.synctex\(busy\) 2>/dev/null

echo "Limpeza concluída. $count arquivos removidos."
