/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Erbubar23 */
/* Placa: los dispositivos del SDK que usa el port (SPI1 de la pantalla y la detección USB).
 * Sin power_init(): la energía queda como la deja el arranque del chip. */
#include "app_config.h"
#include "system/includes.h"
#include "device/includes.h"
#include "asm/includes.h"
#include "usb/otg.h"

/* SPI1 grupo B: PC9 reloj, PC10 datos (fm1_hw.h). Modo 0, solo salida. */
SPI1_PLATFORM_DATA_BEGIN(spi1_data)
    .clk  = 12000000,
    .mode = SPI_STD_MODE,
    .port = 'B',
    .attr = SPI_SCLK_L_UPL_SMPH | SPI_UNIDIR_MODE,
SPI1_PLATFORM_DATA_END()

/* USB0 solo como dispositivo: avisa cuando hay un computador del otro lado */
static const struct otg_dev_data otg_data = {
    .usb_dev_en = 0x01,
    .slave_online_cnt = 10,
    .slave_offline_cnt = 10,
    .detect_mode = OTG_SLAVE_MODE,
    .detect_time_interval = 50,
};

REGISTER_DEVICES(device_table) = {
    {"spi1", &spi_dev_ops, (void *)&spi1_data},
    {"otg", &usb_dev_ops, (void *)&otg_data},
    {"rtc", &rtc_dev_ops, NULL},
};

void board_early_init(void)
{
    devices_init();
}

void board_init(void)
{
    adc_init();
}
