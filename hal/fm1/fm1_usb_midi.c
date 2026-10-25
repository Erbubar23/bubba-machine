/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Erbubar23 */
/* USB-MIDI (clase USB Audio 1.0, MIDIStreaming) sobre la pila USB de dispositivo del SDK.
 *
 * Un puerto MIDI de entrada y uno de salida, endpoints bulk 1 de 64 bytes. El controlador es
 * el USB0 de velocidad completa (el que va al USB-C del FM-1). Los descriptores siguen la
 * especificación "USB Device Class Definition for MIDI Devices 1.0". La puesta en marcha del
 * controlador repite la de apps/common/usb/device/usb_device.c del SDK (Apache-2.0).
 *
 * La interrupción solo guarda los paquetes de 4 bytes que llegan; el armado de SysEx y de
 * mensajes cortos se hace desde las tareas.
 *
 * El envío nunca espera: los eventos van a una cola y salen de a un paquete (64 bytes) cuando el
 * anterior ya salió, desde la interrupción de "paquete enviado" del endpoint (el patrón de la
 * clase HID del SDK, hid.c). Si el computador no lee (ningún programa con el puerto abierto), la
 * cola se llena y lo nuevo se descarta. Antes se usaba usb_g_bulk_write, que espera hasta 200 ms
 * por paquete: con el MIDI clock del núcleo el bucle principal nunca salía de ahí (en el aparato,
 * pantalla congelada en el paso L3 y la enumeración fallando).
 *
 * El controlador se enciende apenas arranca el firmware (fm1_midi_init), sin esperar el aviso
 * de "computador conectado" de la detección del SDK: en el aparato ese aviso no llegó nunca
 * (primera prueba, 2026-10-11: ni el modo seguro apareció en el computador). El FM-1 siempre es
 * dispositivo; con la pull-up puesta y sin computador, no pasa nada. La detección sigue
 * registrada (el SDK la necesita para el reloj del USB) pero ya no apaga ni reinicia nada. */
#include "app_config.h"
#include "system/includes.h"
#include "usb/device/usb_stack.h"
#include "usb_config.h"
#include "event/device_event.h"
#include "asm/includes.h"
#include "os/os_api.h"
#include "fm1_hw.h"

#define MIDI_EP          1
#define MIDI_MAXP        64
#define USB_ID           0

/* ------------------------------------------------------------------ descriptores */
static const u8 DEV_DESC[] = {
    18, USB_DT_DEVICE, 0x10, 0x01,         /* USB 1.1 (velocidad completa) */
    0x00, 0x00, 0x00, 64,
    0x4A, 0x4C, 0x42, 0x4D,                /* VID 0x4C4A (JieLi), PID 0x4D42 */
    0x03, 0x00,                            /* versión 0.3 */
    1, 2, 3, 1,
};
static const u8 LANG_STR[] = {4, 3, 0x09, 0x04};

static const u8 CONFIG_DESC[] = {
    9, USB_DT_CONFIG, 0, 0, 0, 1, 0, 0x80, 50,      /* el total y las interfaces los pone la pila */
};

#define AC_LEN  (9 + 9)
#define MS_CS_LEN (7 + 6 + 6 + 9 + 9)
static const u8 MIDI_DESC[] = {
    /* interfaz 0: Audio Control, sin endpoints */
    9, USB_DT_INTERFACE, 0, 0, 0, 0x01, 0x01, 0x00, 0,
    9, 0x24, 0x01, 0x00, 0x01, 9, 0, 1, 1,            /* cabecera AC: 1 interfaz de streaming (la 1) */
    /* interfaz 1: MIDIStreaming, 2 endpoints */
    9, USB_DT_INTERFACE, 1, 0, 2, 0x01, 0x03, 0x00, 0,
    7, 0x24, 0x01, 0x00, 0x01, (u8)(MS_CS_LEN + 9 + 5 + 9 + 5), (u8)((MS_CS_LEN + 9 + 5 + 9 + 5) >> 8),
    6, 0x24, 0x02, 0x01, 0x01, 0,                     /* MIDI IN embebido, id 1 */
    6, 0x24, 0x02, 0x02, 0x02, 0,                     /* MIDI IN externo, id 2 */
    9, 0x24, 0x03, 0x01, 0x03, 1, 0x02, 0x01, 0,      /* MIDI OUT embebido, id 3 <- 2 */
    9, 0x24, 0x03, 0x02, 0x04, 1, 0x01, 0x01, 0,      /* MIDI OUT externo, id 4 <- 1 */
    9, USB_DT_ENDPOINT, MIDI_EP, 0x02, MIDI_MAXP, 0, 0, 0, 0,          /* bulk OUT */
    5, 0x25, 0x01, 1, 0x01,
    9, USB_DT_ENDPOINT, 0x80 | MIDI_EP, 0x02, MIDI_MAXP, 0, 0, 0, 0,   /* bulk IN */
    5, 0x25, 0x01, 1, 0x03,
};

