/* =============================================================================
 * SENG21213-OS :: Main Kernel  (Stage 2 - Threads, Mutex & Semaphore)
 * File   : kernel/kernel.c
 *
 * PURPOSE
 *   This is the heart of your operating system. Right now it:
 *     1. Initialises VGA text-mode display
 *     2. Initialises the keyboard driver
 *     3. Sets up the IDT, remaps the PIC and programs the PIT for a 100 Hz
 *        timer tick (L09) so the round-robin scheduler can pre-empt
 *     4. Creates two background demo processes to prove pre-emption works
 *     5. Prints a splash screen
 *     6. Runs a minimal interactive shell ("ksh"), itself scheduled as PCB 0,
 *        which can also launch the Stage 2 thread/mutex/semaphore demos
 *
 * ASSIGNMENT MILESTONES  (what YOU will add in later lectures)
 *   Lecture  9  - Process Management  ->  process.h / process.c / scheduler.c        [DONE]
 *   Lecture 10  - Threads & Sync      ->  thread.c / mutex.c / semaphore.c            [DONE]
 *   Lecture 11  - Memory Management   ->  pmm.h     / pmm.c / vmm.c
 *   Lecture 12  - File System         ->  fs.h      / fs.c
 *
 * CODING CONVENTION
 *   - Prefix kernel-internal functions with k_ (e.g. k_strcmp)
 *   - All driver APIs live in their own .h/.c pair
 *   - NEVER call malloc - use the PMM you build in Lecture 11
 * ============================================================================*/

#include "vga.h"
#include "keyboard.h"
#include "process.h"
#include "idt.h"
#include "pit.h"
#include "thread.h"
#include "mutex.h"
#include "semaphore.h"
#include "../include/types.h"

/* ---------------------------------------------------------------------------
 * Forward declarations of shell commands
 * --------------------------------------------------------------------------*/
static void cmd_help(void);
static void cmd_clear(void);
static void cmd_about(void);
static void cmd_echo(const char *args);
static void cmd_mem(void);
static void cmd_version(void);
static void cmd_colour(const char *args);
static void cmd_halt(void);
static void cmd_ps(void);
static void cmd_race(void);
static void cmd_producer(void);

/* ---------------------------------------------------------------------------
 * Utility: minimal string helpers (no libc in a freestanding kernel!)
 * --------------------------------------------------------------------------*/
static int k_strcmp(const char *a, const char *b) {
    while (*a && (*a == *b)) { a++; b++; }
    return (uint8_t)*a - (uint8_t)*b;
}

static int k_strncmp(const char *a, const char *b, size_t n) {
    while (n-- && *a && (*a == *b)) { a++; b++; }
    return n == (size_t)-1 ? 0 : (uint8_t)*a - (uint8_t)*b;
}

static size_t k_strlen(const char *s) {
    size_t n = 0;
    while (s[n]) n++;
    return n;
}

/* Skip leading spaces */
static const char *k_ltrim(const char *s) {
    while (*s == ' ') s++;
    return s;
}

/* Parse an unsigned decimal number at *pp. Returns false (and leaves *pp
 * untouched) if the string does not start with a digit. Used by cmd_colour. */
static bool k_parse_uint(const char **pp, int *out) {
    const char *p = *pp;
    if (*p < '0' || *p > '9') return false;
    int n = 0;
    while (*p >= '0' && *p <= '9') { n = n * 10 + (*p - '0'); p++; }
    *out = n;
    *pp = p;
    return true;
}

/* ---------------------------------------------------------------------------
 * Splash Screen
 * --------------------------------------------------------------------------*/
