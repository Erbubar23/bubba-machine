/* Copia de cpu/wl82/debug.c del SDK AC79 de JieLi (Apache-2.0), con un cambio de Bubba Machine en
 * exception_analyze: la falla deja su huella en la RAM que sobrevive al reinicio y el chip se
 * reinicia (hal/fm1/fm1_scanhw.c la lee al arrancar y la pasa al registro de la consola). */
/**
 * @file debug.c
 * @brief DebugModule
 * @author chenrixin@zh-jieli.net
 * @version 0.1.00
 * @date 2016-10-27
 */
#include "system/includes.h"
#include "asm/debug.h"
#include "asm/p33.h"
#include "asm/clock.h"
#include "asm/crc16.h"
#include "asm/power_interface.h"
#include "asm/sfc_norflash_api.h"
#include "asm/system_reset_reason.h"
#include "system/boot.h"
#include "app_config.h"
#include "fs/fs.h"

#define EXPCPTION_IN_SRAM 		0			//如果出现死机不复位的情况务必需要打开，防止flash挂了,进入异常函数失败复位不了
#define DEBUG_UART				JL_UART2  	//debug串口

extern const char *os_current_task_rom();
extern void log_output_release_deadlock(void);
extern void exception_irq_handler();
extern void sdtap_init(u32 ch);
extern void __wdt_clear(void);
extern void __cpu_reset(void);
static u8 debug_index;

static const char *const debug_msg[32] = {
    /*0---7*/
    "wdt_timout_err",
    "prp_rd_mmu_err",
    "prp_wr_limit_err",
    "prp_wr_mmu_err",
    "axi_rd_inv",
    "axi_wr_inv",
    "c1_rd_mmu_expt",
    "c0_rd_mmu_expt",
    /*8---15*/
    "c1_wr_mmu_expt",
    "c0_wr_mmu_expt",
    "c1_pc_limit_err_r",
    "c1_wr_limit_err_r",
    "c0_pc_limit_err_r",
    "c0_wr_limit_err_r",
    "dmc_pnd",
    "dbg_pnd",
    /*16---23*/
    "c1_if_bus_inv",
    "c1_rd_bus_inv",
    "c1_wr_bus_inv",
    "c0_if_bus_inv",
    "c0_rd_bus_inv",
    "c0_wr_bus_inv",
    "reserved",
    "reserved",
    /*24---31*/
    "reserved",
    "reserved",
    "reserved",
    "reserved",
    "reserved",
    "reserved",
    "reserved",
    "reserved",
};

static const char *const emu_msg[32] = {
    "reserved",     //31
    "reserved",     //30
    "reserved",     //29
    "reserved",     //28

    "reserved",     //27
    "reserved",     //26
    "reserved",     //25
    "reserved",     //24

    "reserved",     //23
    "reserved",     //22
    "reserved",     //21
    "fpu_ine_err",      //20

    "fpu_huge_err",     //19
    "fpu_tiny",     //18
    "fpu_inf_err",      //17
    "fpu_inv_err",      //16

    "reserved",     //15
    "reserved",     //14
    "reserved",     //13
    "reserved",     //12

    "reserved",     //11
    "reserved",     //10
    "reserved",     //9
    "etm_wp0_err",     //8

    "reserved",     //7
    "reserved",     //6
    "reserved",     //5
    "reserved",     //4

    "stackoverflow",     //3
    "div0_err",     //2
    "illeg_err",        //1
    "misalign_err",     //0
};

#ifdef CONFIG_SAVE_EXCEPTION_LOG_IN_FLASH

static u32 exception_log_flash_addr sec(.volatile_ram);
static char exception_log_buf[640];

#define LOG_PATH "mnt/sdfile/EXT_RESERVED/log"

static int get_exception_log_flash_addr(void)
{
    struct vfs_attr file_attr;

    FILE *fp = fopen(LOG_PATH, "r");
    if (fp == NULL) {
        return -1;
    }
    fget_attrs(fp, &file_attr);
    fclose(fp);
    exception_log_flash_addr = file_attr.sclust;
    exception_log_buf[0] = -1;
    sdfile_reserve_zone_read(exception_log_buf, exception_log_flash_addr, sizeof(exception_log_buf), 0);
    if (exception_log_buf[0] != -1) {
        printf("last exception log : \n%s\n", exception_log_buf);
        sdfile_reserve_zone_erase(exception_log_flash_addr, SDFILE_SECTOR_SIZE, 0);
    }
    memset(exception_log_buf, 0, sizeof(exception_log_buf));
    return 0;
}
platform_initcall(get_exception_log_flash_addr);

static void write_exception_log_to_flash(void *data, u32 len)
{
    extern void set_os_init_flag(u8 value);
    set_os_init_flag(0);
    extern void norflash_set_write_cpu_hold(u8 hold_en);
    norflash_set_write_cpu_hold(0);
    /* sdfile_reserve_zone_erase(exception_log_flash_addr, SDFILE_SECTOR_SIZE, 0); */
    sdfile_reserve_zone_write(data, exception_log_flash_addr, len, 0);
}
#endif

//使用此功能必须确保软件上没有出现异常的可能性，电源供电必须足够稳定
#ifdef CONFIG_EXCEPTION_AUTO_FIX_ENABLE

#define FIX_PATH "mnt/sdfile/EXT_RESERVED/fix"
#define EXCEPTION_FIX_MAGIC    				0x3CE8A924
#define EXCEPTION_FIX_INFO_OFFSET			1024
#define EXCEPTION_FIX_CRC_OFFSET			512
#define EXCEPTION_FIX_MAX_STEP 				2	//取以下MAX STEP的最大值
#define EXCEPTION_FIX_VDDIOM_MAX_STEP 		2
#define EXCEPTION_FIX_VDDIOW_MAX_STEP 		0
#define EXCEPTION_FIX_VDC14_MAX_STEP  		0
#define EXCEPTION_FIX_SYSVDD_MAX_STEP 		1
#define EXCEPTION_FIX_SDRAM_CLK_MAX_STEP	1

struct exception_fix_info_t {
    u32 sdram_clock;
    u32 sys_clock;
    u8 sdram_clock_fix_step;
    u8 sys_clock_fix_step;
    u8 vddiom;
    u8 vddiom_fix_step;
    u8 vddiow;
    u8 vddiow_fix_step;
    u8 vdc14;
    u8 vdc14_fix_step;
    u8 sysvdd;
    u8 sysvdd_fix_step;
};

static u32 exception_fix_flash_addr sec(.volatile_ram);

