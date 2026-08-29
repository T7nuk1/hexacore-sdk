#include <stdint.h>

#include "hexa_board.h"
#include "hexa_aht20.h"
#include "hexa_bmp280.h"
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

        uint8_t id;
        hexa_bmp280_whoami(&id);
        xprintf("bmp280 chip id = 0x%02x (ожидается 0x58)\n", id);

        if (hexa_bmp280_init() != HEXA_OK)
                xprintf("bmp280 init FAIL\n");

        if (hexa_aht20_init() != HEXA_OK)
                xprintf("aht20 init FAIL\n");

        while (1) {
                int32_t aht_t, bmp_t;
                uint32_t rh, pa;

                if (hexa_aht20_measure(&aht_t, &rh) == HEXA_OK) {
                        xprintf("aht20:  t=");
                        print_c100(aht_t);
                        xprintf(" C  rh=");
                        print_c100((int32_t)rh);
                        xprintf(" %%\n");
                } else {
                        xprintf("aht20:  FAIL\n");
                }

                if (hexa_bmp280_measure(&bmp_t, &pa) == HEXA_OK) {
                        xprintf("bmp280: t=");
                        print_c100(bmp_t);
                        xprintf(" C  p=%u Pa\n", pa);
                } else {
                        xprintf("bmp280: FAIL\n");
                }

                hexa_delay_ms(1000);
        }
}
