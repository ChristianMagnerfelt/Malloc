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
	Align x;					/* Force alignment of blocks */
};

typedef union header Header;

static Header base;				/* Empty list to get started */
static Header *freep = NULL;	/* Start of free list */

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
void * malloc(size_t nbytes)
{
	Header *p, *prevp;
	Header * morecore(unsigned);
	unsigned nunits;

	if(nbytes == 0) return NULL;

	nunits = (nbytes + sizeof(Header) - 1) / sizeof(Header) + 1;

	if((prevp = freep) == NULL) 
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
void * realloc(void * oldBlock, size_t newSize)
{
	void * newBlock = NULL;
	Header * oldHeader = NULL;
	size_t oldSize = 0;
	
	if(oldBlock == NULL)
	{
		return malloc(newSize);
	}
	else if(newSize == 0)
	{
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
