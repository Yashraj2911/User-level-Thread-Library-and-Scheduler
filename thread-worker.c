// File:	thread-worker.c

// List all group member's name:
//									Yashraj Nikam (yan7)
//									Dhruv Kedia (dk1157)
// username of iLab:rlab1.cs.rutgers.edu
// iLab Server:

#include "thread-worker.h"

//Global counter for total context switches and 
//average turn around and response time
long tot_cntx_switches=0;
double avg_turn_time=0;
double avg_resp_time=0;

long avgTurnCounter = 0;
long avgRespCounter = 0;


rqueue* multiLevelQueues[NUM_OF_MLFQ] = {0};
int currentQueueIndex = 0;
int prBoostCounter = 0;

int threadId = 0;
rqueue *head = NULL;
TCB* current = NULL;

void initTimer(int q)
{	
	struct sigaction sa;
	struct itimerval timer;
	memset (&sa, 0, sizeof (sa));
	sa.sa_handler = &schedule;
	sigaction (SIGPROF, &sa, NULL);
	timer.it_interval.tv_usec = 0; 
	timer.it_interval.tv_sec = 0;
	timer.it_value.tv_usec = q;
	timer.it_value.tv_sec = 0;
	setitimer(ITIMER_PROF, &timer, NULL);
}
TCB* createTcb(worker_t id)
{
	TCB* tcb = (TCB*)malloc(sizeof(TCB));       
	if (threadId && getcontext(&tcb->ctx) < 0)
	{
		printf("getcontext");
		exit(1);
	}
	tcb->ctx.uc_stack.ss_sp=(void*)malloc(STACK_SIZE);
	 if (tcb->ctx.uc_stack.ss_sp == NULL)
	 {
		printf("Failed to allocate stack");
		exit(1);
	}
       // - create and initialize the context of this worker thread
       tcb->ctx.uc_link=NULL;	
	tcb->ctx.uc_stack.ss_size=STACK_SIZE;
	tcb->ctx.uc_stack.ss_flags = 0;
	tcb->threadId = id;
	tcb->quantum = 0;
	tcb->status = READY;
	tcb->blockedBy = -1;
	tcb->rtn = NULL;
	tcb->valuePtr = NULL;
	tcb->firstScheduled = -1;
	tcb->arrivalTime = clock();
	return tcb;
}

void enqueue(rqueue** head,rqueue* new)
{	
	 if(*head==NULL)
       {
       	*head = new;
       	(*head)->next = NULL;       
       }
       else
       {
       	rqueue* temp = *head;
       	while(temp->next)
       		temp = temp->next;
       	temp->next = new;
       	temp->next->next = NULL;
       }
}
void makeMainContext()
{
	current= createTcb(threadId++);
	current -> blockedBy = -1;
	rqueue* new = (rqueue*)malloc(sizeof(rqueue));      
       new -> value = current;
       new -> next = NULL; 
       #ifndef MLFQ
       enqueue(&head,new);
       initTimer(QUANTUM_MLFQ);
       #else
       enqueue(&multiLevelQueues[0],new);
       initTimer(QUANTUM);
       #endif      
	getcontext(&current->ctx);
}

void unblockThreads(rqueue* head,worker_t thread)
{
	if(!head)
		return;
	rqueue* temp = head;
	while(temp)
	{
		if(temp->value->blockedBy == thread)
		{
			temp -> value -> blockedBy = -1;
			temp -> value -> status = READY;
		}
		temp = temp->next;
	}
}
void unblockThreadsInMlfq(worker_t thread)
{
	for(int i=0;i<NUM_OF_MLFQ;i++)
		unblockThreads(multiLevelQueues[i],thread);
}
/* create a new thread */
int worker_create(worker_t * thread, pthread_attr_t * attr, void *(*function)(void*), void * arg) 
{

       // - create Thread Control Block (TCB)
       if(!threadId)
       	makeMainContext();
       *thread = threadId++;
	TCB* tcb = createTcb(*thread);	
       makecontext(&tcb->ctx,(void*)function,1,arg);
       // - allocate space of stack for this thread to run
      
       // after everything is set, push this thread into run queue and 
       rqueue* new = (rqueue*)malloc(sizeof(rqueue));      
       new -> value = tcb;
       new -> next = NULL; 
       
     #ifndef MLFQ
       enqueue(&head,new);
       #else
       enqueue(&multiLevelQueues[0],new);
       #endif           
       // - make it ready for the execution.

       // YOUR CODE HERE
	
    return 0;
};

