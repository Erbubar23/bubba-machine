/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Erbubar23 */
/* Power rails of the FM-1 (0.8.1).
 *
 * Until 0.8.0 the firmware never touched power: it ran both cores at 360 MHz on the rails the
 * chip's boot loader leaves, SYSVDD ~1.26 V and VDC14 ~1.40 V. The FM-1 then switched itself off
 * while browsing fast with TWO CORES on (next boot: power-on reset, a voltage drop), and
 * fm1-x0x#10 documents core-1 RAM misreads and freezes at 360 MHz on those rails.
 *
 * M-VAVE's own firmware (V15) sets the analog rails up before it starts core 1; X0X and Melodee
 * replay that setup. The facts (order and levels) are theirs; the code is ours and only uses the
 * register helpers of the JieLi SDK (asm/p33.h, Apache-2.0):
 *   1. LDO13 on, DCDC13 on, LDO13 off (make before break: the 1.3 V supply moves to the DC-DC)
 *   2. VDDIOM 3.2 V, then VDC14 1.60 V (level 7), then VDDIOW 3.2 V
 *   3. last, SYSVDD 1.38 V (level 15)
 * The clock stays at 360 MHz (Melodee's 320 MHz caused PLL faults and was undone).
 *
 * It runs from board_init (hal/fm1/sdk/board.c), before app_main: no task of ours exists yet and
 * core 1 only runs the scheduler's idle task. Interrupts are off on this core while it runs. The
 * voltages only go up, one level at a time, with a short wait after each step. The registers
 * before and after go to the console (fm1_power_regs, consola.py estado). */
#include "app_config.h"
#include "system/includes.h"
#include "asm/includes.h"
#include "asm/p33.h"
#include "asm/power_interface.h"
#include "fm1_hw.h"

#define PASO_US 100u                      /* settling time after each step (~3 ms in all at most) */

uint8_t fm1_power_regs[2][4];

static void leer(uint8_t r[4])
{
    r[0] = p33_rx_1byte(P3_ANA_CON0);
    r[1] = p33_rx_1byte(P3_ANA_CON5);
    r[2] = p33_rx_1byte(P3_ANA_CON6);
    r[3] = p33_rx_1byte(P3_ANA_CON9);
}

void fm1_power_boot(void)
{
    leer(fm1_power_regs[0]);
    local_irq_disable();

    /* 1. the 1.3 V supply from the LDO to the DC-DC, without a gap */
    LDO13_EN(1);
    delay_us(PASO_US);
    DCDC13_EN(1);
    delay_us(PASO_US);
    LDO13_EN(0);
    delay_us(PASO_US);

    /* 2. I/O and the 1.4 V rail (VDC14 feeds SYSVDD: it goes up first) */
    VDDIOM_VOL_SEL(VDDIOM_VOL_32V);
    delay_us(PASO_US);
    for (int v = VDC14_VOL_GET(); v < VDC14_VOL_SEL_160V; v++) {
        VDC14_VOL_SEL(v + 1);
        delay_us(PASO_US);
    }
    VDDIOW_VOL_SEL(VDDIOW_VOL_32V);
    delay_us(PASO_US);

    /* 3. the core rail, last */
    for (int v = GET_SYSVDD_VOL_SEL(); v < SYSVDD_VOL_SEL_138V; v++) {
        SYSVDD_VOL_SEL(v + 1);
        delay_us(PASO_US);
    }

    local_irq_enable();
    leer(fm1_power_regs[1]);
}
