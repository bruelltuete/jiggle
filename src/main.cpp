#include <cstdio>
#include "pico/stdlib.h"
#include "pico/binary_info.h"
#include "hardware/clocks.h"
#include "hardware/rtc.h"
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