int exception_auto_fix_config(void)
{
    struct vfs_attr file_attr;
    struct exception_fix_info_t *info;
    u16 crc;

    FILE *fp = fopen(FIX_PATH, "r");
    if (fp == NULL) {
        return -1;
    }
    fget_attrs(fp, &file_attr);
    fclose(fp);

    exception_fix_flash_addr = file_attr.sclust;

    u32 *buf = zalloc(1024 * 4);
    if (!buf) {
        return -1;
    }

    if (sdfile_reserve_zone_read(buf, exception_fix_flash_addr, 1024 * 4, 0) != 1024 * 4) {
        goto __exit;
    }

    info = (struct exception_fix_info_t *)&buf[EXCEPTION_FIX_INFO_OFFSET / sizeof(u32)];

    crc = CRC16(info, sizeof(struct exception_fix_info_t));
    if (crc != (buf[EXCEPTION_FIX_CRC_OFFSET / sizeof(u32)] & 0xffff) ||
        info->sdram_clock > clk_get("sdram") || info->sdram_clock < 120 * 1000000 ||
        info->vddiom < TCFG_LOWPOWER_VDDIOM_LEVEL || info->vddiom > VDDIOM_VOL_MAX ||
        info->vddiow < TCFG_LOWPOWER_VDDIOW_LEVEL || info->vddiow > VDDIOW_VOL_MAX ||
        info->vdc14 < VDC14_VOL_SEL_LEVEL || info->vdc14 > VDC14_VOL_SEL_MAX ||
        info->sysvdd < SYSVDD_VOL_SEL_LEVEL || info->sysvdd > SYSVDD_VOL_SEL_MAX) {
        memset(info, 0, sizeof(struct exception_fix_info_t));
        info->sdram_clock = clk_get("sdram");
        info->vddiom = TCFG_LOWPOWER_VDDIOM_LEVEL;
        info->vddiow = TCFG_LOWPOWER_VDDIOW_LEVEL;
        info->vdc14 = VDC14_VOL_SEL_LEVEL;
        info->sysvdd = SYSVDD_VOL_SEL_LEVEL;
    }

    if (buf[0] != 0xffffffff) {
        printf("find exception fix flag");

        sdfile_reserve_zone_erase(exception_fix_flash_addr, SDFILE_SECTOR_SIZE, 0);

#if 0	//此等待延时是为了避免插拔上电不稳定导致的异常从而误触发
        if (crc == (buf[EXCEPTION_FIX_CRC_OFFSET / sizeof(u32)] & 0xffff)) {
            sdfile_reserve_zone_write(&crc, exception_fix_flash_addr + EXCEPTION_FIX_CRC_OFFSET, sizeof(crc), 0);
            sdfile_reserve_zone_write(info, exception_fix_flash_addr + EXCEPTION_FIX_INFO_OFFSET, sizeof(struct exception_fix_info_t), 0);
            os_time_dly(100);
            sdfile_reserve_zone_erase(exception_fix_flash_addr, SDFILE_SECTOR_SIZE, 0);
        }
#endif

        if (buf[0] == EXCEPTION_FIX_MAGIC && system_reset_reason_get() == SYS_RST_SOFT) {
            for (int i = 0; i <= EXCEPTION_FIX_MAX_STEP; ++i) {
                if (info->vddiom_fix_step < i && info->vddiom_fix_step < EXCEPTION_FIX_VDDIOM_MAX_STEP) {
                    info->vddiom_fix_step++;
                    info->vddiom++;
                    break;
                }
                if (info->sysvdd_fix_step < i && info->sysvdd_fix_step < EXCEPTION_FIX_SYSVDD_MAX_STEP) {
                    info->sysvdd_fix_step++;
                    info->sysvdd++;
                    break;
                }
                if (info->vddiow_fix_step < i && info->vddiow_fix_step < EXCEPTION_FIX_VDDIOW_MAX_STEP) {
                    info->vddiow_fix_step++;
                    info->vddiow++;
                    break;
                }
                if (info->vdc14_fix_step < i && info->vdc14_fix_step < EXCEPTION_FIX_VDC14_MAX_STEP) {
                    info->vdc14_fix_step++;
                    info->vdc14++;
                    break;
                }
                //最后才降sdram时钟
                if (i == EXCEPTION_FIX_MAX_STEP) {
                    if (info->sdram_clock_fix_step < i && info->sdram_clock_fix_step < EXCEPTION_FIX_SDRAM_CLK_MAX_STEP) {
                        u32 sdram_clk_tab[4] = {120 * 1000000, 160 * 1000000, 192 * 1000000, 240 * 1000000};
                        for (int k = ARRAY_SIZE(sdram_clk_tab) - 1; k >= 0; --k) {
                            if (sdram_clk_tab[k] == clk_get("sdram") && info->sdram_clock_fix_step < k) {
                                info->sdram_clock_fix_step++;
                                info->sdram_clock = sdram_clk_tab[k - info->sdram_clock_fix_step];
                                break;
                            }
                        }
                        break;
                    }
                }
            }

            crc = CRC16(info, sizeof(struct exception_fix_info_t));
            buf[EXCEPTION_FIX_CRC_OFFSET / sizeof(u32)] = crc;
        }

        if (crc == (buf[EXCEPTION_FIX_CRC_OFFSET / sizeof(u32)] & 0xffff)) {
            sdfile_reserve_zone_write(&crc, exception_fix_flash_addr + EXCEPTION_FIX_CRC_OFFSET, sizeof(crc), 0);
            sdfile_reserve_zone_write(info, exception_fix_flash_addr + EXCEPTION_FIX_INFO_OFFSET, sizeof(struct exception_fix_info_t), 0);
        }
    }

    if (info->vddiom > TCFG_LOWPOWER_VDDIOM_LEVEL && info->vddiom < VDDIOM_VOL_MAX) {
        printf("exception fix vddiom : %d", info->vddiom);
        VDDIOM_VOL_SEL(info->vddiom);
    }
    if (info->vddiow > TCFG_LOWPOWER_VDDIOW_LEVEL && info->vddiow < VDDIOW_VOL_MAX) {
        printf("exception fix vddiow : %d", info->vddiow);
        VDDIOW_VOL_SEL(info->vddiow);
    }
    if (info->vdc14 > VDC14_VOL_SEL_LEVEL && info->vdc14 < VDC14_VOL_SEL_MAX) {
        printf("exception fix vdc14 : %d", info->vdc14);
        VDC14_VOL_SEL(info->vdc14);
    }
    if (info->sysvdd > SYSVDD_VOL_SEL_LEVEL && info->vdc14 < SYSVDD_VOL_SEL_MAX) {
        printf("exception fix sysvdd : %d", info->sysvdd);
        SYSVDD_VOL_SEL(info->sysvdd);
    }
    if (info->sdram_clock < clk_get("sdram")) {
        printf("exception fix sdram clock : %d", info->sdram_clock);
        sdram_clock_set(info->sdram_clock);
    }

__exit:
    free(buf);

    return 0;
}

static void write_exception_fix_to_flash(void)
{
    extern void set_os_init_flag(u8 value);
    set_os_init_flag(0);

    struct vm_info vm = {0};
    norflash_ioctl(NULL, IOCTL_GET_VM_INFO, (u32)&vm);

    if (!exception_fix_flash_addr || exception_fix_flash_addr < vm.vm_saddr + vm.vm_size) {
        return;
    }

    u32 magic = EXCEPTION_FIX_MAGIC;
    extern void norflash_set_write_cpu_hold(u8 hold_en);
    norflash_set_write_cpu_hold(0);
    sdfile_reserve_zone_write(&magic, exception_fix_flash_addr, sizeof(magic), 0);
}
#endif

static void trace_call_stack(int *len)
{
    for (int c = 0; c < CPU_CORE_NUM; c++) {
        printf("CPU%d %x --> %x --> %x --> %x\n", c,
               q32DSP(c)->ETM_PC3, q32DSP(c)->ETM_PC2,
               q32DSP(c)->ETM_PC1, q32DSP(c)->ETM_PC0);
#ifdef CONFIG_SAVE_EXCEPTION_LOG_IN_FLASH
        *len += snprintf(exception_log_buf + *len, sizeof(exception_log_buf) - *len, "CPU%d %x --> %x --> %x --> %x\r\n", c,
                         q32DSP(c)->ETM_PC3, q32DSP(c)->ETM_PC2,
                         q32DSP(c)->ETM_PC1, q32DSP(c)->ETM_PC0);
#endif
    }
}

#if EXPCPTION_IN_SRAM

