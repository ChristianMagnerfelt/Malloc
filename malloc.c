#define _GNU_SOURCE


#include "brk.h"
#include <unistd.h>

#include <string.h> 
#include <stdio.h>

#include <errno.h> 
#include <sys/mman.h>

#define NALLOC 1024		/* Minimum #units to request */

typedef long Align;		/* For alignment to long boundary */

/* block header */
union header
{
	struct
	{
		union header *ptr;		/* Next block if on free list */
    	unsigned size;			/* Size of this block  - what unit? */ 
	} s;
	Align x;			/* Force alignment of blocks */
};

typedef union header Header;

static Header base;				/* Empty list to get started */
static Header *freep = NULL;	/* Start of free list */

/* Number of sepparate free lists, each list increases its 
 * block size by the power of two */
#define QF_NUM_LISTS 6
/* The minimum size is two units where each unit is equal to the sizeof(Header)	*/
int qfMinBlockSize = 2;						
int qfBlockSizes[QF_NUM_LISTS];			/* List of the block size of each free list */
static Header qfBase[QF_NUM_LISTS + 1];		/* Empty lists to get started */
static Header * qfFreep[QF_NUM_LISTS + 1];	/* The start of the free lists */

int min (int a, int b)
{
	if(a < b) 
		return a;
	else
		return b;
}
/* free: Put block ap in the free list */
void free(void * ap)
{
	Header *bp, *p;

	if(ap == NULL) return;		/* Nothing to do */

	bp = (Header *) ap - 1;		/* Point to block header */
	
	for(p = freep; !(bp > p && bp < p->s.ptr); p = p->s.ptr)
	{
		if(p >= p->s.ptr && (bp > p || bp < p->s.ptr))
		{
	  		break;		/* Freed block at atrt or end of arena */
		}
	}
	if(bp + bp->s.size == p->s.ptr)
	{                     /* join to upper nb */
		bp->s.size += p->s.ptr->s.size;
		bp->s.ptr = p->s.ptr->s.ptr;
	}
	else
	{
		bp->s.ptr = p->s.ptr;
	}
    /* Join to lower nbr */	
	if(p + p->s.size == bp)
	  {
	    p->s.size += bp->s.size;
	    p->s.ptr = bp->s.ptr;
	  }
	else
	  {
	    p->s.ptr = bp;
	  }
	freep = p;
}

/* morecore: ask system for more memory */

#ifdef MMAP

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

