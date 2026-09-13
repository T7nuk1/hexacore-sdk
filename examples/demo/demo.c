/*
 * hexacore demo — ходовые испытания платы.
 *
 * Задействует всё, что есть в SDK: I2C-шину и все пять датчиков, OLED со
 * всей его геометрией, кнопки, пищалку и UART. Пять экранов, кнопки
 * переключают. Смысл прошивки — за одну заливку убедиться, что железо
 * живое, и увидеть, на что похож каждый компонент в работе.
 *
 * Кнопки:
 *      0 — предыдущий экран        3 — инверсия экрана
 *      1 — следующий экран         4 — пересканировать шину
 *      2 — звук вкл/выкл
 *
 * Почему три задачи, а не один суперцикл: hexa_oled_flush() держит шину
 * около 100 мс, а опрос датчиков — ещё дольше. В суперцикле всё это время
 * кнопки не опрашиваются и нажатия теряются. Здесь отрисовка, датчики и
 * кнопки разведены по задачам с разными приоритетами, и кнопки остаются
 * живыми, пока экран занят своим килобайтом.
 */

#include <stdint.h>
#include <stdbool.h>

#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

#include "hexa_board.h"
#include "hexa_i2c.h"
#include "hexa_oled.h"
#include "hexa_button.h"
#include "hexa_buzzer.h"
#include "hexa_time.h"
#include "hexa_aht20.h"
#include "hexa_bmp280.h"
#include "hexa_mpu.h"
#include "hexa_rv3032.h"
#include "hexa_vl53l0x.h"
#include "xprintf.h"


/* ---- Периоды задач ---------------------------------------------------
 *
 * configTICK_RATE_HZ здесь 100, то есть тик — 10 мс, и это нижняя граница
 * всему: pdMS_TO_TICKS(5) даёт ноль тиков, а vTaskDelay(0) — это не
 * задержка, а уступка процессора. Поэтому опрос кнопок стоит на 10 мс, а
 * не на 5, как советует hexa_button.h: чаще на этом тике не бывает.
 * Дребезг гасится четырьмя опросами подряд, значит реакция на нажатие —
 * около 40 мс. Для кнопок это незаметно. */
#define UI_PERIOD_MS            250
#define SENSOR_PERIOD_MS        1000
#define INPUT_PERIOD_MS         10

/* Размеры стеков в словах (4 байта). Куча FreeRTOS — 8 КБ на всю плату
 * (configTOTAL_HEAP_SIZE), из них ещё килобайт забирает задача Idle,
 * так что запас тут небольшой и xTaskCreate проверяется. */
#define UI_STACK                320
#define SENSOR_STACK            300
#define INPUT_STACK             160

#define PRIO_UI                 (tskIDLE_PRIORITY + 1)
#define PRIO_SENSOR             (tskIDLE_PRIORITY + 1)
#define PRIO_INPUT              (tskIDLE_PRIORITY + 2)


/* ---- Разделяемое состояние -------------------------------------------
 *
 * Датчики читает одна задача, рисует другая. Структура больше машинного
 * слова, поэтому читатель может застать её наполовину обновлённой —
 * отсюда мьютекс. Флаги интерфейса ниже — по одному байту, их пишет
 * только задача кнопок, и там хватает volatile. */

typedef struct {
        bool aht20, bmp280, mpu, rv3032, vl53l0x, oled;
} present_t;

typedef struct {
        int32_t         t_c100;         /* AHT20: температура, сотые °C */
        uint32_t        rh_c100;        /* AHT20: влажность, сотые %RH  */
        int32_t         bt_c100;        /* BMP280: температура          */
        uint32_t        p_pa;           /* BMP280: давление, Па         */
        int16_t         ax, ay, az;     /* MPU6050: сырой акселерометр  */
        uint16_t        dist_mm;        /* VL53L0X: дистанция           */
        bool            dist_ok;
        hexa_datetime_t now;            /* RV-3032                      */
        bool            time_ok;
        uint32_t        cycles;         /* сколько раз опросили датчики */
} sensors_t;

static sensors_t        s_data;
static present_t        s_present;
static SemaphoreHandle_t s_lock;

enum { SCR_SYSTEM, SCR_CLIMATE, SCR_RANGE, SCR_LEVEL, SCR_CLOCK, SCR_COUNT };