AT(.volatile_ram_code)
static char excep_str[] = "\n\n---------------------exception error ------------------------\n";
AT(.volatile_ram_code)
static char debug_msg_str[32][32] = {
    /*0---7*/
    "wdt_timout_err",
    "prp_rd_mmu_err",
    "prp_wr_limit_err",
    "prp_wr_mmu_err",
    "axi_rd_inv",
    "axi_wr_inv",
    "c1_rd_mmu_expt",
    "c0_rd_mmu_expt",
    /*8---15*/
    "c1_wr_mmu_expt",
    "c0_wr_mmu_expt",
    "c1_pc_limit_err_r",
    "c1_wr_limit_err_r",
    "c0_pc_limit_err_r",
    "c0_wr_limit_err_r",
    "dmc_pnd",
    "dbg_pnd",
    /*16---23*/
    "c1_if_bus_inv",
    "c1_rd_bus_inv",
    "c1_wr_bus_inv",
    "c0_if_bus_inv",
    "c0_rd_bus_inv",
    "c0_wr_bus_inv",
    "reserved",
    "reserved",
    /*24---31*/
    "reserved",
    "reserved",
    "reserved",
    "reserved",
    "reserved",
    "reserved",
    "reserved",
    "reserved",
};
extern void putbyte(char a);
AT(.volatile_ram_code)
static void uart_putchar(char a)
{
    if (DEBUG_UART->CON0 & BIT(0)) {
        if (a == '\n') {
            DEBUG_UART->CON0 |= BIT(13);
            DEBUG_UART->BUF = '\r';
            __asm_csync();
            while ((DEBUG_UART->CON0 & BIT(15)) == 0);
        }
        DEBUG_UART->CON0 |= BIT(13);
        DEBUG_UART->BUF = a;
        __asm_csync();
        while ((DEBUG_UART->CON0 & BIT(15)) == 0);
    }
}
AT(.volatile_ram_code)
static void putbuffer(char *buf)
{
    while (*buf != 0) {
        uart_putchar(*buf++);
    }
}
AT(.volatile_ram_code)
static void puthex(int hex)
{
    uart_putchar(' ');
    uart_putchar('0');
    uart_putchar('x');
    for (char i = 28, p = 0, f = 0; i >= 0; i -= 4) {
        p = (char)(hex >> i) & 0xF;
        if (p || f || i == 0) {
            f = 1;
            uart_putchar(p > 9 ? ('A' + (p - 10)) : ('0' + p));
        }
    }
}
AT(.volatile_ram_code)
static void putretis(unsigned int reti, unsigned int rets)
{
    uart_putchar('r');
    uart_putchar('e');
    uart_putchar('t');
    uart_putchar('i');
    uart_putchar(':');
    puthex(reti);
    uart_putchar('\n');
    uart_putchar('r');
    uart_putchar('e');
    uart_putchar('t');
    uart_putchar('s');
    uart_putchar(':');
    puthex(rets);
    uart_putchar('\n');
}
AT(.volatile_ram_code)
static void putstrandhex(char c0, char c1, char c2, char c3, char c4, char c5, char c6, char c7, char c8, char c9, int hex, int hex1)
{
    char bf[10];
    bf[0] = c0, bf[1] = c1, bf[2] = c2, bf[3] = c3, bf[4] = c4, bf[5] = c5, bf[6] = c6, bf[7] = c7, bf[8] = c8, bf[9] = c9;
    for (int i = 0; i < sizeof(bf); i++) {
        if (bf[i]) {
            uart_putchar(bf[i]);
        }
    }
    puthex(hex);
    if (hex1) {
        puthex(hex1);
    }
    uart_putchar('\n');
}
AT(.volatile_ram_code)
static void putpchex(int cpu_id)
{
    q32DSP(!cpu_id)->CMD_PAUSE = -1;
    if (cpu_id) {
        corex2(0)->C0_CON &= ~BIT(3);
        corex2(0)->C0_CON |= BIT(1);
    } else {
        corex2(0)->C1_CON &= ~BIT(3);
        corex2(0)->C1_CON |= BIT(1);
    }
    putstrandhex('e', 'r', 'r', ' ', 'C', 'P', 'U', '0' + cpu_id, 0, 0, cpu_id, 0);
    for (int c = 0; c < CPU_CORE_NUM; c++) {
        uart_putchar('C');
        uart_putchar('P');
        uart_putchar('U');
        uart_putchar('0' + c);
        uart_putchar(':');
        puthex(q32DSP(c)->ETM_PC3);
        uart_putchar(' ');
        uart_putchar('-');
        uart_putchar('-');
        uart_putchar('>');
        puthex(q32DSP(c)->ETM_PC2);
        uart_putchar(' ');
        uart_putchar('-');
        uart_putchar('-');
        uart_putchar('>');
        puthex(q32DSP(c)->ETM_PC1);
        uart_putchar(' ');
        uart_putchar('-');
        uart_putchar('-');
        uart_putchar('>');
        puthex(q32DSP(c)->ETM_PC0);
        uart_putchar('\n');
    }
}
AT(.volatile_ram_code)
static void putdebugreg(void)
{
    putstrandhex('D', 'B', 'G', '_', 'M', 'S', 'G', ':', 0, 0, corex2(0)->DBG_MSG, 0);
    for (int i = 0, j = 0, num = 0; i < 32; i++) {
        if (BIT(i) & corex2(0)->DBG_MSG) {
            j = 0;
            if (i == 2 || i == 11 || i == 13) {
                num = j == 2 ? corex2(0)->PRP_WR_LIMIT_ERR_NUM : (j == 11 ? corex2(0)->C1_WR_LIMIT_ERR_NUM : corex2(0)->C0_WR_LIMIT_ERR_NUM);
                while (!(num & BIT(j)) && ++j < 8);
                putstrandhex('W', 'R', '_', 'L', 'I', 'M', 'I', 'T', 'L', ':', ((volatile u32 *)(&corex2(0)->WR_LIMIT0_H))[j], ((volatile u32 *)(&corex2(0)->WR_LIMIT0_L))[j]);
            } else if (i == 10 || i == 12) {
                putstrandhex('P', 'C', '_', 'L', 'M', 'T', '0', 'L', '-', 'H', corex2(0)->PC_LIMIT0_L, corex2(0)->PC_LIMIT0_H);
                putstrandhex('P', 'C', '_', 'L', 'M', 'T', '1', 'L', '-', 'H', corex2(0)->PC_LIMIT1_L, corex2(0)->PC_LIMIT1_H);
            } else if (i == 14) {
                putstrandhex('S', 'D', 'R', ' ', 'D', 'B', 'G', 'I', 'N', 'F', JL_SDR->DBG_INFO, 0);
                putstrandhex('S', 'D', 'R', ' ', 'D', 'B', 'G', 'A', 'D', 'R', JL_SDR->DBG_ADR, 0);
                putstrandhex('S', 'D', 'R', ' ', 'D', 'B', 'G', 'S', 'T', 'A', JL_SDR->DBG_START, 0);
                putstrandhex('S', 'D', 'R', ' ', 'D', 'B', 'G', 'E', 'N', 'D', JL_SDR->DBG_END, 0);
            } else if (i == 15) {
                putstrandhex('A', 'X', 'I', '_', 'L', 'M', 'T', 'L', '_', 'H', JL_DBG->LIMIT_L, JL_DBG->LIMIT_H);
                putstrandhex('W', 'R', '_', 'A', 'L', 'W', ' ', 'I', 'D', '0', JL_DBG->WR_ALLOW_ID0, 0);
                putstrandhex('W', 'R', '_', 'A', 'L', 'W', ' ', 'I', 'D', '1', JL_DBG->WR_ALLOW_ID1, 0);
                putstrandhex('W', 'R', '_', 'L', 'M', 'T', ' ', 'I', 'D', ':',  JL_DBG->WR_ALLOW_ID0, 0);
            }
            uart_putchar('R');
            uart_putchar('e');
            uart_putchar('a');
            uart_putchar('s');
            uart_putchar('o');
            uart_putchar('n');
            uart_putchar(':');
            putbuffer((char *)&debug_msg_str[i]);
        }
    }
    uart_putchar('\n');
    uart_putchar('\n');
}
AT(.volatile_ram_code)
___interrupt
static void exception_analyze_sram(void)
{
#if 0
    u32 rets, reti, cpu_id;
    __asm__ volatile("%0 = rets ;" : "=r"(rets));
    __asm__ volatile("%0 = reti ;" : "=r"(reti));
    __asm__ volatile("%0 = cnum" : "=r"(cpu_id) ::);
    putbuffer(excep_str);
    putretis(reti, rets);
    putpchex(cpu_id);
    putdebugreg();
#endif
    __wdt_clear();
    __cpu_reset();
}
#endif

