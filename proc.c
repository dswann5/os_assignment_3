// Daniel Swann and Jerald Liu
// Operating Systems Fall 2015
// Assignment 3: Sleeping and Priority modifications

#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "x86.h"
#include "proc.h"
#include "spinlock.h"
#include "channel.h"

struct {
  struct spinlock lock;
  struct proc proc[NPROC];
} ptable;

// Priority table of proc pointers to ptable elements 
struct proc *priority_table[NPRIORITIES][NPROC];
// Initialize number of processes of each priority to 0
int priority_counter[NPRIORITIES] = {0};
// Initialize the index to keep track of circular priority queues
int priority_index[NPRIORITIES] = {0};
// Beginning of the channel list
struct channel * head_chan = 0;

static struct proc *initproc;

int nextpid = 1;
extern void forkret(void);
extern void trapret(void);

// forward declarations of custom functions
int removeProcFromSleepTable(struct channel *, struct proc *);
int removeProcFromPriorityTable(struct proc *);
int switchPriority(int);

static void wakeup1(void *chan);

// Power function
int pow(int, int);

void
pinit(void)
{
  initlock(&ptable.lock, "ptable");
}

//PAGEBREAK: 32
// Look in the process table for an UNUSED proc.
// If found, change state to EMBRYO and initialize
// state required to run in the kernel.
// Otherwise return 0.
static struct proc*
allocproc(void)
{
  struct proc *p;
  char *sp;

  acquire(&ptable.lock);
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++) {
    if(p->state == UNUSED)
      goto found;
  }
  release(&ptable.lock);
  return 0;

found:
  p->state = EMBRYO;
  p->pid = nextpid++;
  release(&ptable.lock);

  // Allocate kernel stack.
  if((p->kstack = kalloc()) == 0){
    p->state = UNUSED;
    return 0;
  }
  sp = p->kstack + KSTACKSIZE;
  
  // Leave room for trap frame.
  sp -= sizeof *p->tf;
  p->tf = (struct trapframe*)sp;
  
  // Set up new context to start executing at forkret,
  // which returns to trapret.
  sp -= 4;
  *(uint*)sp = (uint)trapret;

  sp -= sizeof *p->context;
  p->context = (struct context*)sp;
  memset(p->context, 0, sizeof *p->context);
  p->context->eip = (uint)forkret;

  return p;
}

//PAGEBREAK: 32
// Set up first user process.
void
userinit(void)
{
  struct proc *p;
  extern char _binary_initcode_start[], _binary_initcode_size[];
  
  p = allocproc();
  initproc = p;
  if((p->pgdir = setupkvm()) == 0)
    panic("userinit: out of memory?");
  inituvm(p->pgdir, _binary_initcode_start, (int)_binary_initcode_size);
  p->sz = PGSIZE;
  memset(p->tf, 0, sizeof(*p->tf));
  p->tf->cs = (SEG_UCODE << 3) | DPL_USER;
  p->tf->ds = (SEG_UDATA << 3) | DPL_USER;
  p->tf->es = p->tf->ds;
  p->tf->ss = p->tf->ds;
  p->tf->eflags = FL_IF;
  p->tf->esp = PGSIZE;
  p->tf->eip = 0;  // beginning of initcode.S

  safestrcpy(p->name, "initcode", sizeof(p->name));
  p->cwd = namei("/");

  // Set priority and update priority table for initproc
  p->priority = DEFAULT_PRIORITY;
  int next_position = priority_counter[DEFAULT_PRIORITY];
  priority_table[DEFAULT_PRIORITY][next_position] = p;
  priority_counter[DEFAULT_PRIORITY]++;
  p->state = RUNNABLE;
}

// Grow current process's memory by n bytes.
// Return 0 on success, -1 on failure.
int
growproc(int n)
{
  uint sz;
  
  sz = proc->sz;
  if(n > 0){
    if((sz = allocuvm(proc->pgdir, sz, sz + n)) == 0)
      return -1;
  } else if(n < 0){
    if((sz = deallocuvm(proc->pgdir, sz, sz + n)) == 0)
      return -1;
  }
  proc->sz = sz;
  switchuvm(proc);
  return 0;
}