static volatile uint8_t s_screen   = SCR_SYSTEM;
static volatile bool    s_inverted = false;
static volatile bool    s_sound    = true;
static volatile bool    s_rescan   = false;


/* ---- Данные для картинок --------------------------------------------- */

/* Логотип 24x24, построчно, старший бит слева — формат hexa_oled_bitmap().
 * Двоичные литералы выбраны не случайно: картинку видно прямо в исходнике
 * и её можно править глазами, без конвертера. */
static const uint8_t logo_hexa[] = {
        0b00000000, 0b00001000, 0b00000000,
        0b00000000, 0b00011000, 0b00000000,
        0b00000000, 0b01111110, 0b00000000,
        0b00000001, 0b11000011, 0b10000000,
        0b00000111, 0b10000001, 0b11100000,
        0b00011110, 0b00000000, 0b01111000,
        0b00111000, 0b00001000, 0b00011100,
        0b00110000, 0b00111100, 0b00001100,
        0b00110000, 0b11000011, 0b00001100,
        0b00110001, 0b00000000, 0b10001100,
        0b00110001, 0b00000000, 0b10001100,
        0b00110001, 0b00000000, 0b10001100,
        0b00110001, 0b00000000, 0b10001100,
        0b00110001, 0b00000000, 0b10001100,
        0b00110001, 0b00000000, 0b10001100,
        0b00110000, 0b11000011, 0b00001100,
        0b00110000, 0b00111100, 0b00001100,
        0b00111000, 0b00001000, 0b00011100,
        0b00011110, 0b00000000, 0b01111000,
        0b00000111, 0b10000001, 0b11100000,
        0b00000001, 0b11000011, 0b10000000,
        0b00000000, 0b01111110, 0b00000000,
        0b00000000, 0b00011000, 0b00000000,
        0b00000000, 0b00000000, 0b00000000,
};

/* sin(6*k градусов) * 256 для k = 0..15, то есть первая четверть круга.
 * Шаг в 6 градусов выбран под циферблат: 60 делений на круг — ровно шаг
 * секундной стрелки. Плавающей точки в сборке нет и не нужно: масштаб 256
 * превращает деление на сдвиг, а точности до пикселя хватает с запасом. */
static const int16_t sin_q[16] = {
           0,   27,   53,   79,  104,  128,  150,  171,
         190,  207,  222,  234,  243,  250,  255,  256,
};

/* Синус для деления круга step = 0..59. Остальные три четверти
 * достраиваются из первой отражением — таблицу хранить целиком незачем. */
static int16_t sin60(uint8_t step)
{
        step %= 60;
        if (step <= 15)  return  sin_q[step];
        if (step <= 30)  return  sin_q[30 - step];
        if (step <= 45)  return (int16_t)-sin_q[step - 30];
        return                  (int16_t)-sin_q[60 - step];
}

static int16_t cos60(uint8_t step) { return sin60((uint8_t)((step + 15) % 60)); }


/* ---- Мелкие помощники отрисовки -------------------------------------- */

/* Шапка экрана: инверсная полоса во всю ширину. Текст цветом 0 — по
 * залитому фону, иначе его не видно. */
static void draw_header(const char *title)
{
        hexa_oled_fill_rect(0, 0, HEXA_OLED_WIDTH, 9, 1);
        hexa_oled_print(title, 2, 1, 0);
}

/* Номер экрана точками в правом углу шапки: сколько экранов и где мы. */
static void draw_screen_dots(uint8_t cur)
{
        for (uint8_t i = 0; i < SCR_COUNT; i++) {
                int16_t x = (int16_t)(HEXA_OLED_WIDTH - 4 - (SCR_COUNT - i) * 6);

                if (i == cur) {
                        hexa_oled_fill_circle(x, 4, 2, 0);
                } else {
                        hexa_oled_circle(x, 4, 2, 0);
                }
        }
}

/* Столбиковый индикатор: рамка плюс заливка пропорционально значению.
 * Значение за пределами шкалы прижимается к краю, а не вылезает из рамки. */