void exception_analyze(int *sp)
{
    int len = 0;

    /* Bubba Machine (0.7): la huella, en los 8 bytes del perro guardián (0x01C7FDF8): 0xB0BB y
     *   [2..5] dónde estaba: RETX; si es 0 (un error del bus llega tarde), RETI
     *   [6] bit 0 el núcleo, bit 1 dentro de un trabajo del reparto (fm1_cpu1.c), bits 2-6 la miga
     *       del dibujo (bm_ui_paso, core/bm_ui_draw.c; temporal, en lugar del bit de EMU_MSG, que
     *       en estas fallas viene vacío), bit 7 = el PC es RETI
     *   [7] bits 0-4 el primer bit de DBG_MSG, bits 5-7 la tarea (1 bubba, 2 audio, 3 app_core,
     *       4 sys_event, 5 temporizadores, 6 ninguna, 7 otra)
     * Después, reinicio: en el FM-1 no hay puerto serie para el resto del informe. */
    {
        extern volatile u8 fm1_c1_trabajo[2];
        extern volatile u8 bm_ui_paso;
        volatile u8 *m = (volatile u8 *)0x01C7FDF8;
        u32 retx = (u32)sp[18], reti = (u32)sp[16], cpu = current_cpu_id();
        u32 pc = retx ? retx : reti;
        u32 emu = q32DSP(cpu)->EMU_MSG, dbg = corex2(0)->DBG_MSG;
        u32 eb = 0, db = 0, tk = 6;
        const char *tn = os_current_task_rom();
        while (eb < 31 && !(emu & BIT(eb))) eb++;
        while (db < 31 && !(dbg & BIT(db))) db++;
        if (tn) {
            if (!strcmp(tn, "bubba")) tk = 1;
            else if (!strcmp(tn, "audio")) tk = 2;
            else if (!strcmp(tn, "app_core")) tk = 3;
            else if (!strcmp(tn, "sys_event")) tk = 4;
            else if (!strcmp(tn, "systimer") || !strcmp(tn, "sys_timer")) tk = 5;
            else tk = 7;
        }
        m[0] = 0xBB; m[1] = 0xB0;
        m[2] = (u8)pc; m[3] = (u8)(pc >> 8); m[4] = (u8)(pc >> 16); m[5] = (u8)(pc >> 24);
        m[6] = (u8)((cpu & 1u) | (fm1_c1_trabajo[cpu & 1u] ? 2u : 0u) | ((bm_ui_paso & 31u) << 2) | (retx ? 0u : 0x80u));
        m[7] = (u8)((db & 31u) | (tk << 5));
        cpu_reset();
    }

    log_output_release_deadlock();

    printf("\r\n\r\n---------------------exception error ------------------------\r\n\r\n");

    u32 cpu_id = current_cpu_id();
    if (cpu_id == 0) {
        q32DSP(1)->CMD_PAUSE = -1;
        corex2(0)->C1_CON &= ~BIT(3) ;
        corex2(0)->C1_CON |= BIT(1) ;
        printf("CPU1 run addr = 0x%x \n", q32DSP(1)->PCRS);
        printf("cpu 0 %s DEBUG_MSG = 0x%x, EMU_MSG = 0x%x C0_CON=%x\n",
               __func__, corex2(0)->DBG_MSG, q32DSP(0)->EMU_MSG, corex2(0)->C0_CON);
    } else {
        q32DSP(0)->CMD_PAUSE = -1;
        corex2(0)->C0_CON &= ~BIT(3) ;
        corex2(0)->C0_CON |= BIT(1) ;
        printf("CPU0 run addr = 0x%x \n", q32DSP(0)->PCRS);
        printf("cpu 1 %s DEBUG_MSG = 0x%x, EMU_MSG = 0x%x C1_CON=%x\n",
               __func__, corex2(0)->DBG_MSG, q32DSP(1)->EMU_MSG, corex2(0)->C1_CON);
    }

#ifdef CONFIG_SAVE_EXCEPTION_LOG_IN_FLASH
    len += snprintf(exception_log_buf + len, sizeof(exception_log_buf) - len, "CPU%d run addr = 0x%x\r\ncpu %d DEBUG_MSG = 0x%x, EMU_MSG = 0x%x C%d_CON=%x\r\n",
                    !cpu_id, q32DSP(!cpu_id)->PCRS, cpu_id, corex2(0)->DBG_MSG, q32DSP(0)->EMU_MSG, cpu_id, cpu_id ? corex2(0)->C1_CON : corex2(0)->C0_CON);
#endif

    trace_call_stack(&len);

    unsigned int reti = sp[16];
    unsigned int rete = sp[17];
    unsigned int retx = sp[18];
    unsigned int rets = sp[19];
    unsigned int psr  = sp[20];
    unsigned int icfg = sp[21];
    unsigned int usp  = sp[22];
    unsigned int ssp  = sp[23];
    unsigned int _sp  = sp[24];

    for (int r = 0; r < 16; r++) {
        printf("R%d: %x\n", r, sp[r]);
    }

    printf("icfg: %x \n", icfg);
    printf("psr:  %x \n", psr);
    printf("rets: 0x%x \n", rets);
    printf("reti: 0x%x \n", reti);
    printf("usp : %x, ssp : %x sp: %x\r\n\r\n", usp, ssp, _sp);
#ifdef CONFIG_SAVE_EXCEPTION_LOG_IN_FLASH
    len += snprintf(exception_log_buf + len, sizeof(exception_log_buf) - len, "icfg: %x\r\n", icfg);
    len += snprintf(exception_log_buf + len, sizeof(exception_log_buf) - len, "psr : %x\r\n", psr);
    len += snprintf(exception_log_buf + len, sizeof(exception_log_buf) - len, "rets: 0x%x\r\n", rets);
    len += snprintf(exception_log_buf + len, sizeof(exception_log_buf) - len, "reti: 0x%x\r\n", reti);
    len += snprintf(exception_log_buf + len, sizeof(exception_log_buf) - len, "usp : %x, ssp : %x sp: %x\r\n", usp, ssp, _sp);
#endif

    for (int i = 0; i < 32; i++) {
        if (q32DSP(cpu_id)->EMU_MSG & BIT(i)) {
            printf("%s\r\n", emu_msg[31 - i]);
            if (i == 3) {
                if (os_current_task_rom()) {
                    printf("current_task : %s\r\n", os_current_task_rom());
#ifdef CONFIG_SAVE_EXCEPTION_LOG_IN_FLASH
                    len += snprintf(exception_log_buf + len, sizeof(exception_log_buf) - len, "%s\r\ncurrent_task : %s\r\n", emu_msg[31 - i], os_current_task_rom());
#endif
                }
                printf("usp limit %x  %x\r\n",
                       q32DSP(cpu_id)->EMU_USP_L, q32DSP(cpu_id)->EMU_USP_H);
                printf("ssp limit %x  %x\r\n",
                       q32DSP(cpu_id)->EMU_SSP_L, q32DSP(cpu_id)->EMU_SSP_H);
            }
        }
    }

    if (os_current_task_rom()) {
        printf("current_task : %s\r\n", os_current_task_rom());
#ifdef CONFIG_SAVE_EXCEPTION_LOG_IN_FLASH
        len += snprintf(exception_log_buf + len, sizeof(exception_log_buf) - len, "\r\ncurrent_task : %s\r\n", os_current_task_rom());
#endif
    }

    printf("\n %s AXI_WR_INV_ID : 0x%x, AXI_RD_INV_ID : 0x%x, PRP_WR_LIMIT_ID : 0x%x, PRP_MMU_ERR_RID : 0x%x, PRP_MMU_ERR_WID : 0x%x\n",
           corex2(0)->AXI_RD_INV_ID == 0X100 ? "CPU0" : "CPU1",
           corex2(0)->AXI_WR_INV_ID,
           corex2(0)->AXI_RD_INV_ID,
           corex2(0)->PRP_WR_LIMIT_ID,
           corex2(0)->PRP_MMU_ERR_RID,
           corex2(0)->PRP_MMU_ERR_WID);

#ifdef CONFIG_SAVE_EXCEPTION_LOG_IN_FLASH
    len += snprintf(exception_log_buf + len, sizeof(exception_log_buf) - len, "%s AXI_WR_INV_ID : 0x%x, AXI_RD_INV_ID : 0x%x, PRP_WR_LIMIT_ID : 0x%x, PRP_MMU_ERR_RID : 0x%x, PRP_MMU_ERR_WID : 0x%x\r\n",
                    corex2(0)->AXI_RD_INV_ID == 0X100 ? "CPU0" : "CPU1",
                    corex2(0)->AXI_WR_INV_ID,
                    corex2(0)->AXI_RD_INV_ID,
                    corex2(0)->PRP_WR_LIMIT_ID,
                    corex2(0)->PRP_MMU_ERR_RID,
                    corex2(0)->PRP_MMU_ERR_WID);
#endif

    int i = 0;
    int j = 0;
    int num = 0;

    for (i = 0; i < 32; i++) {
        if (BIT(i) & corex2(0)->DBG_MSG) {
            j = 0;
            if (i == 2) {
                printf("PRP_MMU_ERR_WID %x %x\n",
                       corex2(0)->PRP_MMU_ERR_WID, corex2(0)->PRP_WR_LIMIT_ERR_NUM);
                num = corex2(0)->PRP_WR_LIMIT_ERR_NUM;
                while (!(num & BIT(j)) && ++j < 8);
                printf("\nWR_LIMITH : 0x%x, WR_LIMITL : 0x%x\n",
                       ((volatile u32 *)(&corex2(0)->WR_LIMIT0_H))[j],
                       ((volatile u32 *)(&corex2(0)->WR_LIMIT0_L))[j]);
#ifdef CONFIG_SAVE_EXCEPTION_LOG_IN_FLASH
                len += snprintf(exception_log_buf + len, sizeof(exception_log_buf) - len, "PRP_MMU_ERR_WID %x %x\r\nWR_LIMITH : 0x%x, WR_LIMITL : 0x%x\r\n",
                                corex2(0)->PRP_MMU_ERR_WID, corex2(0)->PRP_WR_LIMIT_ERR_NUM,
                                ((volatile u32 *)(&corex2(0)->WR_LIMIT0_H))[j],
                                ((volatile u32 *)(&corex2(0)->WR_LIMIT0_L))[j]);
#endif
            } else if (i == 11) {
                num = corex2(0)->C1_WR_LIMIT_ERR_NUM;
                while (!(num & BIT(j)) && ++j < 8);
                printf("\nWR_LIMITH : 0x%x, WR_LIMITL : 0x%x\n",
                       ((volatile u32 *)(&corex2(0)->WR_LIMIT0_H))[j],
                       ((volatile u32 *)(&corex2(0)->WR_LIMIT0_L))[j]);
#ifdef CONFIG_SAVE_EXCEPTION_LOG_IN_FLASH
                len += snprintf(exception_log_buf + len, sizeof(exception_log_buf) - len, "WR_LIMITH : 0x%x, WR_LIMITL : 0x%x\r\n",
                                ((volatile u32 *)(&corex2(0)->WR_LIMIT0_H))[j],
                                ((volatile u32 *)(&corex2(0)->WR_LIMIT0_L))[j]);
#endif
            } else if (i == 13) {
                num = corex2(0)->C0_WR_LIMIT_ERR_NUM;
                while (!(num & BIT(j)) && ++j < 8);
                printf("\nWR_LIMITH : 0x%x, WR_LIMITL : 0x%x\n",
                       ((volatile u32 *)(&corex2(0)->WR_LIMIT0_H))[j],
                       ((volatile u32 *)(&corex2(0)->WR_LIMIT0_L))[j]);
#ifdef CONFIG_SAVE_EXCEPTION_LOG_IN_FLASH
                len += snprintf(exception_log_buf + len, sizeof(exception_log_buf) - len, "WR_LIMITH : 0x%x, WR_LIMITL : 0x%x\r\n",
                                ((volatile u32 *)(&corex2(0)->WR_LIMIT0_H))[j],
                                ((volatile u32 *)(&corex2(0)->WR_LIMIT0_L))[j]);
#endif
            } else if (i == 10) {
                printf("\nPC_LIMIT0_H : 0x%x, PC_LIMIT0_L : 0x%x\n",
                       corex2(0)->PC_LIMIT0_H, corex2(0)->PC_LIMIT0_L);
                printf("\nPC_LIMIT1_H : 0x%x, PC_LIMIT1_L : 0x%x\n",
                       corex2(0)->PC_LIMIT1_H, corex2(0)->PC_LIMIT1_L);
#ifdef CONFIG_SAVE_EXCEPTION_LOG_IN_FLASH
                len += snprintf(exception_log_buf + len, sizeof(exception_log_buf) - len, "PC_LIMIT0_H : 0x%x, PC_LIMIT0_L : 0x%x\r\nPC_LIMIT1_H : 0x%x, PC_LIMIT1_L : 0x%x\r\n",
                                corex2(0)->PC_LIMIT0_H, corex2(0)->PC_LIMIT0_L,
                                corex2(0)->PC_LIMIT1_H, corex2(0)->PC_LIMIT1_L);
#endif
            } else if (i == 12) {
                printf("\nPC_LIMIT0_H : 0x%x, PC_LIMIT0_L : 0x%x\n",
                       corex2(0)->PC_LIMIT0_H, corex2(0)->PC_LIMIT0_L);
                printf("\nPC_LIMIT1_H : 0x%x, PC_LIMIT1_L : 0x%x\n",
                       corex2(0)->PC_LIMIT1_H, corex2(0)->PC_LIMIT1_L);
#ifdef CONFIG_SAVE_EXCEPTION_LOG_IN_FLASH
                len += snprintf(exception_log_buf + len, sizeof(exception_log_buf) - len, "PC_LIMIT0_H : 0x%x, PC_LIMIT0_L : 0x%x\r\nPC_LIMIT1_H : 0x%x, PC_LIMIT1_L : 0x%x\r\n",
                                corex2(0)->PC_LIMIT0_H, corex2(0)->PC_LIMIT0_L,
                                corex2(0)->PC_LIMIT1_H, corex2(0)->PC_LIMIT1_L);
#endif
            } else if (i == 14) {
                printf("JL_SDR->DBG_INFO : 0x%x ,err in JL_SDR->DBG_ADR = 0x%x, JL_SDR->DBG_START : 0x%x JL_SDR->DBG_END : 0x%x\n",
                       JL_SDR->DBG_INFO, JL_SDR->DBG_ADR, JL_SDR->DBG_START, JL_SDR->DBG_END);
#ifdef CONFIG_SAVE_EXCEPTION_LOG_IN_FLASH
                len += snprintf(exception_log_buf + len, sizeof(exception_log_buf) - len, "JL_SDR->DBG_INFO : 0x%x ,err in JL_SDR->DBG_ADR = 0x%x, JL_SDR->DBG_START : 0x%x JL_SDR->DBG_END : 0x%x\r\n",
                                JL_SDR->DBG_INFO, JL_SDR->DBG_ADR, JL_SDR->DBG_START, JL_SDR->DBG_END);
#endif
            } else if (i == 15) {
                printf("\nAXI_RAM_LIMIT0_H : 0x%x, AXI_RAM_LIMIT0_L : 0x%x\n",
                       JL_DBG->LIMIT_H, JL_DBG->LIMIT_L);
                printf("\nJL_DBG->WR_ALLOW_ID0 : 0x%x, JL_DBG->WR_ALLOW_ID1 : 0x%x, JL_DBG->WR_LIMIT_ID : 0x%x\n",
                       JL_DBG->WR_ALLOW_ID0, JL_DBG->WR_ALLOW_ID1, JL_DBG->WR_LIMIT_ID);
#ifdef CONFIG_SAVE_EXCEPTION_LOG_IN_FLASH
                len += snprintf(exception_log_buf + len, sizeof(exception_log_buf) - len, "AXI_RAM_LIMIT0_H : 0x%x, AXI_RAM_LIMIT0_L : 0x%x\r\nJL_DBG->WR_ALLOW_ID0 : 0x%x, JL_DBG->WR_ALLOW_ID1 : 0x%x, JL_DBG->WR_LIMIT_ID : 0x%x\r\n",
                                JL_DBG->LIMIT_H, JL_DBG->LIMIT_L,
                                JL_DBG->WR_ALLOW_ID0, JL_DBG->WR_ALLOW_ID1, JL_DBG->WR_LIMIT_ID);
#endif
            }
            printf("exception reason : %s \n", debug_msg[i]);
#ifdef CONFIG_SAVE_EXCEPTION_LOG_IN_FLASH
            len += snprintf(exception_log_buf + len, sizeof(exception_log_buf) - len, "exception reason : %s \n", debug_msg[i]);
#endif
        }
    }

#ifdef CONFIG_EXCEPTION_AUTO_FIX_ENABLE
    write_exception_fix_to_flash();
#endif
#ifdef CONFIG_SAVE_EXCEPTION_LOG_IN_FLASH
    printf("exception log buf len : %d\n", len);
    write_exception_log_to_flash(exception_log_buf, len + 1);
#endif

    printf("system_reset...\r\n\r\n\r\n");
    log_flush();    //只能在异常中断里调用
    __wdt_clear();
#ifdef SDTAP_DEBUG
    sdtap_init(2);//防止IO被占据,重新初始化SDTAP
    while (1);
#endif
#ifndef CONFIG_FPGA_ENABLE
    __cpu_reset();
    while (1);
#endif
}

