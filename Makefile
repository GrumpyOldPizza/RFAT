CPP             = arm-none-eabi-cpp
CC              = arm-none-eabi-gcc
AS              = arm-none-eabi-as
AR              = arm-none-eabi-ar
LD              = arm-none-eabi-ld
CFLAGS          = -g3 -Os -ffunction-sections -fdata-sections -std=c99 -Wall -Wshadow -Wpointer-arith -Wcast-qual -Wcast-align -Wstrict-prototypes -Wmissing-prototypes -mcpu=cortex-m4 -mthumb -mabi=aapcs -mfpu=fpv4-sp-d16 -mfloat-abi=hard $(DEFINES) $(INCLUDES) 
ASFLAGS         = -g3 -mcpu=cortex-m4 -mthumb -mabi=aapcs  -mfpu=fpv4-sp-d16 -mfloat-abi=hard -D__ASSEMBLY__ $(DEFINES) $(INCLUDES)
LDSCRIPT        = ./CMSIS/Device/TI/TM4C123/Sources/GCC/link_TM4C123.ld
LDFLAGS         = -mcpu=cortex-m4 -mthumb -mabi=aapcs -mfpu=fpv4-sp-d16 -mfloat-abi=hard -nostartfiles -Wl,--gc-sections -Wl,--script=$(LDSCRIPT)
DEFINES         = -D__STACK_SIZE=2048 -D__START=main -DTM4C123GH6PM
INCLUDES        = -I. -I./CMSIS/Include  -I./CMSIS/Device/TI/TM4C123/Include


ASRC            = \
		  CMSIS/Device/TI/TM4C123/Sources/GCC/startup_TM4C123.S

CSRC            = \
		  CMSIS/Device/TI/TM4C123/Sources/system_TM4C123.c \
		  rfat_core.c \
		  rfat_disk.c \
		  tm4c123_disk.c \
		  main.c

OBJS            = $(CSRC:.c=.o) $(ASRC:.S=.o)

BIN             = demo.elf

.PHONY: clean all

all: $(BIN)

$(BIN): $(OBJS)
	$(CC) $(LDFLAGS) $(OBJS) $(LDLIBS) -o $@

clean:
	rm -f $(BIN) $(OBJS) *~

%.o:    %.c
	$(CC) $(CFLAGS) -o $@ -c $<

%.o:    %.S
	$(CC) $(ASFLAGS) -o $@ -c $<

-include $(DEPS)