static void print_splash(void) {
    vga_clear(VGA_BLACK);

    /* Top banner box */
    vga_draw_box(0, 0, 7, 80, VGA_LIGHT_MAGENTA);

    vga_set_cursor(1, 2);
    vga_puts_color("  SENG21213-OS  |  Computer Architecture & Operating Systems",
                   VGA_YELLOW, VGA_BLACK);

    vga_set_cursor(2, 2);
    vga_puts_color("  Stage 2: Threads, Mutex & Semaphore", VGA_LIGHT_CYAN, VGA_BLACK);

    vga_set_cursor(3, 2);
    vga_puts_color("  Faculty of Engineering - Department of Software Engineering",
                   VGA_LIGHT_GREY, VGA_BLACK);

    vga_set_cursor(4, 2);
    vga_puts_color("  Built by students, for students.  Type 'help' to begin.",
                   VGA_LIGHT_GREEN, VGA_BLACK);

    vga_set_cursor(5, 2);
    vga_puts_color("  CPU: i686 (32-bit Protected Mode)  |  Display: VGA 80x25",
                   VGA_DARK_GREY, VGA_BLACK);

    vga_set_cursor(8, 0);
    vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);
    vga_puts("  Welcome! This kernel was compiled from source and booted entirely\n");
    vga_puts("  from bare metal. There is no Linux or Windows underneath - only\n");
    vga_puts("  the code you and your team write.\n");
    vga_puts("\n");
    vga_puts("  Assignment milestones:\n");
    vga_puts_color("    [L09] ", VGA_LIGHT_GREEN, VGA_BLACK);
    vga_puts("Process Management  - PCB, ready queue, round-robin scheduler   (done)\n");
    vga_puts_color("    [L10] ", VGA_LIGHT_GREEN, VGA_BLACK);
    vga_puts("Threads & Sync      - kernel threads, mutex, semaphore          (done)\n");
    vga_puts_color("    [L11] ", VGA_YELLOW, VGA_BLACK);
    vga_puts("Memory Management   - physical page allocator, virtual memory\n");
    vga_puts_color("    [L12] ", VGA_YELLOW, VGA_BLACK);
    vga_puts("File System         - RAM disk, FAT-like directory structure\n");
    vga_puts("\n");

    /* Static labels for the two background demo processes (see
     * demo_process_a/b below) - the spinner characters themselves are
     * drawn directly via vga_put_at() so they don't disturb this cursor. */
    vga_set_cursor(1, 66);
    vga_puts_color("P1:", VGA_LIGHT_GREY, VGA_BLACK);
    vga_set_cursor(1, 73);
    vga_puts_color("P2:", VGA_LIGHT_GREY, VGA_BLACK);
}

/* ---------------------------------------------------------------------------
 * Shell command implementations
 * --------------------------------------------------------------------------*/
static void cmd_help(void) {
    vga_puts_color("\n  SENG21213-OS Shell Commands\n", VGA_YELLOW, VGA_BLACK);
    vga_puts("  ---------------------------------------------\n");
    vga_puts("  help    - Show this help message\n");
    vga_puts("  clear   - Clear the screen\n");
    vga_puts("  about   - About this OS and course\n");
    vga_puts("  echo    - Echo text to screen\n");
    vga_puts("  mem     - Memory map (stub)\n");
    vga_puts("  version - Show kernel name and version\n");
    vga_puts("  colour  - colour <fg> <bg>  (values 0-15)\n");
    vga_puts("  halt    - Disable interrupts and halt the CPU\n");
    vga_puts("  ps      - [L09] List processes (pid/state/esp)\n");
    vga_puts("  race    - [L10] Race-condition demo, with/without a mutex\n");
    vga_puts("  producer- [L10] Producer-consumer demo (bounded buffer)\n");
    vga_puts_color("\n  Milestones (to implement):\n", VGA_LIGHT_CYAN, VGA_BLACK);
    vga_puts("  kill    - [L09] Terminate a process\n");
    vga_puts("  threads - [L10] List kernel threads\n");
    vga_puts("  free    - [L11] Show free memory\n");
    vga_puts("  ls      - [L12] List files\n");
    vga_puts("  cat     - [L12] Print file contents\n\n");
}

static void cmd_clear(void) {
    vga_clear(VGA_BLACK);
}

static void cmd_about(void) {
    vga_puts_color("\n  About SENG21213-OS\n", VGA_LIGHT_CYAN, VGA_BLACK);
    vga_puts("  ---------------------------------------------\n");
    vga_puts("  Architecture : x86 (i686), 32-bit Protected Mode\n");
    vga_puts("  Bootloader   : Custom MBR (NASM)\n");
    vga_puts("  Kernel       : Freestanding C (GCC, no libc)\n");
    vga_puts("  VM Target    : QEMU (qemu-system-i386)\n");
    vga_puts("  Course       : SENG 21213 - Sem 2\n");
    vga_puts("  Reference    : Stallings, OS: Internals & Design Principles\n\n");
}

static void cmd_echo(const char *args) {
    vga_puts("  ");
    vga_puts(args);
    vga_puts("\n");
}

