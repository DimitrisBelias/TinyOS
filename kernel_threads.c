
#include "tinyos.h"
#include "kernel_sched.h"
#include "kernel_proc.h"

/** 
  @brief Create a new thread in the current process.
  */
Tid_t sys_CreateThread(Task task, int argl, void* args)
{
	
    // Έλεγχος ορθότητας
    if (task == NULL) return 0;

    PCB* pcb = CURPROC;        // η διεργασία που το καλεί
    if (pcb == NULL) return 0;

    // --- Δημιουργία και αρχικοποίηση του PTCB ---
    PTCB* ptcb = malloc(sizeof(PTCB));
    if (!ptcb) return 0;

    memset(ptcb, 0, sizeof(ptcb));
    ptcb->task     = task;
    ptcb->argl     = argl;
    ptcb->args     = args;
    ptcb->exitval  = 0;
    ptcb->exited   = 0;
    ptcb->detached = 0;
    ptcb->refcount = 1;  // 1 αναφορά για τον ίδιο του τον εαυτό

    cond_init(&ptcb->exit_cv);
    rlnode_init(&ptcb->ptcb_list_node, ptcb);

    // --- Δημιουργία TCB (kernel-level thread) ---
    TCB tcb = spawn_thread(pcb, start_process_thread);
    if (!tcb) {
        free(ptcb);
        return 0;
    }

    // --- Σύνδεση PTCB και TCB ---
    ptcb->tcb  = tcb;
    tcb->ptcb  = ptcb;

    // --- Προσθήκη στη λίστα threads του process ---
    rlist_push_back(&pcb->ptcb_list, &ptcb->ptcb_list_node);
    pcb->thread_count++;

    // --- Δημιουργία Tid_t ως pointer στο PTCB ---
    Tid_t tid = (Tid_t)ptcb;

    // --- Ξεκίνα το thread (προσθήκη στο ready queue) ---
    wakeup(tcb);

    return tid;
}

/**
  @brief Return the Tid of the current thread.
 */
Tid_t sys_ThreadSelf()
{
  return (Tid_t)(cur_thread()->ptcb);
	//return (Tid_t) cur_thread();
}

/**
  @brief Join the given thread.
  */
int sys_ThreadJoin(Tid_t tid, int* exitval)
{

    if (tid == 0) return -1;  // άκυρο thread id

    PTCB* target = (PTCB*)tid;  // μετατροπή του tid σε pointer προς PTCB
    if (!target) return -1;

    // Αν είναι το ίδιο thread με το τρέχον -> λάθος (δεν μπορείς να κάνεις join τον εαυτό σου)
    if (target == CURTHREAD->ptcb)
        return -1;

    // Αν είναι detached -> αποτυχία (δεν μπορείς να το κάνεις join)
    if (target->detached)
        return -1;

    // Αν δεν έχει τελειώσει ακόμα, περίμενε
    while (!target->exited)
        cond_wait(&target->exit_cv);

    // Αν υπάρχει pointer για exitval, αποθήκευσε την τιμή εξόδου
    if (exitval)
        *exitval = target->exitval;

    // Μείωσε το refcount — αν φτάσει στο 0, κάνε cleanup
    target->refcount--;
    if (target->refcount == 0)
        free(target); 

    return 0;

}

/**
  @brief Detach the given thread.
  */
int sys_ThreadDetach(Tid_t tid)
{
	return -1;
}

/**
  @brief Terminate the current thread.
  */
void sys_ThreadExit(int exitval)
{

}

