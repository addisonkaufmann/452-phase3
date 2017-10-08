#include <usloss.h>
#include <usyscall.h>
#include <phase1.h>
#include <phase2.h>
#include <phase3.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "sems.h"


/* FUNCTION PROTOTYPES */
int spawnReal();
int waitReal();
int start3();
int spawnLaunch(char * arg);
void initProcTable();
void initSemTable();
void initSyscallVec();
void initProc();
void nullsys3();
void spawn();
void wait();
void terminate();
void gettimeofday();
void getpid3();
void cputime();
void semcreate();
void semp();
void semv();
void semfree();
void check_kernel_mode(char * arg);
int isInKernelMode();
int enterUserMode();

typedef struct launchArgs * launchArgsPtr;
typedef struct launchArgs launchArgs;

struct launchArgs {
    int (*func)(char *);
    char * arg;
    char name[150];
};


/* GLOBAL DATA STRUCTURES */

p3Proc ProcTable[MAXPROC];  //phase 3 proctable
sem SemTable[MAXSEMS];      //semaphore table
int debugflag3 = 1;



int start2(char *arg)
{
    int pid;
    int status;
    /*
     * Check kernel mode here.
     */
    check_kernel_mode("start2");
    if (debugflag3){
        USLOSS_Console("start2(): called in kernel mode\n");
    }

    /*
     * Data structure initialization as needed...
     */

    initProcTable();
    initSemTable();
    initSyscallVec();
    initProc(getpid(), -1);
    

    /*
     * Create first user-level process and wait for it to finish.
     * These are lower-case because they are not system calls;
     * system calls cannot be invoked from kernel mode.
     * Assumes kernel-mode versions of the system calls
     * with lower-case names.  I.e., Spawn is the user-mode function
     * called by the test cases; spawn is the kernel-mode function that
     * is called by the syscallHandler; spawnReal is the function that
     * contains the implementation and is called by spawn.
     *
     * Spawn() is in libuser.c.  It invokes USLOSS_Syscall()
     * The system call handler calls a function named spawn() -- note lower
     * case -- that extracts the arguments from the sysargs pointer, and
     * checks them for possible errors.  This function then calls spawnReal().
     *
     * Here, we only call spawnReal(), since we are already in kernel mode.
     *
     * spawnReal() will create the process by using a call to fork1 to
     * create a process executing the code in spawnLaunch().  spawnReal()
     * and spawnLaunch() then coordinate the completion of the phase 3
     * process table entries needed for the new process.  spawnReal() will
     * return to the original caller of Spawn, while spawnLaunch() will
     * begin executing the function passed to Spawn. spawnLaunch() will
     * need to switch to user-mode before allowing user code to execute.
     * spawnReal() will return to spawn(), which will put the return
     * values back into the sysargs pointer, switch to user-mode, and 
     * return to the user code that called Spawn.
     */
    pid = spawnReal("start3", start3, NULL, USLOSS_MIN_STACK, 3);

    /* Call the waitReal version of your wait code here.
     * You call waitReal (rather than Wait) because start2 is running
     * in kernel (not user) mode.
     */
    pid = waitReal(&status);

    return -1000;
} /* start2 */

int spawnReal(char *name, int (*func)(char *), char *arg, long stack_size, long priority){
    if (debugflag3){
        USLOSS_Console("spawnReal(): called to spawn %s\n", name);
    }



    //call fork1 to spawnLaunch


    int pid = fork1(name, spawnLaunch, arg, (int)stack_size, (int)priority);

    if (debugflag3){
        USLOSS_Console("spawnReal(): after fork1, pid = %d\n", pid);
    }

    if (pid < 0){
        fprintf(stderr, "pid < 0. Terminating\n");
        USLOSS_Halt(1); //FIXME: terminate instead of halt
    }

    initProc(pid, getpid());

    return pid;
}

int spawnLaunch(char * arg){
    if (debugflag3){
        USLOSS_Console("spawnLaunch(): called\n");
    }

    //wait for spawnReal to finish creating pte
    //switch to user mode before executing
    //execute func()
    return -1000;
}