// Create a new process copying p as the parent.
// Sets up stack to return as if from system call.
// Caller must set state of returned proc to RUNNABLE.
int
fork(void)
{
  int i, pid;
  struct proc *np;

  // Allocate process.
  if((np = allocproc()) == 0)
    return -1;

  // Copy process state from p.
  if((np->pgdir = copyuvm(proc->pgdir, proc->sz)) == 0){
    kfree(np->kstack);
    np->kstack = 0;
    np->state = UNUSED;
    return -1;
  }
  np->sz = proc->sz;
  np->parent = proc;
  *np->tf = *proc->tf;


  // Clear %eax so that fork returns 0 in the child.
  np->tf->eax = 0;

  for(i = 0; i < NOFILE; i++)
    if(proc->ofile[i])
      np->ofile[i] = filedup(proc->ofile[i]);
  np->cwd = idup(proc->cwd);

  safestrcpy(np->name, proc->name, sizeof(proc->name));
 
  pid = np->pid;

  // Set priority to 0 in new process and update priority table
  np->priority = DEFAULT_PRIORITY;
  int temp_count = priority_counter[DEFAULT_PRIORITY];
  priority_table[DEFAULT_PRIORITY][temp_count] = np;
  priority_counter[DEFAULT_PRIORITY]++; 
 
  // lock to force the compiler to emit the np->state write last.
  acquire(&ptable.lock);
  np->state = RUNNABLE;
  release(&ptable.lock);
  
  return pid;
}

// Exit the current process.  Does not return.
// An exited process remains in the zombie state
// until its parent calls wait() to find out it exited.
void
exit(void)
{
  struct proc *p;
  int fd;

  if(proc == initproc)
    panic("init exiting");

  // Close all open files.
  for(fd = 0; fd < NOFILE; fd++){
    if(proc->ofile[fd]){
      fileclose(proc->ofile[fd]);
      proc->ofile[fd] = 0;
    }
  }

  begin_op();
  iput(proc->cwd);
  end_op();
  proc->cwd = 0;

  acquire(&ptable.lock);

  // Parent might be sleeping in wait().
  wakeup1(proc->parent);

  // Pass abandoned children to init.
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->parent == proc){
      p->parent = initproc;
      if(p->state == ZOMBIE)
        wakeup1(initproc);
    }
  }

  // Jump into the scheduler, never to return.
  proc->state = ZOMBIE;
  sched();
  panic("zombie exit");
}

// Wait for a child process to exit and return its pid.
// Return -1 if this process has no children.
int
wait(void)
{
  struct proc *p;
  int havekids, pid;

  acquire(&ptable.lock);
  for(;;){
    // Scan through table looking for zombie children.
    havekids = 0;
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
      if(p->parent != proc)
        continue;
      havekids = 1;
      if(p->state == ZOMBIE){
        // Found one.
        pid = p->pid;
        kfree(p->kstack);
        p->kstack = 0;
        freevm(p->pgdir);
        p->state = UNUSED;
        p->pid = 0;
        p->parent = 0;
        p->name[0] = 0;
        p->killed = 0;
        
        // Remove zombie process from priority_table
        if (removeProcFromPriorityTable(p) < 0)
            panic("priority table fail");
        
        // Set default priority for this process, just in case
        p->priority = DEFAULT_PRIORITY;

        release(&ptable.lock);
        return pid;
      }
    }

    // No point waiting if we don't have any children.
    if(!havekids || proc->killed){
      release(&ptable.lock);
      return -1;
    }

    // Wait for children to exit.  (See wakeup1 call in proc_exit.)
    sleep(proc, &ptable.lock);  //DOC: wait-sleep
  }
}