/* give CPU possession to other user-level worker threads voluntarily */
int worker_yield() {
	
	current->status = READY;
	schedule();
	return 0;
}
void dequeue(rqueue** head,worker_t delete)
{
	if(!*head)
	{
		printf("No thread created");
		exit(1);
	}	
	else if((*head)->value->threadId == delete)
	{
		rqueue* del = *head;
		*head = (*head) ->next;
		free(del);
	}
	else
	{
		rqueue* temp = *head;
		while(temp->next && temp->next->value->threadId != delete)
			temp = temp->next;
		if(!temp->next)
		{
			perror("Dequeue error");
			exit(1);
		}
		rqueue* del = temp->next;
		temp -> next = temp->next->next;
		del->next = NULL;
		free(del -> value -> ctx.uc_stack.ss_sp);
		del -> value -> ctx.uc_stack.ss_size = 0;
		free(del);
	}
}
void dequeueDestroyed(rqueue* head)
{
	if(!head)
		return;
	rqueue* temp = head;
	while(temp)
	{
		if(temp->value->status == DESTROY)
		{
			worker_t tempThread = temp -> value -> threadId;
			temp = temp -> next;
			dequeue(&head,tempThread);
		}
		else
			temp = temp -> next;	
	}
}
void dequeueDestroyedMLFQ()
{
	for(int i=0; i < NUM_OF_MLFQ; i++)
		dequeueDestroyed(multiLevelQueues[i]);
}
/* terminate a thread */
void worker_exit(void *value_ptr) {
	current -> completionTime = clock();
	double totalCT = (double)(current->completionTime - current->arrivalTime)/CLOCKS_PER_SEC;
	avg_turn_time = (avg_turn_time*avgTurnCounter + totalCT)/++avgTurnCounter;
	double totalRT = (double)(current->firstScheduled - current->arrivalTime)/CLOCKS_PER_SEC;
	avg_resp_time = (avg_resp_time*avgRespCounter + totalRT)/++avgRespCounter;
	current -> status = TERMINATED;	
	if(current -> valuePtr)
	{
		*current -> valuePtr = value_ptr;
		current -> status = DESTROY;
	}
	else
		current -> rtn = value_ptr;
	#ifndef MLFQ
	unblockThreads(head,current->threadId);	
	#else
	unblockThreadsInMlfq(current->threadId);
	#endif
	
	schedule();
};
rqueue* findTCB(rqueue* head,worker_t thread)
{
	rqueue* temp = head;
	while(temp && temp->value->threadId != thread)
		temp = temp->next;
	return temp;
}
rqueue* findInMLFQ(worker_t thread,int* index)
{
	for(int i = 0; i < NUM_OF_MLFQ; i++)
	{
		rqueue* temp = findTCB(multiLevelQueues[i],thread);
		if(temp)
		{
			*index = i;
			return temp;
		}
	}
	*index = -1;
	return NULL;
}

/* Wait for thread termination */
int worker_join(worker_t thread, void **value_ptr) {
	
	#ifndef MLFQ
	rqueue* temp = findTCB(head,thread);
	#else
	int index;
	rqueue* temp = findInMLFQ(thread,&index);
	#endif
	if(!temp)
		return 0;
	if(temp->value->status != TERMINATED)
	{
		current -> status = BLOCKED;
		current -> blockedBy = thread;
		temp -> value ->valuePtr = value_ptr;
		schedule();
	}
	else
	{
		if(value_ptr)
		{
			*value_ptr = temp -> value -> rtn;						
			temp -> value -> status = DESTROY;
		}
	}
	return 0;
};

/* initialize the mutex lock */
int worker_mutex_init(worker_mutex_t *mutex,const pthread_mutexattr_t *mutexattr) {
	mutex -> lock = 0;
	return 0;
};

/* aquire the mutex lock */
int worker_mutex_lock(worker_mutex_t *mutex) {

        // - use the built-in test-and-set atomic function to test the mutex
        // - if the mutex is acquired successfully, enter the critical section
        // - if acquiring mutex fails, push current thread into block list and
        // context switch to the scheduler thread
	while(atomic_flag_test_and_set(&(mutex->lock)));
    return 0;     
};

/* release the mutex lock */
int worker_mutex_unlock(worker_mutex_t *mutex) {
	// - release mutex and make it available again. 
	// - put threads in block list to run queue 
	// so that they could compete for mutex later.	
	mutex->lock = 0;	
	return 0;
};


/* destroy the mutex */
int worker_mutex_destroy(worker_mutex_t *mutex) {
	worker_mutex_unlock(mutex);
	return 0;
};
TCB* findNextJobPSJF()
{	
	if(!head->next&&head->value->status==READY)
		return head->value;
	else if(!head->next&&head->value->status!=READY)
		return NULL;
	rqueue* temp = head;
	if(head->value->status != READY)
	{
		while(temp && temp->value->status != READY)
			temp = temp -> next;
	}	
	if(!temp)
		return NULL;
	if(!temp->next)
		return temp->value;
	TCB* next = temp->value;
	temp = temp -> next;
	while(temp)
	{
		if(next->quantum > temp->value->quantum && temp->value->status == READY)
			next = temp -> value;
		temp = temp -> next;
	}
	return next;
}
/* scheduler */
static void schedule()
{
	#ifndef MLFQ
		dequeueDestroyed(head);
		sched_psjf();
	#else 
		dequeueDestroyedMLFQ();
		sched_mlfq();
	#endif
}

