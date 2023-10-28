#include <cstdio>
#include "pico/stdlib.h"
#include "pico/binary_info.h"
#include "hardware/clocks.h"


int main()
{

    bi_decl(bi_program_description("Jiggle mouse"));

    stdio_init_all();
    printf("\n\nHello, jiggle!\n");

    // debug
    {
        printf("chip version = %i\n", rp2040_chip_version());
        printf("rom  version = %i\n", rp2040_rom_version());
        uint32_t reffreq = frequency_count_khz(CLOCKS_FC0_SRC_VALUE_CLK_REF);
        printf("ref  = %ld kHz\n", reffreq);
        uint32_t xoscfreq = frequency_count_khz(CLOCKS_FC0_SRC_VALUE_XOSC_CLKSRC);
        printf("xosc = %ld kHz\n", xoscfreq);
        uint32_t sysfreq = frequency_count_khz(CLOCKS_FC0_SRC_VALUE_CLK_SYS);
        printf("sys  = %ld kHz\n", sysfreq);
    }

    return 0;
}