static void cmd_mem(void) {
    /* Stage 0 stub - students implement the real PMM in Lecture 11 */
    vga_puts_color("\n  Memory Map (stub - implement PMM in Lecture 11)\n",
                   VGA_LIGHT_CYAN, VGA_BLACK);
    vga_puts("  ---------------------------------------------\n");
    vga_puts("  0x00000000 - 0x000FFFFF  :  First 1 MB (reserved/BIOS)\n");
    vga_puts("  0x00100000 - 0x00EFFFFF  :  Extended memory (usable ~14 MB)\n");
    vga_puts("  0x00F00000 - 0x00FFFFFF  :  BIOS / ROM area\n");
    vga_puts("  0xB8000    - 0xBFFFF     :  VGA frame buffer\n");
    vga_puts_color("\n  TODO: Use BIOS int 0x15, EAX=0xE820 to get real memory map\n\n",
                   VGA_YELLOW, VGA_BLACK);
}

/* L08 - prints the kernel name and version string for this stage */
static void cmd_version(void) {
    vga_puts("\n  SENG21213-OS  v0.3-stage2\n");
    vga_puts("  Stage 2 : Threads, Mutex & Semaphore\n\n");
}

/* L08 - colour <fg> <bg>, both 0-15 (see vga.h vga_color_t) */
static void cmd_colour(const char *args) {
    args = k_ltrim(args);

    int fg, bg;
    if (!k_parse_uint(&args, &fg)) {
        vga_puts_color("  Usage: colour <fg> <bg>   (values 0-15)\n", VGA_LIGHT_RED, VGA_BLACK);
        return;
    }
    args = k_ltrim(args);
    if (!k_parse_uint(&args, &bg)) {
        vga_puts_color("  Usage: colour <fg> <bg>   (values 0-15)\n", VGA_LIGHT_RED, VGA_BLACK);
        return;
    }
    if (fg > 15 || bg > 15) {
        vga_puts_color("  Error: colour values must be 0-15\n", VGA_LIGHT_RED, VGA_BLACK);
        return;
    }

    vga_set_color((vga_color_t)fg, (vga_color_t)bg);
    vga_puts("  Colour changed.\n");
}

/* L08 - disables interrupts and halts the CPU permanently */
static void cmd_halt(void) {
    vga_puts_color("\n  Halting system... interrupts disabled.\n\n", VGA_LIGHT_RED, VGA_BLACK);
    __asm__ __volatile__("cli");
    for (;;) {
        __asm__ __volatile__("hlt");
    }
}

/* L09 - lists every PCB (pid/state/esp); implemented in process.c */
static void cmd_ps(void) {
    process_list();
}

/* ---------------------------------------------------------------------------
 * Stage 1 demo: two background processes that prove pre-emptive round-robin
 * scheduling is actually working. Each spins a small character through
 * "|/-\" at a fixed screen cell, busy-waiting a different number of
 * iterations between updates - so P1 visibly spins faster than P2, and
 * both keep spinning independently while you use the shell.
 *
 * They write ONLY through vga_put_at(), never vga_puts()/vga_printf(),
 * because those move the single shared cursor that the shell prompt also
 * relies on - two processes fighting over one cursor would corrupt the
 * screen the moment the scheduler pre-empts one mid-line.
 * --------------------------------------------------------------------------*/
#define DEMO_CHUNK 50000L  /* how much busy-work between cooperative yields */

static void demo_spin(int row, int col, vga_color_t fg, long delay_iters) {
    static const char spinner[4] = { '|', '/', '-', '\\' };
    int idx = 0;
    for (;;) {
        vga_put_at(row, col, spinner[idx % 4], fg, VGA_BLACK);
        idx++;
        /* Busy-wait in small chunks, yielding between them, instead of one
         * long uninterrupted spin - otherwise, once picked by the
         * scheduler, this process would hog the CPU for a full quantum
         * (L09 §100ms) before anyone else gets a turn. That's invisible
         * with only a couple of slow background spinners (Stage 1), but it
         * badly stalls Stage 2's threads, which yield far more often. */
        for (long done = 0; done < delay_iters; done += DEMO_CHUNK) {
            for (volatile long i = 0; i < DEMO_CHUNK; i++) { }
            process_yield();
        }
    }
}

static void demo_process_a(void) { demo_spin(1, 69, VGA_LIGHT_GREEN, 2000000L); }
static void demo_process_b(void) { demo_spin(1, 76, VGA_LIGHT_CYAN,  6000000L); }

