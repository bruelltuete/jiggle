#include <cstdio>
#include "pico/stdlib.h"
#include "pico/binary_info.h"
#include "hardware/clocks.h"
#include "hardware/pll.h"
#include "hardware/rtc.h"
#include "hardware/structs/scb.h"
#include "tusb.h"
#include "usb.h"

static volatile enum
{
    NOP     = 0,
    START   = 1,
    WAIT,
    FINISH,
}       jigglestate = START;

static void rtcalarm_handler()
{
    rtc_disable_alarm();
    jigglestate = START;
}

static void setup_rtc()
{
    rtc_init();
    // no idea what the actual time is right now.
    // it doesnt matter. this is reasonable enough.
    datetime_t maybetoday = {
        .year = 2023,
        .month = 10,
        .day = 29,
        .dotw = 0,  // sunday
        .hour = 13,
        .min = 59,
        .sec = 55
    };
    rtc_set_datetime(&maybetoday);
}

// before 23 mA, after 10 mA
static void setup_sleep()
{
    io_rw_32    wake0  = CLOCKS_WAKE_EN0_RESET;
    io_rw_32    wake1  = CLOCKS_WAKE_EN1_RESET;
    io_rw_32    sleep0 = CLOCKS_SLEEP_EN0_RESET;
    io_rw_32    sleep1 = CLOCKS_SLEEP_EN1_RESET;

    // not using spi.
    wake0  &= ~(CLOCKS_WAKE_EN0_CLK_SYS_SPI0_BITS  | CLOCKS_WAKE_EN0_CLK_PERI_SPI0_BITS |
                CLOCKS_WAKE_EN0_CLK_SYS_SPI1_BITS  | CLOCKS_WAKE_EN0_CLK_PERI_SPI1_BITS);
    sleep0 &= ~(CLOCKS_SLEEP_EN0_CLK_SYS_SPI0_BITS | CLOCKS_SLEEP_EN0_CLK_PERI_SPI0_BITS |
                CLOCKS_SLEEP_EN0_CLK_SYS_SPI1_BITS | CLOCKS_SLEEP_EN0_CLK_PERI_SPI1_BITS);
    // not using pio.
    wake0  &= ~(CLOCKS_WAKE_EN0_CLK_SYS_PIO0_BITS  | CLOCKS_WAKE_EN0_CLK_SYS_PIO1_BITS);
    sleep0 &= ~(CLOCKS_SLEEP_EN0_CLK_SYS_PIO0_BITS | CLOCKS_SLEEP_EN0_CLK_SYS_PIO1_BITS);
    // not using uart1
    wake1  &= ~(CLOCKS_WAKE_EN1_CLK_SYS_UART1_BITS  | CLOCKS_WAKE_EN1_CLK_PERI_UART1_BITS);
    sleep1 &= ~(CLOCKS_SLEEP_EN1_CLK_SYS_UART1_BITS | CLOCKS_SLEEP_EN1_CLK_PERI_UART1_BITS);
    // not using pwm.
    wake0  &= ~CLOCKS_WAKE_EN0_CLK_SYS_PWM_BITS;
    sleep0 &= ~CLOCKS_SLEEP_EN0_CLK_SYS_PWM_BITS;
    // not useful to us.
    wake1  &= ~CLOCKS_WAKE_EN1_CLK_SYS_TBMAN_BITS;
    sleep1 &= ~CLOCKS_SLEEP_EN1_CLK_SYS_TBMAN_BITS;
    // not using jtag debugging. (picoprobe is swd)
    wake0  &= ~CLOCKS_WAKE_EN0_CLK_SYS_JTAG_BITS;
    sleep0 &= ~CLOCKS_SLEEP_EN0_CLK_SYS_JTAG_BITS;
    // not using adc.
    wake0  &= ~(CLOCKS_WAKE_EN0_CLK_ADC_ADC_BITS  | CLOCKS_WAKE_EN0_CLK_SYS_ADC_BITS);
    sleep0 &= ~(CLOCKS_SLEEP_EN0_CLK_ADC_ADC_BITS | CLOCKS_SLEEP_EN0_CLK_SYS_ADC_BITS);
    // not using i2c.
    wake0  &= ~(CLOCKS_WAKE_EN0_CLK_SYS_I2C0_BITS  | CLOCKS_WAKE_EN0_CLK_SYS_I2C1_BITS);
    sleep0 &= ~(CLOCKS_SLEEP_EN0_CLK_SYS_I2C0_BITS | CLOCKS_SLEEP_EN0_CLK_SYS_I2C1_BITS);

    // while sleeping, we dont need:
    sleep0 &= ~CLOCKS_SLEEP_EN0_CLK_SYS_SIO_BITS;
    sleep0 &= ~CLOCKS_SLEEP_EN0_CLK_SYS_ROM_BITS;
    sleep1 &= ~CLOCKS_SLEEP_EN1_CLK_SYS_XIP_BITS;
    sleep0 &= ~CLOCKS_SLEEP_EN0_CLK_SYS_DMA_BITS;
    sleep0 &= ~(CLOCKS_SLEEP_EN0_CLK_SYS_BUSFABRIC_BITS | CLOCKS_SLEEP_EN0_CLK_SYS_BUSCTRL_BITS);
    sleep0 &= ~(CLOCKS_SLEEP_EN0_CLK_SYS_SRAM0_BITS | CLOCKS_SLEEP_EN0_CLK_SYS_SRAM1_BITS | CLOCKS_SLEEP_EN0_CLK_SYS_SRAM2_BITS | CLOCKS_SLEEP_EN0_CLK_SYS_SRAM3_BITS);
    sleep1 &= ~(CLOCKS_SLEEP_EN1_CLK_SYS_SRAM4_BITS | CLOCKS_SLEEP_EN1_CLK_SYS_SRAM5_BITS);

    clocks_hw->wake_en0 = wake0;
    clocks_hw->wake_en1 = wake1;
    clocks_hw->sleep_en0 = sleep0;
    clocks_hw->sleep_en1 = sleep1;

    hw_set_bits(&scb_hw->scr, M0PLUS_SCR_SLEEPDEEP_BITS);
}

