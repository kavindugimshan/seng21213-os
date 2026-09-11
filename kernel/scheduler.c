/* =============================================================================
 * SENG21213-OS :: Round-Robin Scheduler
 * File   : kernel/scheduler.c
 * L09 §Scheduling - a singly-linked ready queue plus a fixed time quantum.
 *
 * scheduler_tick() is called by the IRQ0 handler (kernel/switch.asm) on
 * every 10 ms PIT tick. Every QUANTUM_TICKS ticks it performs an actual
 * round-robin switch: the running process goes to the back of the ready
 * queue and the process at the front becomes RUNNING. Between quantum
 * boundaries it just returns the same ESP, so the interrupt is effectively
 * a no-op from the running process's point of view.
 * ============================================================================*/
#include "process.h"

#define QUANTUM_TICKS 10   /* 10 ticks x 10 ms = 100 ms per process turn */

static pcb_t *ready_head = NULL;
static pcb_t *ready_tail = NULL;
static uint32_t tick_count = 0;

/* Append p to the back of the ready queue. */
void scheduler_add(pcb_t *p) {
    p->next = NULL;
    if (!ready_head) {
        ready_head = ready_tail = p;
    } else {
        ready_tail->next = p;
        ready_tail = p;
    }
}

/* Take the process at the front of the ready queue off the queue. */
static pcb_t *ready_pop(void) {
    if (!ready_head) return NULL;
    pcb_t *p = ready_head;
    ready_head = ready_head->next;
    if (!ready_head) ready_tail = NULL;
    p->next = NULL;
    return p;
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

uint32_t scheduler_tick(uint32_t current_esp) {
    tick_count++;

    /* Not a quantum boundary yet - keep running the same process. */
    if (tick_count % QUANTUM_TICKS != 0) {
        return current_esp;
    }

    if (current_process) {
        current_process->esp = current_esp;
        if (current_process->state == RUNNING) {
            current_process->state = READY;
            scheduler_add(current_process);
        }
        /* If it was TERMINATED (process_exit), just drop it - it is not
         * re-added to the ready queue and its slot can be reused. */
    }

    pcb_t *next = ready_pop();
    if (!next) {
        /* Nothing else ready (should not happen - the shell's own PCB is
         * always in the queue once running) - keep the current process. */
        return current_esp;
    }

    next->state      = RUNNING;
    current_process  = next;
    return next->esp;
}

void process_yield(void) {
    __asm__ __volatile__("int $0x20");
}

void process_exit(void) {
    current_process->state = TERMINATED;
    __asm__ __volatile__("int $0x20");
    for (;;) { __asm__ __volatile__("hlt"); }  /* should never get here */
}
