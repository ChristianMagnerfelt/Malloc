/*
 * DESCRIPTION:
 *  A test suite for evaluating the performance of our custom malloc
 *  versus the system malloc.
 *
 * AUTHOR:
 *	Daniel Molin <dmol@kth.se>
 *	Christian Magnerfelt <christian.magnerfelt@gmail.com>
 */

#ifdef STRATEGY
#include "malloc.c"
#else
#include <stdlib.h>
#endif

#include <stdbool.h>
#include <stdio.h>

#include <sys/time.h>
#include <sys/resource.h>

#define TIMES 1  /* How many times to do each thing (for timing) */
#define MAX(a,b) ((a > b) ? (a) : (b))

/* Get current memory usage */
int getCurrMemUsage(void);

/* Get current end address of heap */
void * getEndHeap(void);

/* Gets the current timestamp in milliseconds */
long getCurrentTimeMillis();

char *progname;
int sizes[30] = {15, 28, 17, 19, 22, 12, 26, 5, 18, 7, 29, 16, 6, 14,\
                 21, 20, 13, 8, 11, 24, 27, 1, 9, 4, 10, 23, 30, 3, 25, 2}; 

int main(int argc, char *argv[])
{
  if (argc > 0)
    progname = argv[0];
  else
    progname = "";

  #ifdef STRATEGY
  fprintf(stderr, "Evaluating custom malloc.\n");  
  fprintf(stderr, "Strategy: %d\n", STRATEGY);
  #else
  fprintf(stderr, "Evaluating system (stdlib) malloc.\n");  
  #endif

  if(fork() == 0){
    evalSmallPieces();
    return 0;
  }
  wait(NULL);
  if(fork() == 0){
    evalTypicalUse();
    return 0;
  }
  wait(NULL);
  if(fork() == 0){
    evalFragmentedList();
    return 0;
  }
  wait(NULL);
  if(fork() == 0){
    evalBadBestFit();
    return 0;
  }
  wait(NULL);

  return 0;
}

/**
 * Allocates a lot of small pieces of memory.
 */
void evalSmallPieces(){
  void * startMemory, *endMemory;
  int t, i;
  int N = 9000;
  void * addr[N];
  long timeMillis = 0;
  
  for(t = 0; t < TIMES; t++){
    if(t == 0){
      startMemory = getEndHeap();
    }

    long  tmpTime = getCurrentTimeMillis();
    for(i = 0; i < N; i++){
      addr[i] = malloc(1024);
    }
    long timed = (getCurrentTimeMillis()-tmpTime);

    /*fprintf(stderr, "Time to malloc: %li\n", timed);*/
    timeMillis += timed;
    
    if(t == 0){
      endMemory  = getEndHeap();
    }

    /*
    for(i = 0; i < N; i++){
      free(addr[i]);
    } */
  }

  timeMillis = timeMillis/TIMES;
  
  printEvalResults("evalSmallPieces", startMemory, endMemory, timeMillis);
}

/*
  Allocate memory of sizes between 1 and 30 KB (simulating typical use)
*/
void evalTypicalUse(){
  void * startMemory, *endMemory;
  int t, i;
  int N = 9000;
  void * addr[N];
  long timeMillis = 0;

  for(t = 0; t < TIMES; t++){
    if(t == 0){
      startMemory = getEndHeap();
    }

    long tmpTime = getCurrentTimeMillis();
    for(i = 0; i < N; i++){
      addr[i] = malloc(sizes[i%30]*1024);
    }
    long timed = (getCurrentTimeMillis()-tmpTime);

    /*fprintf(stderr, "Time to malloc: %li\n", timed);*/
    timeMillis += timed;
    
    if(t == 0){
      endMemory  = getEndHeap();
    }

    for(i = 0; i < N; i++){
      free(addr[i]);
    } 
  }

  timeMillis = timeMillis/TIMES;
  
  printEvalResults("evalTypicalUse", startMemory, endMemory, timeMillis);
}

/**
  Allocate memory, free some, then allocate some more to evaluate how the
  algorithms cope with a fragmented list with a lot of free memory.
*/
void evalFragmentedList(){
  void * startMemory, *endMemory;
  int i;
  int N = 9000;
  void * addr[N];
  long timeMillis = 0;

  startMemory = getEndHeap();

  /* Allocate all 9000 blocks of sizes 1-30 kB */
  long tmpTime = getCurrentTimeMillis();
  for(i = 0; i < N; i++){
    addr[i] = malloc(sizes[i%30]*1024);
  }
  long timed = (getCurrentTimeMillis()-tmpTime);
  timeMillis += timed;

  /* Free 1000 blocks (every 9th) */
  for(i = 0; i < 1000; i++){
    free(addr[i*9]);
  }

  /* Allocate 1000 blocks again */
  tmpTime = getCurrentTimeMillis();
  for(i = 0; i < 1000; i++){
    addr[i*9] = malloc(sizes[i%30]*1024);
  }
  timeMillis += (getCurrentTimeMillis()-tmpTime);

  endMemory = getEndHeap();

  printEvalResults("evalFragmentedList", startMemory, endMemory, timeMillis);
}

/**
  Allocate a lot of memory, free everything and allocate it again; best fit
  should give worse performance than first, because first will find a free
  block right away while best fit will search the whole list every time.
*/
void evalBadBestFit(){
  void * startMemory, *endMemory;
  int i;
  int N = 9000;
  void * addr[N];
  long timeMillis = 0;

  startMemory = getEndHeap();

  /* Allocate all N blocks of sizes 1 kB */
  long tmpTime = getCurrentTimeMillis();
  for(i = 0; i < N; i++){
    addr[i] = malloc(1024);
  }
  long timed = (getCurrentTimeMillis()-tmpTime);
  timeMillis += timed;

  /* Free N blocks */
  for(i = 0; i < N; i++){
    free(addr[i]);
  }

  /* Allocate N blocks again */
  tmpTime = getCurrentTimeMillis();
  for(i = 0; i < N; i++){
    addr[i] = malloc(1024);
  }
  timeMillis += (getCurrentTimeMillis()-tmpTime);

  endMemory = getEndHeap();

  printEvalResults("evalBadBestFit", startMemory, endMemory, timeMillis);
}
/**
 * Returns the program size (virtual memory) in kB
 */
int getCurrMemUsage(){
  FILE *statm = fopen("/proc/self/statm", "r");
	int size;
	int res = fscanf(statm, "%d", &size);
  fclose(statm);
	return (res == 0 ? -1 : size);
}

/**
 * Returns the current heap end position.
 */
void * getEndHeap(){
	#ifdef STRATEGY
  /* We're testing custom malloc, use endHeap */
  return endHeap();
  #else
  /* We're testing built-in malloc, use sbrk */
  return (void *) sbrk(0);
  #endif
}

/**
  * Prints the results of an evaluation method like this:
  *     EVALUTION_METHOD_NAME   MEMORY_USED(KB)   TIME(MS)
  */
void printEvalResults(char * evalType, void * startMemory, void *endMemory, long ms){
  fprintf(stderr, "%s\t%d\t%li\n", evalType,
                                    (endMemory-startMemory)/getpagesize(),
                                    ms);
}

/**
* Returns the current time in milliseconds (based on the time since the Epoch).
*/
long getCurrentTimeMillis()
{
  struct timeval tv;
  gettimeofday(&tv, NULL);

  return 1000*(tv.tv_sec % 10000)+ (tv.tv_usec/1000);
}