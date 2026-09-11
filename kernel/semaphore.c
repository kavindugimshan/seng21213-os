/* =============================================================================
 * SENG21213-OS :: Counting Semaphore
 * File   : kernel/semaphore.c
 * Same blocking pattern as mutex.c - see the comments there for why each
 * cli/sti and the sti-before-yield ordering are needed.
 * ============================================================================*/
#include "semaphore.h"

void sem_init(semaphore_t *s, int initial_count) {
    s->count     = initial_count;
    s->wait_head = NULL;
    s->wait_tail = NULL;
}

void sem_wait(semaphore_t *s) {
    __asm__ __volatile__("cli");
    while (s->count == 0) {
        pcb_t *me = current_process;
        me->state = BLOCKED;
        pcb_queue_add(&s->wait_head, &s->wait_tail, me);

        __asm__ __volatile__("sti");
        process_yield();
        __asm__ __volatile__("cli");
    }
    s->count--;
    __asm__ __volatile__("sti");
}

void sem_signal(semaphore_t *s) {
    __asm__ __volatile__("cli");
    s->count++;
    pcb_t *waiter = pcb_queue_remove(&s->wait_head, &s->wait_tail);
    __asm__ __volatile__("sti");

    if (waiter) {
        waiter->state = READY;
        scheduler_add(waiter);
    }
}