static void str_desc(u8 *ptr, const char *s)
{
    u32 n = 0;
    while (s[n] && n < 30)
        n++;
    ptr[0] = (u8)(2 + 2 * n);
    ptr[1] = 3;
    for (u32 i = 0; i < n; i++) {
        ptr[2 + 2 * i] = (u8)s[i];
        ptr[3 + 2 * i] = 0;
    }
}

/* lo que la pila del SDK pide a la aplicación */
void get_device_descriptor(u8 *ptr, const usb_dev usb_id) { memcpy(ptr, DEV_DESC, sizeof DEV_DESC); }
void get_language_str(u8 *ptr) { memcpy(ptr, LANG_STR, sizeof LANG_STR); }
void get_manufacture_str(u8 *ptr) { str_desc(ptr, "Bubba Machine"); }
void get_product_str(u8 *ptr) { str_desc(ptr, FM1_USB_PRODUCT); }
void get_iserialnumber_str(u8 *ptr) { str_desc(ptr, "BM-0003"); }
const u8 *usb_get_config_desc(void) { return CONFIG_DESC; }
const u8 *usb_get_string_desc(u32 id) { return NULL; }

/* ------------------------------------------------------------------ estado */
static u8 *ep0_dma, *ep_in_dma, *ep_out_dma;
static volatile u8 configurada, encendida;
static u8 rx_pkt[MIDI_MAXP];

#define RXQ 256                                /* paquetes de 4 bytes */
static u8 rxq[RXQ][4];
static volatile u16 rx_w, rx_r;

static void midi_rx(struct usb_device_t *usb_device, u32 ep)
{
    u32 n = usb_g_bulk_read(USB_ID, MIDI_EP, rx_pkt, MIDI_MAXP, 0), i;
    for (i = 0; i + 4u <= n; i += 4u) {
        u16 w = rx_w, nx = (u16)((w + 1u) % RXQ);
        if (nx == rx_r || !(rx_pkt[i] & 0x0F))
            continue;                          /* cola llena, o paquete vacío */
        memcpy(rxq[w], rx_pkt + i, 4);
        rx_w = nx;
    }
}

/* ------------------------------------------------------------------ cola de envío */
#define TXQ 1024u                              /* bytes: 256 eventos de 4 */
static u8 txq[TXQ];
static volatile u16 tx_w, tx_r;
static volatile u8 tx_ocupado;
static volatile u32 tx_perdidos;

/* carga el siguiente paquete si el endpoint está libre (desde la tarea o la interrupción) */
static void tx_patear(void)
{
    u32 n = 0;
    if (!configurada || tx_ocupado || tx_r == tx_w)
        return;
    while (tx_r != tx_w && n < MIDI_MAXP) {
        ep_in_dma[n++] = txq[tx_r];
        tx_r = (u16)((tx_r + 1u) % TXQ);
    }
    tx_ocupado = 1;
    usb_set_intr_txe(USB_ID, MIDI_EP);
    usb_g_bulk_write(USB_ID, MIDI_EP, NULL, n);   /* el endpoint está libre: no espera */
}

