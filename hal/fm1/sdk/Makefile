# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 Erbubar23
#
# Firmware de Bubba Machine para el FM-1 sobre el SDK AC79 de JieLi (Apache-2.0).
# Las opciones de compilación y enlace siguen las de apps/demo/demo_hello del SDK;
# el SDK no se copia: se lee desde $(SDK). Corre en Linux (el contenedor de
# tools/build.py), con el toolchain de JieLi en /opt/jieli.
#
#   make SDK=/sdk OUT=/tmp/fm1 APP=diag     firmware de diagnóstico (primer paso en el aparato)
#   make SDK=/sdk OUT=/tmp/fm1 APP=bubba    Bubba Machine completo
#
# Sale $(OUT)/app.bin (lo que va dentro del .fwsc) y $(OUT)/sdk.elf, sdk.map, tamaños.

SDK ?= /sdk
OUT ?= /tmp/fm1
APP ?= diag
HERE := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))
REPO := $(abspath $(HERE)/../../..)

TOOL_DIR := /opt/jieli/pi32v2/bin
CC := $(TOOL_DIR)/clang
LD := $(TOOL_DIR)/lto-wrapper
OBJCOPY := $(TOOL_DIR)/objcopy
OBJDUMP := $(TOOL_DIR)/objdump

CFLAGS := -target pi32v2 -integrated-as -mcpu=r3 -mfprev1 -Wuninitialized -Wno-invalid-noreturn \
	-fno-common -Oz -g -flto -fallow-pointer-null -fprefer-gnu-section -femulated-tls \
	-Wno-shift-negative-value -Wframe-larger-than=2560 -mllvm -pi32v2-large-program=true \
	-fms-extensions -fno-unwind-tables -ffunction-sections -fdata-sections -fmessage-length=0

# lo nuestro se compila con avisos; lo del SDK, como lo compila el SDK (-w)
OWN_WARN := -Wall -Wno-unused-parameter -Wno-unused-function -Wno-missing-braces
SDK_WARN := -w

DEFINES := -DSUPPORT_MS_EXTENSIONS -DCONFIG_RELEASE_ENABLE -DCONFIG_CPU_WL82 -DCONFIG_FREE_RTOS_ENABLE \
	-DCONFIG_THREAD_ENABLE -DCONFIG_AUDIO_ONCHIP -D__GCC_PI32V2__ -DCONFIG_NEW_ECC_ENABLE \
	-DCONFIG_EQ_SUPPORT_ASYNC -DEQ_CORE_V1 -DCONFIG_SFC_ENABLE -DCONFIG_NO_SDRAM_ENABLE \
	-DEVENT_HANDLER_NUM_CONFIG=2 -DEVENT_TOUCH_ENABLE_CONFIG=0 -DEVENT_POOL_SIZE_CONFIG=256 \
	-DCONFIG_EVENT_KEY_MAP_ENABLE=0 -DTIMER_POOL_NUM_CONFIG=15 -DAPP_ASYNC_POOL_NUM_CONFIG=0 \
	-DVFS_ENABLE=1 -DUSE_SDFILE_NEW=1 -DSDFILE_STORAGE=1 -DVFS_FILE_POOL_NUM_CONFIG=8 \
	-DVM_MAX_PAGE_ALIGN_SIZE_CONFIG=64*1024 -DVM_MAX_SECTOR_ALIGN_SIZE_CONFIG=64*1024 \
	-DVM_ITEM_MAX_NUM=256 -DCONFIG_UPDATA_ENABLE -DCONFIG_OTA_UPDATA_ENABLE -DCONFIG_ITEM_FORMAT_VM \
	-D_XOPEN_SOURCE=700 -D_GNU_SOURCE -D__ELF__ -D__SHELL__ -DBM_APP_$(APP)=1
ifeq ($(APP),bubba)
DEFINES += -DBM_PROF
endif

INCLUDES := -I$(HERE)/include -I$(REPO)/hal/fm1 -I$(REPO)/core \
	-I$(SDK)/include_lib/newlib/include -I$(SDK)/include_lib -I$(SDK)/include_lib/driver \
	-I$(SDK)/include_lib/driver/device -I$(SDK)/include_lib/driver/cpu/wl82 -I$(SDK)/include_lib/system \
	-I$(SDK)/include_lib/system/generic -I$(SDK)/include_lib/btctrler -I$(SDK)/include_lib/btctrler/port/wl82 \
	-I$(SDK)/include_lib/update -I$(SDK)/include_lib/btstack/third_party/common \
	-I$(SDK)/include_lib/btstack/third_party/rcsp -I$(SDK)/include_lib/utils \
	-I$(SDK)/include_lib/utils/syscfg -I$(SDK)/include_lib/utils/event -I$(SDK)/include_lib/media \
	-I$(SDK)/include_lib/media/cpu/wl82 -I$(SDK)/apps -I$(SDK)/include_lib/system/os \
	-I$(SDK)/apps/common/usb -I$(SDK)/apps/common/usb/device -I$(SDK)/include_lib/driver/device/usb \
	-I$(SDK)/include_lib/driver/device/usb/device

