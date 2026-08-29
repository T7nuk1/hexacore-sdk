#include "hexa_board.h"
#include "hexa_mpu.h"
#include "hexa_time.h"
#include "xprintf.h"
#include <stdint.h>


void app_main(void)
{
        hexa_board_init();

        uint8_t id;
        hexa_mpu_whoami(&id);
        xprintf("mpu whoami = 0x%02x\n", id);

        if (hexa_mpu_init() != HEXA_OK) {
                xprintf("mpu init FAIL\n");
        }

        int16_t ax, ay, az, gx, gy, gz;

        while (1) {
                hexa_mpu_read_accel(&ax, &ay, &az);
                hexa_mpu_read_gyro(&gx, &gy, &gz);

                xprintf("accel: x=%d y=%d z=%d\n", ax, ay, az);
                xprintf("gyro:  x=%d y=%d z=%d\n", gx, gy, gz);

                hexa_delay_ms(500);
        }
}