/* ---------------------------------------------------------------------------
 * Stage 2 demo #1: race condition on a shared global, with and without a
 * mutex. Two threads each add 1 to myglobal RACE_ITERS times. Each
 * iteration does an unprotected read...modify...write with a forced
 * process_yield() sitting right in the middle of it - that guarantees the
 * two threads interleave on (almost) every single increment, instead of
 * leaving it to chance whether a real timer tick ever lands in the tiny
 * window where the bug lives. With the mutex held across the same three
 * lines, that forced yield cannot cause a lost update, because the other
 * thread blocks instead of reading a stale value.
 * --------------------------------------------------------------------------*/
#define RACE_ITERS 500

static volatile int32_t myglobal      = 0;
static volatile int     race_finished = 0;
static int              race_use_mutex;
static mutex_t          race_mutex;

static void race_worker(void *arg) {
    (void)arg;
    for (int i = 0; i < RACE_ITERS; i++) {
        if (race_use_mutex) mutex_lock(&race_mutex);

        int32_t tmp = myglobal;   /* read            */
        process_yield();          /* force the race window to be hit */
        tmp = tmp + 1;            /* modify          */
        myglobal = tmp;           /* write - a lost update happens here
                                    * if another thread's write landed
                                    * in between and this thread's `tmp`
                                    * is now stale. */

        if (race_use_mutex) mutex_unlock(&race_mutex);
    }
    race_finished++;
}

static void run_race_once(int use_mutex) {
    myglobal       = 0;
    race_finished  = 0;
    race_use_mutex = use_mutex;
    if (use_mutex) mutex_init(&race_mutex);

    thread_create(race_worker, NULL);
    thread_create(race_worker, NULL);
    while (race_finished < 2) process_yield();

    vga_printf("  %s mutex : myglobal = %d", use_mutex ? "WITH   " : "WITHOUT", myglobal);
    if (myglobal == RACE_ITERS * 2) {
        vga_puts_color("  (correct)\n", VGA_LIGHT_GREEN, VGA_BLACK);
    } else {
        vga_puts_color("  <-- CORRUPTED, lost updates!\n", VGA_LIGHT_RED, VGA_BLACK);
    }
}

static void cmd_race(void) {
    vga_puts_color("\n  Race-condition demo: 2 threads x ", VGA_YELLOW, VGA_BLACK);
    vga_printf("%d increments of a shared int.\n", RACE_ITERS);
    vga_printf("  Expected correct final value: %d\n\n", RACE_ITERS * 2);

    run_race_once(0);   /* without protection - almost always corrupted */
    run_race_once(1);   /* with the mutex - always correct               */
    vga_puts("\n");
}

/* ---------------------------------------------------------------------------
 * Stage 2 demo #2: bounded-buffer producer-consumer using exactly three
 * counting semaphores - sem_empty (free slots), sem_full (filled slots) and
 * sem_mutex (mutual exclusion on the buffer itself), the classic Stallings
 * solution to the bounded-buffer problem.
 * --------------------------------------------------------------------------*/
#define PC_BUFFER_SIZE 5
#define PC_ITEMS       10

static int         pc_buffer[PC_BUFFER_SIZE];
static int         pc_in, pc_out;
static semaphore_t sem_empty, sem_full, sem_mutex;
static volatile int pc_finished;

static void producer_thread(void *arg) {
    (void)arg;
    for (int item = 1; item <= PC_ITEMS; item++) {
        sem_wait(&sem_empty);              /* wait for a free slot   */
        sem_wait(&sem_mutex);               /* enter critical section */

        pc_buffer[pc_in] = item;
        vga_printf("  [producer] put %d in slot %d\n", item, pc_in);
        pc_in = (pc_in + 1) % PC_BUFFER_SIZE;

        sem_signal(&sem_mutex);
        sem_signal(&sem_full);              /* announce a filled slot */
        process_yield();
    }
    pc_finished++;
}

static void consumer_thread(void *arg) {
    (void)arg;
    for (int i = 0; i < PC_ITEMS; i++) {
        sem_wait(&sem_full);                 /* wait for a filled slot */
        sem_wait(&sem_mutex);

        int item = pc_buffer[pc_out];
        vga_printf("  [consumer] got %d from slot %d\n", item, pc_out);
        pc_out = (pc_out + 1) % PC_BUFFER_SIZE;

        sem_signal(&sem_mutex);
        sem_signal(&sem_empty);              /* announce a free slot */
        process_yield();
    }
    pc_finished++;
}

