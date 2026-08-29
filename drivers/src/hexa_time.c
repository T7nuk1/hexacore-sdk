#include "hexa_time.h"

#if SDK_FREERTOS
#include "FreeRTOS.h"
#include "task.h"

void hexa_time_init(void)
{
        /* FreeRTOS owns the SCR1 mtime timer as its tick source; nothing to
         * init here. HAL_Time_SCR1TIM_Init must NOT be called — it zeroes
         * mtime. */
}

void hexa_delay_ms(uint32_t ms)
{
        vTaskDelay(pdMS_TO_TICKS(ms));
}

#else
#include "mik32_hal_scr1_timer.h"

void hexa_time_init(void)
{
        HAL_Time_SCR1TIM_Init();
}

void hexa_delay_ms(uint32_t ms)
{
        HAL_Time_SCR1TIM_DelayMs(ms);
}

#endif
