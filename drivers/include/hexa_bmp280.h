#ifndef HEXA_BMP280_H
#define HEXA_BMP280_H

#include <stdint.h>
#include <stdbool.h>

#include "hexa_types.h"

/* Bosch BMP280 — давление и температура.
 * Драйвер держит чип в forced mode: одно измерение по запросу, между
 * измерениями сон. Так проще согласовать с моделью start/ready/get и
 * дешевле по питанию, чем normal mode. */

hexa_status_t hexa_bmp280_init(void);
hexa_status_t hexa_bmp280_whoami(uint8_t *id);   /* 0x58 для BMP280 */

/* Запустить одно преобразование. Готово примерно через 10 мс. */
hexa_status_t hexa_bmp280_start(void);
hexa_status_t hexa_bmp280_ready(bool *ready);

/* t_c100 — сотые доли °C, p_pa — давление в паскалях.
 * Температуру драйвер считает всегда: компенсация давления зависит от
 * неё. Любой указатель может быть NULL. */
hexa_status_t hexa_bmp280_get(int32_t *t_c100, uint32_t *p_pa);

/* start + опрос готовности + get. Блокирует вызывающего до ~50 мс. */
hexa_status_t hexa_bmp280_measure(int32_t *t_c100, uint32_t *p_pa);

#endif // HEXA_BMP280_H
