#ifndef HEXA_OLED_H
#define HEXA_OLED_H

#include "stdint.h"

#include "hexa_types.h"
#include "hexa_i2c.h"

/* Монохромный OLED на SSD1306, размер задан HEXA_OLED_WIDTH/HEIGHT.
 *
 * Рисование идёт в буфер кадра в оперативной памяти, а не сразу в
 * дисплей. Пока не вызван hexa_oled_flush(), на экране ничего не
 * меняется. Отсюда обычный порядок работы:
 *
 *      hexa_oled_clear();
 *      hexa_oled_print("hello", 0, 0, 1);
 *      hexa_oled_flush();
 *
 * Координаты: x вправо, y вниз, начало в левом верхнем углу.
 * color: 0 — погасить, 1 — зажечь, любое другое значение — инвертировать.
 */

hexa_status_t hexa_oled_init(void);

/* Шрифт 5x7, шаг между символами 6 пикселей. Строка обрезается по
 * правому краю экрана. Непечатаемые символы и кириллица выводятся
 * пробелом — в шрифте только ASCII 0x20..0x7E. */
hexa_status_t hexa_oled_print(const char* str, uint8_t x, uint8_t y, uint8_t color);

hexa_status_t hexa_oled_pixel(uint8_t x, uint8_t y, uint8_t color);

/* Гасит буфер кадра. На экран не влияет до flush. */
hexa_status_t hexa_oled_clear(void);

/* Выгружает буфер кадра в дисплей — единственное место, где картинка
 * реально обновляется. Передаёт килобайт по шине, так что дёргать это
 * чаще, чем нужно для анимации, смысла нет. */
hexa_status_t hexa_oled_flush(void);

#endif // HEXA_OLED_H
