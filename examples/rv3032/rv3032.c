#include <stdint.h>
#include <stdbool.h>

#include "hexa_board.h"
#include "hexa_rv3032.h"
#include "hexa_time.h"
#include "xprintf.h"


static void print_c100(int32_t v)
{
        const char *sign = (v < 0) ? "-" : "";
        uint32_t a = (uint32_t)((v < 0) ? -v : v);

        xprintf("%s%u.%02u", sign, a / 100, a % 100);
}

void app_main(void)
{
        hexa_board_init();

        if (hexa_rv3032_init() != HEXA_OK) {
                xprintf("rv3032 init FAIL\n");
                while (1) {}
        }

        bool valid;
        if (hexa_rv3032_time_valid(&valid) == HEXA_OK && !valid) {
                /* Часы потеряли питание — выставляем время сборки как
                 * заглушку. В реальном приложении время приходит извне. */
                xprintf("rv3032: time lost, setting default\n");

                const hexa_datetime_t init = {
                        .sec = 0, .min = 0, .hour = 12,
                        .wday = 1,
                        .day = 1, .month = 1, .year = 2026,
                };

                if (hexa_rv3032_set_time(&init) != HEXA_OK)
                        xprintf("rv3032 set_time FAIL\n");
        }

        while (1) {
                hexa_datetime_t now;
                int32_t t_c100;

                if (hexa_rv3032_get_time(&now) == HEXA_OK) {
                        xprintf("%04u-%02u-%02u %02u:%02u:%02u",
                                now.year, now.month, now.day,
                                now.hour, now.min, now.sec);
                } else {
                        xprintf("rv3032 read FAIL");
                }

                if (hexa_rv3032_get_temp(&t_c100) == HEXA_OK) {
                        xprintf("  temp=");
                        print_c100(t_c100);
                        xprintf(" C");
                }

                xprintf("\n");

                hexa_delay_ms(1000);
        }
}
