#include "xprintf.h"

#include "hexa_board.h"
#include "hexa_i2c.h"
#include "hexa_uart.h"

// TODO: Проверить итоговую раскладку
const hexa_pin_t hexa_pins_buttons[BOARD_BTN_COUNT] = {
        { GPIO_0, GPIO_PIN_3 },
        { GPIO_1, GPIO_PIN_3 },
        { GPIO_1, GPIO_PIN_1 },
        { GPIO_1, GPIO_PIN_0 },
        { GPIO_1, GPIO_PIN_2 }
};

const hexa_pin_t hexa_pin_buzzer = { GPIO_0, GPIO_PIN_2 };

static void uart_out(unsigned char c) { hexa_uart_putc((char)c); }
static unsigned char uart_in(void)    { return (unsigned char)hexa_uart_getc(); }

hexa_status_t hexa_board_init(void)
{
        hexa_i2c_init();
        hexa_uart_init();
        xdev_out(uart_out);
        xdev_in(uart_in);
        return HEXA_OK;
}
