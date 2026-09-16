# ArcturusOS

ArcturusOS is a small educational 32-bit x86 operating system kernel written in C11 and Assembly. It boots through the Multiboot protocol, displays output in VGA text mode, and provides a minimal interactive terminal backed by a polling PS/2 keyboard driver.

## Features

- Boots in 32-bit protected mode with GRUB/Multiboot support
- VGA text-mode terminal with a hardware cursor
- Serial output through COM1
- Polling PS/2 keyboard input
- Editable terminal input with left and right arrow navigation
- Built-in commands:
  - `time` — displays the current CMOS RTC time as `HH:MM:SS`
  - `uptime` — displays elapsed time since the kernel booted

## Build

The project requires a 32-bit-capable GCC toolchain, GNU assembler, and GNU Make.

```sh
make
```

This produces `arcturus.bin`.

## Run

Run the kernel directly in QEMU:

```sh
make run
```

To build a bootable ISO instead, install `grub-mkrescue` and run:

```sh
make iso
make run-iso
```

## License

ArcturusOS is free software licensed under the
GNU General Public License version 3 or later.

See [LICENSE](LICENSE) for the full license text.

## Project layout

- `boot.s` — Multiboot header and kernel entry point
- `kernel/` — kernel initialization and terminal input loop
- `terminal/` — VGA terminal and command implementations
- `drivers/` — low-level I/O and PS/2 keyboard driver
- `linker.ld` — kernel linker script

## Notes

This is a freestanding kernel: it does not use a host operating system, standard C library, memory manager, scheduler, or interrupt-driven input yet. It is intended as a compact foundation for learning OS development.
