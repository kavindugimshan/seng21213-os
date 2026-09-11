/* =============================================================================
 * SENG21213-OS :: Blocking Mutex
 * File   : kernel/mutex.c
 *
 * The "locked" flag and the wait queue are shared state that both normal
 * (foreground) threads AND the timer ISR can touch (a tick can fire between
 * any two C statements), so every read-modify-write of them is wrapped in a
 * short cli/sti critical section (L10 - mutual exclusion via disabling
 * interrupts, the lowest-level technique, used here to protect the
 * higher-level mutex_t abstraction itself).
 * ============================================================================*/
#include "mutex.h"

void mutex_init(mutex_t *m) {
    m->locked    = 0;
    m->owner     = NULL;
    m->wait_head = NULL;
    m->wait_tail = NULL;
}

void mutex_lock(mutex_t *m) {
    __asm__ __volatile__("cli");
    while (m->locked) {
        pcb_t *me = current_process;
        me->state = BLOCKED;
        pcb_queue_add(&m->wait_head, &m->wait_tail, me);

        /* sti() must happen before the yield: the CPU freezes the current
         * EFLAGS into this thread's saved context at the moment of the
         * "int $0x20" below, and that saved IF bit is what this thread
         * resumes with once mutex_unlock() wakes it - so it must be 1,
         * or this thread would come back with interrupts permanently off. */
        __asm__ __volatile__("sti");
        process_yield();              /* switches away; resumes here once woken */
        __asm__ __volatile__("cli");
        /* Re-check m->locked: another thread may have grabbed it first. */
    }
    m->locked = 1;
    m->owner  = current_process;
    __asm__ __volatile__("sti");
}

void mutex_unlock(mutex_t *m) {
    __asm__ __volatile__("cli");
    m->locked = 0;
    m->owner  = NULL;

    pcb_t *waiter = pcb_queue_remove(&m->wait_head, &m->wait_tail);
    __asm__ __volatile__("sti");

    if (waiter) {
        waiter->state = READY;
        scheduler_add(waiter);   /* scheduler_add() does its own cli/sti */
    }
}
