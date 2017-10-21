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
long semcreateReal(int val);
void semp();
void semv();
void semfree();
void check_kernel_mode(char * arg);
int isInKernelMode();
int enterUserMode();
p3ProcPtr getCurrentProc();
p3ProcPtr getProc();
void zapChildren();
int getNextSemID();
void cleanupProc();
void dumpProcesses3();
int Terminate();

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
int nextSemId = 0;
int numSems = 0;
int semTableMbox;

int debugflag3 = 0;

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

    //initialize the proc table, create mailboxes for each proc
    initProcTable();

    //intialize sem table, create mailboxes for each semaphore
    initSemTable();

    //intialize system call vector with phase3 function pointers
    initSyscallVec();

    //initialize this proc with parent pid -1
    initProc(getpid(), -1);

    //intialize semtable mutex
    semTableMbox = MboxCreate(1,0);
    

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

    //spawn start3
    pid = spawnReal("start3", start3, NULL, USLOSS_MIN_STACK, 3);

    /* Call the waitReal version of your wait code here.
     * You call waitReal (rather than Wait) because start2 is running
     * in kernel (not user) mode.
     */

    //wait for start3 to finish
    pid = waitReal(&status);

    if (debugflag3){
        USLOSS_Console("start2(): done with waitReal pid = %d\n", pid );
    }

    return 0;
} /* start2 */


/*
Calls fork1 to create new process which starts executing in spawnLaunch(). Initializes
the PTE for the new proc and then wakes up the child process. Returns the pid of the forked child.
*/
int spawnReal(char *name, int (*func)(char *), char *arg, long stack_size, long priority){
    if (debugflag3){
        USLOSS_Console("spawnReal(): called to spawn %s\n", name);
    }
    //call fork1 to spawnLaunch
    int kidpid = fork1(name, spawnLaunch, NULL, (int)stack_size, (int)priority);

    //Error check if fork1 failed
    if (kidpid < 0){
        if (debugflag3){
            USLOSS_Console("spawnReal(): fork1 failed pid = %d", kidpid);
        }
        return -1; 
    }

    //intialize the PTE for the new process
    initProc(kidpid, getpid());

    //get pointer to new proc
    p3ProcPtr kidProc = getProc(kidpid);

    //save function pointer and arg to PTE
    if (arg != NULL){
        memcpy(kidProc->arg, arg, strlen(arg) + 1);
    }

    //copy func to kid proc
    kidProc->func = func;

    //wake up child who's blocked in spawnLaunch()
    MboxSend(kidProc->spawnMboxId, NULL, 0);

    if (debugflag3){
        USLOSS_Console("spawnReal(): pid %d finally finished spawning pid %d\n", getpid(), kidpid );
    }

    //return pid of successful fork
    return kidpid;
}

/*
A newly forked child starts executing here. It waits for parent to finish initializing
the PTE by receiving on it's mbox. It terminates if zapped while waiting. Then it
enters usermode and calls the actually user mode function. Last it calls terminate to finish
if the function code didn't call terminate.
*/
int spawnLaunch(){
    if (debugflag3){
        USLOSS_Console("spawnLaunch(): called by pid %d\n", getpid());
    }

    //get current proc ptr
    p3ProcPtr me = getCurrentProc();

    //wait for spawnReal to finish creating pte
    MboxReceive(me->spawnMboxId, NULL, 0);

    //terminate if zapped while waiting
    if (isZapped()){
        if (debugflag3){
            USLOSS_Console("spawnLaunch(): pid %d was zapped, calling terminate\n", me->pid);
        }
        terminateReal(1);
    }

    //switch to user mode
    enterUserMode();

    //call function
    me->func(me->arg);
    if (debugflag3){
        USLOSS_Console("spawnLaunch(): finished executing func\n");
    }

    //user-mode terminate (becuase we switched to user mode)
    Terminate(1); 

    return 0;
}

/*
Calls join to wait for a child process to finish. Stores the quit status
of the child process in *status. Returns the pid of the quit process.
*/
int waitReal(int * status){
    if (debugflag3){
        USLOSS_Console("waitReal(): called by pid %d\n", getpid());
    }
    //result pointer
    int result;

    //call join to wait for a child to finish
    int pid = join(&result);

    //terminate if join fails
    if (pid < 0){
        fprintf(stderr, "waitReal(): join result < 0, terminate.\n");
        terminateReal(1);
    }

    if (debugflag3){
        USLOSS_Console("waitReal(): pid %d after join of pid %d\n", getpid(), pid);
    }

    //put result into status pointer
    *status = result;

    //returns pid of finished child
    return pid; 
}

/*
Terminates the currently executing process with the given status by zapping
all of its children, removing it from it's parents child list, and calling quit. 
*/
void terminateReal(int status){
    if (debugflag3){
        USLOSS_Console("terminateReal(): called by pid %d with status = %d\n", getpid(), status);
    }

    //get current proc pointer
    p3ProcPtr me = getCurrentProc();

    //zap all the children
    if (me->numKids > 0){
        zapChildren(me);
    }
    
    //reset fields and remove from parent's list 
    cleanupProc(me); 

    //call quit to actually terminate the proc
    quit(status);
}