static void draw_bar(int16_t x, int16_t y, int16_t w, int16_t h,
                     int32_t v, int32_t lo, int32_t hi)
{
        hexa_oled_rect(x, y, w, h, 1);

        if (hi <= lo) {
                return;
        }
        if (v < lo) v = lo;
        if (v > hi) v = hi;

        /* Внутренняя область уже рамки на пиксель с каждой стороны. */
        int16_t inner = (int16_t)(w - 2);
        int16_t fill  = (int16_t)(((int32_t)(v - lo) * inner) / (hi - lo));

        if (fill > 0) {
                hexa_oled_fill_rect((int16_t)(x + 1), (int16_t)(y + 1),
                                    fill, (int16_t)(h - 2), 1);
        }
}

/* Значение в сотых долях как "23.45". Знак печатается отдельно: деление
 * отрицательного на 100 даёт -23 и -45, и "-23.-45" никому не нужно. */
static void fmt_c100(char *buf, int32_t v, const char *unit)
{
        const char *sign = (v < 0) ? "-" : "";
        uint32_t a = (uint32_t)((v < 0) ? -v : v);

        xsprintf(buf, "%s%u.%02u%s", sign, a / 100, a % 100, unit);
}


/* ---- Экраны ---------------------------------------------------------- */

/* Что найдено на шине и сколько циклов опроса прошло. Экран-визитка:
 * если датчик не распаян или отвалился, видно сразу здесь. */
static void screen_system(const sensors_t *d)
{
        char line[24];

        draw_header("SYSTEM");
        hexa_oled_bitmap(2, 12, 24, 24, logo_hexa, 1);

        hexa_oled_print("hexacore SDK", 32, 13, 1);
        xsprintf(line, "board rev %u.%u", BOARD_REV_MAJOR, BOARD_REV_MINOR);
        hexa_oled_print(line, 32, 23, 1);
        xsprintf(line, "polls: %u", d->cycles);
        hexa_oled_print(line, 32, 33, 1);

        hexa_oled_hline(2, 41, 124, 1);

        /* 19 символов по 6 пикселей — ровно в ширину экрана от x=2. */
        xsprintf(line, "%s %s %s %s %s",
                s_present.aht20   ? "AHT" : "---",
                s_present.bmp280  ? "BMP" : "---",
                s_present.mpu     ? "MPU" : "---",
                s_present.rv3032  ? "RTC" : "---",
                s_present.vl53l0x ? "TOF" : "---");
        hexa_oled_print(line, 2, 45, 1);

        hexa_oled_print(s_sound ? "snd on" : "snd off", 2, 55, 1);
        hexa_oled_print("btn4:rescan", 56, 55, 1);
}

/* AHT20 и BMP280: числа плюс шкалы. Шкалы тут не для красоты — по ним
 * видно изменение раньше, чем успеваешь прочитать цифры. */
static void screen_climate(const sensors_t *d)
{
        char line[24];

        draw_header("CLIMATE");

        if (s_present.aht20) {
                fmt_c100(line, d->t_c100, " C");
                hexa_oled_print(line, 2, 13, 1);
                draw_bar(64, 12, 62, 9, d->t_c100, 0, 4000);    /* 0..40 C */

                fmt_c100(line, (int32_t)d->rh_c100, " %RH");
                hexa_oled_print(line, 2, 26, 1);
                draw_bar(64, 25, 62, 9, (int32_t)d->rh_c100, 0, 10000);
        } else {
                hexa_oled_print("AHT20 absent", 2, 18, 1);
        }

        hexa_oled_hline(2, 39, 124, 1);

        if (s_present.bmp280) {
                /* Паскали в гектопаскали: 101325 Па -> 1013.25 гПа. */
                xsprintf(line, "%u.%02u hPa", d->p_pa / 100, d->p_pa % 100);
                hexa_oled_print(line, 2, 43, 1);

                fmt_c100(line, d->bt_c100, " C (bmp)");
                hexa_oled_print(line, 2, 54, 1);
        } else {
                hexa_oled_print("BMP280 absent", 2, 47, 1);
        }
}

/* VL53L0X: дистанция числом и полосой со шкалой. Полоса важнее числа —
 * поднеси ладонь и увидишь, как она едет. */