static void midi_tx(struct usb_device_t *usb_device, u32 ep)
{
    tx_ocupado = 0;
    tx_patear();
}

static void midi_endpoints(struct usb_device_t *usb_device, u32 itf)
{
    usb_g_ep_config(USB_ID, MIDI_EP | USB_DIR_IN, USB_ENDPOINT_XFER_BULK, 0, ep_in_dma, MIDI_MAXP);
    usb_g_ep_config(USB_ID, MIDI_EP | USB_DIR_OUT, USB_ENDPOINT_XFER_BULK, 1, ep_out_dma, MIDI_MAXP);
    usb_g_set_intr_hander(USB_ID, MIDI_EP | USB_DIR_OUT, midi_rx);
    usb_g_set_intr_hander(USB_ID, MIDI_EP | USB_DIR_IN, midi_tx);
    usb_enable_ep(USB_ID, MIDI_EP);
    rx_w = rx_r = 0;
    tx_w = tx_r = 0;
    tx_ocupado = 0;
    configurada = 1;
}

static u32 midi_itf(struct usb_device_t *usb_device, struct usb_ctrlrequest *req)
{
    u8 *tx = usb_get_setup_buffer(usb_device);
    if ((req->bRequestType & USB_TYPE_MASK) != USB_TYPE_STANDARD) {
        usb_set_setup_phase(usb_device, USB_EP0_SET_STALL);
        return 0;
    }
    switch (req->bRequest) {
    case USB_REQ_SET_INTERFACE:
        usb_set_setup_phase(usb_device, req->wValue == 0 && usb_device->bDeviceStates == USB_CONFIGURED ?
                            USB_EP0_STAGE_SETUP : USB_EP0_SET_STALL);
        break;
    case USB_REQ_GET_INTERFACE:
        if (req->wValue || req->wLength != 1 || usb_device->bDeviceStates != USB_CONFIGURED) {
            usb_set_setup_phase(usb_device, USB_EP0_SET_STALL);
        } else {
            tx[0] = 0;
            usb_set_data_payload(usb_device, req, tx, 1);
        }
        break;
    default:
        usb_set_setup_phase(usb_device, USB_EP0_SET_STALL);
        break;
    }
    return 0;
}

static u32 midi_desc(const usb_dev usb_id, u8 *ptr, u32 *itf)
{
    memcpy(ptr, MIDI_DESC, sizeof MIDI_DESC);
    ptr[2] = (u8)*itf;                         /* interfaz Audio Control */
    ptr[9 + 8] = (u8)(*itf + 1);               /* la de streaming que controla */
    ptr[AC_LEN + 2] = (u8)(*itf + 1);          /* interfaz MIDIStreaming */
    usb_set_interface_hander(usb_id, *itf, midi_itf);
    usb_set_interface_hander(usb_id, *itf + 1, midi_itf);
    usb_set_reset_hander(usb_id, *itf + 1, midi_endpoints);
    *itf += 2;
    return sizeof MIDI_DESC;
}

