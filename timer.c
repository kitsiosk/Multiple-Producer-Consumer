#include <pthread.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/time.h>
#include <time.h>
#include <math.h>
#include <stdint.h>

#define QUEUESIZE 4
#define LOOP 1000
#define P 1
#define Q 4
#define PI 3.14159265
#define FUNCTIONREPS 10
#define TIMERREPS 3000
#define PERIOD1 1e5 // in usec
// #define PERIOD2 1e5
// #define PERIOD3 1e4
// // Number of timers( must be equal to P )
// #define NTIMERS 1

void *producer (void *args);
void *consumer (void *args);

typedef struct{
    int Period;
    int TasksToExecute;
    int StartDelay;
    void * (*StartFnc) (void *);
    void * (*StopFnc)  (void *);
    void * (*TimerFnc) (void *);
    void * (*ErrorFnc) (void *);
    void *UserData;
} Timer;

// Array that keeps track of time between prod/cons activations
typedef struct{
  double buff[P*TIMERREPS];
  int nextFree;
} array;

typedef struct {
	void * (*work)(void *);
	void * arg;
} workFunc;

typedef struct {
  workFunc *buf;
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
  Timer * t;
} threadArg;

// Functions to be used in the fifo
// 1) 	threadPrint(): print the thread ID.
// 2)   calculateSin(): computes the sin() of 10(FUNCTIONREPS) consecutive  
//      values in degrees, with the base value being the thread id, so
//		  different threads compute different values
typedef struct { 
	int tid;
	double start;
} threadFuncArg;

void * threadPrint(void *arg){
	threadFuncArg* a = (threadFuncArg *) arg;
	printf("Hello from thread #%d\n", a->tid);
}
void * calculateSin(void *arg){
	threadFuncArg* a = (threadFuncArg *) arg;
	int tid = a->tid;
	double temp;
	for(int i=0; i<FUNCTIONREPS; ++i)
		temp = sin( (tid + i)*PI/10 );
}

queue *queueInit (int n);
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
double TOTAL_DRIFT_G = 0;
// Mutexes to update time arrays
pthread_mutex_t *prodArrayMut;
pthread_mutex_t *conArrayMut;
// Declare global time arrays
array *prodArray;
array *conArray;

// //// Part 2
void * testPrint(void * arg){
    printf("Hello from TimerFnc\n");
}

Timer *timerInit();
void timerDelete(Timer *t);

Timer *timerInit(int Period, int TasksToExecute, int StartDelay){
    printf("Initializing Timer\n");
    Timer *t = (Timer *) malloc( sizeof(Timer) );
    t->Period = Period;
    t->TasksToExecute = TasksToExecute;
    t->StartDelay = StartDelay;
    t->TimerFnc = &testPrint;
    return t;
}

void timerDelete(Timer *t){
  free( t->StartFnc );
  free( t->TimerFnc );
  free( t->StopFnc );
  free( t->ErrorFnc );
  free( t );
}

void start(Timer *t){
    t->TimerFnc(NULL);
}

array * arrayInit(){
  array *x = (array *) malloc( sizeof(array) );
  if( x==NULL ) printf("Error allocating array\n");
  x->nextFree = 0;
  return x;
}

void arrayAdd(array *x, double a){
  if( x->nextFree > P*TIMERREPS ){
    printf("ERROR: Added to full array\n");
    exit(1);
  }
  x->buff[x->nextFree] = a;
  x->nextFree += 1;
}

