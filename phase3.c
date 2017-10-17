#include <usloss.h>
#include <usyscall.h>
#include <phase1.h>
#include <phase2.h>
#include <phase3.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "sems.h"


/* FUNCTION PROTOTYPES */
int spawnReal();
int waitReal();
void terminateReal();
int start3();
int spawnLaunch();
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
p3ProcPtr getCurrentProc();
p3ProcPtr getProc();
void zapChildren();
void cleanupProc();
void dumpProcesses3();

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
int debugflag3 = 0;



int
start2(char *arg)
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
    int kidpid = fork1(name, spawnLaunch, NULL, (int)stack_size, (int)priority);

    if (debugflag3){
        USLOSS_Console("spawnReal(): after fork1, kidpid = %d\n", kidpid);
    }

    if (kidpid < 0){
        fprintf(stderr, "kidpid < 0. Terminating\n");
        USLOSS_Halt(1); //FIXME: terminate instead of halt
    }

    initProc(kidpid, getpid());

    p3ProcPtr kidProc = getProc(kidpid);

    //save function pointer and arg to PTE
    if (arg != NULL){
        memcpy(kidProc->arg, arg, strlen(arg) + 1);
    }
    kidProc->func = func;

    //wake up child who's blocked in spawnLaunch()
    MboxSend(kidProc->privateMboxId, NULL, 0);
    if (debugflag3){
        USLOSS_Console("got here 1\n");
    }


    return kidpid;
}

int spawnLaunch(){
    if (debugflag3){
        USLOSS_Console("spawnLaunch(): called by pid %d\n", getpid());
    }

    //wait for spawnReal to finish creating pte
    p3ProcPtr me = getCurrentProc();
    MboxReceive(me->privateMboxId, NULL, 0);

    //switch to user mode before executing
    if (debugflag3){
        USLOSS_Console("spawnLaunch(): entering user mode and executing func\n");
    }
    enterUserMode();
    me->func();
    if (debugflag3){
        USLOSS_Console("spawnLaunch(): finished executing func\n");
    }
    Terminate(15); 
    return 0;
}

int waitReal(int * status){
    if (debugflag3){
        USLOSS_Console("waitReal(): called by pid %d\n", getpid());
    }
    int result;
    int pid = join(&result);

    if (pid < 0){
        fprintf(stderr, "waitReal(): join result < 0, terminate.\n");
        USLOSS_Halt(1); //FIXME: terminate instead of  halt.
    }
    if (debugflag3){
        USLOSS_Console("waitReal(): pid %d after join of pid %d\n", getpid(), pid);
    }

    *status = result;
    return pid; 
}

void terminateReal(int status){
    if (debugflag3){
        USLOSS_Console("terminateReal(): called by pid %d with status = %d\n", getpid(), status);
    }

    p3ProcPtr me = getCurrentProc();

    if (me->numKids > 0){
        zapChildren(me);
        //zap all children
    }
    
    cleanupProc(me); //reset fields and remove from parent's list 
    quit(status);
}

void zapChildren(p3ProcPtr proc){
    p3ProcPtr child = proc->children;
    while (child != NULL){
        zap(child->pid);
        proc->numKids--;
        child = child->nextChild;
    }
    proc->children = NULL;
}

/*
Reset proc fields and remove proc from parent's list of children
*/
void cleanupProc(p3ProcPtr proc){

    p3ProcPtr parent = getProc(proc->parentPid);
    if (proc->pid == parent->children->pid){
        parent->children = proc->nextChild;
    } else {
        p3ProcPtr curr = parent->children;
        p3ProcPtr prev = NULL;
        while (curr->pid != proc->pid){
            prev = curr;
            curr = curr->nextChild;
        } 
        prev->nextChild = proc->nextChild;
    }

    parent->numKids--;
    proc->pid = -1;
    proc->status = EMPTY;
    proc->parentPid = -1;
    proc->func = NULL;
    proc->children = NULL;
    proc->nextChild = NULL;
    proc->numKids = 0;
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
   terminateReal(15);
} /* nullsys */

