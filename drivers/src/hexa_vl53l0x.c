#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "hexa_board.h"
#include "hexa_vl53l0x.h"
#include "hexa_i2c.h"
#include "hexa_time.h"

/* Именованные регистры — только те, чьё назначение известно из
 * официального API. Всё остальное в последовательностях ниже —
 * недокументированные адреса, оставлены числами намеренно: выдуманное
 * имя создавало бы ложное впечатление, что мы понимаем их смысл. */
#define VL53L0X_SYSRANGE_START                  0x00
#define VL53L0X_SYSTEM_SEQUENCE_CONFIG          0x01
#define VL53L0X_SYSTEM_INTERRUPT_CONFIG_GPIO    0x0A
#define VL53L0X_SYSTEM_INTERRUPT_CLEAR          0x0B
#define VL53L0X_RESULT_INTERRUPT_STATUS         0x13
#define VL53L0X_RESULT_RANGE_STATUS             0x14
#define VL53L0X_FINAL_RANGE_MIN_COUNT_RATE      0x44
#define VL53L0X_MSRC_CONFIG_CONTROL             0x60
#define VL53L0X_GPIO_HV_MUX_ACTIVE_HIGH         0x84
#define VL53L0X_VHV_CONFIG_PAD_SCL_SDA_EXTSUP   0x89
#define VL53L0X_IDENTIFICATION_MODEL_ID         0xC0

#define VL53L0X_MODEL_ID                        0xEE

/* Дистанция лежит со смещением 10 от RESULT_RANGE_STATUS */
#define VL53L0X_RESULT_RANGE_MM         (VL53L0X_RESULT_RANGE_STATUS + 10)

/* Код в битах [6:3] RESULT_RANGE_STATUS: 11 — достоверное измерение,
 * любой другой — отказ той или иной проверки. Бит 7 (data ready) в поле
 * не входит и должен быть отброшен маской. */
#define VL53L0X_RANGE_STATUS_MASK       0x78
#define VL53L0X_RANGE_VALID             11

#define VL53L0X_MEAS_MS                 30
/* Шаг опроса не должен быть меньше тика планировщика (10 мс при
 * configTICK_RATE_HZ = 100): иначе под FreeRTOS pdMS_TO_TICKS даёт ноль,
 * задержки не происходит и таймаут ниже становится враньём. */
#define VL53L0X_POLL_STEP_MS            10
#define VL53L0X_TIMEOUT_MS              100

/* Читается при инициализации из недокументированного регистра 0x91 и
 * записывается обратно перед каждым запуском измерения. */
static uint8_t s_stop_variable;

/* Заводской набор tuning-настроек из официального API ST.
 * Пары "регистр, значение", применяются строго по порядку: часть из них
 * переключает страницы регистров записями в 0xFF. */
static const uint8_t vl53l0x_tuning[][2] = {
        { 0xFF, 0x01 }, { 0x00, 0x00 }, { 0xFF, 0x00 }, { 0x09, 0x00 },
        { 0x10, 0x00 }, { 0x11, 0x00 }, { 0x24, 0x01 }, { 0x25, 0xFF },
        { 0x75, 0x00 }, { 0xFF, 0x01 }, { 0x4E, 0x2C }, { 0x48, 0x00 },
        { 0x30, 0x20 }, { 0xFF, 0x00 }, { 0x30, 0x09 }, { 0x54, 0x00 },
        { 0x31, 0x04 }, { 0x32, 0x03 }, { 0x40, 0x83 }, { 0x46, 0x25 },
        { 0x60, 0x00 }, { 0x27, 0x00 }, { 0x50, 0x06 }, { 0x51, 0x00 },
        { 0x52, 0x96 }, { 0x56, 0x08 }, { 0x57, 0x30 }, { 0x61, 0x00 },
        { 0x62, 0x00 }, { 0x64, 0x00 }, { 0x65, 0x00 }, { 0x66, 0xA0 },
        { 0xFF, 0x01 }, { 0x22, 0x32 }, { 0x47, 0x14 }, { 0x49, 0xFF },
        { 0x4A, 0x00 }, { 0xFF, 0x00 }, { 0x7A, 0x0A }, { 0x7B, 0x00 },
        { 0x78, 0x21 }, { 0xFF, 0x01 }, { 0x23, 0x34 }, { 0x42, 0x00 },
        { 0x44, 0xFF }, { 0x45, 0x26 }, { 0x46, 0x05 }, { 0x40, 0x40 },
        { 0x0E, 0x06 }, { 0x20, 0x1A }, { 0x43, 0x40 }, { 0xFF, 0x00 },
        { 0x34, 0x03 }, { 0x35, 0x44 }, { 0xFF, 0x01 }, { 0x31, 0x04 },
        { 0x4B, 0x09 }, { 0x4C, 0x05 }, { 0x4D, 0x04 }, { 0xFF, 0x00 },
        { 0x44, 0x00 }, { 0x45, 0x20 }, { 0x47, 0x08 }, { 0x48, 0x28 },
        { 0x67, 0x00 }, { 0x70, 0x04 }, { 0x71, 0x01 }, { 0x72, 0xFE },
        { 0x76, 0x00 }, { 0x77, 0x00 }, { 0xFF, 0x01 }, { 0x0D, 0x01 },
        { 0xFF, 0x00 }, { 0x80, 0x01 }, { 0x01, 0xF8 }, { 0xFF, 0x01 },
        { 0x8E, 0x01 }, { 0x00, 0x01 }, { 0xFF, 0x00 }, { 0x80, 0x00 },
};