# Caché de datos completa (32 KiB): la variante del guion de enlace del SDK que no convierte 7 de
# las 8 vías de la caché de datos en RAM (FREE_DACHE_WAY = 0; las tablas del sistema van a la RAM
# normal). Con el valor por defecto la caché de datos efectiva es de 4 KiB (medido en el aparato:
# lecturas al azar en la flash de ~110 ns hasta 4 KiB y de 700-1270 ns desde 8 KiB).
LD_DEFS ?= -DCONFIG_DYNAMIC_SDRAM_ONOFF_ENABLE

# del SDK: arranque y sistema (lo mismo que demo_hello, sin los benchmarks)
SDK_C := \
	apps/common/config/log_config/lib_driver_config.c \
	apps/common/config/log_config/lib_system_config.c \
	apps/common/config/log_config/lib_update_config.c \
	apps/common/config/user_cfg.c \
	apps/common/debug/debug.c \
	apps/common/debug/debug_user.c \
	apps/common/system/init.c \
	apps/common/system/system_reset_reason.c \
	apps/common/system/version.c \
	apps/common/update/update.c \
	cpu/wl82/debug.c \
	cpu/wl82/key/adc_api.c \
	cpu/wl82/port_waked_up.c \
	cpu/wl82/setup.c \
	apps/common/usb/usb_config.c \
	apps/common/usb/usb_epbuf_manager.c
SDK_S := include_lib/driver/version.z.S include_lib/system/version.z.S include_lib/update/version.z.S \
	include_lib/utils/version.z.S

# nuestro
OWN_C := hal/fm1/sdk/board.c hal/fm1/fm1_boot.c hal/fm1/fm1_lcd.c hal/fm1/fm1_keys.c \
	hal/fm1/fm1_usb_midi.c hal/fm1/fm1_update.c hal/fm1/fm1_flash.c core/bm_gfx.c core/bm_lang.c core/gen/bm_font.c \
	core/gen/bm_logo.c
