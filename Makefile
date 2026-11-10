CC_ARM = arm-none-eabi-gcc
CFLAGS_ARM = -mcpu=cortex-m3 -mthumb -std=c17 -O2 -g -Wall -Wextra -ffunction-sections -fdata-sections \
  -Isrc -I$(FREERTOS)/include -I$(FREERTOS)/portable/GCC/ARM_CM3
RTOS = $(FREERTOS)/tasks.c $(FREERTOS)/queue.c $(FREERTOS)/list.c \
  $(FREERTOS)/portable/GCC/ARM_CM3/port.c $(FREERTOS)/portable/MemMang/heap_4.c
FW = src/startup.c src/main.c src/frame.c

build/fw.elf: $(FW) src/*.h src/mps2.ld
	@mkdir -p build
	$(CC_ARM) $(CFLAGS_ARM) $(FW) $(RTOS) -Tsrc/mps2.ld -nostartfiles --specs=nano.specs --specs=nosys.specs \
	  -Wl,--gc-sections -Wl,-Map=build/fw.map -o $@
	arm-none-eabi-size $@

build/test_frame: test/test_frame.c src/frame.c src/frame.h
	@mkdir -p build
	cc -std=c17 -Wall -Wextra -Werror -fsanitize=address,undefined -g test/test_frame.c src/frame.c -o $@

test: build/test_frame build/fw.elf
	./build/test_frame
	python3 test/test_qemu.py build/fw.elf

clean:
	rm -rf build
.PHONY: test clean
