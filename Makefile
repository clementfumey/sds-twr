# Basename for the resulting .elf/.bin/.hex file
RESULT ?= test

# The Project directory
TOPDIR =.

# All Sources
SOURCES = \
  $(wildcard $(TOPDIR)/src/application/*.c) \
  $(wildcard $(TOPDIR)/src/decadriver/*.c) \
  $(wildcard $(TOPDIR)/src/platform/*.c) \
  $(wildcard $(TOPDIR)/src/sys/*.c) \
  $(wildcard $(TOPDIR)/system/src/STM32F10x_StdPeriph_Driver/*.c) \
  $(wildcard $(TOPDIR)/system/src/CMSIS/*.c) \

# start-up code for all STM32F2xx series devices (the one used in decawave is 'cl')
STARTUP = \
  $(TOPDIR)/system/startup_stm32f10x_cl.s \

# All the headers .h (look for changes)
HEADERS = \
  $(wildcard $(TOPDIR)/src/decadriver/*.h) \
  $(wildcard $(TOPDIR)/src/platform/*.h) \
  $(wildcard $(TOPDIR)/src/sys/*.h) \
  $(wildcard $(TOPDIR)/src/compiler/*.h) \
  $(wildcard $(TOPDIR)/src/application/*.h) \
  $(wildcard $(TOPDIR)/system/include/CMSIS/*.h) \
  $(wildcard $(TOPDIR)/system/include/STM32F10x_StdPeriph_Driver/*.h) \

# Linker script (STM32 used in decawave EVB1000 has 256k flash and 64k RAM)
LINKER_SCRIPT = Linkers/stm32_flash_256k_ram_64k.ld

# List of directories for Includes
INCLUDES += \
  -I$(TOPDIR)/src/decadriver \
  -I$(TOPDIR)/src/platform \
  -I$(TOPDIR)/src/sys \
  -I$(TOPDIR)/src/compiler \
  -I$(TOPDIR)/system/include/CMSIS \
  -I$(TOPDIR)/system/include/STM32F10x_StdPeriph_Driver \
  -I$(TOPDIR)/system \

# CFLAGS -DUSE_FULL_ASSERT
CFLAGS +=  -DEBUG -DTRACE -DOS_USE_TRACE_ITM -DSTM32F10X_CL -DUSE_STDPERIPH_DRIVER -fno-common -Wall -Os -g3 -mcpu=cortex-m3 -mthumb -lm
CFLAGS += -ffunction-sections -fdata-sections -Wl,--gc-sections 
CFLAGS += $(INCLUDES) 

CROSS_COMPILE ?= arm-none-eabi-
CC = $(CROSS_COMPILE)gcc
AS = $(CROSS_COMPILE)as
LD = $(CROSS_COMPILE)ld
OBJDUMP = $(CROSS_COMPILE)objdump
OBJCOPY = $(CROSS_COMPILE)objcopy
SIZE = $(CROSS_COMPILE)size

# So that the "build depends on the makefile" trick works no matter the name of
# the makefile
THIS_MAKEFILE := $(lastword $(MAKEFILE_LIST))

#OBJECTS = $(subst src/,obj/,$(subst .c,.o,$(SOURCES)))
OBJECTS = $(subst .c,.o,$(notdir $(SOURCES)))
VPATH = $(dir $(SOURCES))
BUILDIR = ./build

STARTUP_OBJECTS = $(STARTUP:.s=.o)

all: build size

build: $(RESULT).elf $(RESULT).bin $(RESULT).hex $(RESULT).lst

$(RESULT).elf: startup $(OBJECTS) $(HEADERS) $(LINKER_SCRIPT) $(THIS_MAKEFILE)
	@echo -------------------- Linking ----------------------
	$(CC) -Wl,-M=$(RESULT).map -Wl,-T$(LINKER_SCRIPT) $(CFLAGS) $(OBJECTS) $(STARTUP_OBJECTS) -o $@

%.o : %c
	$(CC) $(CFLAGS) -c $< -o $(BUILDIR)/$@
	
startup: $(STARTUP)
	$(AS) -c $(STARTUP) -o $(STARTUP_OBJECTS)

%.hex: %.elf
	$(OBJCOPY) -O ihex $< $@

%.bin: %.elf
	$(OBJCOPY) -O binary $< $@

%.lst: %.elf
	$(OBJDUMP) -x -S $(RESULT).elf > $@

size: $(RESULT).elf
	$(SIZE) $(RESULT).elf

install: build
	st-flash write $(RESULT).bin 0x08000000
	
clean:
	rm -f $(RESULT).elf
	rm -f $(RESULT).bin
	rm -f $(RESULT).map
	rm -f $(RESULT).hex
	rm -f $(RESULT).lst
	rm -f $(OBJECTS)
	
print-%  : ; @echo $* = $($*)

.PHONY: all build size clean install