___interrupt
static void osa_irq_handler(void)
{
    JL_OSA->CON |= BIT(6);
    puts("----------lsb clk lost-----------\n");
}

static void debug_enter_critical(void)
{
    if (corex2(0)->DBG_WR_EN & BIT(0)) {
        return;
    }
    corex2(0)->DBG_WR_EN = 0xe7;//set debug reg write enable
}

static void debug_exit_critical(void)
{
    if (corex2(0)->DBG_WR_EN & BIT(0)) {
        corex2(0)->DBG_WR_EN = 0xe7;//set debug reg write disable
    }
}

static inline int get_debug_index(void)
{
    for (u8 i = 0; i < 8 ; i++) {
        if (!(debug_index & BIT(i))) {
            return i;
        }
    }

    return -1;
}

static void pc0_rang_limit(void *low_addr, void *high_addr)
{
    printf("pc0 :%x---%x\n", (u32)low_addr, (u32)high_addr);

    corex2(0)->DBG_MSG_CLR = 0xffffffff;
    corex2(0)->PC_LIMIT0_H = (u32)high_addr;
    corex2(0)->PC_LIMIT0_L = (u32)low_addr;
}

static void pc1_rang_limit(void *low_addr, void *high_addr)
{
    printf("pc1 :%x---%x\n", (u32)low_addr, (u32)high_addr);

    corex2(0)->DBG_MSG_CLR = 0xffffffff;
    corex2(0)->PC_LIMIT1_H = (u32)high_addr;
    corex2(0)->PC_LIMIT1_L = (u32)low_addr;
}

