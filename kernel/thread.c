/* =============================================================================
 * SENG21213-OS :: Kernel Threads
 * File   : kernel/thread.c
 *
 * process_create() only knows how to start a no-argument function, so
 * thread_create() gives every new thread the same tiny trampoline as its
 * entry point. The trampoline looks up the (fn, arg) pair that belongs to
 * "whichever PCB I am" via the scheduler's current_process pointer - which
 * is already correct by the time the trampoline's first instruction runs,
 * since the scheduler sets it before switching to this thread - then calls
 * fn(arg) and terminates the thread when it returns.
 * ============================================================================*/
#include "thread.h"

typedef struct {
    bool        used;
    uint32_t    pid;
    thread_fn_t fn;
    void       *arg;
} thread_args_t;

static thread_args_t thread_args[MAX_PROCESSES];

static void thread_trampoline(void) {
    thread_fn_t fn  = NULL;
    void       *arg = NULL;

    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (thread_args[i].used && thread_args[i].pid == current_process->pid) {
            fn  = thread_args[i].fn;
            arg = thread_args[i].arg;
            thread_args[i].used = false;
            break;
        }
    }

    if (fn) fn(arg);
    process_exit();
}

pcb_t *thread_create(thread_fn_t fn, void *arg) {
    pcb_t *p = process_create(thread_trampoline);
    if (!p) return NULL;

    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (!thread_args[i].used) {
            thread_args[i].used = true;
            thread_args[i].pid  = p->pid;
            thread_args[i].fn   = fn;
            thread_args[i].arg  = arg;
            break;
        }
    }

    scheduler_add(p);
    return p;
}
