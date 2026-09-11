/* =============================================================================
 * SENG21213-OS :: Process Management
 * File   : kernel/process.h
 * L09 - Process description (Stallings Ch.3) and Round-Robin scheduling (Ch.9).
 *
 * A process is represented by a PCB (Process Control Block). Stage 1 keeps
 * every PCB in a fixed-size static table (MAX_PROCESSES) - no malloc, as per
 * the project's coding convention.
 * ============================================================================*/
#ifndef PROCESS_H
#define PROCESS_H

#include "../include/types.h"

#define MAX_PROCESSES 16
#define STACK_SIZE    4096   /* bytes, per process */

typedef enum {
    PROC_UNUSED,   /* slot never allocated - free for process_create() */
    READY,         /* in the scheduler's ready queue, waiting for the CPU */
    RUNNING,       /* currently executing on the CPU */
    BLOCKED,       /* waiting on a mutex/semaphore (used from Stage 2 on) */
    TERMINATED     /* process_exit() was called; slot may be reused */
} proc_state_t;

typedef struct pcb {
    uint32_t      pid;
    proc_state_t  state;
    uint32_t      esp;              /* saved stack pointer (context switch) */
    uint32_t      eip;              /* entry point - kept for 'ps' display */
    uint32_t      stack[STACK_SIZE / 4];
    struct pcb   *next;             /* singly-linked ready queue */
} pcb_t;

/* The PCB table and the currently-running process are shared between
 * process.c (allocation/bookkeeping) and scheduler.c (the ready queue and
 * the timer-driven round-robin logic). */
extern pcb_t  pcb_table[MAX_PROCESSES];
extern pcb_t *current_process;

/* --- process.c --------------------------------------------------------- */
void   process_init(void);
pcb_t *process_create(void (*entry)(void));
void   process_list(void);          /* implements the 'ps' shell command */

/* Generic singly-linked PCB queue, shared by the scheduler's ready queue
 * (scheduler.c) and the wait queues of mutex_t / semaphore_t (L10) - a
 * blocked process is never in more than one such queue at a time, so it is
 * safe for all of them to reuse the same pcb->next field. */
void   pcb_queue_add(pcb_t **head, pcb_t **tail, pcb_t *p);
pcb_t *pcb_queue_remove(pcb_t **head, pcb_t **tail);

/* --- scheduler.c --------------------------------------------------------
 * scheduler_tick() is called from the IRQ0 assembly stub (switch.asm) on
 * every timer tick; it returns the ESP of the process that should run next.
 * ------------------------------------------------------------------------*/
void     scheduler_init(void);
void     scheduler_add(pcb_t *p);
uint32_t scheduler_tick(uint32_t current_esp);
void     process_yield(void);       /* voluntarily give up the rest of the quantum */
void     process_exit(void);        /* terminate the calling process */

#endif /* PROCESS_H */
