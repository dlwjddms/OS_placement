#include "threads/palloc.h"
#include <bitmap.h>
#include <debug.h>
#include <inttypes.h>
#include <round.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "threads/loader.h"
#include "threads/synch.h"
#include "threads/vaddr.h"

/* Page allocator.  Hands out memory in page-size (or
   page-multiple) chunks.  See malloc.h for an allocator that
   hands out smaller chunks.

   System memory is divided into two "pools" called the kernel
   and user pools.  The user pool is for user (virtual) memory
   pages, the kernel pool for everything else.  The idea here is
   that the kernel needs to have memory for its own operations
   even if user processes are swapping like mad.

   By default, half of system RAM is given to the kernel pool and
   half to the user pool.  That should be huge overkill for the
   kernel pool, but that's just fine for demonstration purposes. */

/* A memory pool. */
struct pool
  {
    struct lock lock;                   /* Mutual exclusion. */
    struct bitmap *used_map;            /* Bitmap of free pages. */
    uint8_t *base;                      /* Base of pool. */
  };

/* Two pools: one for kernel data, one for user pages. */
static struct pool kernel_pool, user_pool;

static void init_pool (struct pool *, void *base, size_t page_cnt,
                       const char *name);
static bool page_from_pool (const struct pool *, void *page);

/* The page allocation algorithm */
enum palloc_allocator pallocator = 0;

/* Initializes the page allocator.  At most USER_PAGE_LIMIT
   pages are put into the user pool. */
void
palloc_init (size_t user_page_limit)
{
  /* Free memory starts at 1 MB and runs to the end of RAM. */
  uint8_t *free_start = ptov (1024 * 1024);
  uint8_t *free_end = ptov (init_ram_pages * PGSIZE);
  size_t free_pages = (free_end - free_start) / PGSIZE;
  //jjeong-print
	printf(" jjeong-num of ram page :  %lu \n",init_ram_pages);
	printf(" jjeong-num of free page: %lu \n",free_pages);

  size_t user_pages = free_pages - 513;
  size_t kernel_pages;
  if (user_pages > user_page_limit)
    user_pages = user_page_limit;
  kernel_pages = free_pages - user_pages;

  init_pool (&kernel_pool, free_start, kernel_pages, "kernel pool");
  init_pool (&user_pool, free_start + kernel_pages * PGSIZE,
             user_pages, "user pool");
}


//jjeong
/* int pool there is pallocator which decides the method of memory allocation
 * 0 is for first fit
 * 1 is for next fit
 * 2 is for best fit
 * 3 is for buddy system
 * */
size_t select_memory_allocate(struct bitmap *b, size_t start, size_t cnt, bool value){
  size_t page_idx;
  if(pallocator==ALLOCATOR_FF)
 	goto PALLOC;	 
  else if(pallocator==ALLOCATOR_NF){
	start = bitmap_scan_NF(b, 0, cnt, false);
 	goto PALLOC;	 
  }
	  
  else if(pallocator==ALLOCATOR_BF){
  	start = bitmap_scan_BF (b, start, cnt, false);
 	goto PALLOC;	 
  }
  else if(pallocator==ALLOCATOR_BUDDY){
  	start= bitmap_scan_BUDDY (b, start, cnt, false);
 	goto PALLOC;	 
  }
  /* if the -ma option is not 0 to 3 we just run first fit*/
  else 
 	goto PALLOC;	 

 /* after each option we store page index for allocation
  * run bitmap_scan and flip using this index
  * */
 PALLOC:
  	page_idx = bitmap_scan_and_flip (b, start, cnt, false);
 	
	/* this is for next fit index updating*/
	if(page_idx!=BITMAP_ERROR)
		b = bitmap_modify_idx(b,page_idx+cnt); 

  return  page_idx;
 
}

/* Obtains and returns a group of PAGE_CNT contiguous free pages.
   If PAL_USER is set, the pages are obtained from the user pool,
   otherwise from the kernel pool.  If PAL_ZERO is set in FLAGS,
   then the pages are filled with zeros.  If too few pages are
   available, returns a null pointer, unless PAL_ASSERT is set in
   FLAGS, in which case the kernel panics. */