int waitReal(int * status){
    if (debugflag3){
        USLOSS_Console("waitReal(): called\n");
    }
    int i = getpid() % MAXPROC;
    int mboxid = ProcTable[i].privateMboxId;
    int result = MboxReceive(mboxid, NULL, 0);
    if (result < 0){
        fprintf(stderr, "waitReal(): mbox receive result < 0, terminate.\n");
        USLOSS_Halt(1); //FIXME: terminate instead of  halt.
    }
    if (debugflag3){
        USLOSS_Console("waitReal(): after mbox receive, result = %d\n", result);
    }

    return 0; 
}

void initProcTable(){
    for (int i = 0; i < MAXPROC; i++){
        ProcTable[i].status = EMPTY;
        ProcTable[i].privateMboxId = MboxCreate(0,0);
        ProcTable[i].children = NULL;
        ProcTable[i].nextChild = NULL;
    }
}

void initSemTable(){
    for (int i = 0; i < MAXSEMS; i++){
        SemTable[i].status = EMPTY;
    }
}

void initSyscallVec(){
    for (int i = 0; i < MAXSYSCALLS; i++){
        systemCallVec[i] = nullsys3;
    }
    systemCallVec[SYS_SPAWN] = spawn;
    systemCallVec[SYS_WAIT] = wait;
    systemCallVec[SYS_TERMINATE] = terminate;
    systemCallVec[SYS_GETTIMEOFDAY] = gettimeofday;
    systemCallVec[SYS_CPUTIME] = cputime;
    systemCallVec[SYS_GETPID] = getpid3;
    systemCallVec[SYS_SEMCREATE] = semcreate;
    systemCallVec[SYS_SEMP] = semp;
    systemCallVec[SYS_SEMV] = semv;
    systemCallVec[SYS_SEMFREE] = semfree;
}

void nullsys3(USLOSS_Sysargs *args){
   //terminate
} /* nullsys */
void spawn(USLOSS_Sysargs *args){
}
void wait(USLOSS_Sysargs *args){
}
void terminate(USLOSS_Sysargs *args){  //conditional send on our parents mailbox to wake them up
}
void gettimeofday(USLOSS_Sysargs *args){
}
void cputime(USLOSS_Sysargs *args){
}
void getpid3(USLOSS_Sysargs *args){
}
void semcreate(USLOSS_Sysargs *args){
}
void semp(USLOSS_Sysargs *args){
}
void semv(USLOSS_Sysargs *args){
}
void semfree(USLOSS_Sysargs *args){
}


//FIXME: maybe we don't need these? - waiting on Piazza confirmation
void check_kernel_mode(char * arg) {
    if (!isInKernelMode()) {
        fprintf(stderr, "%s: Not in kernel mode.\n", arg);
        USLOSS_Halt(1);
    }
}

int isInKernelMode() {
    unsigned int psr = USLOSS_PsrGet();
    unsigned int op = 0x1;
    return psr & op;
}

int enterUserMode() {
    unsigned int psr = USLOSS_PsrGet();
    unsigned int op = 0xfffffffe;
    int result = USLOSS_PsrSet(psr & op);
    if (result == USLOSS_ERR_INVALID_PSR) {
        return -1;
    }
    else {
        return 0;
    }
}

void initProc(int pid, int parentPid){

    int i = pid % MAXPROC;

    //set fields
    ProcTable[i].status = OCCUPIED;
    ProcTable[i].pid = pid;
    ProcTable[i].children = NULL;
    ProcTable[i].nextChild = NULL;
    ProcTable[i].parentPid = parentPid;
    
    //create mailbox
    int mboxid = MboxCreate(0,0);
    if (mboxid < 0){
        fprintf(stderr, "mailboxid < 0. Terminating\n");
        USLOSS_Halt(1); //FIXME: terminate instead of halt
    }
    ProcTable[i].privateMboxId = mboxid;

    //append to parent's children
    if (parentPid > 0){
        int j = parentPid % MAXPROC;
        if (ProcTable[j].children == NULL){
            ProcTable[j].children = &ProcTable[i];
        } else {
            p3ProcPtr curr = ProcTable[j].children;
            p3ProcPtr prev = NULL;
            while (curr != NULL){
                prev = curr;
                curr = curr->nextChild;
            }
            prev->nextChild = &ProcTable[i];
        }
    }
}




