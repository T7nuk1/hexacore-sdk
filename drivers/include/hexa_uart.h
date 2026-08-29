#ifndef HEXA_UART_H
#define HEXA_UART_H

#include <mik32_hal_usart.h>
#include <stdint.h>

#include "hexa_types.h"

/* Отладочный UART, 9600 8N1. Порт задаётся HEXA_UART_INSTANCE в
 * hexa_board.h.
 *
 * Напрямую эти функции почти не нужны: hexa_board_init() подключает их
 * к xprintf, после чего вывод делается через xprintf("...\n").
 * Обмен блокирующий — вызов возвращается, когда байт ушёл. */

hexa_status_t hexa_uart_init(void);
hexa_status_t hexa_uart_putc(char c);

/* Ждёт байт; при таймауте возвращает '\0'. */
char hexa_uart_getc(void);


#endif // HEXA_UART_H
