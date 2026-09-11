/* =============================================================================
 * SENG21213-OS :: Round-Robin Scheduler
 * File   : kernel/scheduler.c
 * L09 §Scheduling - a singly-linked ready queue plus a fixed time quantum.
 *
 * scheduler_tick() is called by the IRQ0 handler (kernel/switch.asm) on
 * every 10 ms PIT tick, AND by process_yield()/mutex_lock()/sem_wait() via
 * a software "int $0x20" (L10). It returns the ESP of the process that
 * should run next.
 *
 * A real hardware tick only causes a switch every QUANTUM_TICKS ticks (fair
 * round-robin between processes that all still want the CPU). But a process
 * that voluntarily yields, or that just blocked on a mutex/semaphore, must
 * be switched away from IMMEDIATELY - never left running - or a blocked
 * thread could fall straight back into the code that just blocked it,
 * corrupting the wait queue it was just added to. `force_switch` and the
 * process's own state (BLOCKED/TERMINATED) are what make that immediate.
 * ============================================================================*/
#include "process.h"

#define QUANTUM_TICKS 10   /* 10 ticks x 10 ms = 100 ms per process turn */

static pcb_t *ready_head = NULL;
static pcb_t *ready_tail = NULL;
static uint32_t tick_count = 0;
static volatile bool force_switch = false;

/* Public: safe to call from foreground (process/thread) code, e.g.
 * thread_create(). Interrupts must be disabled while touching the ready
 * queue, since a timer tick could otherwise fire mid-update and have
 * scheduler_tick() (below) touch the very same list concurrently. */
void scheduler_add(pcb_t *p) {
    __asm__ __volatile__("cli");
    pcb_queue_add(&ready_head, &ready_tail, p);
    __asm__ __volatile__("sti");
}

void scheduler_init(void) {
    /* PCB 0 represents the code that is already running at boot time -
     * the kernel/shell itself. It needs no fabricated stack: the very
     * first time it is preempted, the IRQ0 stub will save its real,
     * already-in-progress ESP for us. */
    pcb_table[0].pid   = 0;
    pcb_table[0].state = RUNNING;
    pcb_table[0].esp   = 0;
    pcb_table[0].next  = NULL;

    current_process = &pcb_table[0];
    ready_head = ready_tail = NULL;
    tick_count = 0;
}

/* Called only from inside the IRQ0 handler, where the CPU has already
 * disabled interrupts on entry (it's an interrupt gate, not a trap gate) -
 * so no extra cli/sti is needed here. */
uint32_t scheduler_tick(uint32_t current_esp) {
    tick_count++;

    bool blocked_or_done = current_process && current_process->state != RUNNING;
    bool quantum_elapsed = (tick_count % QUANTUM_TICKS == 0);

    if (!blocked_or_done && !force_switch && !quantum_elapsed) {
        return current_esp;   /* still this process's turn - nothing to do */
    }
    force_switch = false;

    if (current_process) {
        current_process->esp = current_esp;
        if (current_process->state == RUNNING) {
            current_process->state = READY;
            pcb_queue_add(&ready_head, &ready_tail, current_process);
        }
        /* BLOCKED: the mutex/semaphore that blocked it owns re-adding it
         * to the ready queue later, via scheduler_add().
         * TERMINATED: process_exit() - just drop it, slot is reusable. */
    }

    pcb_t *next = pcb_queue_remove(&ready_head, &ready_tail);
    if (!next) {
        return current_esp;  /* nothing ready (shouldn't normally happen) */
    }

    next->state     = RUNNING;
    current_process = next;
    return next->esp;
}

/* Voluntarily give up the rest of the current quantum right now - used by
 * the Stage 2 demos and by mutex/semaphore when they need to block. */
void process_yield(void) {
    force_switch = true;
    __asm__ __volatile__("int $0x20");
}

void process_exit(void) {
    current_process->state = TERMINATED;
    __asm__ __volatile__("int $0x20");
    for (;;) { __asm__ __volatile__("hlt"); }  /* should never get here */
}
