/*
 * DESCRIPTION:
 *  A test suite for evaluating the performance of our custom malloc
 *  versus the system malloc.
 *
 * AUTHOR:
 *	Daniel Molin <dmol@kth.se>
 *	Christian Magnerfelt <christian.magnerfelt@gmail.com>
 */

#ifdef CUSTOM
#include "malloc.c"
#else
#include <stdlib.h>
#endif

#include <stdbool.h>
#include <stdio.h>
#include "tst.h"

/* Used for tstmemory */
#define SIZE 128
#define BIGSIZE 100
#define TIMES 100
#define BIGTIMES 10
#define SMALLSTRING 64
#define TIMESPAGE 10

#define MAX(a,b) ((a > b) ? (a) : (b))

char *progname;

	

int main(int argc, char *argv[])
{
  if (argc > 0)
    progname = argv[0];
  else
    progname = "";

	#ifdef CUSTOM
	fprintf(stderr, "Evaluating custom malloc.\n");	
	fprintf(stderr, "Strategy: %d\n", STRATEGY);
	#else
	fprintf(stderr, "Evaluating system (stdlib) malloc.\n");	
	#endif

/*	tstmemory(argc, argv);*/

	char *ptr;
	ptr = malloc(2000);
	ptr = "heafasfasfasfs";
	MESSAGE(ptr);
	
	return 0;
}

#ifdef MMAP
#ifndef CUSTOM
static void * __endHeap = 0;

void * endHeap(void)
{
  if(__endHeap == 0) 
  {
  	__endHeap = sbrk(0);
  }
  return __endHeap;
}
#endif
#endif

void tstmemory(int argc, char *argv[])
{
  int i,j;
  float worst;
  char *a[SIZE], *b[BIGSIZE];
  size_t pagesize;
  void * lowbreak, *highbreak, *maxbreak = 0;
  
  /* to generate big memory refs later */
  pagesize = sysconf(_SC_PAGESIZE);

  MESSAGE("Testing memory utility\n");

#ifdef MMAP
  lowbreak = endHeap();
#else
  lowbreak = (void *) sbrk(0);
#endif

  MESSAGE("Getting small pieces of memory\n");
  for(i = 0; i < TIMES; i++){
    for(j = 0; j < SIZE; j++){
      a[j] = malloc(SMALLSTRING);
    } 
#ifdef MMAP
    highbreak = endHeap();
#else
    highbreak = (void *) sbrk(0);
#endif
    maxbreak = MAX(maxbreak, highbreak);
    for(j = 0; j < SIZE; j++){
      free(a[j]);
    }
    if ( i % 10 == 0 ) 
      fprintf(stderr, "%s: Using total of 0x%x of memory\n",
	      progname, (unsigned) (highbreak - lowbreak)); 

  }

  worst = (maxbreak - lowbreak)/ (SMALLSTRING * SIZE * 1.0);
  fprintf(stderr, "%s: Using %2.2f times worst case calculation\n", 
	  progname, worst);
  if ( worst > 2 ) {
    MESSAGE("* ERROR: Test indicates excessive memory usage\n");
  } else {
    MESSAGE("Small memory handled OK\n");
  }

  MESSAGE("Getting big blocks of memory\n");
  for(i = 0; i < BIGTIMES; i++){
    for(j = 0; j < BIGSIZE; j++){
      b[j] = malloc(TIMESPAGE*pagesize);
    }
#ifdef MMAP
    highbreak = endHeap();
#else
    highbreak = (void *) sbrk(0);
#endif

    maxbreak = MAX(maxbreak, highbreak);
    for(j = 0; j < BIGSIZE; j++){
      free(b[j]);
    }
    fprintf(stderr, "%s: Using total of 0x%x of memory\n", 
	    progname, (unsigned) (highbreak - lowbreak));

  }
  MESSAGE("Allocations versus worst case memory usage:\n");
  worst = (maxbreak - lowbreak)/ (BIGSIZE * pagesize * TIMESPAGE * 1.0);
  fprintf(stderr, 
	  "%s: Using %2.2f times worst case calculation\n", 
	  progname, worst);
  if ( worst > 2 ) {
    MESSAGE("* ERROR: Test indicates excessive memory usage\n");
  } else {
    MESSAGE("Big memory handled OK\n");
  }

}
