#include <stdint.h>

#if SDK_FREERTOS
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#endif

#include "hexa_i2c.h"
#include "hexa_types.h"
#include "hexa_board.h"


static I2C_HandleTypeDef s_bus;

#if SDK_FREERTOS
        static SemaphoreHandle_t s_i2c_mutex = NULL;
        static inline void i2c_lock(void) { xSemaphoreTake(s_i2c_mutex, portMAX_DELAY); }
        static inline void i2c_unlock(void) { xSemaphoreGive(s_i2c_mutex); }
#else
        static inline void i2c_lock(void) {}
        static inline void i2c_unlock(void) {}
#endif

static hexa_status_t i2c_map_error(HAL_I2C_ErrorTypeDef e)
{
        switch (e) {
                case I2C_ERROR_NONE:
                        return HEXA_OK;
                case I2C_ERROR_TIMEOUT:
                        return HEXA_ERR_TIMEOUT;
                case I2C_ERROR_NACK:
                        return HEXA_ERR_NACK;
                default:
                        return HEXA_ERR_BUS;
        }
}


hexa_status_t hexa_i2c_init(void)
{
        s_bus.Instance = HEXA_I2C_INSTANCE;

        s_bus.Init.Mode = HAL_I2C_MODE_MASTER;

        s_bus.Init.DigitalFilter = I2C_DIGITALFILTER_OFF;
        s_bus.Init.AnalogFilter = I2C_ANALOGFILTER_DISABLE;
        s_bus.Init.AutoEnd = I2C_AUTOEND_ENABLE;

        s_bus.Clock.PRESC  = 1;
        s_bus.Clock.SCLDEL = 15;
        s_bus.Clock.SDADEL = 15;
        s_bus.Clock.SCLH   = 75;
        s_bus.Clock.SCLL   = 75;

        if (HAL_I2C_Init(&s_bus) != HAL_OK) {
                return HEXA_ERR_BUS;
        }
#if SDK_FREERTOS
        s_i2c_mutex = xSemaphoreCreateMutex();
        if (s_i2c_mutex == NULL) {
                return HEXA_ERR_BUS;
        }
#endif
        return HEXA_OK;
}

hexa_status_t hexa_i2c_read(uint8_t addr, uint8_t reg, uint8_t *buf, uint16_t len)
{
        hexa_status_t st;

        i2c_lock();

        HAL_StatusTypeDef res = HAL_I2C_Master_Transmit(&s_bus, addr, &reg, 1, HEXA_I2C_TIMEOUT_MS);
        if (res != HAL_OK) {
                st = i2c_map_error(s_bus.ErrorCode);
        } else {
                res = HAL_I2C_Master_Receive(&s_bus, addr, buf, len, HEXA_I2C_TIMEOUT_MS);
                st = (res == HAL_OK) ? HEXA_OK : i2c_map_error(s_bus.ErrorCode);
        }

        i2c_unlock();
        return st;
}


/* Чтение без адреса регистра. Нужно устройствам без регистровой модели —
 * например AHT20 отдаёт результат измерения голым чтением после команды. */
hexa_status_t hexa_i2c_read_raw(uint8_t addr, uint8_t *buf, uint16_t len)
{
        i2c_lock();

        HAL_StatusTypeDef res = HAL_I2C_Master_Receive(&s_bus, addr, buf, len, HEXA_I2C_TIMEOUT_MS);
        hexa_status_t st = (res == HAL_OK) ? HEXA_OK : i2c_map_error(s_bus.ErrorCode);

        i2c_unlock();

        return st;
}


hexa_status_t hexa_i2c_write(uint8_t addr, uint8_t reg, const uint8_t *buf, uint16_t len)
{
        uint8_t tmp[32];

        if (len > sizeof(tmp) - 1)
                return HEXA_ERR_PARAM;

        tmp[0] = reg;
        for (uint8_t i = 1; i <= len; i++) {
                tmp[i] = buf[i - 1];
        }

        i2c_lock();

        HAL_StatusTypeDef res = HAL_I2C_Master_Transmit(&s_bus, addr, tmp, len+1, HEXA_I2C_TIMEOUT_MS);
        hexa_status_t st = (res == HAL_OK) ? HEXA_OK : i2c_map_error(s_bus.ErrorCode);

        i2c_unlock();

        return st;
}


hexa_status_t hexa_i2c_write_raw(uint8_t addr, const uint8_t *buf, uint16_t len)
{
        i2c_lock();

        HAL_StatusTypeDef res = HAL_I2C_Master_Transmit(&s_bus, addr, (uint8_t*)buf, len, HEXA_I2C_TIMEOUT_MS);
        hexa_status_t st = (res == HAL_OK) ? HEXA_OK : i2c_map_error(s_bus.ErrorCode);

        i2c_unlock();

        return st;
}

hexa_status_t hexa_i2c_ping(uint8_t addr)
{
        i2c_lock();
        HAL_StatusTypeDef res = HAL_I2C_Master_Transmit(&s_bus, addr, NULL, 0, HEXA_I2C_TIMEOUT_MS);

        hexa_status_t st = (res == HAL_OK) ? HEXA_OK : i2c_map_error(s_bus.ErrorCode);

        i2c_unlock();

        return st;

}

int hexa_i2c_scan(uint8_t *found)
{
        i2c_lock();
        uint8_t counter = 0;

        for (uint8_t i = 0; i < 0x80; i++) {
                found[i] = 0;
        }

        for (uint8_t i = 0; i<0x80; i++) {
                uint8_t dummy;
                if (HAL_I2C_Master_Receive(&s_bus, i, &dummy, 1, 5) == HAL_OK) {
                        counter += 1;
                        found[i] = 1;
                }
        }
        i2c_unlock();
        return counter;
}
