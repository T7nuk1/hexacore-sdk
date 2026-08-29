#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "hexa_board.h"
#include "hexa_bmp280.h"
#include "hexa_i2c.h"
#include "hexa_time.h"

#define BMP280_REG_CALIB        0x88    /* 24 байта: dig_T1..dig_P9 */
#define BMP280_REG_ID           0xD0
#define BMP280_REG_RESET        0xE0
#define BMP280_REG_STATUS       0xF3
#define BMP280_REG_CTRL_MEAS    0xF4
#define BMP280_REG_CONFIG       0xF5
#define BMP280_REG_PRESS        0xF7    /* 6 байт: press[3] + temp[3] */

#define BMP280_CHIP_ID          0x58
#define BMP280_RESET_MAGIC      0xB6

#define BMP280_STATUS_MEASURING (1u << 3)
#define BMP280_STATUS_IM_UPDATE (1u << 0)

/* ctrl_meas: osrs_t[7:5] | osrs_p[4:2] | mode[1:0].
 * x1 по обоим каналам — минимальное время преобразования; сглаживать
 * шум приложение может само, а IIR-фильтр чипа в forced mode всё равно
 * не даёт выигрыша при одиночных измерениях. */
#define BMP280_OSRS_T_X1        (1u << 5)
#define BMP280_OSRS_P_X1        (1u << 2)
#define BMP280_MODE_SLEEP       0x00
#define BMP280_MODE_FORCED      0x01

#define BMP280_CTRL_FORCED      (BMP280_OSRS_T_X1 | BMP280_OSRS_P_X1 | BMP280_MODE_FORCED)

#define BMP280_RESET_MS         5
#define BMP280_MEAS_MS          10      /* x1/x1: ~6.4 мс max по даташиту */
/* Шаг опроса не должен быть меньше тика планировщика (10 мс при
 * configTICK_RATE_HZ = 100): иначе под FreeRTOS pdMS_TO_TICKS даёт ноль,
 * задержки не происходит и таймаут ниже становится враньём. */
#define BMP280_POLL_STEP_MS     10
#define BMP280_TIMEOUT_MS       50

#define BMP280_CALIB_LEN        24
#define BMP280_DATA_LEN         6

struct bmp280_calib {
        uint16_t t1;
        int16_t  t2, t3;
        uint16_t p1;
        int16_t  p2, p3, p4, p5, p6, p7, p8, p9;
};

static struct bmp280_calib s_calib;

static uint16_t le16u(const uint8_t *p)
{
        return (uint16_t)((uint16_t)p[1] << 8 | p[0]);
}

static int16_t le16s(const uint8_t *p)
{
        return (int16_t)le16u(p);
}

static hexa_status_t bmp280_read_calib(void)
{
        uint8_t raw[BMP280_CALIB_LEN];

        hexa_status_t res = hexa_i2c_read(HEXA_BMP280_ADDR, BMP280_REG_CALIB,
                                          raw, BMP280_CALIB_LEN);
        if (res != HEXA_OK)
                return res;

        /* Все коэффициенты little-endian; знаковость — по даташиту,
         * T1 и P1 без знака, остальные со знаком. */
        s_calib.t1 = le16u(&raw[0]);
        s_calib.t2 = le16s(&raw[2]);
        s_calib.t3 = le16s(&raw[4]);
        s_calib.p1 = le16u(&raw[6]);
        s_calib.p2 = le16s(&raw[8]);
        s_calib.p3 = le16s(&raw[10]);
        s_calib.p4 = le16s(&raw[12]);
        s_calib.p5 = le16s(&raw[14]);
        s_calib.p6 = le16s(&raw[16]);
        s_calib.p7 = le16s(&raw[18]);
        s_calib.p8 = le16s(&raw[20]);
        s_calib.p9 = le16s(&raw[22]);

        return HEXA_OK;
}

/* Компенсация температуры, 32-битный вариант из даташита BMP280 §3.11.3.
 * Возвращает сотые доли °C и заполняет t_fine — он нужен для давления. */
static int32_t bmp280_compensate_t(int32_t adc_t, int32_t *t_fine)
{
        int32_t var1, var2;

        var1 = ((((adc_t >> 3) - ((int32_t)s_calib.t1 << 1))) * ((int32_t)s_calib.t2)) >> 11;
        var2 = (((((adc_t >> 4) - ((int32_t)s_calib.t1)) *
                  ((adc_t >> 4) - ((int32_t)s_calib.t1))) >> 12) *
                ((int32_t)s_calib.t3)) >> 14;

        *t_fine = var1 + var2;

        return (*t_fine * 5 + 128) >> 8;
}

/* Компенсация давления, 32-битный вариант из даташита. 64-битный точнее
 * примерно на 1 Па, но тянет __divdi3 из libgcc — на rv32imc это того
 * не стоит. Результат сразу в паскалях. */
static uint32_t bmp280_compensate_p(int32_t adc_p, int32_t t_fine)
{
        int32_t var1, var2;
        uint32_t p;

        var1 = (t_fine >> 1) - (int32_t)64000;
        var2 = (((var1 >> 2) * (var1 >> 2)) >> 11) * ((int32_t)s_calib.p6);
        var2 = var2 + ((var1 * ((int32_t)s_calib.p5)) << 1);
        var2 = (var2 >> 2) + (((int32_t)s_calib.p4) << 16);
        var1 = ((((int32_t)s_calib.p3 * (((var1 >> 2) * (var1 >> 2)) >> 13)) >> 3) +
                ((((int32_t)s_calib.p2) * var1) >> 1)) >> 18;
        var1 = ((((int32_t)32768 + var1)) * ((int32_t)s_calib.p1)) >> 15;

        if (var1 == 0)
                return 0;       /* деление на ноль при битой калибровке */

        /* Вычитание var2 намеренно в беззнаковой арифметике, как в
         * даташите: в int32 промежуточный результат может переполниться,
         * а это UB. По модулю 2^32 битовая картина та же. */
        p = ((uint32_t)((int32_t)1048576 - adc_p) - (uint32_t)(var2 >> 12)) * 3125u;

        if (p < 0x80000000u)
                p = (p << 1) / ((uint32_t)var1);
        else
                p = (p / (uint32_t)var1) * 2u;

        var1 = (((int32_t)s_calib.p9) * ((int32_t)(((p >> 3) * (p >> 3)) >> 13))) >> 12;
        var2 = (((int32_t)(p >> 2)) * ((int32_t)s_calib.p8)) >> 13;
        p = (uint32_t)((int32_t)p + ((var1 + var2 + (int32_t)s_calib.p7) >> 4));

        return p;
}