void pc_rang_limit(void *low_addr0, void *high_addr0, void *low_addr1, void *high_addr1)
{
    debug_enter_critical();

    if (low_addr0 && high_addr0) {
        pc0_rang_limit(low_addr0, high_addr0);
    }

    if (low_addr1 && high_addr1) {
        pc1_rang_limit(low_addr1, high_addr1);
    }

    debug_exit_critical();
}

#define wr_limit_h    ((volatile u32 *)&corex2(0)->WR_LIMIT0_H)
#define wr_limit_l    ((volatile u32 *)&corex2(0)->WR_LIMIT0_L)
#define wr_allow_num  ((volatile u32 *)&corex2(0)->WR_ALLOW_ID0)

int dev_write_range_limit(int limit_index, void *low_addr, void *high_addr, u32 is_allow_write_out, int dev_id)
{
    if (limit_index == -1) {
        limit_index = get_debug_index();
    }
    /*
    	else if (debug_index & BIT(limit_index)) {
            return debug_index;
        }
    */

    if (limit_index == -1) {
        return -1;
    }

    debug_enter_critical();

    debug_index |= BIT(limit_index);

#if 1
    printf("dev %x write %s low_addr:%x--high_addr:%x\n",
           dev_id,
           is_allow_write_out ? ("allow") : ("limit"),
           (u32)low_addr,
           (u32)high_addr);
#endif

    corex2(0)->DBG_MSG_CLR = 0xffffffff;

    wr_limit_h[limit_index] = (u32)high_addr;
    wr_limit_l[limit_index] = (u32)low_addr;

    if (dev_id != -1) {
        corex2(0)->PRP_WR_ALLOW_EN |= BIT(limit_index);
        wr_allow_num[limit_index] = dev_id;
    }
    if (!is_allow_write_out) {
        corex2(0)->PRP_OUTLIM_ERR_EN |= BIT(limit_index);
    }
    corex2(0)->PRP_WR_LIMIT_EN |= BIT(limit_index);

    debug_exit_critical();

#if 1
    printf("WR_LIM%dL:%x--WR_LIM%dH:%x--corex2(0)->DBG_CON:%x--PRP_WR_LIMIT_EN:0x%x\n",
           limit_index,
           wr_limit_l[limit_index],
           limit_index,
           wr_limit_h[limit_index],
           corex2(0)->DBG_CON,
           corex2(0)->PRP_WR_LIMIT_EN);
#endif

    return 0;
}
void all_dev_range_limit(void *addr, int size)//所有外设写进异常(地址:sram+经过cach的sdram)
{
    int limit_index = 0;
    debug_enter_critical();
    corex2(0)->PRP_WR_LIMIT_EN &= ~BIT(limit_index);
    corex2(0)->DBG_MSG_CLR = 0xffffffff;
    debug_index |= BIT(limit_index);
    corex2(0)->WR_LIMIT0_L = (u32)((u32)addr);
    corex2(0)->WR_LIMIT0_H = (u32)((u32)addr + size);
    corex2(0)->WR_ALLOW_ID0 = 0;
    corex2(0)->PRP_WR_LIMIT_EN = BIT(limit_index);
    debug_exit_critical();
}
void all_dev_range_unlimit(void *addr)
{
    debug_enter_critical();
    corex2(0)->DBG_MSG_CLR = 0xffffffff;
    corex2(0)->WR_LIMIT0_L = 0;
    corex2(0)->WR_LIMIT0_H = 0;
    corex2(0)->WR_ALLOW_ID0 = 0;
    corex2(0)->PRP_WR_LIMIT_EN = 0;
    debug_exit_critical();
}
int cpu_write_range_limit(void *low_addr, u32 win_size)
{
    void *high_addr = low_addr + win_size - 1;

    int i = get_debug_index();
    if (i == -1) {
        return -1;
    }

    debug_enter_critical();

    debug_index |= BIT(i);

    corex2(0)->DBG_MSG_CLR = 0xffffffff;

    wr_limit_h[i] = (u32)high_addr;
    wr_limit_l[i] = (u32)low_addr;

    corex2(0)->C0_WR_LIMIT_EN |= BIT(i);
    corex2(0)->C1_WR_LIMIT_EN |= BIT(i);

    debug_exit_critical();

    return 0;
}

void cpu_write_range_unlimit(void *low_addr)
{
    int i;

    debug_enter_critical();

    corex2(0)->DBG_MSG_CLR = 0xffffffff;

    for (i = 0; i < 8; i++) {
        if (wr_limit_l[i] == (u32)low_addr) {
            wr_limit_h[i] = 0;
            wr_limit_l[i] = 0;
            debug_index &= ~BIT(i);
            corex2(0)->C0_WR_LIMIT_EN &= ~BIT(i);
            corex2(0)->C1_WR_LIMIT_EN &= ~BIT(i);
        }
    }

    debug_exit_critical();
}

