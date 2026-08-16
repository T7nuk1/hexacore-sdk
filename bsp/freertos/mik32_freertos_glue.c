/*
 * Copyright (C) 2023, Syntacore Ltd.
 * All Rights Reserved.
 */

/*
*  SCRx FreeRTOS Demo
*  @copyright Copyright (C) 2019, Syntacore Ltd.
*  All Rights Reserved.
*  @brief FreeRTOS SCR-specific handlers and more
*/

#include <FreeRTOS.h>
#include <task.h>
#include <stdio.h>

#include "xprintf.h"

/*-----------------------------------------------------------*/

void vApplicationMallocFailedHook( void )
{
    taskDISABLE_INTERRUPTS();
    xprintf("\nApplication Malloc Failed!\n");
    while(1) {}
}
/*-----------------------------------------------------------*/

void vAssertCalled( void )
{
    register unsigned long ra asm ("ra");

    taskDISABLE_INTERRUPTS();
    xprintf("\nvAssertCalled (ret addr= 0x%lx)!\n", ra);

    vTaskEndScheduler();
}

void print_exception(uint32_t cause)
{
	portDISABLE_INTERRUPTS();
	uint32_t mepc = read_csr(mepc);
	uint32_t mtval = read_csr(mtval);
	xprintf("cause: 0x%08x\n", cause);
	if (cause == 0x00000001)
	{
		xprintf("instruction access fault\n");
	}
	else if (cause == 0x00000002)
	{
		xprintf("illegal instruction\n");
	}
	xprintf("pc 0x%08x\n", mepc);
	xprintf("inst 0x%08x\n", mtval);

	uint32_t sp_reg;
	asm volatile(
		"add %0, x0, sp"
		"\n\t"
		: "=r"(sp_reg));
	xprintf("SP = 0x%08X\n", sp_reg);

	while (1)
	{
	}
}

// Обработчик исключений.
void freertos_risc_v_application_exception_handler(void)
{
	uint32_t cause = read_csr(mcause);
	if (!(cause & 0x80000000))
	{
		print_exception(cause);
	}
}

/* mtvec в SCR1 (MIK32) фиксирован (WARL) -> crt0 ставит его на trap_entry,
 * тот делает j raw_trap_handler. raw_trap_handler в crt0 объявлен как .weak -
 * перебиваем его здесь и уводим в порт FreeRTOS. */

extern void freertos_risc_v_trap_handler(void);

__attribute__((naked, section(".ram_text")))
void raw_trap_handler(void)
{
        asm volatile ("j freertos_risc_v_trap_handler");
}