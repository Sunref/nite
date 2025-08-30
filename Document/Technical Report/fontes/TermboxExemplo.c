#include <termbox.h>

int main() {
    tb_init();  // inicializa termbox
    tb_clear();
    tb_printf(0, 0, TB_WHITE, TB_BLACK, "Hello, World!");
    tb_present();
    tb_poll_event(NULL); // espera tecla
    tb_shutdown();
    return 0;
}
