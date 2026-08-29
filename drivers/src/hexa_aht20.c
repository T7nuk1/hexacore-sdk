#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "hexa_board.h"
#include "hexa_aht20.h"
#include "hexa_i2c.h"
#include "hexa_time.h"

#define AHT20_CMD_INIT          0xBE
#define AHT20_CMD_TRIGGER       0xAC
#define AHT20_CMD_SOFT_RESET    0xBA

#define AHT20_STATUS_BUSY       (1u << 7)
#define AHT20_STATUS_CAL        (1u << 3)

#define AHT20_POWERUP_MS        100     /* даташит: 100 мс до первой команды */
#define AHT20_INIT_MS           10
#define AHT20_RESET_MS          20
#define AHT20_MEAS_MS           80      /* типичное время преобразования */
/* Шаг опроса не должен быть меньше тика планировщика (10 мс при
 * configTICK_RATE_HZ = 100): иначе под FreeRTOS pdMS_TO_TICKS даёт ноль,
 * задержки не происходит и таймаут ниже становится враньём. */
#define AHT20_POLL_STEP_MS      10
#define AHT20_TIMEOUT_MS        200

/* status + 5 байт данных + CRC */
#define AHT20_FRAME_LEN         7

static uint8_t aht20_crc8(const uint8_t *data, uint8_t len)
{
        /* CRC-8/NRSC-5: полином 0x31, начальное значение 0xFF */
        uint8_t crc = 0xFF;

        for (uint8_t i = 0; i < len; i++) {
                crc ^= data[i];
                for (uint8_t bit = 0; bit < 8; bit++) {
                        if (crc & 0x80)
                                crc = (uint8_t)((crc << 1) ^ 0x31);
                        else
                                crc = (uint8_t)(crc << 1);
                }
        }

        return crc;
}

static hexa_status_t aht20_read_status(uint8_t *status)
{
        return hexa_i2c_read_raw(HEXA_AHT20_ADDR, status, 1);
}

hexa_status_t hexa_aht20_init(void)
{
        uint8_t status;
        hexa_status_t res;

        hexa_delay_ms(AHT20_POWERUP_MS);

        res = aht20_read_status(&status);
        if (res != HEXA_OK)
                return res;

        if (status & AHT20_STATUS_CAL)
                return HEXA_OK;

        /* Калибровка не загружена — инициализируем. Байты параметров
         * заданы даташитом, смысла не имеют. */
        const uint8_t cmd[3] = { AHT20_CMD_INIT, 0x08, 0x00 };

        res = hexa_i2c_write_raw(HEXA_AHT20_ADDR, cmd, sizeof(cmd));
        if (res != HEXA_OK)
                return res;

        hexa_delay_ms(AHT20_INIT_MS);

        res = aht20_read_status(&status);
        if (res != HEXA_OK)
                return res;

        return (status & AHT20_STATUS_CAL) ? HEXA_OK : HEXA_ERR;
}

hexa_status_t hexa_aht20_start(void)
{
        const uint8_t cmd[3] = { AHT20_CMD_TRIGGER, 0x33, 0x00 };

        return hexa_i2c_write_raw(HEXA_AHT20_ADDR, cmd, sizeof(cmd));
}

hexa_status_t hexa_aht20_ready(bool *ready)
{
        if (ready == NULL)
                return HEXA_ERR_PARAM;

        uint8_t status;
        hexa_status_t res = aht20_read_status(&status);
        if (res != HEXA_OK)
                return res;

        *ready = (status & AHT20_STATUS_BUSY) == 0;

        return HEXA_OK;
}

hexa_status_t hexa_aht20_get(int32_t *t_c100, uint32_t *rh_c100)
{
        uint8_t frame[AHT20_FRAME_LEN];

        hexa_status_t res = hexa_i2c_read_raw(HEXA_AHT20_ADDR, frame, AHT20_FRAME_LEN);
        if (res != HEXA_OK)
                return res;

        if (frame[0] & AHT20_STATUS_BUSY)
                return HEXA_ERR;

        if (aht20_crc8(frame, AHT20_FRAME_LEN - 1) != frame[AHT20_FRAME_LEN - 1])
                return HEXA_ERR_BUS;

        /* Два 20-битных поля, упакованных встык: влажность в байтах 1..3
         * (старшие 20 бит), температура — в 3..5 (младшие 20 бит). */
        uint32_t rh_raw = ((uint32_t)frame[1] << 12) |
                          ((uint32_t)frame[2] << 4)  |
                          ((uint32_t)frame[3] >> 4);

        uint32_t t_raw  = (((uint32_t)frame[3] & 0x0F) << 16) |
                          ((uint32_t)frame[4] << 8) |
                          ((uint32_t)frame[5]);

        if (rh_c100 != NULL) {
                /* rh% = raw / 2^20 * 100, в сотых: raw * 10000 / 2^20.
                 * 10000/2^20 == 625/2^16, а raw * 625 <= 6.6e8 — влезает
                 * в uint32 без 64-битной арифметики. */
                *rh_c100 = (rh_raw * 625u) >> 16;
        }

        if (t_c100 != NULL) {
                /* t°C = raw / 2^20 * 200 - 50, в сотых: raw * 20000 / 2^20 - 5000.
                 * 20000/2^20 == 1250/2^16, raw * 1250 <= 1.4e9 — тоже влезает. */
                *t_c100 = (int32_t)((t_raw * 1250u) >> 16) - 5000;
        }

        return HEXA_OK;
}

hexa_status_t hexa_aht20_measure(int32_t *t_c100, uint32_t *rh_c100)
{
        hexa_status_t res = hexa_aht20_start();
        if (res != HEXA_OK)
                return res;

        hexa_delay_ms(AHT20_MEAS_MS);

        for (uint32_t waited = AHT20_MEAS_MS; waited <= AHT20_TIMEOUT_MS;
             waited += AHT20_POLL_STEP_MS) {
                bool ready;

                res = hexa_aht20_ready(&ready);
                if (res != HEXA_OK)
                        return res;

                if (ready)
                        return hexa_aht20_get(t_c100, rh_c100);

                hexa_delay_ms(AHT20_POLL_STEP_MS);
        }

        return HEXA_ERR_TIMEOUT;
}