ifeq ($(APP),diag)
OWN_C += hal/fm1/fm1_diag.c
else
OWN_C := $(sort $(OWN_C) hal/fm1/fm1_main.c hal/fm1/fm1_consola.c hal/fm1/fm1_bench.c hal/fm1/fm1_audio.c hal/fm1/fm1_scanhw.c hal/fm1/fm1_scan.c 	$(patsubst $(REPO)/%,%,$(wildcard $(REPO)/core/*.c $(REPO)/core/gen/*.c)))
endif

LIBS := --start-group $(SDK)/include_lib/newlib/pi32v2-lib/libm.a $(SDK)/include_lib/newlib/pi32v2-lib/libc.a \
	$(SDK)/include_lib/newlib/pi32v2-lib/libcompiler_rt.a \
	--start-group $(SDK)/cpu/wl82/liba/cpu.a $(SDK)/cpu/wl82/liba/event.a $(SDK)/cpu/wl82/liba/system.a \
	$(SDK)/cpu/wl82/liba/cfg_tool.a $(SDK)/cpu/wl82/liba/fs.a $(SDK)/cpu/wl82/liba/common_lib.a \
	$(SDK)/cpu/wl82/liba/update.a --end-group \
	-T$(OUT)/sdk.ld -M=$(OUT)/sdk.map --end-group

LFLAGS := --plugin-opt=-pi32v2-always-use-itblock=false --plugin-opt=-enable-ipra=true \
	--plugin-opt=-pi32v2-merge-max-offset=4096 --plugin-opt=-pi32v2-enable-simd=true --plugin-opt=mcpu=r3 \
	--plugin-opt=-global-merge-on-const --plugin-opt=-inline-threshold=5 \
	--plugin-opt=-inline-max-allocated-size=32 --plugin-opt=-inline-normal-into-special-section=true \
	--plugin-opt=-dont-used-symbol-list=malloc,free,sprintf,printf,puts,putchar \
	--plugin-opt=-pi32v2-enable-rep-memop --plugin-opt=-emulated-tls --sort-common \
	--plugin-opt=-used-symbol-file=$(OUT)/sdk_used_list.used \
	--plugin-opt=-enable-movable-region=true \
	--plugin-opt=-movable-region-section-prefix=.movable.slot. \
	--plugin-opt=-movable-region-stub-section-prefix=.movable.stub. \
	--plugin-opt=-movable-region-prefix=.movable.region. \
	--plugin-opt=-movable-region-stub-prefix=__movable_stub_ \
	--plugin-opt=-movable-region-stub-swi-number=-2 \
	--plugin-opt=-movable-region-map-file-list=$(SDK)/apps/common/movable/funcname.txt \
	--plugin-opt=-movable-region-section-map-file-list=$(OUT)/section.txt \
	--plugin-opt=-movable-region-exclude-func-file-list=$(SDK)/apps/common/movable/exclude.txt \
	--plugin-opt=-pi32v2-large-program=true

SDK_OBJS := $(addprefix $(OUT)/sdk/,$(SDK_C:.c=.o) $(SDK_S:.S=.o))
# C++ (motores de terceros): sin excepciones, RTTI ni biblioteca de C++ (hal/fm1/compat).
ifeq ($(APP),bubba)
MSFA := dx7note env exp2 fm_core fm_op_kernel freqlut lfo pitchenv porta sin EngineMsfa
OWN_CXX := core/bm_fm.cpp core/bm_drums.cpp core/bm_analog.cpp $(addprefix third_party/msfa/,$(addsuffix .cpp,$(MSFA)))
# Plaits y stmlib (Mutable Instruments, MIT): batería modelada y ANALOG
OWN_CC := third_party/plaits/resources.cc third_party/stmlib/dsp/units.cc third_party/stmlib/utils/random.cc
endif
CXX := $(TOOL_DIR)/clang++
# Los motores se compilan para velocidad (-O2) y sin LTO: el enlazador del SDK optimiza para tamaño
# con inlining casi apagado, y el núcleo de la FM vive de funciones en línea (docs/TRAZA_FM1.md).
FAST := $(filter-out -Oz -flto -ffunction-sections -fdata-sections,$(CFLAGS)) -O2
CXXFLAGS := $(FAST) -std=gnu++14 -fno-exceptions -fno-rtti -fno-threadsafe-statics -DBM_NO_CXXLIB -DTEST -DPLAITS_SAMPLE_RATE=44118.0f -I$(REPO)/third_party 	-I$(REPO)/hal/fm1/compat -I$(REPO)/third_party/msfa -I$(REPO)/core -isystem $(SDK)/include_lib/newlib/include
OWN_OBJS := $(addprefix $(OUT)/own/,$(OWN_C:.c=.o) $(OWN_CXX:.cpp=.o) $(OWN_CC:.cc=.o))
# el cálculo de audio del núcleo, también para velocidad
FAST_C := core/bm_audio.c core/bm_dsp.c core/bm_engines.c core/bm_seq.c
CPP := $(CC) $(CFLAGS) $(DEFINES) $(INCLUDES) -D__LD__ -E -P

.PHONY: all
all: $(OUT)/app.bin

$(OUT)/sdk.ld: $(HERE)/include/app_config.h
	@mkdir -p $(OUT)
	$(CPP) $(SDK)/cpu/wl82/sdk_used_list.c -o $(OUT)/sdk_used_list.used
	$(CPP) $(SDK)/apps/common/movable/section.c -o $(OUT)/section.txt
	$(CPP) $(LD_DEFS) $(SDK)/cpu/wl82/sdk_ld.c -o $@

$(OUT)/sdk/%.o: $(SDK)/%.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) $(SDK_WARN) $(DEFINES) $(INCLUDES) -c $< -o $@

$(OUT)/sdk/%.o: $(SDK)/%.S
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) $(SDK_WARN) $(DEFINES) $(INCLUDES) -c $< -o $@

$(OUT)/own/%.o: $(REPO)/%.c
	@mkdir -p $(@D)
	$(CC) $(if $(filter $(FAST_C),$*.c),$(FAST),$(CFLAGS)) $(OWN_WARN) $(DEFINES) $(INCLUDES) -c $< -o $@

$(OUT)/own/%.o: $(REPO)/%.cpp
	@mkdir -p $(@D)
	$(CXX) $(CXXFLAGS) -Wno-unused-parameter -c $< -o $@

$(OUT)/own/%.o: $(REPO)/%.cc
	@mkdir -p $(@D)
	$(CXX) $(CXXFLAGS) -Wno-unused-parameter -c $< -o $@

$(OUT)/sdk.elf: $(OUT)/sdk.ld $(SDK_OBJS) $(OWN_OBJS)
	$(LD) -o $@ $(SDK_OBJS) $(OWN_OBJS) $(LFLAGS) $(LIBS)

# app.bin = .text + .data + .ram0_data + .cache_ram_data, como el post-build del SDK
$(OUT)/app.bin: $(OUT)/sdk.elf
	$(OBJCOPY) -O binary -j .text $< $(OUT)/text.bin
	$(OBJCOPY) -O binary -j .data $< $(OUT)/data.bin
	$(OBJCOPY) -O binary -j .ram0_data $< $(OUT)/ram0_data.bin
	$(OBJCOPY) -O binary -j .cache_ram_data $< $(OUT)/cache_ram_data.bin
	cat $(OUT)/text.bin $(OUT)/data.bin $(OUT)/ram0_data.bin $(OUT)/cache_ram_data.bin > $@
	$(OBJDUMP) -section-headers -address-mask=0x1ffffff $< > $(OUT)/sections.txt
	@cat $(OUT)/sections.txt
	@echo "app.bin: $$(stat -c %s $@) bytes"
