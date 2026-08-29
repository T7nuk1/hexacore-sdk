#ifndef HEXA_BUTTON_H
#define HEXA_BUTTON_H

#include <stdint.h>
#include <stdbool.h>

#include "hexa_types.h"
#include "hexa_board.h"

/* Кнопки платы. Номер n — индекс в hexa_pins_buttons[] из hexa_board.c,
 * считая с нуля; всего BOARD_BTN_COUNT штук.
 *
 * Дребезг подавляется счётчиком: состояние меняется только после
 * четырёх подряд одинаковых опросов. Поэтому весь драйвер держится на
 * регулярном вызове hexa_button_scan(). */

hexa_status_t hexa_buttons_init(void);

/* Опрашивает все кнопки и обновляет их состояние.
 * Вызывать с периодичностью 1-10 мс: реже — кнопки будут тормозить,
 * чаще — дребезг перестанет подавляться. */
void hexa_button_scan(void);

/* Кнопка нажата прямо сейчас. Пока держат — возвращает true. */
bool hexa_button_down(uint8_t n);

/* Кнопку нажали с прошлого вызова. Возвращает true ровно один раз на
 * каждое нажатие: факт события сбрасывается при чтении. */
bool hexa_button_pressed(uint8_t n);

#endif // HEXA_BUTTON_H