static void screen_range(const sensors_t *d)
{
        char line[24];

        draw_header("RANGE");

        if (!s_present.vl53l0x) {
                hexa_oled_print("VL53L0X absent", 2, 28, 1);
                return;
        }

        if (d->dist_ok) {
                xsprintf(line, "%u mm", d->dist_mm);
        } else {
                xsprintf(line, "no target");
        }
        hexa_oled_print(line, 2, 14, 1);

        draw_bar(2, 26, 124, 14, d->dist_ok ? d->dist_mm : 0, 0, 1200);

        /* Засечки шкалы каждые 300 мм: 124 пикселя на 1200 мм. */
        for (int16_t mm = 0; mm <= 1200; mm += 300) {
                int16_t x = (int16_t)(2 + (mm * 124) / 1200);
                hexa_oled_vline(x, 42, 4, 1);
        }
        hexa_oled_print("0", 2, 48, 1);
        hexa_oled_print("600", 54, 48, 1);
        hexa_oled_print("1.2m", 104, 48, 1);
}

/* MPU6050 как пузырьковый уровень. Акселерометр в покое показывает
 * вектор g: наклонишь плату — он уедет в сторону наклона, и пузырёк
 * вместе с ним. Заводской диапазон +-2g, то есть 16384 единицы на g. */
static void screen_level(const sensors_t *d)
{
        char line[24];
        const int16_t cx = 40, cy = 37, r = 24;

        draw_header("LEVEL");

        if (!s_present.mpu) {
                hexa_oled_print("MPU6050 absent", 2, 28, 1);
                return;
        }

        hexa_oled_circle(cx, cy, r, 1);
        hexa_oled_circle(cx, cy, 4, 1);
        hexa_oled_hline((int16_t)(cx - r), cy, (int16_t)(2 * r + 1), 1);
        hexa_oled_vline(cx, (int16_t)(cy - r), (int16_t)(2 * r + 1), 1);

        /* 1g отклоняет пузырёк на весь радиус: ax * r / 16384.
         * Считаем в int32_t — произведение не влезает в 16 бит. */
        int16_t bx = (int16_t)(((int32_t)d->ax * r) / 16384);
        int16_t by = (int16_t)(((int32_t)d->ay * r) / 16384);

        hexa_oled_fill_circle((int16_t)(cx + bx), (int16_t)(cy - by), 3, 1);

        xsprintf(line, "x%6d", d->ax);
        hexa_oled_print(line, 74, 16, 1);
        xsprintf(line, "y%6d", d->ay);
        hexa_oled_print(line, 74, 28, 1);
        xsprintf(line, "z%6d", d->az);
        hexa_oled_print(line, 74, 40, 1);
}

/* RV-3032: стрелочный циферблат плюс цифры. Циферблат целиком собран из
 * hexa_oled_line() и таблицы синусов — ради него она и заведена. */
static void screen_clock(const sensors_t *d)
{
        char line[24];
        const int16_t cx = 32, cy = 38, r = 24;

        draw_header("CLOCK");

        if (!s_present.rv3032) {
                hexa_oled_print("RV-3032 absent", 2, 28, 1);
                return;
        }
        if (!d->time_ok) {
                hexa_oled_print("time invalid", 2, 28, 1);
                return;
        }

        hexa_oled_circle(cx, cy, r, 1);

        /* Часовые метки — каждое пятое деление из шестидесяти. */
        for (uint8_t i = 0; i < 60; i += 5) {
                int16_t x = (int16_t)(cx + (int32_t)sin60(i) * (r - 3) / 256);
                int16_t y = (int16_t)(cy - (int32_t)cos60(i) * (r - 3) / 256);
                hexa_oled_hline(x, y, 1, 1);    /* точка знаковыми координатами */
        }

        /* Часовая стрелка идёт не рывками по часам, а ползёт вместе с
         * минутной: 5 делений на час плюс доля от минут. */
        uint8_t hs = (uint8_t)((d->now.hour % 12) * 5 + d->now.min / 12);

        hexa_oled_line(cx, cy,
                (int16_t)(cx + (int32_t)sin60(hs) * (r - 12) / 256),
                (int16_t)(cy - (int32_t)cos60(hs) * (r - 12) / 256), 1);
        hexa_oled_line(cx, cy,
                (int16_t)(cx + (int32_t)sin60(d->now.min) * (r - 6) / 256),
                (int16_t)(cy - (int32_t)cos60(d->now.min) * (r - 6) / 256), 1);
        hexa_oled_line(cx, cy,
                (int16_t)(cx + (int32_t)sin60(d->now.sec) * (r - 3) / 256),
                (int16_t)(cy - (int32_t)cos60(d->now.sec) * (r - 3) / 256), 1);

        xsprintf(line, "%02u:%02u:%02u", d->now.hour, d->now.min, d->now.sec);
        hexa_oled_print(line, 64, 26, 1);
        xsprintf(line, "%02u.%02u.%04u", d->now.day, d->now.month, d->now.year);
        hexa_oled_print(line, 64, 40, 1);
}


