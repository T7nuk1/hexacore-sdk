#define MIK32V2
#include "mik32_hal.h"
#include "mik32_hal_pcc.h"
#include "mik32_hal_gpio.h"

#include "FreeRTOS.h"
#include "task.h"

#define LED_PIN  GPIO_PIN_7
#define LED_PORT GPIO_2

static void GPIO_Init(void)
{
	GPIO_InitTypeDef GPIO_InitStruct = {0};

	__HAL_PCC_GPIO_2_CLK_ENABLE();

	GPIO_InitStruct.Pin  = LED_PIN;
	GPIO_InitStruct.Mode = HAL_GPIO_MODE_GPIO_OUTPUT;
	GPIO_InitStruct.Pull = HAL_GPIO_PULL_NONE;
	HAL_GPIO_Init(LED_PORT, &GPIO_InitStruct);
}

static void blink_task(void *arg)
{
	(void)arg;
	for (;;)
	{
		HAL_GPIO_TogglePin(LED_PORT, LED_PIN);
		vTaskDelay(pdMS_TO_TICKS(500));
	}
}

void app_main(void)
{
	GPIO_Init();

	xTaskCreate(blink_task, "blink",
	            256, NULL, tskIDLE_PRIORITY + 1, NULL);
}