static void cmd_producer(void) {
    vga_puts_color("\n  Producer-Consumer demo (buffer size 5, 10 items)\n",
                   VGA_YELLOW, VGA_BLACK);
    vga_puts("  ---------------------------------------------\n");

    pc_in = pc_out = 0;
    pc_finished = 0;
    sem_init(&sem_empty, PC_BUFFER_SIZE);
    sem_init(&sem_full, 0);
    sem_init(&sem_mutex, 1);

    thread_create(producer_thread, NULL);
    thread_create(consumer_thread, NULL);
    while (pc_finished < 2) process_yield();

    vga_puts_color("  Done - all items produced and consumed, no corruption.\n\n",
                   VGA_LIGHT_GREEN, VGA_BLACK);
}

/* ---------------------------------------------------------------------------
 * Shell process
 * --------------------------------------------------------------------------*/
static char  shell_buf[256];
static char  prompt[] = "\n  ksh> ";

static void shell_run(void) {
    vga_puts_color("\n  Kernel Shell ready. Type 'help' for commands.\n",
                   VGA_LIGHT_GREEN, VGA_BLACK);

    while (true) {
        vga_puts_color(prompt, VGA_LIGHT_GREEN, VGA_BLACK);
        kb_readline(shell_buf, sizeof(shell_buf));

        /* Trim leading whitespace */
        const char *cmd = k_ltrim(shell_buf);
        if (k_strlen(cmd) == 0) continue;

        /* Dispatch */
        if (k_strcmp(cmd, "help")    == 0) { cmd_help();    continue; }
        if (k_strcmp(cmd, "clear")   == 0) { cmd_clear();   continue; }
        if (k_strcmp(cmd, "about")   == 0) { cmd_about();   continue; }
        if (k_strcmp(cmd, "mem")     == 0) { cmd_mem();     continue; }
        if (k_strcmp(cmd, "version") == 0) { cmd_version(); continue; }
        if (k_strcmp(cmd, "halt")    == 0) { cmd_halt();    continue; }
        if (k_strcmp(cmd, "ps")      == 0) { cmd_ps();      continue; }
        if (k_strcmp(cmd, "race")     == 0) { cmd_race();     continue; }
        if (k_strcmp(cmd, "producer") == 0) { cmd_producer(); continue; }

        if (k_strncmp(cmd, "echo ", 5) == 0) {
            cmd_echo(k_ltrim(cmd + 5));
            continue;
        }

        if (k_strcmp(cmd, "colour") == 0 || k_strncmp(cmd, "colour ", 7) == 0) {
            cmd_colour(cmd + 6);
            continue;
        }

        /* Milestone stubs */
        if (k_strcmp(cmd, "kill")    == 0 ||
            k_strcmp(cmd, "threads") == 0 ||
            k_strcmp(cmd, "free")    == 0 ||
            k_strcmp(cmd, "ls")      == 0 ||
            k_strcmp(cmd, "cat")     == 0) {
            vga_puts_color("  [TODO] This command is not yet implemented.\n",
                           VGA_YELLOW, VGA_BLACK);
            vga_puts("  Implement it as part of your lecture assignment.\n");
            continue;
        }

        vga_puts_color("  Unknown command: ", VGA_LIGHT_RED, VGA_BLACK);
        vga_puts(cmd);
        vga_puts("\n  Type 'help' for a list of commands.\n");
    }
}

/* ---------------------------------------------------------------------------
 * Kernel entry point - called from kernel_entry.asm
 * --------------------------------------------------------------------------*/
void kernel_main(void) {
    vga_init();
    kb_init();

    /* L09 - bring up interrupts and the scheduler before enabling IF.
     * Order matters: the IDT/PIT must be programmed, and the process
     * table + ready queue must already contain PCB 0 (this very
     * execution context) and the two demo processes, before we turn
     * interrupts on - otherwise IRQ0 could fire into an empty scheduler. */
    idt_init();
    pit_init(100);          /* 100 Hz -> a tick every 10 ms */
    process_init();
    scheduler_init();

    pcb_t *pa = process_create(demo_process_a);
    pcb_t *pb = process_create(demo_process_b);
    if (pa) scheduler_add(pa);
    if (pb) scheduler_add(pb);

    __asm__ __volatile__("sti");   /* pre-emption is now live */

    print_splash();
    shell_run();

    /* Should never reach here */
    __asm__ __volatile__("hlt");
}
