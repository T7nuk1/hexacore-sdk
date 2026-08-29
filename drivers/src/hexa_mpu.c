#include <stdint.h>
#include <string.h>

#include "hexa_board.h"
#include "hexa_mpu.h"
#include "hexa_i2c.h"
#include "hexa_time.h"


hexa_status_t hexa_mpu_init(void)
{
        hexa_status_t res;
        uint8_t val;

        val = 0x80; /* PWR_MGMT_1: DEVICE_RESET */
        res = hexa_i2c_write(HEXA_MPU_ADDR, 0x6B, &val, 1);
        if (res != HEXA_OK) return res;

        hexa_delay_ms(100);

        val = 0x01; /* PWR_MGMT_1: wake, CLKSEL=1 (PLL X-gyro) */
        res = hexa_i2c_write(HEXA_MPU_ADDR, 0x6B, &val, 1);
        if (res != HEXA_OK) return res;

        val = 0x00; /* PWR_MGMT_2: all axes active */
        res = hexa_i2c_write(HEXA_MPU_ADDR, 0x6C, &val, 1);
        if (res != HEXA_OK) return res;

        return HEXA_OK;
}

hexa_status_t hexa_mpu_whoami(uint8_t *id)
{
        if (id == NULL) {
                return HEXA_ERR_PARAM;
        }

        hexa_status_t res = hexa_i2c_read(HEXA_MPU_ADDR, 0x75, id, 1);

        return res;
}

hexa_status_t hexa_mpu_read_accel(int16_t *x, int16_t *y, int16_t *z)
{
        if (x == NULL || y == NULL || z == NULL)
                return HEXA_ERR_PARAM;

        uint8_t buf[6];

        hexa_status_t res = hexa_i2c_read(HEXA_MPU_ADDR, 0x3B, buf, 6);

        if (res != HEXA_OK) return res;

        *x = (int16_t)((buf[0] << 8) | buf[1]);
        *y = (int16_t)((buf[2] << 8) | buf[3]);
        *z = (int16_t)((buf[4] << 8) | buf[5]);

        return HEXA_OK;
}

hexa_status_t hexa_mpu_read_gyro(int16_t *gx, int16_t *gy, int16_t *gz)
{
        if (gx == NULL || gy == NULL || gz == NULL)
                return HEXA_ERR_PARAM;

        uint8_t buf[6];

        hexa_status_t res = hexa_i2c_read(HEXA_MPU_ADDR, 0x43, buf, 6);

        if (res != HEXA_OK) return res;

        *gx = (int16_t)((buf[0] << 8) | buf[1]);
        *gy = (int16_t)((buf[2] << 8) | buf[3]);
        *gz = (int16_t)((buf[4] << 8) | buf[5]);

        return HEXA_OK;
}