static void setup_clocks()
{
    uint32_t    roscfreq = 6 * MHZ;     // ballpark, does not matter.
    clock_configure(clk_sys, CLOCKS_CLK_SYS_CTRL_SRC_VALUE_CLKSRC_CLK_SYS_AUX, CLOCKS_CLK_SYS_CTRL_AUXSRC_VALUE_ROSC_CLKSRC, roscfreq, roscfreq);
    clock_configure(clk_ref, CLOCKS_CLK_REF_CTRL_SRC_VALUE_ROSC_CLKSRC_PH, 0, roscfreq, roscfreq);

    // something like: pico-sdk/src/rp2_common/hardware_clocks/scripts/vcocalc.py -l 48
    pll_init(pll_sys, 1, 768 * MHZ, 4, 4);
    const uint32_t pllfreq = 48 * MHZ;
    clock_configure(clk_sys, CLOCKS_CLK_SYS_CTRL_SRC_VALUE_CLKSRC_CLK_SYS_AUX, CLOCKS_CLK_SYS_CTRL_AUXSRC_VALUE_CLKSRC_PLL_SYS, pllfreq, pllfreq);
    // same as sdk, keep running directly off xosc at 12mhz.
    clock_configure(clk_ref, CLOCKS_CLK_REF_CTRL_SRC_VALUE_XOSC_CLKSRC, 0, 12 * MHZ, 12 * MHZ);

    // we are not using adc.
    clock_stop(clk_adc);
    // peri (ie uart) runs off xosc at fixed speed.
    clock_configure(clk_peri, 0, CLOCKS_CLK_PERI_CTRL_AUXSRC_VALUE_XOSC_CLKSRC, 12 * MHZ, 12 * MHZ);

    const int rtcfreq = 12 * MHZ / KHZ;
    clock_configure(clk_rtc, 0, CLOCKS_CLK_RTC_CTRL_AUXSRC_VALUE_XOSC_CLKSRC, 12 * MHZ, rtcfreq);
    rtc_hw->clkdiv_m1 = rtcfreq - 1;

    // usb runs off pll_sys.
    clock_stop(clk_usb);
    pll_deinit(pll_usb);
    clock_configure(clk_usb, 0, CLOCKS_CLK_USB_CTRL_AUXSRC_VALUE_CLKSRC_PLL_SYS, pllfreq, 48 * MHZ);
}

static void prime_rtc_alarm(int secfromnow)
{
    int minfromnow = secfromnow / 60;
    secfromnow %= 60;

    datetime_t  now;
    rtc_get_datetime(&now);

    // wake us up in about a minute or two...
    int minoverflow = (now.sec + secfromnow) / 60;
    now.sec = (now.sec + secfromnow) % 60;
    now.min = (now.min + minfromnow + minoverflow) % 60;
    // technically this is a repeating alarm. this way we do not have to worry about figuring out the correct hours/days/etc.
    now.hour = now.day = now.dotw = now.month = now.year = -1;
    // beware: the rtc does not stop during debugging! so if you step through here, the alarm is likely to have expired already.
    rtc_set_alarm(&now, &rtcalarm_handler);
}

int main()
{
    bi_decl(bi_program_description("Jiggle mouse"));

    setup_clocks();
    setup_sleep();

    stdio_init_all();
    printf("\n\nHello, jiggle!\n");

    setup_rtc();

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


    tusb_init();

    while (true)
    {
        tud_task();
        if (tud_task_event_ready())
            // keep spinning usb tasks as long as there are any
            continue;

        if (tud_hid_ready())
        {
            // jiggle time
            switch (jigglestate)
            {
                case NOP:
                    break;
                case START:
                    if (tud_suspended())
                        tud_remote_wakeup();
                    else
                    {
                        printf("jiggle");
                        tud_hid_mouse_report(REPORT_ID_MOUSE, 0, 1, 0, 0, 0);
                        jigglestate = WAIT;
                    }
                    break;
                case WAIT:
                    sleep_ms(10);
                    jigglestate = FINISH;
                    break;
                case FINISH:
                    printf("\n");
                    tud_hid_mouse_report(REPORT_ID_MOUSE, 0, -1, 0, 0, 0);
                    jigglestate = NOP;
                    prime_rtc_alarm(1);   // FIXME: randomise
                    break;
            }
        }

        sleep_ms(1);
    }

    return 0;
}
