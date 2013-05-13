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


static Header *morecore(unsigned nu)
{
	void *cp;
	Header *up;
	
	#ifdef MMAP
		unsigned noPages;
		if(__endHeap == 0)
		{
			__endHeap = sbrk(0);
		}
	#endif

	if(nu < NALLOC)
	{
		nu = NALLOC;
	}
	
	#ifdef MMAP
		noPages = ((nu*sizeof(Header))-1)/getpagesize() + 1;
		cp = mmap(__endHeap, 
				noPages * getpagesize(), 
				PROT_READ | PROT_WRITE, 
				MAP_SHARED | MAP_ANONYMOUS, 
				-1, 0);
		nu = (noPages * getpagesize()) / sizeof(Header);
		__endHeap += noPages * getpagesize();
	#else
		cp = sbrk(nu*sizeof(Header));
	#endif
	
	/* no space at all */
	if(cp == (void *) -1)
	{                                 
		perror("failed to get more memory");
		return NULL;
	}
	up = (Header *) cp;
	up->s.size = nu;
	free((void *)(up+1));
	return freep;
}

void * malloc(size_t nbytes)
{
	Header *p, *prevp;
	Header * morecore(unsigned);
	unsigned nunits;

	if(nbytes == 0) return NULL;

	nunits = (nbytes+sizeof(Header)-1)/sizeof(Header) +1;

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
