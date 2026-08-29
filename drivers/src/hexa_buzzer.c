#include <mik32_hal_timer32.h>

#include "hexa_buzzer.h"


static TIMER32_HandleTypeDef htimer32_1;
static TIMER32_CHANNEL_HandleTypeDef htimer32_channel2;

static hexa_status_t timer32_init(void)
{
        htimer32_1.Instance = TIMER32_1;
        htimer32_1.Top = 7543;
        htimer32_1.State = TIMER32_STATE_DISABLE;
        htimer32_1.Clock.Source = TIMER32_SOURCE_PRESCALER;
        htimer32_1.Clock.Prescaler = 0;
        htimer32_1.InterruptMask = 0;
        htimer32_1.CountMode = TIMER32_COUNTMODE_FORWARD;

        if (HAL_Timer32_Init(&htimer32_1) != HAL_OK) {
                return HEXA_ERR;
        }

        htimer32_channel2.TimerInstance = htimer32_1.Instance;
        htimer32_channel2.ChannelIndex = TIMER32_CHANNEL_2;
        htimer32_channel2.PWM_Invert = TIMER32_CHANNEL_NON_INVERTED_PWM;
        htimer32_channel2.Mode = TIMER32_CHANNEL_MODE_PWM;
        htimer32_channel2.CaptureEdge = TIMER32_CHANNEL_CAPTUREEDGE_RISING;
        htimer32_channel2.OCR = 7544 >> 1;
        htimer32_channel2.Noise = TIMER32_CHANNEL_FILTER_OFF;

        if (HAL_Timer32_Channel_Init(&htimer32_channel2) != HAL_OK) {
                return HEXA_ERR;
        }

        HAL_Timer32_Channel_OCR_Set(&htimer32_channel2, 0);
        HAL_Timer32_Channel_Enable(&htimer32_channel2);
        HAL_Timer32_Value_Clear(&htimer32_1);
        HAL_Timer32_Start(&htimer32_1);

        return HEXA_OK;
}


hexa_status_t hexa_buzzer_init(void)
{
        return timer32_init();
}


hexa_status_t hexa_buzzer_tone(uint16_t tone)
{
        if (tone == 0) {
                hexa_buzzer_off();
                return HEXA_ERR_PARAM;
        }

        uint32_t top = 32000000u / tone;
        HAL_Timer32_Top_Set(&htimer32_1, top);
        HAL_Timer32_Channel_OCR_Set(&htimer32_channel2, top >> 1);
        return HEXA_OK;
}

hexa_status_t hexa_buzzer_off(void)
{
        HAL_Timer32_Channel_OCR_Set(&htimer32_channel2, 0);
        HAL_Timer32_Value_Clear(&htimer32_1);
        return HEXA_OK;
}