/* ---- Задачи ---------------------------------------------------------- */

static void ui_task(void *arg)
{
        (void)arg;
        TickType_t next = xTaskGetTickCount();

        for (;;) {
                sensors_t snap;

                /* Под мьютексом только копирование: рисовать можно и без
                 * него, а вот держать шину заблокированной все 100 мс
                 * отрисовки — значит остановить задачу датчиков. */
                xSemaphoreTake(s_lock, portMAX_DELAY);
                snap = s_data;
                xSemaphoreGive(s_lock);

                hexa_oled_clear();

                switch (s_screen) {
                        case SCR_CLIMATE: screen_climate(&snap); break;
                        case SCR_RANGE:   screen_range(&snap);   break;
                        case SCR_LEVEL:   screen_level(&snap);   break;
                        case SCR_CLOCK:   screen_clock(&snap);   break;
                        default:          screen_system(&snap);  break;
                }

                draw_screen_dots(s_screen);

                /* Инверсия всего кадра одним прямоугольником цвета 2 —
                 * то, ради чего у color вообще есть третье значение. */
                if (s_inverted) {
                        hexa_oled_fill_rect(0, 0, HEXA_OLED_WIDTH, HEXA_OLED_HEIGHT, 2);
                }

                hexa_oled_flush();

                vTaskDelayUntil(&next, pdMS_TO_TICKS(UI_PERIOD_MS));
        }
}

static void detect(void)
{
        uint8_t found[128] = { 0 };
        int n = hexa_i2c_scan(found);

        xprintf("i2c: %d device(s)\n", n);
        for (int a = 0; a < 128; a++) {
                if (found[a]) {
                        xprintf("  0x%02x\n", a);
                }
        }

        /* Датчик считается рабочим, только если и откликнулся на шине, и
         * успешно инициализировался. Одного отклика мало: чужой чип на том
         * же адресе ответит, а настроиться не сможет. */
        s_present.oled    = found[HEXA_OLED_ADDR]   && hexa_oled_init()    == HEXA_OK;
        s_present.aht20   = found[HEXA_AHT20_ADDR]  && hexa_aht20_init()   == HEXA_OK;
        s_present.bmp280  = found[HEXA_BMP280_ADDR] && hexa_bmp280_init()  == HEXA_OK;
        s_present.mpu     = found[HEXA_MPU_ADDR]    && hexa_mpu_init()     == HEXA_OK;
        s_present.rv3032  = found[HEXA_RV3032_ADDR] && hexa_rv3032_init()  == HEXA_OK;
        s_present.vl53l0x = found[HEXA_VL53L0X_ADDR] && hexa_vl53l0x_init() == HEXA_OK;

        xprintf("present: oled=%d aht=%d bmp=%d mpu=%d rtc=%d tof=%d\n",
                s_present.oled, s_present.aht20, s_present.bmp280,
                s_present.mpu, s_present.rv3032, s_present.vl53l0x);
}

static void sensor_task(void *arg)
{
        (void)arg;
        TickType_t next = xTaskGetTickCount();

        for (;;) {
                sensors_t v = { 0 };

                if (s_rescan) {
                        s_rescan = false;
                        detect();
                }

                /* Опрос идёт в локальную копию, без захвата мьютекса:
                 * measure() у AHT20 и VL53L0X ждёт десятки миллисекунд, и
                 * всё это время интерфейс был бы заперт. Под мьютексом —
                 * только присваивание в конце. */
                if (s_present.aht20) {
                        hexa_aht20_measure(&v.t_c100, &v.rh_c100);
                }
                if (s_present.bmp280) {
                        hexa_bmp280_measure(&v.bt_c100, &v.p_pa);
                }
                if (s_present.mpu) {
                        hexa_mpu_read_accel(&v.ax, &v.ay, &v.az);
                }
                if (s_present.vl53l0x) {
                        v.dist_ok = (hexa_vl53l0x_measure(&v.dist_mm) == HEXA_OK);
                }
                if (s_present.rv3032) {
                        v.time_ok = (hexa_rv3032_get_time(&v.now) == HEXA_OK);
                }

                xSemaphoreTake(s_lock, portMAX_DELAY);
                v.cycles = s_data.cycles + 1;
                s_data = v;
                xSemaphoreGive(s_lock);

                vTaskDelayUntil(&next, pdMS_TO_TICKS(SENSOR_PERIOD_MS));
        }
}