void *
palloc_get_multiple (enum palloc_flags flags, size_t page_cnt)
{
  struct pool *pool = flags & PAL_USER ? &user_pool : &kernel_pool;
  void *pages;
  size_t page_idx;

  if (page_cnt == 0)
    return NULL;

  lock_acquire (&pool->lock);

  //jjeong
  /* for memory allocating 
   * 1. find the index fot allocation with allocation method
   * 2. update the bitmap representing allocation status
   * */
  page_idx = select_memory_allocate (pool->used_map, 0, page_cnt, false);
  lock_release (&pool->lock);

  if (page_idx != BITMAP_ERROR)
    pages = pool->base + PGSIZE * page_idx;
  else
    pages = NULL;

  if (pages != NULL) 
    {
      if (flags & PAL_ZERO)
        memset (pages, 0, PGSIZE * page_cnt);
    }
  else 
    {
	
      if (flags & PAL_ASSERT)
        PANIC ("palloc_get: out of pages");
    }

  return pages;
}

/* Obtains a single free page and returns its kernel virtual
   address.
   If PAL_USER is set, the page is obtained from the user pool,
   otherwise from the kernel pool.  If PAL_ZERO is set in FLAGS,
   then the page is filled with zeros.  If no pages are
   available, returns a null pointer, unless PAL_ASSERT is set in
   FLAGS, in which case the kernel panics. */
void *
palloc_get_page (enum palloc_flags flags) 
{
  return palloc_get_multiple (flags, 1);
}

/* Frees the PAGE_CNT pages starting at PAGES. */
void
palloc_free_multiple (void *pages, size_t page_cnt) 
{
  struct pool *pool;
  size_t page_idx;

  ASSERT (pg_ofs (pages) == 0);
  if (pages == NULL || page_cnt == 0)
    return;

  if (page_from_pool (&kernel_pool, pages))
    pool = &kernel_pool;
  else if (page_from_pool (&user_pool, pages))
    pool = &user_pool;
  else
    NOT_REACHED ();

  page_idx = pg_no (pages) - pg_no (pool->base);

#ifndef NDEBUG
  memset (pages, 0xcc, PGSIZE * page_cnt);
#endif

  ASSERT (bitmap_all (pool->used_map, page_idx, page_cnt));
  //jjeong
  /* This is for BUDDY system */
  if(pallocator ==ALLOCATOR_BUDDY)
 	 bitmap_merge_buddy(page_idx,page_cnt);
  bitmap_set_multiple (pool->used_map, page_idx, page_cnt, false);
}

/* Frees the page at PAGE. */
void
palloc_free_page (void *page) 
{
  palloc_free_multiple (page, 1);
}

/* Obtains a status of the page pool */
void
palloc_get_status (enum palloc_flags flags)
{
  struct pool *pool = flags & PAL_USER ? &user_pool : &kernel_pool;

  lock_acquire (&pool->lock);
  bitmap_dump2 (pool->used_map);
  lock_release (&pool->lock);
}

/* Initializes pool P as starting at START and ending at END,
   naming it NAME for debugging purposes. */
static void
init_pool (struct pool *p, void *base, size_t page_cnt, const char *name) 
{
  /* We'll put the pool's used_map at its base.
     Calculate the space needed for the bitmap
     and subtract it from the pool's size. */

	//jjeong-print
	printf("init pool first %lu \n",page_cnt);
  size_t bm_pages = DIV_ROUND_UP (bitmap_buf_size (page_cnt), PGSIZE);
	//jjeong-print
	printf("init pool second %lu \n",bm_pages);
  if (bm_pages > page_cnt)
    PANIC ("Not enough memory in %s for bitmap.", name);
  page_cnt -= bm_pages;
//jjeong
/*
 * bitmap is used in 1 pagr so 513 - 1 = 512
 * so free kenerl page is 512 
 * */
  printf ("%zu pages available in %s.\n", page_cnt, name);

  /* Initialize the pool. */
  lock_init (&p->lock);
  p->used_map = bitmap_create_in_buf (page_cnt, base, bm_pages * PGSIZE);
  p->base = base + bm_pages * PGSIZE;
}

/* Returns true if PAGE was allocated from POOL,
   false otherwise. */
static bool
page_from_pool (const struct pool *pool, void *page) 
{
  size_t page_no = pg_no (page);
  size_t start_page = pg_no (pool->base);
  size_t end_page = start_page + bitmap_size (pool->used_map);

  return page_no >= start_page && page_no < end_page;
}