static Header * morecore(unsigned int numUnits)
{
	void *cp;
	Header *up;
	char * newEnd = NULL;
	
	#ifdef MMAP
		unsigned int numPages;
		if(__endHeap == 0)
		{
			__endHeap = sbrk(0);
		}
	#endif

	if(numUnits < NALLOC)
	{
		numUnits = NALLOC;
	}
	
	#ifdef MMAP
		/* Number of pages = (number of units * header size - 1) / Size of page + 1) */
		numPages = ((numUnits * sizeof(Header)) - 1) / getpagesize() + 1;
		
		/* Create a memory mapping starting that the end of the heap (if possible) 
		 * with size equal to the number of pages * the page size 
		 */
		cp = mmap(__endHeap, 
				numPages * getpagesize(), 
				PROT_READ | PROT_WRITE, 
				MAP_SHARED | MAP_ANONYMOUS, 
				-1, 0);
				
		numUnits = (numPages * getpagesize()) / sizeof(Header);
		newEnd = (char *)__endHeap;
		newEnd += numPages * getpagesize();
		__endHeap = (void *) newEnd;
	#else
		cp = sbrk(numUnits * sizeof(Header));
	#endif
	
	/* no space at all */
	if(cp == (void *) -1)
	{                                 
		perror("failed to get more memory");
		return NULL;
	}
	/* Set page size in the first header of the newly allocated block */
	up = (Header *) cp;
	up->s.size = numUnits;
	free((void *)(up + 1));
	return freep;
}
void * firstFitMalloc(size_t nbytes)
{
	Header *p, *prevp;
	Header * morecore(unsigned);
	unsigned nunits;

	/* Calculate the number of units in ( Headers ) required 
	 * to store the given amount of nbytes data */
	nunits = (nbytes + sizeof(Header) - 1) / sizeof(Header) + 1;

	/* Set previous pointer to point backwards next free block */
	prevp = freep;
	/* Nothing have yet been allocated, set everything to point to first element in list and size 
	 * to 0 */	
	if(prevp == NULL) 
	{
	  base.s.ptr = freep = prevp = &base;
	  base.s.size = 0;
	}
		
	for(p= prevp->s.ptr;  ; prevp = p, p = p->s.ptr)
	{
		/* big enough */	
		if(p->s.size >= nunits)
		{
			/* exactly */
			if (p->s.size == nunits)
			{                         
				prevp->s.ptr = p->s.ptr;
			}
			else 
			{
				/* allocate tail end */                                       
				p->s.size -= nunits;
				p += p->s.size;
				p->s.size = nunits;
			}

			freep = prevp;
			return (void *)(p+1);
		}

		/* wrapped around free list */
		if(p == freep)
		{                                     
			if((p = morecore(nunits)) == NULL)
			{
				return NULL;	/* none left */
			}
		}
	}

}
void * bestFitMalloc(size_t nbytes)
{
	Header *p, *prevp;
	Header * morecore(unsigned);
	unsigned nunits;
	
	/* For saving the smallest block */
	Header *smallest = NULL;
	Header *smallestPrev = NULL;	

	/* Calculate the number of units in ( Headers ) required 
	 * to store the given amount of nbytes data */
	nunits = (nbytes + sizeof(Header) - 1) / sizeof(Header) + 1;
	
	/* Set previous pointer to point backwards next free block */
	prevp = freep;
	/* Nothing have yet been allocated, set everything to point to first element in list and size 
	 * to 0 */	
	if(prevp == NULL) 
	{
	  base.s.ptr = freep = prevp = &base;
	  base.s.size = 0;
	}

	for(p= prevp->s.ptr;  ; prevp = p, p = p->s.ptr)
	{
		/* Scan for smallest block of size >= nunits */
		if(p->s.size >= nunits && (smallest == NULL 
			|| p->s.size < smallest->s.size))
		{
			/* Found one smaller than previous smallest */
			smallest = p;
			smallestPrev = prevp;
		}

		/* Reached end of free list */
		if(p == freep)
		{ 
			if(smallest != NULL)
			{
				/* We've found a free big enough block, allocate it! */
				if (smallest->s.size == nunits)
				{                     
					/* block fits exactly */
					smallestPrev->s.ptr = smallest->s.ptr;
				}
				else 
				{
					/* allocate tail end */                                       
					smallest->s.size -= nunits;
					smallest += smallest->s.size;
					smallest->s.size = nunits;
				}

				freep = smallestPrev;
				return (void *)(smallest+1);
			}
			/* otherwise, get more memory */
			else if((p = morecore(nunits)) == NULL)
			{
				return NULL;	/* none left */
			}
		}
	}		
}
void * quickFitMalloc(size_t nbytes)
{
	Header *p, *prevp;
	Header * morecore(unsigned);
	unsigned nunits;	
	int idx;
	
	if(nbytes > qfBlockSizes[QF_NUM_LISTS - 1])
	{
		return firstFitMalloc(nbytes);
	}
	else
	{
		/* Get the index of the list with the most optimal fit */
		for(idx = 0; idx < QF_NUM_LISTS; idx++)
		{
			if(qfBlockSizes[idx] >= nunits) break;
		}
	}
	
	prevp = qfFreep[idx];
	
	/* Calculate the number of units in ( Headers ) required 
	 * to store the given amount of nbytes data */
	nunits = (nbytes + sizeof(Header) - 1) / sizeof(Header) + 1;	
	
	/* If this is the first allocation initialize free list */
	if(prevp == NULL)
	{
		int i;
		int size = qfMinBlockSize;
		for(i = 0; i < QF_NUM_LISTS; i++)
		{
			qfBlockSizes[i] = size;		/* Set block size of the current free list */
			size *= 2;					/* Increase block size by the poer of two */
			qfBase[i].s.ptr = qfFreep[i] = &qfBase[i];
			qfBase[i].s.size = 0;
		}
		qfBase[i].s.ptr = qfFreep[i] = &qfBase[i];
		qfBase[i].s.size = 0;
	}		

	p = prevp->s.ptr;
	
	if(p->s.size >= nunits)
	{
		p->s.size -= nunits;
		p += qfBlockSizes[idx];
		p->s.size = nunits;
		qfFreep[idx] = prevp;
		return (void *)(p + 1);
	}

	/* No free block in free list, get more */                                  
	if((p = morecore(nunits)) == NULL)
	{
		return NULL;	/* none left */
	}		
	prevp = p;
	p = p->s.ptr;

	p->s.size -= nunits;
	p += qfBlockSizes[idx];
	p->s.size = nunits;
	qfFreep[idx] = prevp;
	return (void *)(p + 1);		
}
void * malloc(size_t nbytes)
{
	if(nbytes == 0) 
	{
		return NULL;
	}	
	/* STRATEGY 1: First fit */
	else if(STRATEGY == 1)
	{
		firstFitMalloc(nbytes);
	}
	/* STRATEGY 2: Best fit */
	else if(STRATEGY == 2)
	{
		bestFitMalloc(nbytes);
	}
	/* STRATEGY 4: Quick fit */
	else if(STRATEGY == 4)
	{
		quickFitMalloc(nbytes);
	}
}

void * realloc(void * oldBlock, size_t newSize)
{
	void * newBlock = NULL;
	Header * oldHeader = NULL;
	size_t oldSize = 0;
	


	if(oldBlock == NULL)
	{
		/* if oldBlock is NULL then we just allocate as normal */
		return malloc(newSize);
	}
	else if(newSize == 0)
	{
		/* newSize is zero which means that no data will be moved to 
		 * the new block (free the old block) */
		free(oldBlock);
		return NULL;
	}
	else
	{
		newBlock = malloc(newSize);
		
		/* If malloc fails then we return a NULL ptr */
		if(newBlock == NULL) return NULL;
		
		/* Get the old block header */
		oldHeader = (Header *) oldBlock - 1;
		oldSize = (oldHeader->s.size - 1) * sizeof(Header);
		
		/* Move the old data to the new area */
		memmove(newBlock, oldBlock, min(newSize, oldSize));
		
		/* Clean up the old data */
		free(oldBlock);
		
		return newBlock;
	}
}
