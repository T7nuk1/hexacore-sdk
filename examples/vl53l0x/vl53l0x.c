#include <stdint.h>

#include "hexa_board.h"
#include "hexa_vl53l0x.h"
#include "hexa_time.h"
#include "xprintf.h"


void app_main(void)
{
        hexa_board_init();

        uint8_t id;
        hexa_vl53l0x_whoami(&id);
        xprintf("vl53l0x model id = 0x%02x (ожидается 0xee)\n", id);

        if (hexa_vl53l0x_init() != HEXA_OK) {
                xprintf("vl53l0x init FAIL\n");
                while (1) {}
        }

        while (1) {
                uint16_t mm;

                switch (hexa_vl53l0x_measure(&mm)) {
                case HEXA_OK:
                        xprintf("range = %u mm\n", mm);
                        break;
                case HEXA_ERR:
                        /* Чип отработал, но забраковал результат:
                         * цель вне диапазона или слишком слабый отклик. */
                        xprintf("range = out of range (raw %u)\n", mm);
                        break;
                default:
                        xprintf("range read FAIL\n");
                        break;
                }

                hexa_delay_ms(200);
        }
}
