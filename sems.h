
#define EMPTY 0
#define OCCUPIED 1

typedef struct p3Proc* p3ProcPtr;
typedef struct sem* semPtr;


typedef struct p3Proc p3Proc;
typedef struct sem sem;

struct p3Proc {
    int pid;        //pid of phase3 proc
    int status;     //status of proc
    int privateMboxId;
    p3ProcPtr children;
    p3ProcPtr nextChild;
};

struct sem {
    int value;      //value of semaphore
    int status;     //status of semaphore
};



