#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "hexa_board.h"
#include "hexa_rv3032.h"
#include "hexa_i2c.h"

/* Карта регистров RV-3032-C7 */
#define RV3032_REG_100TH        0x00
#define RV3032_REG_SEC          0x01
#define RV3032_REG_MIN          0x02
#define RV3032_REG_HOUR         0x03
#define RV3032_REG_WDAY         0x04
#define RV3032_REG_DATE         0x05
#define RV3032_REG_MONTH        0x06
#define RV3032_REG_YEAR         0x07
#define RV3032_REG_STATUS       0x0D
#define RV3032_REG_TEMP_LSB     0x0E
#define RV3032_REG_TEMP_MSB     0x0F
#define RV3032_REG_CTRL1        0x10

/* Status (0x0D) */
#define RV3032_STATUS_VLF       (1u << 0)       /* напряжение падало ниже порога */
#define RV3032_STATUS_PORF      (1u << 1)       /* был power-on reset */

/* Регистры времени — 7 подряд, читаются/пишутся одной пачкой */
#define RV3032_TIME_LEN         7

static uint8_t bcd_to_bin(uint8_t v)
{
        return (uint8_t)((v >> 4) * 10 + (v & 0x0F));
}

static uint8_t bin_to_bcd(uint8_t v)
{
        return (uint8_t)(((v / 10) << 4) | (v % 10));
}

static void unpack_time(const uint8_t *raw, hexa_datetime_t *dt)
{
        /* Верхние биты в регистрах времени зарезервированы — маскируем,
         * иначе bcd_to_bin выдаст мусор. */
        dt->sec   = bcd_to_bin(raw[0] & 0x7F);
        dt->min   = bcd_to_bin(raw[1] & 0x7F);
        dt->hour  = bcd_to_bin(raw[2] & 0x3F);
        dt->wday  = raw[3] & 0x07;      /* единственный регистр не в BCD */
        dt->day   = bcd_to_bin(raw[4] & 0x3F);
        dt->month = bcd_to_bin(raw[5] & 0x1F);
        dt->year  = (uint16_t)(2000 + bcd_to_bin(raw[6]));
}

hexa_status_t hexa_rv3032_init(void)
{
        uint8_t status;

        /* WHO_AM_I у чипа нет — проверяем, что он вообще отзывается.
         * Регистры конфигурации в EEPROM намеренно не трогаем: заводских
         * настроек хватает для работы от основного питания. */
        return hexa_i2c_read(HEXA_RV3032_ADDR, RV3032_REG_STATUS, &status, 1);
}

hexa_status_t hexa_rv3032_time_valid(bool *valid)
{
        if (valid == NULL)
                return HEXA_ERR_PARAM;

        uint8_t status;
        hexa_status_t res = hexa_i2c_read(HEXA_RV3032_ADDR, RV3032_REG_STATUS, &status, 1);
        if (res != HEXA_OK)
                return res;

        *valid = (status & (RV3032_STATUS_PORF | RV3032_STATUS_VLF)) == 0;

        return HEXA_OK;
}

hexa_status_t hexa_rv3032_get_time(hexa_datetime_t *dt)
{
        if (dt == NULL)
                return HEXA_ERR_PARAM;

        uint8_t raw[RV3032_TIME_LEN];
        hexa_status_t res;

        res = hexa_i2c_read(HEXA_RV3032_ADDR, RV3032_REG_SEC, raw, RV3032_TIME_LEN);
        if (res != HEXA_OK)
                return res;

        /* Чтение 7 регистров не атомарно: если секунды перевернулись в
         * середине пачки, старшие поля уже от новой минуты/часа/суток.
         * Ловим по изменившимся секундам и перечитываем — второй раз
         * попасть в тот же переворот нельзя. */
        uint8_t sec_again;
        res = hexa_i2c_read(HEXA_RV3032_ADDR, RV3032_REG_SEC, &sec_again, 1);
        if (res != HEXA_OK)
                return res;

        if ((sec_again & 0x7F) != (raw[0] & 0x7F)) {
                res = hexa_i2c_read(HEXA_RV3032_ADDR, RV3032_REG_SEC, raw, RV3032_TIME_LEN);
                if (res != HEXA_OK)
                        return res;
        }

        unpack_time(raw, dt);

        return HEXA_OK;
}

hexa_status_t hexa_rv3032_set_time(const hexa_datetime_t *dt)
{
        if (dt == NULL)
                return HEXA_ERR_PARAM;

        if (dt->sec > 59 || dt->min > 59 || dt->hour > 23 ||
            dt->wday > 6 ||
            dt->day < 1 || dt->day > 31 ||
            dt->month < 1 || dt->month > 12 ||
            dt->year < 2000 || dt->year > 2099)
                return HEXA_ERR_PARAM;

        uint8_t raw[RV3032_TIME_LEN];

        raw[0] = bin_to_bcd(dt->sec);
        raw[1] = bin_to_bcd(dt->min);
        raw[2] = bin_to_bcd(dt->hour);
        raw[3] = dt->wday;
        raw[4] = bin_to_bcd(dt->day);
        raw[5] = bin_to_bcd(dt->month);
        raw[6] = bin_to_bcd((uint8_t)(dt->year - 2000));

        hexa_status_t res = hexa_i2c_write(HEXA_RV3032_ADDR, RV3032_REG_SEC, raw, RV3032_TIME_LEN);
        if (res != HEXA_OK)
                return res;

        /* Время выставлено — снимаем флаги "содержимое недостоверно".
         * Оба сбрасываются записью нуля, остальные биты статуса не трогаем. */
        uint8_t status;
        res = hexa_i2c_read(HEXA_RV3032_ADDR, RV3032_REG_STATUS, &status, 1);
        if (res != HEXA_OK)
                return res;

        status &= (uint8_t)~(RV3032_STATUS_PORF | RV3032_STATUS_VLF);

        return hexa_i2c_write(HEXA_RV3032_ADDR, RV3032_REG_STATUS, &status, 1);
}

hexa_status_t hexa_rv3032_get_temp(int32_t *t_c100)
{
        if (t_c100 == NULL)
                return HEXA_ERR_PARAM;

        uint8_t raw[2];  /* [0] = 0x0E, [1] = 0x0F */

        hexa_status_t res = hexa_i2c_read(HEXA_RV3032_ADDR, RV3032_REG_TEMP_LSB, raw, 2);
        if (res != HEXA_OK)
                return res;

        /* 12 бит со знаком, шаг 1/16 °C: старшие 8 бит в 0x0F,
         * младшие 4 — в старшем полубайте 0x0E. Собираем в int16 со
         * сдвигом влево на 4, затем арифметический сдвиг вправо
         * доразмножает знак. */
        int16_t t16 = (int16_t)(((uint16_t)raw[1] << 8) | (raw[0] & 0xF0));
        int32_t sixteenths = t16 >> 4;

        /* 1/16 °C -> сотые: * 100 / 16 = * 25 / 4 */
        *t_c100 = (sixteenths * 25) / 4;

        return HEXA_OK;
}
