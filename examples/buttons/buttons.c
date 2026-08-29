#define MIK32V2

#include <mik32_hal.h>
#include <mik32_hal_pcc.h>
#include <mik32_hal_gpio.h>
#include "hexa_button.h"
#include "hexa_time.h"

#define TIMER_OCR 32000


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


void app_main(void)
{
        HAL_Init();
        system_clock_config();
        hexa_time_init();
        GPIO_Init();
        hexa_buttons_init();

        while (1) {
                hexa_button_scan();
                if (hexa_button_down(1)) {
                        HAL_GPIO_WritePin(GPIO_2, GPIO_PIN_7, 1);
                } else {
                    HAL_GPIO_WritePin(GPIO_2, GPIO_PIN_7, 0);
                }

                hexa_delay_ms(20);
        }
}
