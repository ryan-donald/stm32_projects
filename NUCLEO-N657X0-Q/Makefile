TARGET  = blinky
BUILD   = build

PREFIX  ?= arm-none-eabi-
CC      = $(PREFIX)gcc
OBJCOPY = $(PREFIX)objcopy
SIZE    = $(PREFIX)size
READELF = $(PREFIX)readelf
OPENOCD ?= openocd
CUBEPROG ?= $(HOME)/STMicroelectronics/STM32Cube/STM32CubeProgrammer/bin

# shared driver sources
COMMON = ../common
COMMON_SRCS = $(shell find $(COMMON) -name '*.c')

# board sources and CMSIS startup
SRCS = $(shell find src -name '*.c') \
       cmsis/device/system_stm32n6xx_fsbl.c \
       cmsis/device/startup_stm32n657xx_fsbl.s

LDSCRIPT = STM32N657X0HXQ_AXISRAM2.ld

# Cortex-M55 with double-precision FPU
MCU = -mcpu=cortex-m55 -mthumb -mfloat-abi=hard

DEFS = -DSTM32N657xx
INCS = -Icmsis/core -Icmsis/device $(addprefix -I,$(shell find src $(COMMON) -type d))

CFLAGS  = $(MCU) -mcmse $(DEFS) $(INCS) -std=c11 -Og -g3 -Wall -Wextra \
          -ffunction-sections -fdata-sections -MMD -MP
ASFLAGS = $(MCU) -x assembler-with-cpp -g3
LDFLAGS = $(MCU) -T$(LDSCRIPT) --specs=nano.specs --specs=nosys.specs \
          -Wl,--gc-sections -Wl,-Map=$(BUILD)/$(TARGET).map -Wl,--print-memory-usage

OBJS = $(addprefix $(BUILD)/,$(addsuffix .o,$(basename $(SRCS)))) \
       $(patsubst $(COMMON)/%.c,$(BUILD)/common/%.o,$(COMMON_SRCS))

all: $(BUILD)/$(TARGET).elf $(BUILD)/$(TARGET).bin

$(BUILD)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/common/%.o: $(COMMON)/%.c
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
	$(OPENOCD) -f interface/stlink.cfg -c "transport select swd" -f target/stm32n6x.cfg \
	        -c "init; reset halt; load_image $<; verify_image $<" \
	        -c "resume [expr {$$($(READELF) -h $< | awk '/Entry point/ {print $$4}') & ~1}]; exit"

$(BUILD)/$(TARGET)_trusted.bin: $(BUILD)/$(TARGET).bin
	$(CUBEPROG)/STM32_SigningTool_CLI -bin $< -nk -t fsbl -hv 2.3 -o $@ --align -s

flash-boot: $(BUILD)/$(TARGET)_trusted.bin
	$(CUBEPROG)/STM32_Programmer_CLI -c port=SWD mode=HOTPLUG \
	        -el $(CUBEPROG)/ExternalLoader/MX25UM51245G_STM32N6570-NUCLEO.stldr -hardRst -w $< 0x70000000 -v

clean:
	rm -rf $(BUILD)

-include $(OBJS:.o=.d)

.PHONY: all flash flash-boot clean
