#include "termbox2.h"

int main() {
    tb_init();
    tb_clear();
    tb_printf(0, 0, TB_WHITE, TB_BLACK, "Hello, World!");
    tb_present();
    tb_peek_event(NULL, -1); // espera tecla
    tb_shutdown();
    return 0;
}
