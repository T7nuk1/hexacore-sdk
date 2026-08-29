#ifndef HEXA_AHT20_H
#define HEXA_AHT20_H

#include <stdint.h>
#include <stdbool.h>

#include "hexa_types.h"

/* ASAIR AHT20 — температура и относительная влажность.
 * На модуле GY-BMP280+AHT20 живёт на той же шине, что и BMP280. */

hexa_status_t hexa_aht20_init(void);

/* Запустить преобразование. Результат готов примерно через 80 мс. */
hexa_status_t hexa_aht20_start(void);
hexa_status_t hexa_aht20_ready(bool *ready);

/* t_c100 — сотые доли °C, rh_c100 — сотые доли %RH. Любой указатель
 * может быть NULL, если значение не нужно. */
hexa_status_t hexa_aht20_get(int32_t *t_c100, uint32_t *rh_c100);

/* start + опрос готовности + get. Блокирует вызывающего до ~100 мс. */
hexa_status_t hexa_aht20_measure(int32_t *t_c100, uint32_t *rh_c100);

#endif // HEXA_AHT20_H
