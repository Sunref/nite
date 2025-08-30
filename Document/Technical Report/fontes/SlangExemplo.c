#include <slang.h>

int main() {
    SLtt_get_terminfo();   // inicializa terminfo do slang
    SLsmg_init_smg();      // inicializa "screen management"
    SLsmg_gotorc(0, 0);    // vai para (0,0)
    SLsmg_write_string("Hello, World!");
    SLsmg_refresh();
    getchar();             // espera tecla
    SLsmg_reset_smg();
    return 0;
}