hexa_status_t hexa_bmp280_whoami(uint8_t *id)
{
        if (id == NULL)
                return HEXA_ERR_PARAM;

        return hexa_i2c_read(HEXA_BMP280_ADDR, BMP280_REG_ID, id, 1);
}

hexa_status_t hexa_bmp280_init(void)
{
        uint8_t val;
        hexa_status_t res;

        res = hexa_bmp280_whoami(&val);
        if (res != HEXA_OK)
                return res;

        if (val != BMP280_CHIP_ID)
                return HEXA_ERR;

        val = BMP280_RESET_MAGIC;
        res = hexa_i2c_write(HEXA_BMP280_ADDR, BMP280_REG_RESET, &val, 1);
        if (res != HEXA_OK)
                return res;

        hexa_delay_ms(BMP280_RESET_MS);

        /* После сброса чип копирует калибровку из NVM в регистры и
         * держит im_update взведённым. Читать раньше — получить нули. */
        for (uint32_t waited = 0; waited <= BMP280_TIMEOUT_MS;
             waited += BMP280_POLL_STEP_MS) {
                res = hexa_i2c_read(HEXA_BMP280_ADDR, BMP280_REG_STATUS, &val, 1);
                if (res != HEXA_OK)
                        return res;

                if ((val & BMP280_STATUS_IM_UPDATE) == 0)
                        break;

                hexa_delay_ms(BMP280_POLL_STEP_MS);
        }

        if (val & BMP280_STATUS_IM_UPDATE)
                return HEXA_ERR_TIMEOUT;

        res = bmp280_read_calib();
        if (res != HEXA_OK)
                return res;

        /* config: t_sb и filter не влияют на forced mode, spi3w_en выключен */
        val = 0x00;
        res = hexa_i2c_write(HEXA_BMP280_ADDR, BMP280_REG_CONFIG, &val, 1);
        if (res != HEXA_OK)
                return res;

        /* Чип остаётся в sleep до первого hexa_bmp280_start() */
        val = BMP280_OSRS_T_X1 | BMP280_OSRS_P_X1 | BMP280_MODE_SLEEP;

        return hexa_i2c_write(HEXA_BMP280_ADDR, BMP280_REG_CTRL_MEAS, &val, 1);
}

hexa_status_t hexa_bmp280_start(void)
{
        uint8_t val = BMP280_CTRL_FORCED;

        return hexa_i2c_write(HEXA_BMP280_ADDR, BMP280_REG_CTRL_MEAS, &val, 1);
}

hexa_status_t hexa_bmp280_ready(bool *ready)
{
        if (ready == NULL)
                return HEXA_ERR_PARAM;

        uint8_t status;
        hexa_status_t res = hexa_i2c_read(HEXA_BMP280_ADDR, BMP280_REG_STATUS, &status, 1);
        if (res != HEXA_OK)
                return res;

        *ready = (status & BMP280_STATUS_MEASURING) == 0;

        return HEXA_OK;
}

hexa_status_t hexa_bmp280_get(int32_t *t_c100, uint32_t *p_pa)
{
        uint8_t raw[BMP280_DATA_LEN];

        hexa_status_t res = hexa_i2c_read(HEXA_BMP280_ADDR, BMP280_REG_PRESS,
                                          raw, BMP280_DATA_LEN);
        if (res != HEXA_OK)
                return res;

        /* 20-битные значения, big-endian, выровнены влево в 24 битах */
        int32_t adc_p = (int32_t)(((uint32_t)raw[0] << 12) |
                                  ((uint32_t)raw[1] << 4)  |
                                  ((uint32_t)raw[2] >> 4));

        int32_t adc_t = (int32_t)(((uint32_t)raw[3] << 12) |
                                  ((uint32_t)raw[4] << 4)  |
                                  ((uint32_t)raw[5] >> 4));

        int32_t t_fine;
        int32_t t = bmp280_compensate_t(adc_t, &t_fine);

        if (t_c100 != NULL)
                *t_c100 = t;

        if (p_pa != NULL)
                *p_pa = bmp280_compensate_p(adc_p, t_fine);

        return HEXA_OK;
}

hexa_status_t hexa_bmp280_measure(int32_t *t_c100, uint32_t *p_pa)
{
        hexa_status_t res = hexa_bmp280_start();
        if (res != HEXA_OK)
                return res;

        hexa_delay_ms(BMP280_MEAS_MS);

        for (uint32_t waited = BMP280_MEAS_MS; waited <= BMP280_TIMEOUT_MS;
             waited += BMP280_POLL_STEP_MS) {
                bool ready;

                res = hexa_bmp280_ready(&ready);
                if (res != HEXA_OK)
                        return res;

                if (ready)
                        return hexa_bmp280_get(t_c100, p_pa);

                hexa_delay_ms(BMP280_POLL_STEP_MS);
        }

        return HEXA_ERR_TIMEOUT;
}