//使用前一定要先完全flush
int sdr_read_write_range_limit(u32 low_addr, u32 high_addr,
                               int id0, int id1,
                               u8 rd0_permit, u8 wr0_permit,
                               u8 rd1_permit, u8 wr1_permit,
                               u8 wr0_over_area_permit, u8 wr1_over_area_permit)
{
    if (id0 == -1 && id1 == -1) {
        return -1;
    }

    JL_SDR->DBG_INFO = 0;
    JL_SDR->DBG_INFO |= BIT(13);
    if (id0 != -1) {
        JL_SDR->DBG_INFO |= ((id0 >> 6) & 0xf);
        JL_SDR->DBG_INFO &= ~BIT(4);
        JL_SDR->DBG_INFO &= ~BIT(5);
        JL_SDR->DBG_INFO &= ~BIT(14);
        if (rd0_permit) {
            JL_SDR->DBG_INFO |= BIT(5);
        }
        if (wr0_permit) {
            JL_SDR->DBG_INFO |= BIT(4);
        }
        if (wr0_over_area_permit) {
            JL_SDR->DBG_INFO |= BIT(14);
        }
    }
    if (id1 != -1) {
        JL_SDR->DBG_INFO |= ((id1 >> 6) & 0xf) << 6;
        JL_SDR->DBG_INFO &= ~BIT(10);
        JL_SDR->DBG_INFO &= ~BIT(11);
        JL_SDR->DBG_INFO &= ~BIT(15);
        if (rd1_permit) {
            JL_SDR->DBG_INFO |= BIT(11);
        }
        if (wr1_permit) {
            JL_SDR->DBG_INFO |= BIT(10);
        }
        if (wr1_over_area_permit) {
            JL_SDR->DBG_INFO |= BIT(15);
        }
    }
    JL_SDR->DBG_START 	= low_addr;
    JL_SDR->DBG_END 	= high_addr;

    JL_SDR->DBG_INFO |= BIT(12);
    /* matrt id
    ISC0:0
    ISC1:1
    LSB:2
    JPG:3
    CPI0:4
    CPU1:5
    CACH:6
    DMA_COPY:7
    EMI:8
    IMD:9
     */
    return 0;
}

int sdr_read_write_range_unlimit(void)
{
    JL_SDR->DBG_INFO &= ~BIT(12);
    __asm_csync();
    return 0;
}

int axi_ram_write_range_limit(u32 low_addr, u32 high_addr, u32 is_allow_write_out, int id0, int id1)
{
#if 0
    printf("axi_ram id0 %x id1 %x write_out %s low_addr:%x--high_addr:%x\n",
           id0, id1,
           is_allow_write_out ? ("allow") : ("limit"),
           low_addr,
           high_addr);
#endif

    if (low_addr < 0x1bfe000 || low_addr >= 0x1c00000) {
        return -1;
    }

    if (high_addr < 0x1bfe000 || high_addr >= 0x1c00000 || high_addr < low_addr) {
        return -1;
    }

    JL_DBG->LIMIT_H = high_addr;
    JL_DBG->LIMIT_L = low_addr;

    if (id0 == -1 && id1 == -1) {
        JL_DBG->CON &= ~BIT(1);
    } else {
        if (id0 != -1) {
            JL_DBG->WR_ALLOW_ID0 = id0;
            JL_DBG->CON |= BIT(1);
        }
        if (id1 != -1) {
            JL_DBG->WR_ALLOW_ID1 = id1;
            JL_DBG->CON |= BIT(1);
        }
        if (!is_allow_write_out) {
            JL_DBG->CON |= BIT(2);
        }
    }

    JL_DBG->CON |= (BIT(6) | BIT(3) | BIT(0));

#if 0
    printf("WR_LIML:0x%x--WR_LIMH:0x%x-- JL_DBG->CON:0x%x \n",
           JL_DBG->LIMIT_L,
           JL_DBG->LIMIT_H,
           JL_DBG->CON);
#endif

    return 0;
}

void debug_clear(void)
{
    debug_enter_critical();

    JL_SDR->DBG_INFO = 0;
    JL_DBG->CON = 0;

    corex2(0)->DBG_CON = 0;
    corex2(0)->DBG_EN = 0;

    corex2(0)->C0_WR_LIMIT_EN = 0;
    corex2(0)->C1_WR_LIMIT_EN = 0;

    corex2(0)->PRP_WR_LIMIT_EN = 0;
    corex2(0)->PRP_WR_ALLOW_EN = 0;
    corex2(0)->PRP_OUTLIM_ERR_EN = 0;

    corex2(0)->DBG_MSG_CLR = 0xffffffff;

    q32DSP(0)->EMU_MSG = -1;
    q32DSP(1)->EMU_MSG = -1;

    debug_index = 0;

    debug_exit_critical();
}

__attribute__((noinline))
void sp_ovf_unen(void)
{
    q32DSP(OS_CPU_ID)->EMU_CON &= ~ BIT(3);
}

__attribute__((noinline))
void sp_ovf_en(void)
{
#ifndef SDTAP_DEBUG
    q32DSP(OS_CPU_ID)->EMU_CON |= BIT(3); //如果用户使用setjmp longjmp, 或者使用sdtap gdb调试 务必要删掉这句话
#endif
}

void debug_msg_clear(void)
{
    debug_enter_critical();

    JL_DBG->CON |= BIT(6);
    JL_SDR->DBG_INFO |= BIT(11);
    corex2(0)->DBG_CON |= (BIT(17) | BIT(18));
    corex2(0)->DBG_MSG_CLR = 0xffffffff;

    debug_exit_critical();
}

extern u32 cpu0_sstack_begin[];
extern u32 cpu0_sstack_end[];
extern u32 cpu1_sstack_begin[];
extern u32 cpu1_sstack_end[];

void debug_init(void)
{
    u32 cpu_id = current_cpu_id();

#if EXPCPTION_IN_SRAM
    request_irq(IRQ_EXCEPTION_IDX, 7, exception_analyze_sram, 0xff);
#else
    request_irq(IRQ_EXCEPTION_IDX, 7, exception_irq_handler, 0xff);
#endif

    JL_OSA->CON = 0x6D;	//监控lsb_clk
    request_irq(IRQ_OSA_IDX, 7, osa_irq_handler, 0);

#ifdef SDTAP_DEBUG
    sdtap_init(2);
#endif

    q32DSP(cpu_id)->EMU_CON = 0;

    if (cpu_id == 0) {
        debug_clear();

        debug_enter_critical();
        corex2(0)->DBG_EN |= (0x3f << 16);
        corex2(0)->DBG_EN |= (0x3  << 4);
        debug_exit_critical();

        q32DSP(0)->EMU_CON &= ~ BIT(3);
        q32DSP(0)->EMU_SSP_L = (int)cpu0_sstack_begin;
        q32DSP(0)->EMU_SSP_H = (int)cpu0_sstack_end;
        q32DSP(0)->EMU_USP_L = 0;
        q32DSP(0)->EMU_USP_H = -1;
        q32DSP(0)->EMU_CON |= BIT(3);

        printf("cpu 0 usp limit %x  %x\r\n", q32DSP(0)->EMU_USP_L, q32DSP(0)->EMU_USP_H);
        printf("cpu 0 ssp limit %x  %x\r\n", q32DSP(0)->EMU_SSP_L, q32DSP(0)->EMU_SSP_H);
    } else {
#if !defined CONFIG_VIDEO_ENABLE && !defined CONFIG_UI_ENABLE && !defined CONFIG_RTOS_AND_MM_LIB_CODE_SECTION_IN_SDRAM && !defined CONFIG_RF_TRIM_CODE_MOVABLE
        pc_rang_limit(&text_rodata_begin, &text_rodata_end,
                      &ram_text_rodata_begin, &ram_text_rodata_end);
#endif

        q32DSP(1)->EMU_CON &= ~BIT(3);
        q32DSP(1)->EMU_SSP_L = (int)cpu1_sstack_begin;
        q32DSP(1)->EMU_SSP_H = (int)cpu1_sstack_end;
        q32DSP(1)->EMU_USP_L = 0;
        q32DSP(1)->EMU_USP_H = -1;
        q32DSP(1)->EMU_CON |= BIT(3);

        printf("cpu 1 usp limit %x  %x\r\n", q32DSP(1)->EMU_USP_L, q32DSP(1)->EMU_USP_H);
        printf("cpu 1 ssp limit %x  %x\r\n", q32DSP(1)->EMU_SSP_L, q32DSP(1)->EMU_SSP_H);
    }

    /* q32DSP(cpu_id)->EMU_CON |= (0xf << 0) | (0x1 << 8) | (0x1f << 16); */
    /* q32DSP(cpu_id)->EMU_CON |= (0xf << 0) | (0x1 << 8) | (0x06 << 16); */

    q32DSP(cpu_id)->EMU_CON |= (BIT(2) | BIT(8));
    q32DSP(cpu_id)->ETM_CON |= BIT(0);
}

