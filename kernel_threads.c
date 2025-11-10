
#include "tinyos.h"
#include "kernel_sched.h"
#include "kernel_proc.h"
#include "util.h"
#include "kernel_cc.h"
#include "kernel_streams.h"
#include "kernel_thread.h"

/** 
  @brief Create a new thread in the current process.
  */
Tid_t sys_CreateThread(Task task, int argl, void* args)
{
	PCB* curproc = CURPROC;
  PTCB* ptcb = initialize_ptcb();
  ptcb->task = task;
  ptcb->argl = argl;
  ptcb->args = args;

  ptcb->tcb = spawn_thread(curproc, ptcb, start_thread);
  Tid_t tid = (Tid_t)(ptcb->tcb->ptcb);

  rlist_push_front(&curproc->ptcb_list, &ptcb->ptcb_list_node);
  curproc->thread_count++;

  wakeup(ptcb->tcb);

  return tid;
}

/**
  @brief Return the Tid of the current thread.
 */
Tid_t sys_ThreadSelf()
{
  TCB* tcb = cur_thread();
  Tid_t tid = (Tid_t)tcb->ptcb;
  return tid;
}

/**
  @brief Join the given thread.
  */
int sys_ThreadJoin(Tid_t tid, int* exitval)
{

if(tid == NOTHREAD || tid == sys_ThreadSelf())  //Error cases: A thread tries to join a thread with invalid ID or tries to join itself.
    return -1; 

  PCB* curproc = CURPROC;
  PTCB *ptcb = (PTCB*)tid;

  rlnode* node = rlist_find(&curproc->ptcb_list, ptcb, NULL); //We look in the processes ptcb list for the thread we try to join. 
  if(node == NULL )  //If its not found we return NULL and return with -1.                                        
    return -1;
  
  //If we got here it means that we are about to join the given thread.
    
  node->ptcb->refcount++;
  while(node->ptcb->exited==0 && node->ptcb->detached==0){    //We join if its not exited and detached
    kernel_wait(&node->ptcb->exit_cv, SCHED_USER); //We will get back here only when the thread we joined calls kernel_broadcast(if it becomes detached or if it exis.)
  }
  node->ptcb->refcount--;
  
  if(node->ptcb->detached==1) //if we came back because the thread we joined became detached then return -1
    return -1;

  //If we get here the thread is exited.

  if(exitval!=NULL) //if exitval==NULL means that the joiner thread didn't want to save somewhere the exit value of the joined thread.
    (*exitval)=node->ptcb->exitval;

  if(node->ptcb->refcount==0){                //In case that many threads (T1,T2) joined a thread (T3) then when T3 broadcasts T1 and T2 they will not run in parallel
    rlist_remove(&node->ptcb->ptcb_list_node);//They will be scheduled by our scheduler so one of them will get here first (T1) and it would remove the ptcb                                             //so the other thread (T2) will not be able to find the ptcb and will lead to a segmentation fault.
    free(node->ptcb);                         //so the other thread T2 will not be able to find ptcb and lead to a segmentation fault.
    return 0;                                 //Thats why we make sure ONLY the last thread that comes here after the kernel_broadcast will clean the ptcb.
  }
return 0;

}

/**
  @brief Detach the given thread.
  */
int sys_ThreadDetach(Tid_t tid)
{
  if(tid == NOTHREAD)
    return -1;
  PCB* curproc = CURPROC;
  PTCB *ptcb = (PTCB*)tid;
  rlnode* node = rlist_find(&curproc->ptcb_list, ptcb, NULL);
  if(node==NULL)
    return -1;
  if(node->ptcb->exited==1){
    return -1;
  }
  else
  {
    node->ptcb->detached = 1;
    kernel_broadcast(&node->ptcb->exit_cv); //We are now detached so we wake up any thread that is waiting for us because there's no reason to wait anymore
    return 0;
  }
  
}

void sys_ThreadExit(int exitval)
{
    PCB* curproc = CURPROC;
    TCB* curthread = cur_thread();

    /* ---------------- CASE 1: Last thread of the process ---------------- */
    if (curproc->thread_count == 1) {

        /* Clean up all PTCBs except the current one */
        rlnode* head = &curproc->ptcb_list;
        rlnode* node = head->next;
        while (node != head) {
            rlnode* next = node->next;
            PTCB* pt = node->ptcb;
            if (pt != curthread->ptcb) {
                rlist_remove(node);
                free(pt);
            }
            node = next;
        }

        /* Reparent any children to init process */
        PCB* initpcb = get_pcb(1);
        while (!is_rlist_empty(&curproc->children_list)) {
            rlnode* child = rlist_pop_front(&curproc->children_list);
            child->pcb->parent = initpcb;
            rlist_push_front(&initpcb->children_list, child);
        }

        /* Move exited children to init’s exited list */
        if (!is_rlist_empty(&curproc->exited_list)) {
            rlist_append(&initpcb->exited_list, &curproc->exited_list);
            kernel_broadcast(&initpcb->child_exit);
        }

        /* If not init process, notify parent */
        if (get_pid(curproc) != 1) {
            rlist_push_front(&curproc->parent->exited_list, &curproc->exited_node);
            kernel_broadcast(&curproc->parent->child_exit);
        }

        assert(is_rlist_empty(&curproc->children_list));
        assert(is_rlist_empty(&curproc->exited_list));

        /* Release process arguments */
        if (curproc->args) {
            free(curproc->args);
            curproc->args = NULL;
        }

        /* Close file descriptors */
        for (int i = 0; i < MAX_FILEID; i++) {
            if (curproc->FIDT[i]) {
                FCB_decref(curproc->FIDT[i]);
                curproc->FIDT[i] = NULL;
            }
        }

        /* Mark this thread and process as exited */
        curthread->ptcb->exitval = exitval;
        curthread->ptcb->exited = 1;
        kernel_broadcast(&curthread->ptcb->exit_cv); // consistency
        curproc->thread_count--;

        curproc->main_thread = NULL;
        curproc->pstate = ZOMBIE;

        /* Sleep forever */
        kernel_sleep(EXITED, SCHED_USER);
        return;
    }

    /* ---------------- CASE 2: Normal (not last) thread ---------------- */
    curthread->ptcb->exitval = exitval;
    curthread->ptcb->exited  = 1;
    kernel_broadcast(&curthread->ptcb->exit_cv);  // wake all joiners
    curproc->thread_count--;

    if (curthread->ptcb->detached) {
        /*
         * Detached threads may still have joiners that woke up.
         * Only free now if refcount == 0 (no one holds a pointer).
         * Otherwise, the last joiner will clean it up after refcount--.
         */
        if (curthread->ptcb->refcount == 0) {
            rlist_remove(&curthread->ptcb->ptcb_list_node);
            free(curthread->ptcb);
        }
    }

    /* Sleep permanently — scheduler will clean the TCB. */
    kernel_sleep(EXITED, SCHED_USER);
}


    