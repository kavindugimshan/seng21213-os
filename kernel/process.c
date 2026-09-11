/* =============================================================================
 * SENG21213-OS :: Process Table
 * File   : kernel/process.c
 * L09 §PCB - allocation and bookkeeping for the fixed-size process table.
 * Scheduling (the ready queue and the timer tick handler) lives in
 * scheduler.c; this file only owns the table itself and process_create().
 * ============================================================================*/
#include "process.h"
#include "vga.h"

pcb_t  pcb_table[MAX_PROCESSES];
pcb_t *current_process = NULL;

static uint32_t next_pid = 0;

void process_init(void) {
    for (int i = 0; i < MAX_PROCESSES; i++) {
        pcb_table[i].pid   = 0;
        pcb_table[i].state = PROC_UNUSED;
        pcb_table[i].esp   = 0;
        pcb_table[i].eip   = 0;
        pcb_table[i].next  = NULL;
    }
    next_pid = 0;
}

/* Builds a fake interrupt frame at the top of the new process's stack so
 * that the very first time the scheduler switches to it, "popad; iret" in
 * switch.asm resumes execution at `entry` exactly as if it had already been
 * preempted once before. This is the standard trick used to start a process
 * without special-casing "first run" in the scheduler (L09 §context switch).
 *
 * Fabricated stack layout, low address (= esp) to high address:
 *   EDI, ESI, EBP, ESP(dummy), EBX, EDX, ECX, EAX,   <- popad reads these
 *   EIP, CS, EFLAGS                                  <- iret reads these
 */
pcb_t *process_create(void (*entry)(void)) {
    pcb_t *p = NULL;
    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (pcb_table[i].state == PROC_UNUSED || pcb_table[i].state == TERMINATED) {
            p = &pcb_table[i];
            break;
        }
    }
    if (!p) return NULL;   /* process table full */

    p->pid   = ++next_pid;
    p->state = READY;
    p->eip   = (uint32_t)entry;
    p->next  = NULL;

    uint32_t *sp = (uint32_t *)((uint8_t *)p->stack + STACK_SIZE);
    *(--sp) = 0x00000202;        /* EFLAGS: IF=1 (interrupts enabled), reserved bit 1 set */
    *(--sp) = 0x08;              /* CS: kernel code segment (see boot/boot.asm GDT)      */
    *(--sp) = (uint32_t)entry;   /* EIP: where the process starts executing              */
    *(--sp) = 0;                 /* EAX */
    *(--sp) = 0;                 /* ECX */
    *(--sp) = 0;                 /* EDX */
    *(--sp) = 0;                 /* EBX */
    *(--sp) = 0;                 /* ESP (dummy - discarded by popad) */
    *(--sp) = 0;                 /* EBP */
    *(--sp) = 0;                 /* ESI */
    *(--sp) = 0;                 /* EDI */

    p->esp = (uint32_t)sp;
    return p;
}

void pcb_queue_add(pcb_t **head, pcb_t **tail, pcb_t *p) {
    p->next = NULL;
    if (!*head) {
        *head = *tail = p;
    } else {
        (*tail)->next = p;
        *tail = p;
    }
}

pcb_t *pcb_queue_remove(pcb_t **head, pcb_t **tail) {
    if (!*head) return NULL;
    pcb_t *p = *head;
    *head = p->next;
    if (!*head) *tail = NULL;
    p->next = NULL;
    return p;
}

static const char *state_name(proc_state_t s) {
    switch (s) {
        case READY:      return "READY";
        case RUNNING:    return "RUNNING";
        case BLOCKED:    return "BLOCKED";
        case TERMINATED: return "TERMINATED";
        default:         return "UNUSED";
    }
}

void process_list(void) {
    vga_puts_color("\n  PID  STATE       ESP\n", VGA_YELLOW, VGA_BLACK);
    vga_puts("  ---  ----------  ----------\n");
    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (pcb_table[i].state == PROC_UNUSED) continue;
        vga_printf("  %d    %s", pcb_table[i].pid, state_name(pcb_table[i].state));
        vga_puts("      0x");
        vga_printf("%x\n", pcb_table[i].esp);
    }
    vga_puts("\n");
}