#if 0
static u64 cpu_cnt[CPU_CORE_NUM][4] ;

void cpu_effic_init(void)
{
    debug_enter_critical();
    for (u8 j = 0 ; j < CPU_CORE_NUM ; ++j) {
        if (j == 0) {
            corex2(0)->DBG_CON |= (0x7);	///< enable if_inv_en of_inv_en ex_inv_en
            ((u32 *)(&cpu_cnt[0][0]))[0] = corex2(0)->C0_IF_UACNTL ;
            ((u32 *)(&cpu_cnt[0][0]))[1] = corex2(0)->C0_IF_UACNTH ;
            ((u32 *)(&cpu_cnt[0][1]))[0] = corex2(0)->C0_RD_UACNTL ;
            ((u32 *)(&cpu_cnt[0][1]))[1] = corex2(0)->C0_RD_UACNTH ;
            ((u32 *)(&cpu_cnt[0][2]))[0] = corex2(0)->C0_WR_UACNTL ;
            ((u32 *)(&cpu_cnt[0][2]))[1] = corex2(0)->C0_WR_UACNTH ;
            ((u32 *)(&cpu_cnt[0][3]))[0] = corex2(0)->C0_TL_CKCNTL ;
            ((u32 *)(&cpu_cnt[0][3]))[1] = corex2(0)->C0_TL_CKCNTH ;
        } else {
            corex2(0)->DBG_CON |= (0x7 << 8);	///< enable if_inv_en of_inv_en ex_inv_en
            ((u32 *)(&cpu_cnt[1][0]))[0] = corex2(0)->C1_IF_UACNTL ;
            ((u32 *)(&cpu_cnt[1][0]))[1] = corex2(0)->C1_IF_UACNTH ;
            ((u32 *)(&cpu_cnt[1][1]))[0] = corex2(0)->C1_RD_UACNTL ;
            ((u32 *)(&cpu_cnt[1][1]))[1] = corex2(0)->C1_RD_UACNTH ;
            ((u32 *)(&cpu_cnt[1][2]))[0] = corex2(0)->C1_WR_UACNTL ;
            ((u32 *)(&cpu_cnt[1][2]))[1] = corex2(0)->C1_WR_UACNTH ;
            ((u32 *)(&cpu_cnt[1][3]))[0] = corex2(0)->C1_TL_CKCNTL ;
            ((u32 *)(&cpu_cnt[1][3]))[1] = corex2(0)->C1_TL_CKCNTH ;
        }
    }
    debug_exit_critical();
}

void cpu_effic_calc(void)
{
    u64 cpu_cnt_tmp[CPU_CORE_NUM][4] ;
    u64 all_cnt ;
    u64 effic ;
    int i = 0 ;
    int j ;
    static u8 init_flag = 0;

    if (!init_flag) {
        cpu_effic_init();
        init_flag = 1;
        return;		//初始化后需要隔一段时间
    }

    for (j = 0 ; j < CPU_CORE_NUM ; j++) {
        if (j == 0) {
            ((u32 *)(&cpu_cnt_tmp[0][0]))[0] = corex2(0)->C0_IF_UACNTL ;
            ((u32 *)(&cpu_cnt_tmp[0][0]))[1] = corex2(0)->C0_IF_UACNTH ;
            ((u32 *)(&cpu_cnt_tmp[0][1]))[0] = corex2(0)->C0_RD_UACNTL ;
            ((u32 *)(&cpu_cnt_tmp[0][1]))[1] = corex2(0)->C0_RD_UACNTH ;
            ((u32 *)(&cpu_cnt_tmp[0][2]))[0] = corex2(0)->C0_WR_UACNTL ;
            ((u32 *)(&cpu_cnt_tmp[0][2]))[1] = corex2(0)->C0_WR_UACNTH ;
            ((u32 *)(&cpu_cnt_tmp[0][3]))[0] = corex2(0)->C0_TL_CKCNTL ;
            ((u32 *)(&cpu_cnt_tmp[0][3]))[1] = corex2(0)->C0_TL_CKCNTH ;
        } else if (j == 1) {
            ((u32 *)(&cpu_cnt_tmp[1][0]))[0] = corex2(0)->C1_IF_UACNTL ;
            ((u32 *)(&cpu_cnt_tmp[1][0]))[1] = corex2(0)->C1_IF_UACNTH ;
            ((u32 *)(&cpu_cnt_tmp[1][1]))[0] = corex2(0)->C1_RD_UACNTL ;
            ((u32 *)(&cpu_cnt_tmp[1][1]))[1] = corex2(0)->C1_RD_UACNTH ;
            ((u32 *)(&cpu_cnt_tmp[1][2]))[0] = corex2(0)->C1_WR_UACNTL ;
            ((u32 *)(&cpu_cnt_tmp[1][2]))[1] = corex2(0)->C1_WR_UACNTH ;
            ((u32 *)(&cpu_cnt_tmp[1][3]))[0] = corex2(0)->C1_TL_CKCNTL ;
            ((u32 *)(&cpu_cnt_tmp[1][3]))[1] = corex2(0)->C1_TL_CKCNTH ;
        }
    }

    const char *CNTLX[] = {"IF_UACNT", "RD_UACNT", "WR_UACNT", "TL_CKCNT"};

    for (j = 0 ; j < CPU_CORE_NUM ; j++) {
        all_cnt = 0 ;
        u64 ddd = cpu_cnt_tmp[j][3] - cpu_cnt[j][3] ;
        /*printf("cpu[%d] TL_CKCNT = %d \r\n", j, ddd);*/

        for (i = 0 ; i < 3 ; i++) {
            /*printf("0x%08x%08x , 0x%08x%08x \r\n" ,((u32*) &cpu_cnt[j][i])[1],((u32*) &cpu_cnt[j][i])[0]  ,((u32*)&cpu_cnt_tmp[j][i])[1], ((u32*)&cpu_cnt_tmp[j][i])[0]);*/
            effic = cpu_cnt_tmp[j][i] - cpu_cnt[j][i] ;
            /*printf("sub = %08x%08x \r\n" ,((u32*)&effic)[1] ,((u32*)&effic)[0]  );*/
            all_cnt += effic ;
            /*printf("cpu[%d] %s = %d \r\n", j, CNTLX[i], effic);*/
            printf("cpu[%d] %s effic = %llu%% \r\n", j, CNTLX[i], effic * 100 / ddd) ;
        }

        /*printf("0x%08x%08x , 0x%08x%08x \r\n" ,((u32*) &cpu_cnt[j][i])[1],((u32*) &cpu_cnt[j][i])[0]  ,((u32*)&cpu_cnt_tmp[j][i])[1], ((u32*)&cpu_cnt_tmp[j][i])[0]);*/

        effic =  cpu_cnt_tmp[j][3] - cpu_cnt[j][3] ;

        /*printf("all cnt  = %08x%08x \r\n" ,((u32*)&effic)[1] ,((u32*)&effic)[0]);*/
        effic = (all_cnt * 100)   / effic   ;
        printf("cpu %d TL_CKCNT effic %d%% \r\n", j, (u32)effic) ;
        memcpy((void *)&cpu_cnt[j], (void *)&cpu_cnt_tmp[j], sizeof(u64) * 4) ;
    }
}
#endif