//PAGEBREAK: 42
// Per-CPU process scheduler.
// Each CPU calls scheduler() after setting itself up.
// Scheduler never returns.  It loops, doing:
//  - choose a process to run
//  - swtch to start running that process
//  - eventually that process transfers control
//      via swtch back to the scheduler.
void
scheduler(void)
{
  int top_prior_ran = 1;
  int num = top_prior_ran;
  int curr_priority = HIGHEST_PRIORITY;
  struct proc *p;

  for(;;){
    // Enable interrupts on this processor.
    sti();
    // Loop over process table looking for process to run.
    acquire(&ptable.lock);

    // Find size of this priority array
    int num_procs = priority_counter[curr_priority];

    if (num_procs >= 1) {
      // Look through priority table
      // If our index is out of range (i.e., >= num_procs in our priority row), reset to 0
      if (priority_index[curr_priority] >= num_procs)
        priority_index[curr_priority] = 0;

      int i, index;
      // Loop through all elements of array starting at index
      for (i=0; i<num_procs; i++) {
        index = (priority_index[curr_priority] + i) % num_procs;
        p = priority_table[curr_priority][index];
        if (p->state != RUNNABLE)
          continue;
        priority_index[curr_priority] = index + 1;
        
        // Switch to chosen process.  It is the process's job
        // to release ptable.lock and then reacquire it
        // before jumping back to us.
        proc = p;
        switchuvm(p);
        p->state = RUNNING;
        
        swtch(&cpu->scheduler, proc->context);
        switchkvm();
        // Process is done running for now.
        // It should have changed its p->state before coming back.
        proc = 0;
        // break to avoid running more RUNNABLE processes
        break; 
      }       
    }
    release(&ptable.lock);

    // Compute next priority   
    if (num%2==0) {
      num/=2;
      curr_priority++;
    } else {
      curr_priority = HIGHEST_PRIORITY;
      top_prior_ran++;
      if (top_prior_ran > pow(2,NPRIORITIES-1)) {
        top_prior_ran = 1;
      }
      num = top_prior_ran;
    }
  }
}

// Enter scheduler.  Must hold only ptable.lock
// and have changed proc->state.
void
sched(void)
{
  int intena;

  if(!holding(&ptable.lock))
    panic("sched ptable.lock");
  if(cpu->ncli != 1)
    panic("sched locks");
  if(proc->state == RUNNING)
    panic("sched running");
  if(readeflags()&FL_IF)
    panic("sched interruptible");
  intena = cpu->intena;
  swtch(&proc->context, cpu->scheduler);
  cpu->intena = intena;
}

// Give up the CPU for one scheduling round.
void
yield(void)
{
  acquire(&ptable.lock);  //DOC: yieldlock
  proc->state = RUNNABLE;
  sched();
  release(&ptable.lock);
}

// A fork child's very first scheduling by scheduler()
// will swtch here.  "Return" to user space.
void
forkret(void)
{
  static int first = 1;
  // Still holding ptable.lock from scheduler.
  release(&ptable.lock);

  if (first) {
    // Some initialization functions must be run in the context
    // of a regular process (e.g., they call sleep), and thus cannot 
    // be run from main().
    first = 0;
    iinit(ROOTDEV);
    initlog(ROOTDEV);
  }
  
  // Return to "caller", actually trapret (see allocproc).
}

