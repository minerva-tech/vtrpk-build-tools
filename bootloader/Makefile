
UTIL_DIR = utils

UBL_SOURCES=ubl.c boot.c selfcopy.c uartboot.c device.c debug.c uart.c util.c nand.c nandboot.c device_nand.c dma.c usb.c

SFT_SOURCES=boot.c selfcopy.c sft.c uartboot_sft.c device.c debug.c uart.c util.c nand.c nandboot.c device_nand.c dma.c usb.c

ENTRYPOINT=boot

OBJECTS:=$(patsubst %.c,%.o,$(SOURCES))

UBL_LINKERSCRIPT=ubl.lds
SFT_LINKERSCRIPT=sft.lds

CROSSCOMPILE=arm-none-eabi-
CC=$(CROSSCOMPILE)gcc
AS=$(CROSSCOMPILE)as
OBJCOPY=$(CROSSCOMPILE)objcopy
OBJDUMP=$(CROSSCOMPILE)objdump
OBJCOPYFLAGS = --gap-fill 0xFF -S

CFLAGS:= -ggdb -Os -march=armv5te -mtune=arm926ej-s -fomit-frame-pointer -pipe -Iinclude -Wall -nostartfiles

HOSTCC = gcc
H_CFLAGS = -g -pipe

SHOEHORN = $(UTIL_DIR)/shoehorn
SHOEHORN_SRC = $(UTIL_DIR)/shoehorn.c

SFH = $(UTIL_DIR)/sfh
SFH_SRC = $(UTIL_DIR)/sfh.c

SDH = $(UTIL_DIR)/sdh
SDH_SRC = $(UTIL_DIR)/sdh.c

UART_TEST = $(UTIL_DIR)/uart_test
UART_TEST_SRC = $(UTIL_DIR)/uart_test.c

all: ubl_nand.bin ubl_nand_1.bin ubl_uart.bin ubl_mmc.bin sft.bin $(SHOEHORN) $(SFH) $(SDH) $(UART_TEST)

.PHONY : clean
clean:
	-rm -f -v *.bin *.asm *.o sft $(SHOEHORN) $(SFH) $(SDH) $(UART_TEST)

ubl_uart.bin: start.S $(UBL_SOURCES) $(UBL_LINKERSCRIPT)
	$(CC) -DARM270_DDR216_OSC24 $(CFLAGS) -Wl,-T$(UBL_LINKERSCRIPT) $(UBL_SOURCES) -o ubl
	$(OBJDUMP) -dS ubl > ubl_uart.asm
	$(OBJCOPY) $(OBJCOPYFLAGS) -O binary ubl $@
	-rm -f ubl

ubl_nand_1.bin: start.S $(UBL_SOURCES) $(UBL_LINKERSCRIPT)
	$(CC) -DARM270_DDR216_OSC24 -DUBL_NAND -DDMA -DNAND_DISABLE_WRITE_OP -DSILENT $(CFLAGS) -Wl,-T$(UBL_LINKERSCRIPT) $(UBL_SOURCES) -o ubl
	#$(CC) -DARM270_DDR216_OSC24 -DUBL_NAND -DDMA -DNAND_DISABLE_WRITE_OP $(CFLAGS) -Wl,-T$(UBL_LINKERSCRIPT) $(UBL_SOURCES) -o ubl
	$(OBJDUMP) -dS ubl > ubl_nand.asm
	$(OBJCOPY) $(OBJCOPYFLAGS) -O binary ubl $@
	-rm -f ubl

ubl_nand.bin: start.S $(UBL_SOURCES) $(UBL_LINKERSCRIPT)
	$(CC) -DARM270_DDR216_OSC24 -DUBL_NAND -DDMA -DNAND_DISABLE_WRITE_OP -DMACH_TYPE_OGONEK_2 $(CFLAGS) -Wl,-T$(UBL_LINKERSCRIPT) $(UBL_SOURCES) -o ubl
	$(OBJDUMP) -dS ubl > ubl_nand_2.asm
	$(OBJCOPY) $(OBJCOPYFLAGS) -O binary ubl $@
	-rm -f ubl

ubl_mmc.bin: start.S $(UBL_SOURCES) $(UBL_LINKERSCRIPT)
	$(CC) -DARM270_DDR216_OSC24 -DUBL_SD_MMC $(CFLAGS) -Wl,-T$(UBL_LINKERSCRIPT) $(UBL_SOURCES) sdmmc.c sdmmcboot.c -o ubl
	$(OBJDUMP) -dS ubl > ubl_mmc.asm
	$(OBJCOPY) $(OBJCOPYFLAGS) -O binary ubl $@
	-rm -f ubl

sft.bin: start.S $(SFT_SOURCES) $(SFT_LINKERSCRIPT)
	$(CC) -DARM270_DDR216_OSC24 -DUBL_NAND $(CFLAGS) -Wl,-T$(SFT_LINKERSCRIPT) $(SFT_SOURCES) -o sft
	$(OBJDUMP) -dS sft > sft.asm
	$(OBJCOPY) $(OBJCOPYFLAGS) -O binary sft $@
	-rm -f sft

$(SHOEHORN): $(SHOEHORN_SRC)
	$(HOSTCC) $(H_CFLAGS) -o $@ $<

$(SFH): $(SFH_SRC)
	$(HOSTCC) $(H_CFLAGS) -o $@ $<

$(SDH): $(SDH_SRC)
	$(HOSTCC) $(H_CFLAGS) -o $@ $<

$(UART_TEST): $(UART_TEST_SRC)
	$(HOSTCC) $(H_CFLAGS) -o $@ $<
