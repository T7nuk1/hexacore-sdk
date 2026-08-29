#include <stdbool.h>
#include <stdint.h>

#include "mik32_hal_gpio.h"
#include "mik32_hal_pcc.h"
#include "csr.h"
#include "scr1_csr_encoding.h"

#if SDK_FREERTOS
        #include "FreeRTOS.h"
        #include "task.h"
        #define HEXA_CRIT_ENTER()       taskENTER_CRITICAL()
        #define HEXA_CRIT_EXIT()       taskEXIT_CRITICAL()
#else
        #define HEXA_CRIT_ENTER()       uint32_t _mstatus = read_csr(mstatus); clear_csr(mstatus, MSTATUS_MIE)
        #define HEXA_CRIT_EXIT()        write_csr(mstatus, _mstatus)
#endif

#include "hexa_button.h"

#define DEBOUNCE_MAX 4

static uint8_t integ[BOARD_BTN_COUNT];
static uint8_t state;
static uint8_t pressed;

void hexa_button_scan(void)
{
        for (uint8_t i = 0; i < BOARD_BTN_COUNT; i++) {
                bool down = (HAL_GPIO_ReadPin(hexa_pins_buttons[i].port,
                                              hexa_pins_buttons[i].pin) != 0);

                if (down) {
                        if (integ[i] < DEBOUNCE_MAX) integ[i]++;
                } else {
                        if (integ[i] > 0) integ[i]--;
                }

                uint8_t mask = (1u << i);
                if (integ[i] == DEBOUNCE_MAX) {
                        if (!(state & mask)) {
                                state |= mask;
                                pressed |= mask;
                        }
                } else if (integ[i] == 0) {
                        state &= ~mask;
                }
        }
}

hexa_status_t hexa_buttons_init(void)
{
        __HAL_PCC_GPIO_0_CLK_ENABLE();
        __HAL_PCC_GPIO_1_CLK_ENABLE();

        GPIO_InitTypeDef gpio_init_struct = { 0 };
        gpio_init_struct.Mode = HAL_GPIO_MODE_GPIO_INPUT;
        gpio_init_struct.Pull = HAL_GPIO_PULL_NONE;

        for (uint8_t i = 0; i < BOARD_BTN_COUNT; i++ ) {
                gpio_init_struct.Pin = hexa_pins_buttons[i].pin;

                HAL_GPIO_Init(hexa_pins_buttons[i].port, &gpio_init_struct);
        }

        return HEXA_OK;
}

bool hexa_button_down(uint8_t n)
{
        if (n >= BOARD_BTN_COUNT) return false;
        return (state >> n) & 1u;
}

bool hexa_button_pressed(uint8_t n)
{
        if (n >= BOARD_BTN_COUNT) return false;
        uint8_t mask = (1u << n);

        HEXA_CRIT_ENTER();
        bool p = (pressed & mask) != 0;
        pressed &= ~mask;
        HEXA_CRIT_EXIT();
        return p;
}
