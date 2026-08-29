#ifndef HEXA_BOARD_H
#define HEXA_BOARD_H

#include "hexa_types.h"

/* Описание конкретной платы: раскладка выводов, адреса на шине,
 * параметры периферии. Драйверы ничего этого не знают и берут всё
 * отсюда — под другую плату правится только этот файл и hexa_board.c. */

// Информация о ревизии платы
#define BOARD_REV_MAJOR 0
#define BOARD_REV_MINOR 1

// Раздел I2C
#define HEXA_I2C_INSTANCE       I2C_1
#define HEXA_I2C_TIMEOUT_MS     100     /* сколько ждать ответа от устройства */

/* Адреса на шине, 7-битные. Совпадать не должны — если вешаешь новый
 * датчик, сверься со списком и прогони hexa_i2c_scan(). */
#define HEXA_OLED_ADDR          0x3C
#define HEXA_MPU_ADDR           0x68
#define HEXA_RV3032_ADDR        0x51
#define HEXA_VL53L0X_ADDR       0x29
#define HEXA_AHT20_ADDR         0x38
#define HEXA_BMP280_ADDR        0x76    /* 0x77, если SDO подтянут к VDD */

// Раздел кнопок
/* Должно совпадать с числом элементов hexa_pins_buttons[] в hexa_board.c */
#define BOARD_BTN_COUNT 5

// Раздел UART
#define HEXA_UART_INSTANCE      UART_0

// Раздел OLED
#define HEXA_OLED_WIDTH     128
#define HEXA_OLED_HEIGHT     64
#define FB_OFFSET       1       /* нулевой байт буфера занят командой SSD1306 */


/* Раскладка выводов, определена в hexa_board.c */
extern const hexa_pin_t hexa_pin_buzzer;
extern const hexa_pin_t hexa_pins_buttons[BOARD_BTN_COUNT];

/* Поднимает I2C и UART и подключает UART к xprintf.
 * Зовётся первой строкой app_main(); отдельно инициализировать шину
 * или отладочный вывод после этого не нужно.
 *
 * Датчики этот вызов не трогает — каждый поднимается своим init. */
hexa_status_t hexa_board_init(void);

#endif // HEXA_BOARD_H
