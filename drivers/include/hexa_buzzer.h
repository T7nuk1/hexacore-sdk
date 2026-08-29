#ifndef HEXA_BUZZER_H
#define HEXA_BUZZER_H

#include "stdint.h"

#include "hexa_types.h"

/* Пищалка на аппаратном ШИМ (TIMER32_1, канал 2), скважность 50%.
 * Таймер занят целиком — для других задач он недоступен. */

hexa_status_t hexa_buzzer_init(void);

/* Включает звук частотой tone в герцах и сразу возвращает управление:
 * звучание продолжается, пока не позовут hexa_buzzer_off().
 * Длительность ноты отмеряет вызывающий через hexa_delay_ms().
 * tone == 0 глушит звук и возвращает HEXA_ERR_PARAM. */
hexa_status_t hexa_buzzer_tone(uint16_t tone);

hexa_status_t hexa_buzzer_off(void);

#endif // HEXA_BUZZER_H
