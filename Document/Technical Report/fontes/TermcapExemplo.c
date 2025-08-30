#include <stdio.h>
#include <termcap.h>
#include <unistd.h>

int main() {
    char term_buffer[2048];
    char *termtype = getenv("TERM");
    char *cm;
    tgetent(term_buffer, termtype);

    cm = tgetstr("cm", NULL); // comando "cursor motion"
    tputs(tgoto(cm, 0, 0), 1, putchar); // move cursor para (0,0)
    printf("Hello, World!\n");
    return 0;
}