// Atomically release lock and sleep on chan.
// Reacquires lock when awakened.
void
sleep(void *chan, struct spinlock *lk)
{
  if(proc == 0)
    panic("sleep");

  if(lk == 0)
    panic("sleep without lk");

  // Must acquire ptable.lock in order to
  // change p->state and then call sched.
  // Once we hold ptable.lock, we can be
  // guaranteed that we won't miss any wakeup
  // (wakeup runs with ptable.lock locked),
  // so it's okay to release lk.
  if(lk != &ptable.lock){  //DOC: sleeplock0
    acquire(&ptable.lock);  //DOC: sleeplock1
    release(lk);
  }

  // Go to sleep.
  proc->chan = chan;
  proc->state = SLEEPING;

  struct channel * new_chan;
  // Check for existing channel, or adding it to the front of the chanel list 
  // Also adds this proc to appropriate channel.
  if (head_chan == 0) {
    if ((new_chan = (struct channel *) kalloc()) == 0) {
        panic("failed kalloc to channel");
    }

    // Add first channel to first in list
    new_chan->chan = chan;
    new_chan->num_sleeping = 0;
    new_chan->sleeptable[new_chan->num_sleeping++] = proc;
    new_chan->next_chan = 0;
    head_chan = new_chan;
  } else { // Check for channel existence by iterating through list
    int exists = 0;
    struct channel * curr = head_chan;
    while (curr->next_chan != 0) {
      if (curr->chan == chan) {
        // Add proc to this chan
        curr->sleeptable[curr->num_sleeping++] = proc;
        exists = 1;
        break;
      }
      curr = curr->next_chan;
    }

    // Channel does not exist
    if (!exists) {
      // Add channel to front of linkedlist
      if ((new_chan = (struct channel *) kalloc()) == 0) {
          panic("failed kalloc to channel");
      }
      new_chan->chan = chan;
      new_chan->num_sleeping = 0;
      new_chan->sleeptable[new_chan->num_sleeping++] = proc;
      new_chan->next_chan = head_chan;
      head_chan = new_chan;
    }
  }
  sched();
  // Remove proc from sleep table if it's there
  removeProcFromSleepTable(head_chan, proc);

  // Tidy up.
  proc->chan = 0;
  
  // Reacquire original lock.
  if(lk != &ptable.lock){  //DOC: sleeplock2
    release(&ptable.lock);
    acquire(lk);
  }
}

//PAGEBREAK!
// Wake up all processes sleeping on chan.
// The ptable lock must be held.
static void
wakeup1(void *chan)
{
  struct channel *cur_chan = head_chan;
  struct channel *prev_chan = 0;

  // Find all procs in this channel by iterating through channel list
  while (cur_chan != 0) 
  {
    if (cur_chan->chan == chan) 
    {
      int i;
      // Wake up the processes on this channel
      // We know they're already SLEEPING
      for (i=0;i<cur_chan->num_sleeping;i++) {
          cur_chan->sleeptable[i]->state = RUNNABLE;
      }
      // Remove channel from memory, with case for removing head
      if (cur_chan == head_chan) {
        head_chan = cur_chan->next_chan;
      }
      else {
        prev_chan->next_chan = cur_chan->next_chan;
      }
      kfree((char *)cur_chan);
      break;
    }
    prev_chan = cur_chan;
    cur_chan=cur_chan->next_chan;
  } 
}

// Wake up all processes sleeping on chan.
void
wakeup(void *chan)
{
  acquire(&ptable.lock);
  wakeup1(chan);
  release(&ptable.lock);
}

// Kill the process with the given pid.
// Process won't exit until it returns
// to user space (see trap in trap.c).
int
kill(int pid)
{
  struct proc *p;
  int ret = -1;
  //struct channel *cur_chan = head_chan;

  acquire(&ptable.lock);
  // First find channel
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->pid == pid){
      p->killed = 1;
      // Wake process from sleep if necessary.
      if(p->state == SLEEPING) 
        p->state = RUNNABLE;
      ret = 0;
      break;
    }
  }
  if (ret == -1) {
    release(&ptable.lock);
    return -1;
  }
  // Remove proc from sleep table if it's there
  removeProcFromSleepTable(head_chan, p);
  
  release(&ptable.lock);
  return ret;
}

//PAGEBREAK: 36
// Print a process listing to console.  For debugging.
// Runs when user types ^P on console.
// No lock to avoid wedging a stuck machine further.
void
procdump(void)
{
  static char *states[] = {
  [UNUSED]    "unused",
  [EMBRYO]    "embryo",
  [SLEEPING]  "sleep ",
  [RUNNABLE]  "runble",
  [RUNNING]   "run   ",
  [ZOMBIE]    "zombie"
  };
  int i;
  struct proc *p;
  char *state;
  uint pc[10];
  
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->state == UNUSED)
      continue;
    if(p->state >= 0 && p->state < NELEM(states) && states[p->state])
      state = states[p->state];
    else
      state = "???";
    cprintf("%d %s %s", p->pid, state, p->name);
    if(p->state == SLEEPING){
      getcallerpcs((uint*)p->context->ebp+2, pc);
      for(i=0; i<10 && pc[i] != 0; i++)
        cprintf(" %p", pc[i]);
    }
    cprintf("\n");
  }
}