static void input_task(void *arg)
{
        (void)arg;
        TickType_t next = xTaskGetTickCount();
        uint8_t beep = 0;               /* сколько тиков ещё звучать */

        for (;;) {
                hexa_button_scan();

                if (hexa_button_pressed(0)) {
                        s_screen = (uint8_t)((s_screen + SCR_COUNT - 1) % SCR_COUNT);
                        beep = 3;
                }
                if (hexa_button_pressed(1)) {
                        s_screen = (uint8_t)((s_screen + 1) % SCR_COUNT);
                        beep = 3;
                }
                if (hexa_button_pressed(2)) {
                        s_sound = !s_sound;
                }
                if (hexa_button_pressed(3)) {
                        s_inverted = !s_inverted;
                        beep = 3;
                }
                if (hexa_button_pressed(4)) {
                        s_rescan = true;
                        beep = 6;
                }

                /* Щелчок отмеряется тиками, а не задержкой: задача кнопок
                 * не имеет права заснуть на длину ноты — за это время
                 * потеряется следующее нажатие. */
                if (beep) {
                        if (s_sound) {
                                hexa_buzzer_tone(2200);
                        }
                        if (--beep == 0) {
                                hexa_buzzer_off();
                        }
                }

                vTaskDelayUntil(&next, pdMS_TO_TICKS(INPUT_PERIOD_MS));
        }
}

static void splash(void)
{
        hexa_oled_clear();
        hexa_oled_bitmap(52, 8, 24, 24, logo_hexa, 1);
        hexa_oled_print("hexacore", 40, 38, 1);
        hexa_oled_print("demo", 52, 48, 1);
        hexa_oled_rect(0, 0, HEXA_OLED_WIDTH, HEXA_OLED_HEIGHT, 1);
        hexa_oled_flush();

        hexa_delay_ms(1200);
}

void app_main(void)
{
        hexa_board_init();
        hexa_time_init();

        xprintf("\n=== hexacore demo ===\n");

        detect();
        hexa_buttons_init();
        hexa_buzzer_init();

        if (s_present.oled) {
                splash();
        } else {
                xprintf("oled absent, uart only\n");
        }

        s_lock = xSemaphoreCreateMutex();
        if (s_lock == NULL) {
                xprintf("mutex alloc FAIL\n");
                return;
        }

        /* Куча под задачи тесная — 8 КБ на всё. Если стека не хватило,
         * xTaskCreate вернёт ошибку здесь, а не превратится в загадочное
         * зависание после старта планировщика. */
        bool ok = true;

        /* Складывать коды через &= нельзя: при нехватке памяти
         * xTaskCreate возвращает -1, а -1 & pdPASS — это pdPASS. */
        ok &= (xTaskCreate(input_task,  "input",  INPUT_STACK,  NULL, PRIO_INPUT,  NULL) == pdPASS);
        ok &= (xTaskCreate(sensor_task, "sensor", SENSOR_STACK, NULL, PRIO_SENSOR, NULL) == pdPASS);
        ok &= (xTaskCreate(ui_task,     "ui",     UI_STACK,     NULL, PRIO_UI,     NULL) == pdPASS);

        if (!ok) {
                xprintf("task alloc FAIL: не хватило configTOTAL_HEAP_SIZE\n");
                return;
        }

        /* Куча тут действительно на пределе, поэтому остаток печатается:
         * если после старта видно меньше пары сотен байт — стеки задач
         * выше подрезаны слишком оптимистично. К показанному числу можно
         * мысленно прибавить 2 КБ: столько вернёт стек самой app_main,
         * когда bsp удалит её задачу сразу после выхода отсюда. */
        xprintf("heap free: %u B (+2048 B после выхода из app_main)\n",
                (unsigned)xPortGetFreeHeapSize());
        xprintf("running: btn0/1 screen, btn2 sound, btn3 invert, btn4 rescan\n");

        /* Возврат из app_main — это не конец программы: bsp_main.c
         * удалит задачу-обёртку, и её стек (2 КБ) вернётся в кучу.
         * Работать дальше будут три созданные выше задачи. */
}
