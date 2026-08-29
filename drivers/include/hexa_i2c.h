#ifndef HEXA_I2C_H
#define HEXA_I2C_H

#include <stdint.h>
#include <mik32_hal_i2c.h>

#include "hexa_types.h"

/* Шина I2C в режиме мастера — общая для всех датчиков платы.
 * Адреса везде 7-битные (0x00..0x7F), сдвигать их не нужно.
 * Под FreeRTOS доступ к шине защищён мьютексом, так что вызывать
 * эти функции из разных задач безопасно. */

/* Настраивает контроллер и (под FreeRTOS) создаёт мьютекс шины.
 * Обычно отдельно не вызывается — это делает hexa_board_init(). */
hexa_status_t hexa_i2c_init(void);

/* Регистровый доступ: сначала передаётся адрес регистра, потом данные.
 * Так устроено большинство датчиков. */
hexa_status_t hexa_i2c_write(uint8_t addr, uint8_t reg, const uint8_t *buf, uint16_t len);
hexa_status_t hexa_i2c_read(uint8_t addr, uint8_t reg, uint8_t *buf, uint16_t len);

/* Голая передача без адреса регистра: для устройств с командной
 * моделью вместо регистровой (AHT20) и для выгрузки кадра в OLED. */
hexa_status_t hexa_i2c_write_raw(uint8_t addr, const uint8_t *buf, uint16_t len);
hexa_status_t hexa_i2c_read_raw(uint8_t addr, uint8_t *buf, uint16_t len);

/* HEXA_OK, если по адресу кто-то откликнулся. */
hexa_status_t hexa_i2c_ping(uint8_t addr);

/* Обходит все 128 адресов. found — массив на 128 байт, в него
 * выставляются единицы по найденным адресам. Возвращает их количество.
 * Первое, чем стоит проверять новый датчик. */
int           hexa_i2c_scan(uint8_t *found);


#endif // HEXA_I2C_H