/* ------------------------------------------------------------------ encendido */
static void usb_arranca(void)
{
    if (encendida)
        return;
    if (!ep_in_dma) {
        ep_in_dma = usb_alloc_ep_dmabuffer(USB_ID, MIDI_EP | USB_DIR_IN, MIDI_MAXP);
        ep_out_dma = usb_alloc_ep_dmabuffer(USB_ID, MIDI_EP, MIDI_MAXP * 2);
    }
    /* Desconectar del todo antes de conectar: controlador apagado, D+ y D- sueltas, y al menos
     * 20 ms así (dato de X0X, fm1_usb_reset). Después de un reinicio por software (el cargador de
     * actualización reinicia el chip con el USB conectado) el computador no se entera de que el
     * aparato se fue y la enumeración falla (primera prueba: Windows 0000:0004 a las 07:06:34). */
    JL_USB->CON0 = 0;
    JL_USB->CON1 = 0;
    JL_USB_IO->CON0 = 0x0Cu;
    /* Reloj del USB = PLL de 48 MHz (CLK_CON1 bits 0-1 en 0; dato de X0X, fm1_usb_attach). El SDK
     * no lo fija al encender el USB: depende de lo que dejó el arranque. Sin esto el USB anduvo
     * solo después del modo del chip (que lo configura) y falló al prender en frío ("error de
     * solicitud de descriptor", 2026-10-11). */
    JL_CLOCK->CLK_CON1 &= ~3u;
    os_time_dly(10);                          /* 100 ms */
    usb_add_desc_config(USB_ID, MAX_INTERFACE_NUM, NULL);
    usb_add_desc_config(USB_ID, 0, midi_desc);
    /* como usb_device_init() del SDK */
    usb_config(USB_ID);
    usb_g_sie_init(USB_ID);
    usb_write_power(USB_ID, 0x40);             /* velocidad completa */
    usb_slave_init(USB_ID);
    if (!ep0_dma)
        ep0_dma = usb_alloc_ep_dmabuffer(USB_ID, 0, 64);
    for (int ep = 0; ep < 5; ep++)
        usb_set_dma_raddr(USB_ID, ep, ep0_dma);
    usb_write_intr_usbe(USB_ID, INTRUSB_RESET_BABBLE | INTRUSB_SUSPEND);
    usb_clr_intr_txe(USB_ID, -1);
    usb_clr_intr_rxe(USB_ID, -1);
    usb_set_intr_txe(USB_ID, 0);
    usb_set_intr_rxe(USB_ID, 0);
    usb_g_isr_reg(USB_ID, 3, 0);
    encendida = 1;
}

static void __attribute__((unused)) usb_para(void)
{
    if (!encendida)
        return;
    configurada = 0;
    usb_g_hold(USB_ID);
    usb_release(USB_ID);
    usb_sie_close(USB_ID);
    encendida = 0;
}

/* la detección del SDK (dispositivo "otg"): si avisa que hay computador y el USB no estaba
 * encendido, se enciende; el aviso de que se fue no apaga nada */
static void otg_evento(struct sys_event *e)
{
    struct device_event *ev = (struct device_event *)e->payload;
    const char *m = (const char *)ev->value;
    if (e->from != DEVICE_EVENT_FROM_OTG || !m || m[0] != 's')
        return;
    if (ev->event == DEVICE_EVENT_IN)
        usb_arranca();
}

/* lo mismo que el SDK en usb_device.c: la detección lo usa para esperar el SOF del computador.
 * Con el controlador ya encendido no se toca nada (reiniciarlo cortaría la conexión). */
u32 usb_otg_sof_check_init(const usb_dev id)
{
    if (encendida)
        return 1;
    if (!ep0_dma)
        ep0_dma = usb_alloc_ep_dmabuffer(id, 0, 64);
    usb_g_sie_init(id);
    usb_write_intr_usbe(id, 0);
    usb_clr_intr_txe(id, -1);
    usb_clr_intr_rxe(id, -1);
    unrequest_irq(IRQ_USB_CTRL_IDX, 0);
    usb_set_dma_raddr(id, 0, ep0_dma);
    for (int ep = 1; ep < USB_MAX_HW_EPNUM; ep++)
        usb_disable_ep(id, ep);
    usb_sof_clr_pnd(id);
    return 1;
}

void fm1_midi_init(void)
{
    register_sys_event_handler(SYS_DEVICE_EVENT, DEVICE_EVENT_FROM_OTG, 2, otg_evento);
    usb_arranca();
}

int fm1_midi_on(void) { return encendida; }

int fm1_midi_ready(void) { return configurada; }

/* ------------------------------------------------------------------ envío */
static void tx_put(u8 cin, u8 a, u8 b, u8 c)
{
    u16 w = tx_w, nx = (u16)((w + 4u) % TXQ);
    if (nx == tx_r || (u16)((w + 4u) % TXQ) == tx_r) {
        tx_perdidos++;                         /* cola llena: el computador no está leyendo */
        return;
    }
    txq[w] = cin; txq[w + 1u] = a; txq[w + 2u] = b; txq[w + 3u] = c;
    tx_w = nx;
}