/*
Calls zap on all the children of the given proc, and decrements the number of children. 
Then sets the child list of proc to NULL. 
*/
void zapChildren(p3ProcPtr proc){
    //Get the first child
    p3ProcPtr child = proc->children;

    //loop through all children
    while (child != NULL){
        if (debugflag3){
            USLOSS_Console("terminateReal(): pid %d waiting to zap pid %d\n", proc->pid, child->pid);
        }
        int x = child->pid;
        //call zap and wait for child to quit
        zap(child->pid);
        if (debugflag3){
            USLOSS_Console("terminateReal(): pid %d finished zapping pid %d\n", proc->pid, x);
        }
        //child is no longer on proc's list
        //decrement numkids
        proc->numKids--;
        //reset childptr to the next child
        child = proc->children;
    }
    proc->children = NULL;
}

/*
Reset proc fields and remove proc from parent's list of children
*/
void cleanupProc(p3ProcPtr proc){

    //get the proc's parent
    p3ProcPtr parent = getProc(proc->parentPid);

    //remove proc from parent's list
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

    //reset all fields of the child
    parent->numKids--;
    proc->pid = -1;
    proc->status = EMPTY;
    proc->parentPid = -1;
    proc->func = NULL;
    proc->children = NULL;
    proc->nextChild = NULL;
    proc->numKids = 0;
}

/*
Initialize the proc table fields, and create the mailboxes
needed by the procs. 
*/
void initProcTable(){
    for (int i = 0; i < MAXPROC; i++){
        ProcTable[i].status = EMPTY;
        ProcTable[i].privateMboxId = MboxCreate(0,0);
        ProcTable[i].spawnMboxId = MboxCreate(1,0);
        ProcTable[i].children = NULL;
        ProcTable[i].nextChild = NULL;
    }
}

void initSemTable(){
    for (int i = 0; i < MAXSEMS; i++){
        SemTable[i].status = EMPTY;
        SemTable[i].mbox = MboxCreate(1,0);
        SemTable[i].zapped = 0;
    }
}

/*
Initialize the system call vector to call our functions
or nullsys3.
*/
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

/*
Called by the syscalls that we did not implement in phase3
*/
void nullsys3(USLOSS_Sysargs *args){
    //just terminate
   terminateReal(1);
} /* nullsys */

/*
Syscall function, error checks and calls spawnReal
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
    //call spawnReal with proper args char *name, int (*func)(char *), char *arg, long stack_size, long priority
    int (*func)(char *) = args->arg1;
    char * arg = args->arg2;
    int stack_size = (uintptr_t) args->arg3; //TODO: change to pointer, dereference
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
        terminateReal(1);
    }
    enterUserMode();
}

/*
Syscall function, error checks and call waitReal
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
        terminateReal(1);
    }
    enterUserMode();
}

/*
Syscall function, calls terminateReal
Input
    arg1: termination code for the process.
*/
void terminate(USLOSS_Sysargs *args){  //conditional send on our parents mailbox to wake them up
    int status = (uintptr_t)args->arg1;
    terminateReal(status);
    enterUserMode();

}

void gettimeofday(USLOSS_Sysargs *args){
    int status;
    int result = USLOSS_DeviceInput(USLOSS_CLOCK_DEV, 0, &status);
    if (result != USLOSS_DEV_OK) {
        if (debugflag3) {
            USLOSS_Console("gettimeofday(): clock device call failed.\n");
        }
        terminateReal(1);
    }
    args->arg1 = (void*)(long)status;
    if (isZapped()){
        terminateReal(1);
    }
    enterUserMode();
}
void cputime(USLOSS_Sysargs *args){
    args->arg1 = (void*)(long)readtime();
    if (isZapped()){
        terminateReal(1);
    }
    enterUserMode();
}

void getpid3(USLOSS_Sysargs *args){
    args->arg1 = (void *)(long)getpid();
    if (isZapped()){
        terminateReal(1);
    }
    enterUserMode();
}

void semcreate(USLOSS_Sysargs *args){
    if (debugflag3) {
        USLOSS_Console("semcreate(): called.\n");
    }
    int initNum = (uintptr_t)args->arg1;

    if (initNum < 0 || numSems >= MAXSEMS) {
        args->arg4 = (void *)-1;
        enterUserMode();
        return;
    }
    else {
        args->arg4 = 0;
    }

    args->arg1 = (void *)semcreateReal(initNum);

    if (debugflag3) {
        USLOSS_Console("semcreate(): test.\n");
    }
    if (isZapped()) {
        terminateReal(1); 
    }
    enterUserMode();
}

long semcreateReal(int val) {
    if (debugflag3) {
        USLOSS_Console("semcreateReal(): called.\n");
    }
    MboxSend(semTableMbox, NULL, 0);
    int semId = getNextSemID();
    SemTable[semId].status = OCCUPIED;
    SemTable[semId].value = val;
    SemTable[semId].blockedList = NULL;
    SemTable[semId].zapped = 0;
    numSems++;

    MboxReceive(semTableMbox, NULL, 0);
    return (long)semId;
}

