/* =============================================================================
 * SENG21213-OS :: Blocking Mutex
 * File   : kernel/mutex.h
 * L10 §Mutual exclusion - a thread that calls mutex_lock() on a held mutex
 * is moved to BLOCKED and taken off the CPU entirely (not spin-waiting)
 * until mutex_unlock() wakes it.
 * ============================================================================*/
#ifndef MUTEX_H
#define MUTEX_H

#include "process.h"

typedef struct {
    volatile int locked;
    pcb_t *owner;
    pcb_t *wait_head;
    pcb_t *wait_tail;
} mutex_t;

void mutex_init(mutex_t *m);
void mutex_lock(mutex_t *m);
void mutex_unlock(mutex_t *m);

#endif /* MUTEX_H */
