#ifndef HEXA_TIME_H
#define HEXA_TIME_H

#include <stdint.h>

/* Единая задержка для обоих режимов SDK. Драйверы зовут только её,
 * поэтому один и тот же код работает и в bare metal, и под FreeRTOS. */

/* В bare metal поднимает таймер SCR1. Под FreeRTOS не делает ничего:
 * тем же таймером ядро считает свои тики, и трогать его нельзя.
 * Вызывать всё равно нужно — чтобы код не зависел от режима сборки. */
void hexa_time_init(void);

/* bare metal — активное ожидание, ядро занято.
 * FreeRTOS — vTaskDelay, задача спит и отдаёт процессор другим. */
void hexa_delay_ms(uint32_t ms);

#endif // HEXA_TIME_H
