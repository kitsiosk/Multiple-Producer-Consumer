#include <pthread.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/time.h>
#include <time.h>
#include <math.h>

#define QUEUESIZE 100
#define LOOP 1000
#define P 1
#define Q 4
#define PI 3.14159265
#define FUNCTIONSREPS 10

void *producer (void *args);
void *consumer (void *args);

typedef struct {
	void * (*work)(void *);
	void * arg;
} workFunc;

typedef struct {
  workFunc buf[QUEUESIZE];
  long head, tail;
  int full, empty;
  pthread_mutex_t *mut;
  pthread_cond_t *notFull, *notEmpty;
} queue;

// Argument struct for the pthread_create() function.
// Besides the queue attribute of the single producer-single consumer
// it must also contain the thread id
typedef struct{
	queue *q;
	int tid;
} threadArg;

// Functions to be used in the fifo
// 1) 	threadPrint(): print the thread ID.
// 2)   calculateSin(): computes the sin() of 10(FUNCTIONSREPS) consecutive  
//      values in degrees, with the base value being the thread id, so
//		  different threads compute different values
typedef struct { 
	int tid;
	time_t start;
} threadFuncArg;

void * threadPrint(void *arg){
	threadFuncArg* a = (threadFuncArg *) arg;
	printf("Hello from thread #%d\n", a->tid);
}
void * calculateSin(void *arg){
	threadFuncArg* a = (threadFuncArg *) arg;
	int tid = a->tid;
	double temp;
	for(int i=0; i<FUNCTIONSREPS; ++i)
		temp = sin( (tid + i)*PI/10 );
}

queue *queueInit (void);
void queueDelete (queue *q);
void queueAdd (queue *q, workFunc in);
void queueDel (queue *q, workFunc *out);

// Define global variable to signal termination of producers
int PRODUCERS_TERMINATED = 0;

// Mutex to detect when producers stop
pthread_mutex_t *prodCountMut;
int finishedProducers = 0;

// Mutex to update global variable TOTAL_TIME_G from each consumer thread
pthread_mutex_t *timeMut;
double TOTAL_TIME_G = 0;

int main ()
{
  queue *fifo;
  pthread_t *pro, *con;
  threadArg *proArgs, *conArgs;

  pro = (pthread_t *) malloc( P * sizeof(pthread_t) );
  con = (pthread_t *) malloc( Q * sizeof(pthread_t) );
  proArgs = (threadArg *) malloc( P * sizeof(threadArg) );
  conArgs = (threadArg *) malloc( Q * sizeof(threadArg) );
  
  // Mutex initializations
  prodCountMut = (pthread_mutex_t *) malloc (sizeof (pthread_mutex_t));
  pthread_mutex_init(prodCountMut, NULL);
  timeMut = (pthread_mutex_t *) malloc (sizeof(pthread_mutex_t));
  pthread_mutex_init(timeMut, NULL);
  
  // // Array that holds elapsed time at each consumer thread
  // // It will be passed by value to the thread.
  // double timeElapsed[Q];
  // for(int i=0; i<Q; ++i) timeElapsed[i] = 0;

  fifo = queueInit ();
  if (fifo ==  NULL) {
    fprintf (stderr, "main: Queue Init failed.\n");
    exit (1);
  }
  
  // Span consumers
  for(int i=0; i<Q; ++i){
	  conArgs[i].tid = i;
	  conArgs[i].q = fifo;
	  pthread_create (&con[i], NULL, consumer, (void *)(conArgs + i));
  }

  // Span producers
  for(int i=0; i<P; ++i){
	  proArgs[i].tid = i;
	  proArgs[i].q = fifo;
	  pthread_create (&pro[i], NULL, producer, (void *)(proArgs + i));
  }
  
  // Terminate producers
  for(int i=0; i<P; ++i)
	  pthread_join (pro[i], NULL);
	  
  // Terminate consumers
  for(int i=0; i<Q; ++i)
	  pthread_join (con[i], NULL);

		
  printf("Overall average time elapsed: %lf\n", TOTAL_TIME_G/(P*LOOP));
  
  queueDelete (fifo);
  pthread_mutex_destroy(prodCountMut);
  pthread_mutex_destroy(timeMut);
  free(prodCountMut);
  free(timeMut);

  return 0;
}