static hexa_status_t reg_write(uint8_t reg, uint8_t val)
{
        return hexa_i2c_write(HEXA_VL53L0X_ADDR, reg, &val, 1);
}

static hexa_status_t reg_read(uint8_t reg, uint8_t *val)
{
        return hexa_i2c_read(HEXA_VL53L0X_ADDR, reg, val, 1);
}

/* Данные шире байта чип отдаёт big-endian, адрес регистра при этом
 * остаётся 8-битным — поэтому хелперы локальные, в hexa_i2c не лезут. */
static hexa_status_t reg_write16(uint8_t reg, uint16_t val)
{
        uint8_t buf[2] = { (uint8_t)(val >> 8), (uint8_t)(val & 0xFF) };

        return hexa_i2c_write(HEXA_VL53L0X_ADDR, reg, buf, 2);
}

static hexa_status_t reg_read16(uint8_t reg, uint16_t *val)
{
        uint8_t buf[2];

        hexa_status_t res = hexa_i2c_read(HEXA_VL53L0X_ADDR, reg, buf, 2);
        if (res != HEXA_OK)
                return res;

        *val = (uint16_t)(((uint16_t)buf[0] << 8) | buf[1]);

        return HEXA_OK;
}

static hexa_status_t reg_set_bits(uint8_t reg, uint8_t set, uint8_t clear)
{
        uint8_t val;

        hexa_status_t res = reg_read(reg, &val);
        if (res != HEXA_OK)
                return res;

        val = (uint8_t)((val & ~clear) | set);

        return reg_write(reg, val);
}

/* Ожидание взведённого флага прерывания. Используется и калибровкой,
 * и обёрткой measure. */
static hexa_status_t wait_interrupt(void)
{
        for (uint32_t waited = 0; waited <= VL53L0X_TIMEOUT_MS;
             waited += VL53L0X_POLL_STEP_MS) {
                uint8_t status;

                hexa_status_t res = reg_read(VL53L0X_RESULT_INTERRUPT_STATUS, &status);
                if (res != HEXA_OK)
                        return res;

                if (status & 0x07)
                        return HEXA_OK;

                hexa_delay_ms(VL53L0X_POLL_STEP_MS);
        }

        return HEXA_ERR_TIMEOUT;
}

/* Одиночный проход опорной калибровки. vhv_init_byte различает два
 * режима: 0x40 — калибровка VHV, 0x00 — калибровка фазы. */
static hexa_status_t single_ref_calibration(uint8_t vhv_init_byte)
{
        hexa_status_t res;

        res = reg_write(VL53L0X_SYSRANGE_START, (uint8_t)(0x01 | vhv_init_byte));
        if (res != HEXA_OK)
                return res;

        res = wait_interrupt();
        if (res != HEXA_OK)
                return res;

        res = reg_write(VL53L0X_SYSTEM_INTERRUPT_CLEAR, 0x01);
        if (res != HEXA_OK)
                return res;

        return reg_write(VL53L0X_SYSRANGE_START, 0x00);
}

#define TRY(expr) do { \
        hexa_status_t _res = (expr); \
        if (_res != HEXA_OK) \
                return _res; \
} while (0)

hexa_status_t hexa_vl53l0x_whoami(uint8_t *id)
{
        if (id == NULL)
                return HEXA_ERR_PARAM;

        return reg_read(VL53L0X_IDENTIFICATION_MODEL_ID, id);
}

