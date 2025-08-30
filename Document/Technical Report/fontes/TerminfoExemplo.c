#include <stdio.h>
#include <curses.h>
#include <term.h>

int main() {
    setupterm(NULL, fileno(stdout), NULL);
    putp(tparm(cursor_address, 0, 0)); // move cursor para (0,0)
    printf("Hello, World!\n");
    return 0;
}
