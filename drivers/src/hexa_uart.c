#include "hexa_board.h"

#include "hexa_uart.h"


static USART_HandleTypeDef uart_st = { 0 };

hexa_status_t hexa_uart_init(void)
{
        uart_st.Instance = HEXA_UART_INSTANCE;
        uart_st.transmitting = Enable;
        uart_st.receiving = Enable;
        uart_st.frame = Frame_8bit;
        uart_st.bit_direction = LSB_First;
        uart_st.stop_bit = StopBit_1;
        uart_st.mode = Asynchronous_Mode;
        uart_st.xck_mode = XCK_Mode3;
        uart_st.rts_mode = AlwaysEnable_mode;
        uart_st.channel_mode = Duplex_Mode;
        uart_st.baudrate = 9600;
        return (HAL_USART_Init(&uart_st) == HAL_OK) ? HEXA_OK : HEXA_ERR_BUS;
}

hexa_status_t hexa_uart_putc(char c)
{
        return HAL_USART_Transmit(&uart_st, c, USART_TIMEOUT_DEFAULT) ? HEXA_OK : HEXA_ERR_BUS;
}

char hexa_uart_getc(void)
{
        char buf = '\0';
        HAL_USART_Receive(&uart_st, &buf, USART_TIMEOUT_DEFAULT);
        return buf;
}
