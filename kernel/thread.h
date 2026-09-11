/* =============================================================================
 * SENG21213-OS :: Kernel Threads
 * File   : kernel/thread.h
 * L10 - A "thread" here is a schedulable PCB, exactly like a Stage 1
 * process, that runs a function taking one argument. Since this kernel has
 * no virtual memory yet (Stage 3), every process already runs in the same
 * flat physical address space - so a thread genuinely does "share the
 * address space" with everything else, which is the property this
 * assignment stage is about, without needing a separate address-space
 * concept to turn off.
 * ============================================================================*/
#ifndef THREAD_H
#define THREAD_H

#include "process.h"

typedef void (*thread_fn_t)(void *arg);

/* Creates and schedules a new kernel thread that will call fn(arg). */
pcb_t *thread_create(thread_fn_t fn, void *arg);

#endif /* THREAD_H */