void arrayPrint(array *x, int size){
  for(int i=0; i<size; ++i) printf("%0.0f ", x->buff[i]);
  printf("\n");
}
// ////
int main(){
    printf("Starting Main\n");

    // int periods[3] = {1e6, 1e5, 1e4};
    // int timerreps[3] = {120, 1200, 12000};
    //Initialize queue
    queue *fifo;
    fifo = queueInit (QUEUESIZE);
    if (fifo ==  NULL) {
      fprintf (stderr, "main: Queue Init failed.\n");
      exit (1);
    }

    // Initialize time arrays
    prodArray = arrayInit();
    conArray = arrayInit();
  
    // Allocate memory for prod/cons threads, arguements for threads
    // and timers
    pthread_t *pro, *con;
    pro = (pthread_t *) malloc( P * sizeof(pthread_t) );
    con = (pthread_t *) malloc( Q * sizeof(pthread_t) );
    threadArg *proArgs, *conArgs;
    proArgs = (threadArg *) malloc( P * sizeof(threadArg) );
    conArgs = (threadArg *) malloc( Q * sizeof(threadArg) );
    Timer *t;
    t = (Timer *) malloc( P * sizeof(Timer) );

    // Mutex initializations
    prodCountMut = (pthread_mutex_t *) malloc(sizeof(pthread_mutex_t));
    timeMut      = (pthread_mutex_t *) malloc(sizeof(pthread_mutex_t));
    prodArrayMut = (pthread_mutex_t *) malloc(sizeof(pthread_mutex_t));
    conArrayMut  = (pthread_mutex_t *) malloc(sizeof(pthread_mutex_t));
    pthread_mutex_init(prodCountMut, NULL);
    pthread_mutex_init(timeMut,      NULL);
    pthread_mutex_init(prodArrayMut, NULL);
    pthread_mutex_init(conArrayMut,  NULL);

    // Span consumers
    for(int i=0; i<Q; ++i){
      conArgs[i].tid = i;
      conArgs[i].q = fifo;
      pthread_create (&con[i], NULL, consumer, (void *)(conArgs + i));
    }

    // Span producers
    for(int i=0; i<P; ++i){
        t[i] = *timerInit(PERIOD1, TIMERREPS, 0);
        proArgs[i].tid = i;
        proArgs[i].q = fifo;
        proArgs[i].t = (t+i);
        pthread_create (&pro[i], NULL, producer, (void *)(proArgs + i));
    }

    // Terminate producers
    for(int i=0; i<P; ++i)
      pthread_join (pro[i], NULL);

    // Terminate consumers
    for(int i=0; i<Q; ++i)
      pthread_join (con[i], NULL);

    // printf("Overall average time elapsed: %lf\n", TOTAL_TIME_G/(P*4));
    // arrayPrint(prodArray, P*TIMERREPS);
    // arrayPrint(conArray,  P*TIMERREPS );

    FILE *f1 = fopen("prodArrTest.txt", "w");
    for(int i=0; i<P*TIMERREPS; ++i){
      fprintf(f1, "%0.0f,", prodArray->buff[i]);
    }
    fclose(f1);
    FILE *f2 = fopen("conArrTest.txt", "w");
    for(int i=0; i<P*TIMERREPS; ++i){
      fprintf(f2, "%0.0f,", conArray->buff[i]);
    }
    fclose(f2);
    // Clean up
    queueDelete (fifo);
    pthread_mutex_destroy(prodCountMut);
    pthread_mutex_destroy(timeMut);
    free(prodCountMut);
    free(timeMut);
    free( t );

    return 0;
}

void *producer (void *arg)
{
  queue *fifo;
  int i, tid;
  Timer *t;
  double total_drift = 0;
  double currentTime;

  struct timeval tv;
  // Time elapsed since last insert to queue
  double last;
  // Get current time
  gettimeofday( &tv, NULL );
  last = 1e6*tv.tv_sec + tv.tv_usec;
  currentTime =  last;
  
  threadArg *proArg;
  proArg = (threadArg *) arg;
  fifo = proArg->q;
  tid = proArg->tid;
  t = proArg->t;

  for (i = 0; i < t->TasksToExecute; ++i) {
    // if( i % 100 == 0 ) printf("%d/%d\n", i, t->TasksToExecute);
    pthread_mutex_lock (fifo->mut);
    while (fifo->full) {
      // printf ("producer: queue FULL.\n");
      pthread_cond_wait (fifo->notFull, fifo->mut);
    }
    
    // Update the time when the last element was added to the queue
    // struct timeval tv;
    gettimeofday(&tv, NULL); 
    last = 1e6*tv.tv_sec + tv.tv_usec;

    
    // Create fifo item to insert to queue
    workFunc item;
    item.work = &calculateSin;
    threadFuncArg *a = (threadFuncArg *) malloc( sizeof(threadFuncArg) );
    a->tid = tid;
    // Get current time
    gettimeofday( &tv, NULL );
    // Pass current time to queue item
    a->start = 1e6*tv.tv_sec + tv.tv_usec;
    item.arg = (void *) a;
    // printf("%d\n", a->start);

    queueAdd (fifo, item);
    // printf("Current Queue size: %d\n", fifo->tail - fifo->head);
    pthread_mutex_unlock (fifo->mut);
    pthread_cond_signal (fifo->notEmpty);

    // After unlocking mutex, go to sleep
    // Calculate sleep time from current time, last time and Period
    double drift = last - currentTime;

    arrayAdd(prodArray, drift);
    double sleepTime = t->Period - drift;
    if( sleepTime > 0 ){
      // printf("Sleeping for %ld usec\n", sleepTime);
      usleep(sleepTime);
    }else{
      printf("Not sleeping\n");
    }
    struct timeval tv;
    gettimeofday(&tv, NULL); 
    currentTime = 1e6*tv.tv_sec + tv.tv_usec;
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
      // printf ("consumer: queue EMPTY from thread #%d and %d.\n", tid, PRODUCERS_TERMINATED);
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
    double start = arg->start;
    double end;
    double elapsedTime;
    struct timeval tv;
    gettimeofday(&tv, NULL); 
    end = 1e6*tv.tv_sec + tv.tv_usec;
    elapsedTime = (end - start);
    pthread_mutex_lock(timeMut);
    arrayAdd(conArray, elapsedTime);
    pthread_mutex_unlock(timeMut);
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

queue *queueInit (int n)
{
  queue *q;

  q = (queue *)malloc (sizeof (queue));
  if (q == NULL) return (NULL);

  q->buf = (workFunc *) malloc(n * sizeof(workFunc));

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
