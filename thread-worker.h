// File:	worker_t.h

// List all group member's name:
// username of iLab:
// iLab Server:

#ifndef WORKER_T_H
#define WORKER_T_H

#define _GNU_SOURCE

/* To use Linux pthread Library in Benchmark, you have to comment the USE_WORKERS macro */
#define USE_WORKERS 1
#define NUM_OF_MLFQ 8
#define STACK_SIZE SIGSTKSZ
#define QUANTUM 2000
#define QUANTUM_MLFQ 1000
#define PR_BOOST_QUANTUM 10000

/* include lib header files that you need here: */
#include <unistd.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <ucontext.h>
#include <signal.h>
#include <sys/time.h>
#include <string.h>
#include <stdatomic.h>
#include<time.h>

typedef uint worker_t;

typedef enum status{READY,RUNNING,BLOCKED,TERMINATED,DESTROY} workerStatus;

typedef struct TCB {
	/* add important states in a thread control block */
	worker_t threadId;											// thread Id
	worker_t blockedBy;
	workerStatus status;													// thread status
	ucontext_t ctx;	 											// thread context
	//int* stack;													// thread stack
	void* rtn;													//return address
	void** valuePtr;
	int quantum;												//no of quantum elapsed
	clock_t arrivalTime;
	clock_t completionTime;
	clock_t firstScheduled;
	// And more ...

	// YOUR CODE HERE
} TCB; 
/* mutex struct definition */
typedef struct rqueue{
	TCB* value;
	struct rqueue* next;
}rqueue;

typedef struct worker_mutex_t {
	/* add something here */
	int lock;
	// YOUR CODE HERE
} worker_mutex_t;
/* define your data structures here: */
// Feel free to add your own auxiliary data structures (linked list or queue etc...)

// YOUR CODE HERE


/* Function Declarations: */

/* create a new thread */
int worker_create(worker_t * thread, pthread_attr_t * attr, void
    *(*function)(void*), void * arg);

/* give CPU pocession to other user level worker threads voluntarily */
int worker_yield();

/* terminate a thread */
void worker_exit(void *value_ptr);

/* wait for thread termination */
int worker_join(worker_t thread, void **value_ptr);

/* initial the mutex lock */
int worker_mutex_init(worker_mutex_t *mutex, const pthread_mutexattr_t
    *mutexattr);

/* aquire the mutex lock */
int worker_mutex_lock(worker_mutex_t *mutex);

/* release the mutex lock */
int worker_mutex_unlock(worker_mutex_t *mutex);

/* destroy the mutex */
int worker_mutex_destroy(worker_mutex_t *mutex);

static void schedule();
static void sched_psjf();
static void sched_mlfq();
void priorityBoost();
/* Function to print global statistics. Do not modify this function.*/
void print_app_stats(void);

#ifdef USE_WORKERS
#define pthread_t worker_t
#define pthread_mutex_t worker_mutex_t
#define pthread_create worker_create
#define pthread_exit worker_exit
#define pthread_join worker_join
#define pthread_mutex_init worker_mutex_init
#define pthread_mutex_lock worker_mutex_lock
#define pthread_mutex_unlock worker_mutex_unlock
#define pthread_mutex_destroy worker_mutex_destroy
#endif

#endif
