#define MIK32V2

#include <mik32_hal.h>
#include <mik32_hal_pcc.h>
#include <mik32_hal_gpio.h>
#include <mik32_hal_scr1_timer.h>

#include "hexa_buzzer.h"

#define TIMER_OCR 32000  /* ~1 ms tick @ 32 MHz */

/* Частоты нот, Гц */
#define C4 262
#define D4 294
#define E4 330
#define F4 349
#define G4 392
#define REST 0

typedef struct {
        uint16_t freq;
        uint16_t dur;  /* мс */
} note_t;

/* "Ода к радости" — Бетховен */
static const note_t melody[] = {
        {E4,400}, {E4,400}, {F4,400}, {G4,400},
        {G4,400}, {F4,400}, {E4,400}, {D4,400},
        {C4,400}, {C4,400}, {D4,400}, {E4,400},
        {E4,600}, {D4,200}, {D4,800},

        {E4,400}, {E4,400}, {F4,400}, {G4,400},
        {G4,400}, {F4,400}, {E4,400}, {D4,400},
        {C4,400}, {C4,400}, {D4,400}, {E4,400},
        {D4,600}, {C4,200}, {C4,800},
        {REST,400},
};

#define MELODY_LEN (sizeof(melody) / sizeof(melody[0]))
#define NOTE_GAP 40  /* пауза между нотами, мс */

volatile unsigned int millis = 0;

void system_clock_config(void)
{
        PCC_InitTypeDef PCC_OscInit = {0};

        PCC_OscInit.OscillatorEnable = PCC_OSCILLATORTYPE_ALL;
        PCC_OscInit.FreqMon.OscillatorSystem = PCC_OSCILLATORTYPE_OSC32M;
        PCC_OscInit.FreqMon.ForceOscSys = PCC_FORCE_OSC_SYS_UNFIXED;
        PCC_OscInit.FreqMon.Force32KClk = PCC_FREQ_MONITOR_SOURCE_OSC32K;
        PCC_OscInit.AHBDivider = 0;
        PCC_OscInit.APBMDivider = 0;
        PCC_OscInit.APBPDivider = 0;
        PCC_OscInit.HSI32MCalibrationValue = 128;
        PCC_OscInit.LSI32KCalibrationValue = 8;
        PCC_OscInit.RTCClockSelection = PCC_RTC_CLOCK_SOURCE_AUTO;
        PCC_OscInit.RTCClockCPUSelection = PCC_CPU_RTC_CLOCK_SOURCE_OSC32K;
        HAL_PCC_Config(&PCC_OscInit);
}

void GPIO_Init(void)
{
        __HAL_PCC_GPIO_0_CLK_ENABLE();
}

void trap_handler(void)
{
        millis++;

        __HAL_SCR1_TIMER_SET_TIME(0);
        __HAL_SCR1_TIMER_SET_CMP(TIMER_OCR);
}

static void delay_ms(unsigned int ms)
{
        unsigned int start = millis;
        while (millis - start < ms) { }
}

void app_main(void)
{
        HAL_Init();
        system_clock_config();
        GPIO_Init();

        HAL_SCR1_Timer_Init(HAL_SCR1_TIMER_CLKSRC_INTERNAL, 0);
        __HAL_SCR1_TIMER_SET_CMP(TIMER_OCR);
        __HAL_SCR1_TIMER_IRQ_ENABLE();

        hexa_buzzer_init();

        while (1) {
                for (unsigned int i = 0; i < MELODY_LEN; i++) {
                        if (melody[i].freq == REST) {
                                hexa_buzzer_off();
                                delay_ms(melody[i].dur);
                                continue;
                        }
                        hexa_buzzer_tone(melody[i].freq);
                        delay_ms(melody[i].dur - NOTE_GAP);
                        hexa_buzzer_off();
                        delay_ms(NOTE_GAP);
                }
        }
}