// Changes the priority of the current process by increment amount
// Checks to make sure increment is within bounds, otherwise defaults to max or min
int change_priority(int increment)
{
    acquire(&ptable.lock);
    // Calculate actual increment value
    int new_priority = proc->priority;
    new_priority = new_priority + increment;
    if (new_priority < HIGHEST_PRIORITY) {
       new_priority = HIGHEST_PRIORITY;
    }
    if (new_priority > LOWEST_PRIORITY) {
       new_priority = LOWEST_PRIORITY;
    }
    if (new_priority == proc->priority) {
       // Same priority, do nothing
        release(&ptable.lock);
        return 0;
    }
    // Increment this proc by moving it to the proper priority slot in the ptable
    // store return value in temp ret because we still need to release the ptable lock
    int ret = switchPriority(new_priority);

    release(&ptable.lock);
    return ret;
}

// Remove the given process from the sleeptable, if it exists
// Returns -1 on failure, 0 on success
int removeProcFromSleepTable(struct channel * head, struct proc * p) {
  struct channel * cur_chan = head;
  // Find process in channel data structure, delete it
  while (cur_chan != 0) 
  {
    if (cur_chan->chan == p->chan) 
    {
      int i;
      int found = 0;
      for (i=0;i<cur_chan->num_sleeping;i++) {
        // Shift element to right of process one index to the left
        if (found) {
          cur_chan->sleeptable[i-1] = cur_chan->sleeptable[i];
        } else if (cur_chan->sleeptable[i]->pid == p->pid) {
          // We found our process
          found = 1;
        }
      }
      if (found) {
        cur_chan->num_sleeping--;
        return 0;
      }
    }
    cur_chan=cur_chan->next_chan;
  }
  return -1;
}

// Remove the given process from the priority table, if it exists
// Returns -1 on failure, 0 on success
int removeProcFromPriorityTable(struct proc * p) {
  int i;
  int prior = p->priority;
  int found = 0;
  for (i=0;i<priority_counter[prior];i++) {
    if (found) {
      priority_table[prior][i-1] = priority_table[prior][i];
    } else if (priority_table[prior][i] == p) {
      found = 1;
    }
  }
  if (found) {
    // Decrease size of this priority array
    priority_counter[prior]--;
    return 0;
  }
  return -1;
}

// Removes proc pointer from one priority queue and moves it to another 
// Returns -1 on failure and 0 on success
int switchPriority(int new_priority) {
  int i;
  int found = 0;
  struct proc *temp_proc, *p;
  
  // Search for this process in its proper priority queue
  for (i=0;i<priority_counter[proc->priority];i++) {
    p = priority_table[proc->priority][i];
    if (found) {
      // Shift subsequent processes one index lower in the priority
      priority_table[proc->priority][i-1] = priority_table[proc->priority][i];
    } else if (p == proc) {
      found = 1;
      temp_proc = p;
    }
  }
  if (!found) {
    return -1;
  }
  // Decrease number of procs in this priority queue
  priority_counter[proc->priority]--;

  // Place p at the end of the new priority row
  int temp_count = priority_counter[new_priority];
  priority_table[new_priority][temp_count] = temp_proc;
  temp_proc->priority = new_priority;
  priority_counter[new_priority]++;
  return 0;
}

// Returns an int, base^exponent for priority testing
// Only takes positive (and zero) exponents
int pow(int base, int exponent) {
    int sum = base;
    if (exponent == 0)
	    return 1;
    int i;
    for (i=1;i<exponent;i++) {
	    sum*=base;
    }
    return sum;
}
