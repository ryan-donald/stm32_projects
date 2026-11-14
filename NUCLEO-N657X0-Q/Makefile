TARGET  = blinky
BUILD   = build

PREFIX  ?= arm-none-eabi-
CC      = $(PREFIX)gcc
OBJCOPY = $(PREFIX)objcopy
SIZE    = $(PREFIX)size
READELF = $(PREFIX)readelf
# STM32N6 support (target/stm32n6x.cfg) is only in OpenOCD built from master, not 0.12.0
OPENOCD ?= openocd

# Every .c file under src/ is built; every directory under src/ is an include path
SRCS = $(shell find src -name '*.c') \
       cmsis/device/system_stm32n6xx_fsbl.c \
       cmsis/device/startup_stm32n657xx_fsbl.s

LDSCRIPT = STM32N657X0HXQ_AXISRAM2.ld

# Cortex-M55 with double-precision FPU, running in the secure state (-mcmse)
MCU = -mcpu=cortex-m55 -mthumb -mfloat-abi=hard

DEFS = -DSTM32N657xx
INCS = -Icmsis/core -Icmsis/device $(addprefix -I,$(shell find src -type d))

CFLAGS  = $(MCU) -mcmse $(DEFS) $(INCS) -std=c11 -Og -g3 -Wall -Wextra \
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

# No internal flash: load into AXISRAM2 and jump to Reset_Handler. Needs the board in
# dev boot (BOOT1 switch high); the program is lost on power cycle.
flash: $(BUILD)/$(TARGET).elf
	$(OPENOCD) -f interface/stlink-dap.cfg -f target/stm32n6x.cfg \
	        -c "init; reset halt; load_image $<; verify_image $<" \
	        -c "resume [expr {$$($(READELF) -h $< | awk '/Entry point/ {print $$4}') & ~1}]; exit"

clean:
	rm -rf $(BUILD)

-include $(OBJS:.o=.d)

.PHONY: all flash clean
