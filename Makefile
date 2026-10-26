TARGET  = blinky
BUILD   = build

PREFIX  ?= arm-none-eabi-
CC      = $(PREFIX)gcc
OBJCOPY = $(PREFIX)objcopy
SIZE    = $(PREFIX)size

# Every .c file under src/ is built; every directory under src/ is an include path
SRCS = $(shell find src -name '*.c') \
       cmsis/device/system_stm32h7xx.c \
       cmsis/device/startup_stm32h723xx.s

LDSCRIPT = STM32H723ZGTX_FLASH.ld

# Cortex-M7 with double-precision FPU
MCU = -mcpu=cortex-m7 -mthumb -mfpu=fpv5-d16 -mfloat-abi=hard

DEFS = -DSTM32H723xx -DUSE_PWR_LDO_SUPPLY
INCS = -Icmsis/core -Icmsis/device $(addprefix -I,$(shell find src -type d))

CFLAGS  = $(MCU) $(DEFS) $(INCS) -std=c11 -Og -g3 -Wall -Wextra \
          -ffunction-sections -fdata-sections -MMD -MP
ASFLAGS = $(MCU) -x assembler-with-cpp -g3
LDFLAGS = $(MCU) -T$(LDSCRIPT) --specs=nano.specs --specs=nosys.specs \
          -Wl,--gc-sections -Wl,-Map=$(BUILD)/$(TARGET).map -Wl,--print-memory-usage

OBJS = $(addprefix $(BUILD)/,$(addsuffix .o,$(basename $(SRCS))))

all: $(BUILD)/$(TARGET).elf $(BUILD)/$(TARGET).bin

$(BUILD)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/%.o: %.s
	@mkdir -p $(dir $@)
	$(CC) $(ASFLAGS) -c $< -o $@

$(BUILD)/$(TARGET).elf: $(OBJS) $(LDSCRIPT)
	$(CC) $(OBJS) $(LDFLAGS) -o $@
	$(SIZE) $@

$(BUILD)/$(TARGET).bin: $(BUILD)/$(TARGET).elf
	$(OBJCOPY) -O binary $< $@

flash: $(BUILD)/$(TARGET).elf
	openocd -f interface/stlink.cfg -f target/stm32h7x.cfg \
	        -c "program $< verify reset exit"

clean:
	rm -rf $(BUILD)

-include $(OBJS:.o=.d)

.PHONY: all flash clean
