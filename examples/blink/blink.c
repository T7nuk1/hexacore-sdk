#define MIK32V2

#include <mik32_hal.h>
#include <mik32_hal_pcc.h>
#include <mik32_hal_gpio.h>
#include <mik32_hal_scr1_timer.h>


#define PIN_LED 7
#define TIMER_OCR 32000

volatile unsigned int millis = 0;

void system_clock_config(void)
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

void GPIO_Init(void)
{
        GPIO_InitTypeDef GPIO_InitStruct = {0};

        __HAL_PCC_GPIO_2_CLK_ENABLE();

        GPIO_InitStruct.Pin = GPIO_PIN_7;
        GPIO_InitStruct.Mode = HAL_GPIO_MODE_GPIO_OUTPUT;
        GPIO_InitStruct.Pull = HAL_GPIO_PULL_NONE;
        HAL_GPIO_Init(GPIO_2, &GPIO_InitStruct);
        HAL_GPIO_WritePin(GPIO_2, GPIO_InitStruct.Pin, __LOW);
}

void trap_handler(void)
{
        millis++;

        __HAL_SCR1_TIMER_SET_TIME(0);
        __HAL_SCR1_TIMER_SET_CMP(TIMER_OCR);
}

void app_main(void)
{
        HAL_Init();
        system_clock_config();
        GPIO_Init();

        HAL_SCR1_Timer_Init(HAL_SCR1_TIMER_CLKSRC_INTERNAL, 0);
        __HAL_SCR1_TIMER_SET_CMP(TIMER_OCR);
        __HAL_SCR1_TIMER_IRQ_ENABLE();

        unsigned int last_toggle = millis;

        while (1) {
                if (millis - last_toggle >= 1000) {
                        HAL_GPIO_TogglePin(GPIO_2, GPIO_PIN_7);
                        last_toggle = millis;
                }
        }
}
