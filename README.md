# Modified xv6-riscv

## Installation

Download the folder and in the directory, run the following command:
```sh
make qemu
```
In order to use specialised scheduler options, PBS or FCFS use the following command:
```sh
make qemu SCHEDULER=$OPTION
```

### Specification 1

- Added a function `sys_trace` in `sysproc.c` which assigns the mask
```c
uint64
sys_trace()
{
    int mask;
    if(argint(0, &mask) < 0)
        return -1;
    myproc()->mask = mask;
    return 0;
}
```

- Added a new file `strace.c` to accept user arguments for the implementation of strace. Aso added it in the `Makefile`

```c
#include "kernel/param.h"
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int main(int argc, char *argv[]) {
    int i;
    char *subargv[MAXARG];

    if(argc < 3 || (argv[1][0] < '0' || argv[1][0] > '9')){
        fprintf(2, "Error: %s mask command\n", argv[0]);
        exit(1);
    }

    if (trace(atoi(argv[1])) < 0) {
        fprintf(2, "%s: trace failed\n", argv[0]);
        exit(1);
    }
    
    for(i = 2; i < argc && i < MAXARG; i++){
    	subargv[i-2] = argv[i];
    }
    exec(subargv[0], subargv);
    exit(1);
}
```

- Added mask as a variable in `proc.h` and copied the mask from parent to child in the `fork()` function in `proc.c`
- Arrays `syscall_names` and `syscallnum` was made.initialised
- The `syscall` in `syscall.c` is modified to print the values of the strace


### Specification 2

#### **FCFS**

- In order to check if any `SCHEDULER` is specified, or defaulting to `RR` otherwise. The following code is added to the makefile:

```makefile
ifndef SCHEDULER
    SCHEDULER:=RR
endif

CFLAGS+="-D$(SCHEDULER)"
```

- `yield()` is disabled in kernel and user traps in `trap.c` as it is a non-preemptive scheduler.
- Added the FCFS functionality in `scheduler` in `proc.c`. The first loop finds the earliest created process and the following code runs it:

    ```c
    #ifdef FCFS
    struct proc *firstComeProc = 0;
    for (p = proc; p < &proc[NPROC]; p++)
    {
      acquire(&p->lock);
      if (p->state == RUNNABLE)
      {
        if (!firstComeProc || firstComeProc->ctime > p->ctime)
        {
          if (firstComeProc)
            release(&firstComeProc->lock);
          firstComeProc = p;
          continue;
        }
      }
      release(&p->lock);
    }
    // As long as firstproc contains a process
    if (firstComeProc)
    {
      firstComeProc->num_runs++;
      firstComeProc->state = RUNNING;
      c->proc = firstComeProc;
      swtch(&c->context, &firstComeProc->context); //context switching
      c->proc = 0;
      release(&firstComeProc->lock);
    }
    #else
    ```

#### **PBS**

- Several variables for utility is added to `struct proc` in `proc.h` and initialised in `allocproc()` in `proc.c`
- The scheduling functionality for PBS  is added in `scheduler()` in `proc.c`
- Niceness is calculated using the following algorithm:
```
niceness = Int(10 * stime / (stime + rtime))
```
- dp is calculated using the following algorithm:
```md
DP = max(0, min(SP − niceness + 5, 100))
```
The priority with the lowest priority number is run first.

```c
#ifdef PBS
struct proc *priorityProc = 0;
int dp = 120;
for (p = proc; p < &proc[NPROC]; p++)
{
    acquire(&p->lock);
    int niceness = 5;

    if(p->num_runs > 0 && p->sleeptime + p->rtime != 0)
    {
    niceness = (int)((p->sleeptime * 10) / (p->sleeptime + p->rtime));
    }
    int proc_dp = p->priority - niceness + 5 < 100 ? (p->priority - niceness + 5) : 100;
    proc_dp = proc_dp < 100 ? proc_dp: 100;
    proc_dp = 0 > proc_dp ? 0:proc_dp;

    if (p->state == RUNNABLE)
    {
    if (!priorityProc || proc_dp < dp || 
    (dp==proc_dp && 
    (p->num_runs < priorityProc->num_runs || (p->num_runs == priorityProc->num_runs && p->ctime < priorityProc->ctime )))) 
    {
        if(priorityProc)
            release(&priorityProc->lock);

        dp = proc_dp;
        priorityProc = p;
        continue;
    }
    }
    release(&p->lock);
}
if (priorityProc)
{
    priorityProc->num_runs++;
    priorityProc->starttime = ticks;
    priorityProc->state = RUNNING;
    priorityProc->rtime = 0;
    priorityProc->sleeptime = 0;

    c->proc = priorityProc;
    swtch(&c->context, &priorityProc->context);
    c->proc = 0;
    release(&priorityProc->lock);
}

#else
```

- System call `set_priority` is added to modify the static priority using nearly the same method as strace in spec 1

`set_priority.c`

```c
#include "kernel/param.h"
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
  int priority, pid;
  if(argc < 3){
    fprintf(2,"Invalid number of arguments\n");
    exit(1);
  }

  priority = atoi(argv[1]);
  pid = atoi(argv[2]);

  if (priority > 100 || priority <> 0){
    fprintf(2,"Entered priority is invalide(0-100)!\n");
    exit(1);
  }
  set_priority(priority, pid);
  exit(1);
}
```

`sysproc.c`

```c
uint64 sys_set_priority()
{
    int pid, priority;
    if(argint(0, &priority) < 0)
        return -1;
    if(argint(1, &pid) < 0)
        return -1;

    return set_priority(priority, pid);
}
```

#### MLFQ (Sub-question)

- 5 priority queues with different time slices are created.
- An initiated process is pushed to the end of the highest priority process.
- All processes in a higher priority queue needs to be executed before any process at a lower level is ran
- Preemption is carried out if a process overuses its time slice and the said process will be inserted into a lower level queue.
- Processes that relinquish control can use this to their advantage as, instead of using the complete time slice and getting entered into a lower level queue, it can relinquish control right before the time slice is completed and stay at the same priority level. If the process repeatedly does this, it ensures that its priority will never go down and gets extended CPU time despite having large total run times.

### Specification 3

- In the struct proc, added a variable, total run time, `total_rtime` which calculates the total running time of the process in the struct proc
- Calculate the waiting time, `wtime`, from the precalculated variables in the process
- Changed the code in procdump to print the values when it is scheduling for PBS differently.

```c
#ifdef PBS
    int wtime = ticks - p->ctime - p->total_rtime;
    printf("%d %s %s\t%d\t%d\t%d", p->pid, state, p->name, p->total_rtime, wtime, p->num_runs);
    printf("\n");
#else
```

### Benchmark

It Ran on 3 CPUs. The performance analysis for the 3 scheduling methods are as follows:
- **RR**: Average rtime 21,  wtime 123
- **FCFS**: Average rtime 52,  wtime 62
- **PBS**: Average rtime 24,  wtime 108

----
