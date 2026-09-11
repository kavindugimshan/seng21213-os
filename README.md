# SENG21213-OS

> **Course**: SENG 21213 – Computer Architecture & Operating Systems
> **Assignment**: Build your own x86 Operating System, one lecture milestone at a time.

This repository holds my semester-long OS assignment. Starting from the Stage 0
starter kit provided in the course, the kernel gains one new subsystem per
lecture milestone: process management, threading, memory management, and
finally a simple file system.

## Project Structure

```
seng21213-os/
├── boot/
│   └── boot.asm          # MBR bootloader (NASM, 16-bit real mode -> 32-bit protected mode)
├── kernel/
│   ├── kernel_entry.asm  # Protected-mode entry point, calls kernel_main()
│   ├── kernel.c          # Main kernel: shell loop, command dispatch
│   ├── vga.c / vga.h     # VGA 80x25 text-mode driver
│   ├── keyboard.c / .h   # PS/2 keyboard polling driver
├── include/
│   └── types.h           # Primitive integer types (freestanding, no libc)
├── linker.ld             # Linker script - places the kernel at 0x10000
├── Makefile              # Build system
└── README.md             # This file
```

## Milestone Schedule

| Stage / Lecture | Milestone                    | Tag             | Status |
|------------------|------------------------------|-----------------|--------|
| Stage 0 (L07-L08) | Boot, VGA & Shell            | `v0.1-stage0`   | Done |
| Stage 1 (L09)     | Process Table & Scheduler    | `v0.2-stage1`   | Pending |
| Stage 2 (L10)     | Threads, Mutex & Semaphore   | `v0.3-stage2`   | Pending |
| Stage 3 (L11)     | Physical Memory Manager      | `v0.4-stage3`   | Pending |
| Stage 4 (L12)     | RAM Disk File System         | `v0.5-stage4`   | Pending |

## Building & Running

Tested on Ubuntu 22.04 / WSL2 Ubuntu, with the packages below installed:

```bash
sudo apt install nasm gcc gcc-multilib binutils qemu-system-x86 gdb make
```

```bash
make          # builds seng21213.img
make run      # boots the image in QEMU
make clean    # removes build/ and seng21213.img
```

`make run` launches `qemu-system-i386` with the built disk image. Close the
QEMU window (or press Ctrl+A then X in a terminal-only session) to exit.

## Understanding the Boot Process

```
Power On
  |
  v
BIOS loads the 512-byte MBR (boot/boot.asm) at 0x7C00
  |  Reads 64 sectors (the kernel) from disk into RAM at 0x10000
  |  Builds a 3-entry GDT (null, code, data)
  |  Sets the PE bit in CR0 -> switches to 32-bit Protected Mode
  |  Far-jumps to 0x10000
  v
kernel/kernel_entry.asm  (Protected Mode, 32-bit)
  |  Calls kernel_main()
  v
kernel/kernel.c -> kernel_main()
  |  vga_init()      - clear screen, set up the text-mode driver
  |  kb_init()       - flush the PS/2 keyboard buffer
  |  print_splash()  - welcome banner
  |  shell_run()     - interactive shell (infinite command loop)
```

## Stage 0: Boot, VGA & Shell

Stage 0 brings the machine up from power-on to an interactive command shell,
entirely without an OS underneath - just the bootloader and kernel in this
repository.

**Implemented shell commands:**

| Command  | Arguments      | Behaviour                                              |
|----------|----------------|---------------------------------------------------------|
| `help`   | -              | Lists all available commands                            |
| `clear`  | -              | Clears the screen and resets the cursor to (0,0)         |
| `echo`   | `<text>`       | Prints the given text back to the screen                 |
| `version`| -              | Prints the kernel name and version string                |
| `colour` | `<fg> <bg>`    | Changes the text colour (0-15 for each, see `vga.h`)     |
| `halt`   | -              | Disables interrupts and halts the CPU                    |
| `about`  | -              | Extra: prints build/architecture info                    |
| `mem`    | -              | Extra: stub memory map (real PMM added in Stage 3)       |

`ps`, `kill`, `threads`, `free`, `ls`, `cat` are recognised but print a
"not yet implemented" message - they are wired up in later stages.

### How to test Stage 0

1. `make run` and wait for the splash screen and `ksh>` prompt to appear.
2. `help` - check every command above is listed.
3. `echo hello world` - should print `hello world`.
4. `version` - should print the kernel name and `v0.1-stage0`.
5. `colour 14 1` - subsequent shell text should turn yellow-on-blue.
6. `clear` - screen should blank and the cursor return to the top-left.
7. `halt` - should print a message, then the CPU stops (QEMU window freezes,
   no reboot or crash).

No optional/bonus extensions are implemented in this stage.

## Debugging

```bash
make run-debug         # QEMU paused, GDB stub on tcp::1234
# in another terminal:
gdb
(gdb) target remote :1234
(gdb) set architecture i386
(gdb) symbol-file build/kernel.elf
(gdb) break kernel_main
(gdb) continue
```

## References

| Topic                | Reference                                    |
|-----------------------|-----------------------------------------------|
| x86 Protected Mode    | Intel IA-32 Manual, Vol 3, Chapter 3          |
| VGA Text Mode         | OSDev Wiki: Text UI                           |
| Process Management    | Stallings, *OS: Internals & Design Principles*, Ch.3-4 |
| Memory Management     | Stallings, Ch.7-8                              |
| OSDev community       | https://wiki.osdev.org                         |