int getNextSemID() {
    //return next available semaphore
    while (SemTable[nextSemId % MAXSEMS].status != EMPTY){
        nextSemId = (nextSemId + 1) % MAXSEMS;
    }

    return nextSemId;
}


void semp(USLOSS_Sysargs *args){
    int semId = (uintptr_t)args->arg1;
    int mboxId = SemTable[semId].mbox;
    MboxSend(mboxId, NULL, 0);

    if (semId < 0 || semId >= MAXSEMS || SemTable[semId].status == EMPTY) {
        args->arg4 = (void *)-1;
        return;
    }
    else {
        args->arg4 = (void *)0;
    }

    if (SemTable[semId].value > 0) {
        SemTable[semId].value--;
    }
    else {
        // Add this process to the semaphore blocked list
        int procId = getpid();
        p3ProcPtr myProc = &ProcTable[procId];
        if (SemTable[semId].blockedList == NULL) {
            SemTable[semId].blockedList = myProc;
        }
        else {
            p3ProcPtr curr = SemTable[semId].blockedList;
            p3ProcPtr prev = NULL;
            while (curr != NULL){
                prev = curr;
                curr = curr->nextBlocked;
            }
            prev->nextBlocked = myProc;
        }
        if (debugflag3){
            USLOSS_Console("semp(): must block current proc on semaphore as value = %d.\n", SemTable[semId].value);
        }
        MboxReceive(mboxId, NULL, 0); // Release mutex on this semaphore for others
        MboxReceive(myProc->privateMboxId, NULL, 0); // block awaiting a V()
        if (debugflag3){
            USLOSS_Console("semp(): process %d awoken from block.\n", getpid());
        }
        if (isZapped() || SemTable[semId].zapped){
            terminateReal(1);
        }
        enterUserMode();
        return;
    }
    
    MboxReceive(mboxId, NULL, 0);
    if (isZapped() || SemTable[semId].zapped){
        terminateReal(1);
    }
    enterUserMode();
}

void semv(USLOSS_Sysargs *args){
    int semId = (uintptr_t)args->arg1;
    int mbodId = SemTable[semId].mbox;
    MboxSend(mbodId, NULL, 0);

    if (semId < 0 || semId >= MAXSEMS || SemTable[semId].status == EMPTY) {
        args->arg4 = (void *)-1;
        return;
    }
    else {
        args->arg4 = (void *)0;
    }

    if (SemTable[semId].blockedList == NULL) {
        if (debugflag3){
            USLOSS_Console("semv(): incrementing semaphore.\n");
        }
        SemTable[semId].value++;
    } 
    else {
        if (debugflag3){
            USLOSS_Console("semv(): waking up blocked proc.\n");
        }
        int wakeupId = SemTable[semId].blockedList->privateMboxId;
        SemTable[semId].blockedList = SemTable[semId].blockedList->nextBlocked;
        MboxSend(wakeupId, NULL, 0);
    }
    
    MboxReceive(mbodId, NULL, 0);
    if (isZapped()){
        terminateReal(1);
    }
    enterUserMode();
}
void semfree(USLOSS_Sysargs *args){
    int semId = (uintptr_t)args->arg1;
    int mboxId = SemTable[semId].mbox;
    MboxSend(mboxId, NULL, 0);

    if (semId < 0 || semId >= MAXSEMS || SemTable[semId].status == EMPTY) {
        args->arg4 = (void *)-1;
        return;
    }
    else if (SemTable[semId].blockedList != NULL) {
        args->arg4 = (void *)1;
    }
    else {
        args->arg4 = (void *)0;
    }

    if (SemTable[semId].blockedList != NULL) {
        if (debugflag3) {
            USLOSS_Console("semfree(): terminating all procs blocked on this semaphore.\n");
        }
        SemTable[semId].zapped = 1;
        p3ProcPtr curr = SemTable[semId].blockedList;
        while (curr != NULL) {
            p3ProcPtr temp = curr;
            curr = curr->nextBlocked;
            MboxSend(temp->privateMboxId, NULL, 0);
        }
    }



    SemTable[semId].status = EMPTY;
    SemTable[semId].blockedList = NULL;
    numSems--;

    MboxReceive(mboxId, NULL, 0);
    if (isZapped()){
        terminateReal(1);
    }
    enterUserMode();
}

/*
Halts if not in kernel mode
*/
void check_kernel_mode(char * arg) {
    if (!isInKernelMode()) {
        fprintf(stderr, "%s: Not in kernel mode.\n", arg);
        USLOSS_Halt(1);
    }
}

/*
returns boolean of in kernel mode
*/
int isInKernelMode() {
    unsigned int psr = USLOSS_PsrGet();
    unsigned int op = 0x1;
    return psr & op;
}

/*
Enters user mode by calling psrget()
*/
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

/*
Set all fields to the process and append to parent's list
*/
void initProc(int pid, int parentPid){

    p3ProcPtr proc = getProc(pid);

    //set fields
    proc->status = OCCUPIED;
    proc->pid = pid;
    proc->children = NULL;
    proc->nextChild = NULL;
    proc->nextBlocked = NULL;
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

/*
debug print
*/
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





