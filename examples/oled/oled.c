#include "hexa_board.h"
#include "hexa_oled.h"
#include "xprintf.h"
#include "FreeRTOS.h"
#include "task.h"
#include <stdint.h>


void app_main(void)
{
        hexa_board_init();

        if (hexa_oled_init() != HEXA_OK) {
                xprintf("oled init FAIL\n");
                vTaskDelete(NULL);
        }

        hexa_oled_clear();
        hexa_oled_print("hexacore v0.0.1", 10, 10, 1);
        hexa_oled_flush();

        /* Кадр отрисован и больше не меняется. Задаче здесь делать
         * нечего — засыпаем, чтобы не отбирать процессор у остальных. */
        while (1) {
                vTaskDelay(portMAX_DELAY);
        }
}