void *producer (void *arg)
{
  queue *fifo;
  int i, tid;
  
  threadArg *proArg;
  proArg = (threadArg *) arg;
  fifo = proArg->q;
  tid = proArg->tid;

  for (i = 0; i < LOOP; i++) {
    pthread_mutex_lock (fifo->mut);
    while (fifo->full) {
      //printf ("producer: queue FULL.\n");
      pthread_cond_wait (fifo->notFull, fifo->mut);
    }
    
    // Create fifo item to insert to queue
    workFunc item;
    item.work = &calculateSin;
    
    threadFuncArg *a = (threadFuncArg *) malloc( sizeof(threadFuncArg) );
    a->tid = tid;
    struct timeval tv;
    gettimeofday(&tv, NULL); 
    a->start=tv.tv_usec;
    item.arg = (void *) a;
    
    queueAdd (fifo, item);
    pthread_mutex_unlock (fifo->mut);
    pthread_cond_signal (fifo->notEmpty);
  }
  
  pthread_mutex_lock(prodCountMut);
  finishedProducers += 1;
  if(finishedProducers == P){
	  PRODUCERS_TERMINATED = 1;
	  // In case all producers terminated but consumer is stuck in cond_wait
	  pthread_cond_broadcast(fifo->notEmpty);
  }
  //printf("Terminated producer #%d. Totally %d terminated\n", tid, finishedProducers);
  pthread_mutex_unlock(prodCountMut);

  return (NULL);
}

void *consumer (void *arg)
{
  queue *fifo;
  int i, tid;
  workFunc d;
  
  threadArg *conArg;
  conArg = (threadArg *) arg;
  fifo = conArg->q;
  tid = conArg->tid;
  
  // Total time of each consumer thread. It will be added up finally
  double total_time = 0;

  while(1) {
    pthread_mutex_lock (fifo->mut);
    //printf("Waking thread #%d\n", tid);
    while (fifo->empty && !PRODUCERS_TERMINATED) {
      //printf ("consumer: queue EMPTY from thread #%d and %d.\n", tid, PRODUCERS_TERMINATED);
      pthread_cond_wait (fifo->notEmpty, fifo->mut);
      //printf("Continuing from thread #%d\n", tid);
    }
    
    if( fifo->empty && PRODUCERS_TERMINATED ){
      pthread_mutex_unlock(fifo->mut);
      pthread_cond_broadcast(fifo->notEmpty);
      break;
	  }

    queueDel (fifo, &d);
    pthread_mutex_unlock (fifo->mut);
    pthread_cond_signal (fifo->notFull);
    
    // Measure time
    threadFuncArg *arg = (threadFuncArg *) d.arg;
    time_t start = arg->start;
    //start = d.start;
    time_t end;
    double elapsedTime;
    struct timeval tv;
    gettimeofday(&tv, NULL); 
    end=tv.tv_usec;
    elapsedTime = (end - start);
    total_time += elapsedTime;
    
    // Do the actual work
    (*d.work)(d.arg);
    
    if( fifo->empty && PRODUCERS_TERMINATED ) break;

  }
  
  pthread_mutex_lock(timeMut);
  TOTAL_TIME_G += total_time;
  pthread_mutex_unlock(timeMut);
  //printf("Total time elapsed at consumer #%d is %lf\n", tid, total_time);
  //printf("%d %d\n", fifo->empty, PRODUCERS_TERMINATED);
  
  // To avoid getting stuck in while( fifo->notEmpty ) loop
  pthread_cond_signal (fifo->notEmpty);
  return (NULL);
}

queue *queueInit (void)
{
  queue *q;

  q = (queue *)malloc (sizeof (queue));
  if (q == NULL) return (NULL);

  q->empty = 1;
  q->full = 0;
  q->head = 0;
  q->tail = 0;
  q->mut = (pthread_mutex_t *) malloc (sizeof (pthread_mutex_t));
  pthread_mutex_init (q->mut, NULL);
  q->notFull = (pthread_cond_t *) malloc (sizeof (pthread_cond_t));
  pthread_cond_init (q->notFull, NULL);
  q->notEmpty = (pthread_cond_t *) malloc (sizeof (pthread_cond_t));
  pthread_cond_init (q->notEmpty, NULL);
	
  return (q);
}

void queueDelete (queue *q)
{
  pthread_mutex_destroy (q->mut);
  free (q->mut);	
  pthread_cond_destroy (q->notFull);
  free (q->notFull);
  pthread_cond_destroy (q->notEmpty);
  free (q->notEmpty);
  free (q);
}

void queueAdd (queue *q, workFunc in)
{
  q->buf[q->tail] = in;
  q->tail++;
  if (q->tail == QUEUESIZE)
    q->tail = 0;
  if (q->tail == q->head)
    q->full = 1;
  q->empty = 0;

  return;
}

void queueDel (queue *q, workFunc *out)
{
  *out = q->buf[q->head];

  q->head++;
  if (q->head == QUEUESIZE)
    q->head = 0;
  if (q->head == q->tail)
    q->empty = 1;
  q->full = 0;

  return;
}
