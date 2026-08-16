#define MIK32V2
#include "mik32_hal.h"
#include "mik32_hal_pcc.h"
#include "mik32_hal_scr1_timer.h"

#include "FreeRTOS.h"
#include "task.h"

extern void app_main(void);

#define APP_MAIN_STACK 512
#define APP_MAIN_PRIO   (tskIDLE_PRIORITY + 1)

static void SystemClock_Config(void)
{
        PCC_InitTypeDef PCC_OscInit = {0};

        PCC_OscInit.OscillatorEnable = PCC_OSCILLATORTYPE_ALL;
        PCC_OscInit.FreqMon.OscillatorSystem = PCC_OSCILLATORTYPE_OSC32M;
        PCC_OscInit.FreqMon.ForceOscSys = PCC_FORCE_OSC_SYS_UNFIXED;
        PCC_OscInit.FreqMon.Force32KClk = PCC_FREQ_MONITOR_SOURCE_OSC32K;
        PCC_OscInit.AHBDivider = 0;
        PCC_OscInit.APBMDivider = 0;
        PCC_OscInit.APBPDivider = 0;
        PCC_OscInit.HSI32MCalibrationValue = 128;
        PCC_OscInit.LSI32KCalibrationValue = 8;
        PCC_OscInit.RTCClockSelection = PCC_RTC_CLOCK_SOURCE_AUTO;
        PCC_OscInit.RTCClockCPUSelection = PCC_CPU_RTC_CLOCK_SOURCE_OSC32K;
        HAL_PCC_Config(&PCC_OscInit);
}

static void app_main_task(void *arg)
{
        (void)arg;
        app_main();
        vTaskDelete(NULL);
}

int main(void)
{
        HAL_Init();
        SystemClock_Config();

        /* SCR1 timer — источник mtime для FreeRTOS tick.
         * Обязательно до vTaskStartScheduler: порт программирует
         * mtimecmp исходя из configCPU_CLOCK_HZ при старте. */
        __HAL_SCR1_TIMER_SET_DIVIDER(0);
        __HAL_SCR1_TIMER_ENABLE();

        xTaskCreate(app_main_task, "app_main",
                APP_MAIN_STACK, NULL, APP_MAIN_PRIO, NULL);
        vTaskStartScheduler();
        for (;;) {}
}
