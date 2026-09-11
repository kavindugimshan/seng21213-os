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
│   ├── process.h / process.c   # PCB table + process_create() (Stage 1)
│   ├── scheduler.c             # Round-robin ready queue + timer tick handler
│   ├── switch.asm              # IRQ0 context-switch stub (PUSHAD/POPAD)
│   ├── idt.c / idt.h           # IDT + 8259 PIC remap (needed for IRQ0)
│   ├── pit.c / pit.h           # i8253 PIT driver (100 Hz timer tick)
│   ├── thread.c / thread.h     # kernel threads: thread_create(fn, arg) (Stage 2)
│   ├── mutex.c / mutex.h       # blocking mutex_lock()/mutex_unlock()
│   └── semaphore.c / semaphore.h  # counting semaphore sem_wait()/sem_signal()
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
| Stage 1 (L09)     | Process Table & Scheduler    | `v0.2-stage1`   | Done |
| Stage 2 (L10)     | Threads, Mutex & Semaphore   | `v0.3-stage2`   | Done |
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

`kill`, `threads`, `free`, `ls`, `cat` are recognised but print a
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

## Stage 1: Process Table & Scheduler

Stage 1 adds pre-emptive multitasking: a fixed-size process table (PCBs), a
round-robin scheduler driven by a 100 Hz hardware timer, and a real context
switch between process stacks.

**How it fits together:**

- `kernel/idt.c` builds a minimal Interrupt Descriptor Table and remaps the
  8259 PIC so IRQ0 (the timer) is delivered on interrupt vector `0x20`,
  without colliding with CPU exception vectors. Every other IRQ line is
  masked off - the keyboard is still polled (Stage 0), not interrupt-driven.
- `kernel/pit.c` programs the i8253 timer to fire IRQ0 every 10 ms (100 Hz).
- `kernel/switch.asm` is the IRQ0 handler: it saves the running process's
  registers (`pushad`), calls `scheduler_tick()` in C with the current stack
  pointer, loads whatever stack pointer that function returns, sends the
  End-Of-Interrupt to the PIC, then restores registers (`popad`) and returns
  (`iret`) - possibly into a completely different process.
- `kernel/process.c` owns the static `pcb_table[MAX_PROCESSES]` array and
  `process_create()`, which fabricates a fake saved context so a brand-new
  process starts at its entry point the first time it's switched to.
- `kernel/scheduler.c` owns the ready queue (a singly-linked list via
  `pcb->next`) and `scheduler_tick()`, which performs an actual round-robin
  switch every 10 ticks (a 100 ms quantum).

**Proving two processes run concurrently:** at boot, two background
processes are created automatically (`demo_process_a`, `demo_process_b` in
`kernel/kernel.c`). Each spins a `|/-\` character in a fixed corner of the
screen (`P1:` and `P2:`, top-right), busy-waiting a different number of
iterations between updates so they visibly spin at different rates -
independently of, and concurrently with, the interactive shell (PCB 0).
They write through the new `vga_put_at()` helper so they never disturb the
shell's cursor.

**New/changed shell command:**

| Command | Arguments | Behaviour                                    |
|---------|-----------|-----------------------------------------------|
| `ps`    | -         | Lists every PCB: pid, state, saved ESP         |

### How to test Stage 1

1. `make run` and wait for the splash screen.
2. Watch the top-right corner: `P1:` and `P2:` should each show a spinning
   character (`|/-\`), changing at visibly different speeds - this is the
   two background processes running concurrently.
3. While they keep spinning, use the shell normally: `help`, `echo test`,
   `version` - it should stay fully responsive.
4. `ps` - should list 3 PCBs (pid 0 = the shell, pid 1 and 2 = the demo
   processes) with their state (`RUNNING`/`READY`) and ESP.
5. `halt` - should still stop the CPU cleanly with no crash or reboot.

No optional/bonus extensions are implemented in this stage.

## Stage 2: Threads, Mutex & Semaphore

Stage 2 adds kernel threads and the two classic synchronisation primitives
on top of Stage 1's scheduler, and uses them to demonstrate a real race
condition and a correct producer-consumer solution.

**How it fits together:**

- `kernel/thread.h/.c` - `thread_create(fn, arg)` creates a schedulable PCB
  (via `process_create()`) whose entry point is a small trampoline that
  looks up the (fn, arg) pair for "whichever PCB I currently am" and calls
  `fn(arg)`. Since this kernel has no virtual memory yet, every thread
  already runs in the same flat address space as everything else - which is
  exactly the property "kernel threads sharing an address space" is about.
- `kernel/mutex.h/.c` - `mutex_lock()`/`mutex_unlock()`. A thread that finds
  the mutex held is moved to `BLOCKED` and taken off the CPU entirely (not
  spin-waiting): it is added to the mutex's own wait queue and immediately
  switched away. `mutex_unlock()` wakes the next waiter, if any.
- `kernel/semaphore.h/.c` - `sem_wait()`/`sem_signal()`, a counting
  semaphore built the same way (block instead of spin when the count is 0).
- `kernel/process.h/.c` gained `pcb_queue_add()`/`pcb_queue_remove()`, a
  small generic linked-queue helper shared by the scheduler's ready queue
  and by every mutex/semaphore's wait queue (a process is only ever in one
  such queue at a time, so they can all reuse `pcb->next`).
- **Scheduler fix:** Stage 1's `scheduler_tick()` only ever switched
  processes at a 100 ms quantum boundary. That is fine for routine
  round-robin, but a thread that just blocked (or that explicitly calls
  `process_yield()`) must be switched away from *immediately*, or it would
  fall straight back into the code that just blocked it. `scheduler_tick()`
  now also forces a switch whenever the current process's state is no
  longer `RUNNING`, or a new `process_yield()` requested one.

**Race-condition demo (`race`):** two threads each increment a shared
`myglobal` 500 times. Every iteration deliberately does an unprotected
read...`process_yield()`...modify...write, which reliably forces the two
threads to interleave on (almost) every increment - instead of hoping a
real timer tick happens to land in the tiny natural race window. Run once
without a mutex (final value comes out wrong - lost updates) and once with
a mutex held across the same three lines (always comes out correct).

**Producer-consumer demo (`producer`):** a producer and a consumer thread
share a 5-slot bounded buffer, synchronised with exactly three counting
semaphores - `sem_empty` (free slots), `sem_full` (filled slots) and
`sem_mutex` (mutual exclusion on the buffer itself). 10 items are produced
and consumed; the demo prints every put/get so you can see the buffer never
overflows, underflows, or loses an item.

**New shell commands:**

| Command    | Arguments | Behaviour                                             |
|------------|-----------|--------------------------------------------------------|
| `race`     | -         | Runs the race-condition demo without, then with, a mutex |
| `producer` | -         | Runs the bounded-buffer producer-consumer demo            |

### How to test Stage 2

1. `make run` and wait for the splash screen (the Stage 1 spinners should
   still be running in the top-right corner).
2. `race` - prints `WITHOUT mutex : myglobal = <something less than 1000>`
   followed by `<-- CORRUPTED, lost updates!`, then
   `WITH mutex : myglobal = 1000  (correct)`.
3. `producer` - prints 10 `[producer] put N in slot S` / `[consumer] got N
   from slot S` pairs in order, ending with "Done - all items produced and
   consumed, no corruption."
4. `ps` - the finished demo threads should show as `TERMINATED`, while
   pid 0-2 (shell + Stage 1 spinners) are still `RUNNING`/`READY`.
5. `halt` - should still stop the CPU cleanly with no crash or reboot.

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
