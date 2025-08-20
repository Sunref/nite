#include <ncurses.h>

int main(int argc, char ** argv){
  initscr();
  start_color();
  use_default_colors();

  init_pair(1, COLOR_RED, -1);
  printw("Uma string que fica piscando loucamente!");
  mvchgat(0, 0, -1, A_BLINK, 1, NULL);

  refresh();
  getch();
  endwin();
  return 0;
}