hexa_status_t hexa_vl53l0x_init(void)
{
        uint8_t id;

        TRY(hexa_vl53l0x_whoami(&id));

        if (id != VL53L0X_MODEL_ID)
                return HEXA_ERR;

        /* --- data init --- */

        /* Модуль GY-530 питает чип от 2.8 В, а подтяжки шины — от того же
         * источника, что и MCU. Бит разрешает повышенные уровни на SCL/SDA. */
        TRY(reg_set_bits(VL53L0X_VHV_CONFIG_PAD_SCL_SDA_EXTSUP, 0x01, 0x00));

        TRY(reg_write(0x88, 0x00));

        TRY(reg_write(0x80, 0x01));
        TRY(reg_write(0xFF, 0x01));
        TRY(reg_write(0x00, 0x00));
        TRY(reg_read(0x91, &s_stop_variable));
        TRY(reg_write(0x00, 0x01));
        TRY(reg_write(0xFF, 0x00));
        TRY(reg_write(0x80, 0x00));

        /* Отключаем проверки по скорости счёта для MSRC и pre-range:
         * с ними чип бракует слабоотражающие цели. */
        TRY(reg_set_bits(VL53L0X_MSRC_CONFIG_CONTROL, 0x12, 0x00));

        /* Минимальная скорость счёта финального диапазона, 0.25 MCPS
         * в формате Q9.7: 0.25 * 128 = 32. */
        TRY(reg_write16(VL53L0X_FINAL_RANGE_MIN_COUNT_RATE, 32));

        TRY(reg_write(VL53L0X_SYSTEM_SEQUENCE_CONFIG, 0xFF));

        /* --- tuning --- */

        for (size_t i = 0; i < sizeof(vl53l0x_tuning) / sizeof(vl53l0x_tuning[0]); i++)
                TRY(reg_write(vl53l0x_tuning[i][0], vl53l0x_tuning[i][1]));

        /* --- прерывание по готовности измерения, активный уровень низкий --- */

        TRY(reg_write(VL53L0X_SYSTEM_INTERRUPT_CONFIG_GPIO, 0x04));
        TRY(reg_set_bits(VL53L0X_GPIO_HV_MUX_ACTIVE_HIGH, 0x00, 0x10));
        TRY(reg_write(VL53L0X_SYSTEM_INTERRUPT_CLEAR, 0x01));

        /* --- опорная калибровка ---
         * Обязательна: без неё чип выдаёт смещённые и нестабильные
         * значения. Каждый этап требует своего набора шагов
         * последовательности, после — восстанавливаем рабочий набор. */

        TRY(reg_write(VL53L0X_SYSTEM_SEQUENCE_CONFIG, 0x01));
        TRY(single_ref_calibration(0x40));

        TRY(reg_write(VL53L0X_SYSTEM_SEQUENCE_CONFIG, 0x02));
        TRY(single_ref_calibration(0x00));

        TRY(reg_write(VL53L0X_SYSTEM_SEQUENCE_CONFIG, 0xE8));

        return HEXA_OK;
}

hexa_status_t hexa_vl53l0x_start(void)
{
        TRY(reg_write(0x80, 0x01));
        TRY(reg_write(0xFF, 0x01));
        TRY(reg_write(0x00, 0x00));
        TRY(reg_write(0x91, s_stop_variable));
        TRY(reg_write(0x00, 0x01));
        TRY(reg_write(0xFF, 0x00));
        TRY(reg_write(0x80, 0x00));

        return reg_write(VL53L0X_SYSRANGE_START, 0x01);
}

hexa_status_t hexa_vl53l0x_ready(bool *ready)
{
        if (ready == NULL)
                return HEXA_ERR_PARAM;

        uint8_t status;

        TRY(reg_read(VL53L0X_RESULT_INTERRUPT_STATUS, &status));

        *ready = (status & 0x07) != 0;

        return HEXA_OK;
}

hexa_status_t hexa_vl53l0x_get(uint16_t *mm)
{
        if (mm == NULL)
                return HEXA_ERR_PARAM;

        uint8_t status;

        TRY(reg_read(VL53L0X_RESULT_RANGE_STATUS, &status));
        TRY(reg_read16(VL53L0X_RESULT_RANGE_MM, mm));
        TRY(reg_write(VL53L0X_SYSTEM_INTERRUPT_CLEAR, 0x01));

        uint8_t code = (uint8_t)((status & VL53L0X_RANGE_STATUS_MASK) >> 3);

        return (code == VL53L0X_RANGE_VALID) ? HEXA_OK : HEXA_ERR;
}

hexa_status_t hexa_vl53l0x_measure(uint16_t *mm)
{
        if (mm == NULL)
                return HEXA_ERR_PARAM;

        TRY(hexa_vl53l0x_start());

        hexa_delay_ms(VL53L0X_MEAS_MS);

        for (uint32_t waited = VL53L0X_MEAS_MS; waited <= VL53L0X_TIMEOUT_MS;
             waited += VL53L0X_POLL_STEP_MS) {
                bool ready;

                TRY(hexa_vl53l0x_ready(&ready));

                if (ready)
                        return hexa_vl53l0x_get(mm);

                hexa_delay_ms(VL53L0X_POLL_STEP_MS);
        }

        return HEXA_ERR_TIMEOUT;
}
