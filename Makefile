CC=gcc
AS=as
LD=gcc

CFLAGS=-std=c11 -O2 -Wall -Wextra -ffreestanding -m32 -fno-pie -fno-pic -fno-stack-protector -mgeneral-regs-only
ASFLAGS=--32
LDFLAGS=-ffreestanding -O2 -nostdlib -m32 -no-pie -Wl,--build-id=none

SRCS = $(wildcard kernel/*.c) $(wildcard terminal/*.c) $(wildcard drivers/*.c) $(wildcard filesystem/*.c)
OBJS = boot.o $(SRCS:.c=.o)

all: arcturus.bin

boot.o: boot.s
	$(AS) $(ASFLAGS) $< -o $@

%.o: %.c
	$(CC) -c $< -o $@ $(CFLAGS)

arcturus.bin: $(OBJS) linker.ld
	$(LD) -T linker.ld -o $@ $(OBJS) $(LDFLAGS)

iso: arcturus.bin
	mkdir -p isodir/boot/grub
	cp arcturus.bin isodir/boot/arcturus.bin
	echo 'set timeout=0' > isodir/boot/grub/grub.cfg
	echo 'set default=0' >> isodir/boot/grub/grub.cfg
	echo 'menuentry "ArcturusOs" {' >> isodir/boot/grub/grub.cfg
	echo '	multiboot /boot/arcturus.bin' >> isodir/boot/grub/grub.cfg
	echo '}' >> isodir/boot/grub/grub.cfg
	grub-mkrescue -o arcturus.iso isodir

disk.img:
	dd if=/dev/zero of=$@ bs=1M count=16 status=none

run: arcturus.bin disk.img
	qemu-system-i386 -kernel arcturus.bin -m 128M -serial stdio -drive file=disk.img,format=raw,if=ide

run-iso: iso
	qemu-system-i386 -cdrom arcturus.iso -m 128M -serial stdio

clean:
	rm -f *.o kernel/*.o terminal/*.o drivers/*.o filesystem/*.o arcturus.bin arcturus.iso
	rm -rf isodir