/* Pre-emptive Shortest Job First (POLICY_PSJF) scheduling algorithm */
static void sched_psjf() 
{		
	if(head)
	{
		current -> quantum++;
		TCB* prev = current;
		if(prev->status == RUNNING)
			prev->status = READY;
		current = findNextJobPSJF();		
		if(current)
		{			
			current -> status = RUNNING;			
			tot_cntx_switches++;
			if(current->firstScheduled == -1)
				current -> firstScheduled = clock();
			initTimer(QUANTUM);
			swapcontext(&prev->ctx,&current->ctx);
		}
		else	
			current = prev;
		
	}	
}
void priorityBoost()
{
	int i = 1;
	if(!multiLevelQueues[0])	
		for(i=1;i<NUM_OF_MLFQ;i++)
			if(multiLevelQueues[i])
			{
				multiLevelQueues[0] = multiLevelQueues[i];	
				multiLevelQueues[i] = NULL;
				i++;
				break;
			}
	if(i == NUM_OF_MLFQ)
		return;
	for(i; i < NUM_OF_MLFQ; i++)
	{
		if(multiLevelQueues[i])
		{
			rqueue* temp = multiLevelQueues[0];
			while(temp->next)
				temp = temp -> next;
			temp -> next = multiLevelQueues[i];
			multiLevelQueues[i] = NULL;
		}	
	}
	prBoostCounter = 0;
}
void removeFromQueue(rqueue* remove,int currentIndex)
{
	if(remove == multiLevelQueues[currentIndex])
	{
		rqueue* temp = multiLevelQueues[currentIndex] -> next;
		multiLevelQueues[currentIndex] -> next = NULL;
		multiLevelQueues[currentIndex] = temp;
	}
	else
	{
		rqueue* temp = multiLevelQueues[currentIndex];
		while(temp->next && temp->next->value->threadId != remove->value->threadId)
			temp = temp -> next;
		temp ->next = remove -> next;
		remove -> next = NULL;
	}
}

void reducePriority()
{
	int currentIndex;	
	rqueue* prev = findInMLFQ(current->threadId,&currentIndex);
	if(!prev)
		return;
	if(currentIndex == -1)											//all jobs terminated
	{
		current = NULL;
		return;
	}
	else if(currentIndex == NUM_OF_MLFQ - 1)					//Already in lowest queue, move to tail
	{		
		rqueue* oldHead = multiLevelQueues[currentIndex];	
		if(multiLevelQueues[currentIndex] == prev)	
			if(multiLevelQueues[currentIndex]->next)
				multiLevelQueues[currentIndex] = multiLevelQueues[currentIndex] -> next;	
			else 
				return;
		else
		{
			rqueue* temp = oldHead;
			while(temp->next && temp->next->value->threadId != prev->value->threadId)
				temp = temp -> next;
			temp -> next = prev->next;
			prev->next = NULL;
		}
		rqueue* last = oldHead;
		while(last -> next)
			last = last -> next;
		last -> next = prev;		
		prev -> next = NULL;	
		return;
	}
	removeFromQueue(prev,currentIndex);
	enqueue(&multiLevelQueues[currentIndex+1],prev);
	return;	
}
TCB* findNextJobMLFQ(int* index)
{
	for(int i = 0; i < NUM_OF_MLFQ; i++)
		if(multiLevelQueues[i])
		{
			if(multiLevelQueues[i] ->value -> status == READY)
			{
				*index = i;
				return multiLevelQueues[i] -> value;
			}
			else
			{
				rqueue* temp = multiLevelQueues[i];
				while(temp && temp->value->status != READY)
					temp = temp -> next;
				if(!temp)
					continue;
				*index = i;
				return temp -> value;
			}
		}			
	*index = -1;
	return NULL;
}
/* Preemptive MLFQ scheduling algorithm */
static void sched_mlfq() 
{
	current -> quantum++;
	TCB* prev = current;
	if(prBoostCounter >= PR_BOOST_QUANTUM)
		priorityBoost();
	else
		reducePriority();	
	if(prev->status == RUNNING)
		prev->status = READY;
	int index;
	current = findNextJobMLFQ(&index);	
	if(current)
	{
		current -> status = RUNNING;
		prBoostCounter += QUANTUM_MLFQ+index*QUANTUM_MLFQ;
		initTimer(QUANTUM_MLFQ+index*QUANTUM_MLFQ);
		tot_cntx_switches++;
		if(current->firstScheduled == -1)
			current -> firstScheduled = clock();
		swapcontext(&prev->ctx,&current->ctx);
	}
	else
		current = prev;
}

//DO NOT MODIFY THIS FUNCTION
/* Function to print global statistics. Do not modify this function.*/
void print_app_stats(void) {

       fprintf(stderr, "Total context switches %ld \n", tot_cntx_switches);
       fprintf(stderr, "Average turnaround time %lf \n", avg_turn_time);
       fprintf(stderr, "Average response time  %lf \n", avg_resp_time);
}
// YOUR CODE HERE
