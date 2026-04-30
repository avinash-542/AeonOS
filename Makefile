CC      = aarch64-linux-gnu-gcc
CFLAGS  = -ffreestanding -nostdinc -nostdlib -nostartfiles -O2

SRCS    = boot/boot.S kernel/main.c
TARGET  = aeonos.img

all: $(TARGET)

$(TARGET): $(SRCS) linker.ld
	$(CC) $(CFLAGS) -T linker.ld -o aeonos.elf $(SRCS)
	aarch64-linux-gnu-objcopy -O binary aeonos.elf $(TARGET)

run: $(TARGET)
	qemu-system-aarch64 \
		-M virt \
		-cpu cortex-a57 \
		-nographic \
		-kernel $(TARGET)

clean:
	rm -f *.elf *.img

.PHONY: all run clean