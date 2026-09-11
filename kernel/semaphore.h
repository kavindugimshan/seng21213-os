/* =============================================================================
 * SENG21213-OS :: Counting Semaphore
 * File   : kernel/semaphore.h
 * L10 §Semaphores - sem_wait() (P) blocks while count == 0, sem_signal() (V)
 * increments count and wakes one waiter if any are blocked.
 * ============================================================================*/
#ifndef SEMAPHORE_H
#define SEMAPHORE_H

#include "process.h"

typedef struct {
    volatile int count;
    pcb_t *wait_head;
    pcb_t *wait_tail;
} semaphore_t;

void sem_init(semaphore_t *s, int initial_count);
void sem_wait(semaphore_t *s);
void sem_signal(semaphore_t *s);

#endif /* SEMAPHORE_H */