int fm1_midi_send(const uint8_t *m, uint32_t n)
{
    uint32_t i = 0;
    if (!configurada || !n)
        return -1;
    CPU_CRITICAL_ENTER();
    if (m[0] == 0xF0) {                        /* SysEx: de a 3 bytes, el último paquete dice cuántos */
        while (n - i > 3) {
            tx_put(0x04, m[i], m[i + 1], m[i + 2]);
            i += 3;
        }
        if (n - i == 3)
            tx_put(0x07, m[i], m[i + 1], m[i + 2]);
        else if (n - i == 2)
            tx_put(0x06, m[i], m[i + 1], 0);
        else
            tx_put(0x05, m[i], 0, 0);
    } else if (m[0] >= 0x80 && m[0] < 0xF0 && n >= 2) {
        tx_put((u8)(m[0] >> 4), m[0], m[1], n > 2 ? m[2] : 0);
    } else if (m[0] >= 0xF8) {
        tx_put(0x0F, m[0], 0, 0);
    }
    tx_patear();
    CPU_CRITICAL_EXIT();
    return 0;
}

uint32_t fm1_midi_lost(void) { return tx_perdidos; }

/* ------------------------------------------------------------------ recepción */
#define SYX_MAX 1024
static u8 syx[SYX_MAX], listo[SYX_MAX];
static u32 syx_n, listo_n;
#define CORTOS 128                             /* mensajes cortos esperando al bucle principal */
static u8 cortos[CORTOS][3];
static u8 c_w, c_r;

static void corto(u8 a, u8 b, u8 c)
{
    u8 nx = (u8)((c_w + 1u) % CORTOS);
    if (nx == c_r)
        return;
    cortos[c_w][0] = a; cortos[c_w][1] = b; cortos[c_w][2] = c;
    c_w = nx;
}

/* procesa paquetes hasta vaciar la cola o completar un SysEx (que queda en `listo`) */
static void bombear(void)
{
    while (rx_r != rx_w && !listo_n) {
        if ((u8)((c_w + 1u) % CORTOS) == c_r)
            break;                             /* llena: el resto espera en rxq (un note-off no se pierde; SLOOP) */
        u8 *p = rxq[rx_r];
        u8 cin = p[0] & 0x0F, k = 0;
        rx_r = (u16)((rx_r + 1u) % RXQ);
        switch (cin) {
        case 0x4: k = 3; break;
        case 0x5: k = 1; break;
        case 0x6: k = 2; break;
        case 0x7: k = 3; break;
        default:
            if ((cin >= 0x8 || cin == 0x2 || cin == 0x3) && (p[1] & 0x80u))   /* sin byte de estado: mal formado */
                corto(p[1], p[2], p[3]);
            continue;
        }
        if (cin == 0x5 && p[1] >= 0xF8) {      /* tiempo real de un byte */
            corto(p[1], 0, 0);
            continue;
        }
        if (p[1] == 0xF0)
            syx_n = 0;
        for (u8 j = 0; j < k; j++)
            if (syx_n < SYX_MAX)
                syx[syx_n++] = p[1 + j];
        if (cin != 0x4 && syx_n && syx[0] == 0xF0 && syx[syx_n - 1] == 0xF7 && syx_n < SYX_MAX) {
            memcpy(listo, syx, syx_n);
            listo_n = syx_n;
        }
        if (cin != 0x4)
            syx_n = 0;
    }
}

int fm1_midi_get_sysex(uint8_t *buf, uint32_t max)
{
    uint32_t n;
    bombear();
    n = listo_n;
    listo_n = 0;
    if (!n || n > max)
        return 0;                              /* nada, o más largo de lo esperado: se descarta */
    memcpy(buf, listo, n);
    return (int)n;
}

int fm1_midi_get_short(uint8_t msg[3])
{
    bombear();
    if (c_r == c_w)
        return 0;
    memcpy(msg, cortos[c_r], 3);
    c_r = (u8)((c_r + 1u) % CORTOS);
    return 1;
}