/*
Input
    arg1: address of the function to spawn.
    arg2: parameter passed to spawned function.
    arg3: stack size (in bytes).
    arg4: priority.
    arg5: character string containing process’s name.

Output: 
    arg1: the PID of the newly created process; -1 if a process could not be created.
    arg4: -1 if illegal values are given as input; 0 otherwise.
*/
void spawn(USLOSS_Sysargs *args){
    if (debugflag3){
        USLOSS_Console("spawn(): called to spawn %s\n", args->arg5);
    }
    //call spawnReal with proper args char *name, int (*func)(char *), char *arg, long stack_size, long priority
    int (*func)(char *) = args->arg1;
    char * arg = args->arg2;
    int stack_size = (uintptr_t) args->arg3;
    int priority = (uintptr_t) args->arg4;
    char * name = args->arg5;

    long errorcode = 0;
    //error checks
    if (name == NULL || func == NULL){
        errorcode = -1;
    }
    if (name != NULL && (strlen(name) >= MAXNAME - 1)){
        errorcode = -1;
    }
    if (stack_size < USLOSS_MIN_STACK){
        errorcode = -1;
    }
    if (priority > 6 || priority < 1) {
        errorcode = -1;
    }


    long result = (long)spawnReal(name, func, arg, stack_size, priority);
    if (result < 0){
        result = -1;
    }

    if (debugflag3){
        USLOSS_Console("spawn(): returning result = %d, errorcode = %d\n", result, errorcode);
    }

    args->arg1 = (void *)result;
    args->arg4 = (void *)errorcode;

    if (isZapped()){
        terminateReal(15);
    }
    enterUserMode();
}

/*
Output
    arg1: process id of the terminating child.
    arg2: the termination code of the child.
*/
void wait(USLOSS_Sysargs *args){
    int status;

    long kidpid = (long)waitReal(&status);
    long result = (long) status;


    args->arg1 = (void * )kidpid;
    args->arg2 = (void * )result;

    if (isZapped()){
        terminateReal(15);
    }
    enterUserMode();
}

/*
Input
    arg1: termination code for the process.
*/
void terminate(USLOSS_Sysargs *args){  //conditional send on our parents mailbox to wake them up
    int status = (uintptr_t)args->arg1;
    terminateReal(status);
    enterUserMode();

}
void gettimeofday(USLOSS_Sysargs *args){

    if (isZapped()){
        terminateReal(15);
    }
    enterUserMode();
}
void cputime(USLOSS_Sysargs *args){
    if (isZapped()){
        terminateReal(15);
    }
}
void getpid3(USLOSS_Sysargs *args){
    if (isZapped()){
        terminateReal(15);
    }
    enterUserMode();
}
void semcreate(USLOSS_Sysargs *args){
    if (isZapped()){
        terminateReal(15);
    }
    enterUserMode();
}
void semp(USLOSS_Sysargs *args){
    if (isZapped()){
        terminateReal(15);
    }
    enterUserMode();
}
void semv(USLOSS_Sysargs *args){
    if (isZapped()){
        terminateReal(15);
    }
    enterUserMode();
}
void semfree(USLOSS_Sysargs *args){
    if (isZapped()){
        terminateReal(15);
    }
    enterUserMode();
}


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

    p3ProcPtr proc = getProc(pid);

    //set fields
    proc->status = OCCUPIED;
    proc->pid = pid;
    proc->children = NULL;
    proc->nextChild = NULL;
    proc->func = NULL;
    proc->parentPid = parentPid;

    //append to parent's children
    if (parentPid > 0){
        p3ProcPtr parentProc = getProc(parentPid);
        if (parentProc->children == NULL){
            parentProc->children = proc;
        } else {
            p3ProcPtr curr = parentProc->children;
            p3ProcPtr prev = NULL;
            while (curr != NULL){
                prev = curr;
                curr = curr->nextChild;
            }
            prev->nextChild = proc;
        }
        parentProc->numKids++;
    }
}

p3ProcPtr getCurrentProc(){
    return getProc(getpid());
}

p3ProcPtr getProc(int pid) {
    return &ProcTable[pid % MAXPROC];
}

void dumpProcesses3() {
    char * statuses[2];
    statuses[OCCUPIED] = "OCCUPIED";
    statuses[EMPTY] = "EMPTY";


    USLOSS_Console(" SLOT   PID   PARENTPID     STATUS     NUM CHILDREN \n");
    USLOSS_Console("------ ----- ----------- ------------ --------------\n");
    for (int i = 0; i < MAXPROC; i++){
            p3ProcPtr temp = &ProcTable[i];
            int parentpid = temp->parentPid; 
            USLOSS_Console("%6d %5d %11d %12s %14d\n", i, temp->pid, parentpid, statuses[temp->status], temp->numKids);
    }
}





