#include <ncurses.h>
#include <stdio.h>

int main() {
    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    
    mvprintw(0, 0, "Key Code Debug Tool");
    mvprintw(1, 0, "Press any key to see its code (ESC to exit)");
    mvprintw(2, 0, "Try: Arrow keys, F1, F2, Backspace, etc.");
    mvprintw(3, 0, "----------------------------------------");
    
    int line = 4;
    int ch;
    
    while ((ch = getch()) != 27) { // ESC to exit
        mvprintw(line, 0, "Key pressed: %d (0x%X)", ch, ch);
        
        // Show special key names
        if (ch == KEY_UP) mvprintw(line, 25, "= KEY_UP");
        else if (ch == KEY_DOWN) mvprintw(line, 25, "= KEY_DOWN");
        else if (ch == KEY_LEFT) mvprintw(line, 25, "= KEY_LEFT");
        else if (ch == KEY_RIGHT) mvprintw(line, 25, "= KEY_RIGHT");
        else if (ch == KEY_F(1)) mvprintw(line, 25, "= KEY_F(1)");
        else if (ch == KEY_F(2)) mvprintw(line, 25, "= KEY_F(2)");
        else if (ch == KEY_BACKSPACE) mvprintw(line, 25, "= KEY_BACKSPACE");
        else if (ch == 10) mvprintw(line, 25, "= ENTER");
        else if (ch >= 32 && ch <= 126) mvprintw(line, 25, "= '%c'", ch);
        
        line++;
        if (line > LINES - 2) {
            line = 4;
            for (int i = 4; i < LINES - 1; i++) {
                move(i, 0);
                clrtoeol();
            }
        }
        refresh();
    }
    
    endwin();
    printf("Key debug tool finished.\n");
    return 0;
}