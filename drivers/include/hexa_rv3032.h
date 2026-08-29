#ifndef HEXA_RV3032_H
#define HEXA_RV3032_H

#include <stdint.h>
#include <stdbool.h>

#include "hexa_types.h"

/* Micro Crystal RV-3032-C7 — внешние часы реального времени.
 * Не путать с внутренним RTC MIK32 (mik32_hal_rtc.h). */

typedef struct {
        uint8_t  sec;           /* 0..59 */
        uint8_t  min;           /* 0..59 */
        uint8_t  hour;          /* 0..23 */
        uint8_t  wday;          /* 0..6 — диапазон регистра чипа; какой день
                                 * считать нулевым, решает приложение */
        uint8_t  day;           /* 1..31 */
        uint8_t  month;         /* 1..12 */
        uint16_t year;          /* 2000..2099 */
} hexa_datetime_t;

hexa_status_t hexa_rv3032_init(void);

/* Часы держат время от собственного питания. После первой подачи питания
 * содержимое регистров случайно — проверяй valid перед доверием времени. */
hexa_status_t hexa_rv3032_time_valid(bool *valid);

hexa_status_t hexa_rv3032_get_time(hexa_datetime_t *dt);
hexa_status_t hexa_rv3032_set_time(const hexa_datetime_t *dt);

/* Встроенный термодатчик, сотые доли °C. Разрешение чипа — 0.0625 °C. */
hexa_status_t hexa_rv3032_get_temp(int32_t *t_c100);

#endif // HEXA_RV3032_H
