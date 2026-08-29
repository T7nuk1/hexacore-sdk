#ifndef HEXA_TYPES_H
#define HEXA_TYPES_H

#include <mik32_hal_gpio.h>

/* Общий код возврата всех функций SDK. Проверять стоит на равенство
 * HEXA_OK: остальные значения нужны, только когда важна причина отказа. */
typedef enum {
        HEXA_OK = 0,
        HEXA_ERR_TIMEOUT,       /* устройство не ответило за отведённое время */
        HEXA_ERR_NACK,          /* адрес на шине никто не подтвердил */
        HEXA_ERR_BUS,           /* сбой шины или периферии */
        HEXA_ERR_PARAM,         /* некорректный аргумент, до железа не дошло */
        HEXA_ERR,               /* прочее: чип ответил, но не тем, чем ожидали */
} hexa_status_t;

/* Вывод МК как пара "порт + пин". Раскладка платы задаётся такими
 * структурами в hexa_board.c, драйверы её не хардкодят. */
typedef struct {
        GPIO_TypeDef *port;
        HAL_PinsTypeDef pin;
} hexa_pin_t;

#endif // HEXA_TYPES_H
