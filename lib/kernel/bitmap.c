#include "bitmap.h"
#include <debug.h>
#include <limits.h>
#include <round.h>
#include <stdio.h>
#include "threads/malloc.h"
#include "threads/palloc.h"
#ifdef FILESYS
#include "filesys/file.h"
#endif

/* Element type.

   This must be an unsigned integer type at least as wide as int.

   Each bit represents one bit in the bitmap.
   If bit 0 in an element represents bit K in the bitmap,
   then bit 1 in the element represents bit K+1 in the bitmap,
   and so on. */
typedef unsigned long elem_type;

/* Number of bits in an element. */
#define ELEM_BITS (sizeof (elem_type) * CHAR_BIT)

/* From the outside, a bitmap is an array of bits.  From the
   inside, it's an array of elem_type (defined above) that
   simulates an array of bits. */
struct bitmap
  {
    size_t bit_cnt;     /* Number of bits. */
    elem_type *bits;    /* Elements that represent bits. */
    //jjeong
    size_t last_idx;	/*for next fit - storing lastest position*/
  };

/* Returns the index of the element that contains the bit
   numbered BIT_IDX. */
static inline size_t
elem_idx (size_t bit_idx) 
{
  return bit_idx / ELEM_BITS;
}

/* Returns an elem_type where only the bit corresponding to
   BIT_IDX is turned on. */
static inline elem_type
bit_mask (size_t bit_idx) 
{
  return (elem_type) 1 << (bit_idx % ELEM_BITS);
}

/* Returns the number of elements required for BIT_CNT bits. */
static inline size_t
elem_cnt (size_t bit_cnt)
{
  return DIV_ROUND_UP (bit_cnt, ELEM_BITS);
}

/* Returns the number of bytes required for BIT_CNT bits. */
static inline size_t
byte_cnt (size_t bit_cnt)
{
  return sizeof (elem_type) * elem_cnt (bit_cnt);
}

/* Returns a bit mask in which the bits actually used in the last
   element of B's bits are set to 1 and the rest are set to 0. */
static inline elem_type
last_mask (const struct bitmap *b) 
{
  int last_bits = b->bit_cnt % ELEM_BITS;
  return last_bits ? ((elem_type) 1 << last_bits) - 1 : (elem_type) -1;
}

/* Creation and destruction. */

/* Creates and returns a pointer to a newly allocated bitmap with room for
   BIT_CNT (or more) bits.  Returns a null pointer if memory allocation fails.
   The caller is responsible for freeing the bitmap, with bitmap_destroy(),
   when it is no longer needed. */
struct bitmap *
bitmap_create (size_t bit_cnt) 
{
  struct bitmap *b = malloc (sizeof *b);
  if (b != NULL)
    {
      b->bit_cnt = bit_cnt;
      b->bits = malloc (byte_cnt (bit_cnt));
      if (b->bits != NULL || bit_cnt == 0)
        {
          bitmap_set_all (b, false);
          return b;
        }
      free (b);
    }
  return NULL;
}

/* Creates and returns a bitmap with BIT_CNT bits in the
   BLOCK_SIZE bytes of storage preallocated at BLOCK.
   BLOCK_SIZE must be at least bitmap_needed_bytes(BIT_CNT). */
struct bitmap *
bitmap_create_in_buf (size_t bit_cnt, void *block, size_t block_size UNUSED)
{
  struct bitmap *b = block;
  
  ASSERT (block_size >= bitmap_buf_size (bit_cnt));

  b->bit_cnt = bit_cnt;
  b->bits = (elem_type *) (b + 1);
  //jjeong
  b->last_idx =0;
  bitmap_set_all (b, false);
  return b;
}
//jjeong
/* update the lastest allocated page index */
struct bitmap* bitmap_modify_idx(struct bitmap *b,size_t idx){
	b->last_idx = idx;
	return b;
}
//jjeong
/* return the lastest allocated page index */
struct bitmap* bitmap_lastest_idx(struct bitmap *b){
	return b->last_idx;
}
//jjeong
/* find the page index for next fit
 * 1. use last_idx variable in bitmap 
 *  and scan the bitmap starting from this index
 *  return the index if you find correct index
 * 2. if there is not enough pages,
 *   scan again starting from index 0
 * 3. if there is not enough pages either,
 *  return the page index 0.  
 * */
size_t bitmap_scan_NF(struct bitmap *b, size_t start, size_t cnt, bool value){
 ASSERT (start <= b->bit_cnt);
 ASSERT (start + cnt <= b->bit_cnt);
 ASSERT (b != NULL);
 
 size_t page_idx =0;

	start = bitmap_lastest_idx(b); 
	if(start+cnt <= b->bit_cnt)
		page_idx = bitmap_scan(b, start, cnt, false);
	if(page_idx==BITMAP_ERROR || start + cnt > b->bit_cnt){
		page_idx = bitmap_scan(b, 0, cnt, false);
		if(page_idx==BITMAP_ERROR)
			page_idx =0;
	}
	return page_idx;
}
//jjeong
/* find index for bestfit
 * 1. find every space that can be allocated
 * 2. select the smallest splace among those spaces
 * */
size_t bitmap_scan_BF(struct bitmap *b, size_t start, size_t cnt, bool value){

  ASSERT (b != NULL);
  ASSERT (start <= b->bit_cnt);
  ASSERT (start + cnt <= b->bit_cnt);

  size_t idx=start,page_idx=start;
  size_t min=b->bit_cnt;
  size_t last = b->bit_cnt - cnt;
  size_t len =0 ;
  for(; start<=last ; start=page_idx+len){
	/* find page index whether there is enough space for page allocation*/
  	page_idx = bitmap_scan(b,start,cnt,value);
       if(page_idx != BITMAP_ERROR){
	        len =0 ;
		/*if there is enough space check its length(len)*/
       		for(size_t i =0 ; page_idx+i<b->bit_cnt ;i++){
			if(bitmap_test(b,page_idx+i)==value)
				len ++;
			else
				break;
		}	
		/*if its length(len) is smaller than stored length (min)
		 * change the stored length to current length (len) and 
		 * change the page index (idx) in to len's index (page_idx) */
       		if(min>len){
			min = len;
			idx = page_idx;
		}
         }else 	return idx;
 
     }
  return idx;
}

//jjeong
 #define BUDDY_INDEX 10  /* this index is for page 1,2,4,8,16 ... 512 */
 #define USED_INDEX 32	/* this index is for  USED_SIZE is 16 and for represent 512 page we need 32 of this */
 #define USED_SIZE 16	/* the total page unusage is represented with bits this size is 16 */
  struct buddy_info{ uint16_t used_bud[USED_INDEX]; };
  struct buddy_info bud[BUDDY_INDEX];
//jjeong 
/* initialize the the buddy structure*/
void buddy_info_init(){
	for(size_t i=0; i<BUDDY_INDEX;i++){
		for(size_t j=0; j<USED_INDEX;j++)
			bud[i].used_bud[j]=0;
	}
	/* If no page is allocated, the size of 
	 * the buddy is 512 because the memory is not cut.
	 * Save this index */
	bud[BUDDY_INDEX-1].used_bud[0]=1;
}
//jjeong
/* check if there is any bud for allocation 
 * ex) if this is for bud[2] :
 *  This function is for checking whether there is any 
 *  unused buddy that is size 4 .
 * */
bool buddy_empty(struct buddy_info bud){
	
	for(size_t j=0; j<USED_INDEX;j++){
		if(bud.used_bud[j]!=0)
			return false;
	}

	return true;
}
//jjeong
/* Find the smallest page index for buddy
 * ex) if this is for bud[2] :
 *  This function is for find the smallest unused page index.
 *  So, if there is unused buddy which size is 4, its page index 
 *  would be set 1 in bud[2] - If there page index 0 and 8
 *   can be used, bud[2].used_bud[0] will be 00000000100000001
 *   in this situation 0 is smallest unused page index 
 *   and we have to return 0.  
 * */
size_t find_first_buddy_idx(struct buddy_info* bud){

	for(size_t i =0; i<USED_INDEX;i++){
		if(bud->used_bud[i]!=0){
		size_t tmp = bud->used_bud[i];
		for(size_t j=0 ; j<USED_SIZE;j++){
			size_t remainder = tmp%2;
			if(remainder){
			   bud->used_bud[i] -= 1<<j;
			   tmp= (i*USED_SIZE+j); 
			   return tmp;
			}
			 tmp = tmp/2;
		 }
		}
	
	}	
	return 0;
}
//jjeong
/*This finction is recursive function
 * and this is for finding the page index
 *  which has size size ^bud_idx 
 * */
size_t find_buddy_idx(size_t bud_idx){
	
	if(bud_idx==BUDDY_INDEX)
		return NULL;

	/* If there is no unused buddy make the buddy 
	 * by dividing 2^(i+1)
	 * */
	if(buddy_empty(bud[bud_idx])){
		size_t idx;
		idx =find_buddy_idx(bud_idx+1);
		bud[bud_idx].used_bud[idx/USED_SIZE]=1<<(idx%USED_SIZE);
		idx = idx + (1<<bud_idx);
		bud[bud_idx].used_bud[idx/USED_SIZE]+=1<<(idx%USED_SIZE);	
	}

		return find_first_buddy_idx(&bud[bud_idx]); 
}

//jjeong
/* find the value idx which is 
 * 2^idx >=cnt>2^(idx-1)
 * and call function to find page index
 * for size 2^idx
 * */
size_t bitmap_scan_BUDDY(struct bitmap *b, size_t start, size_t cnt, bool value){

  ASSERT (b != NULL);
  ASSERT (start <= b->bit_cnt);
  ASSERT (start + cnt <= b->bit_cnt);
  size_t page_idx=start,idx;
    for(idx=0; idx<BUDDY_INDEX;idx++){
    	if(cnt<=(1<<idx))
		break;
    }
	page_idx = find_buddy_idx(idx);
  return page_idx;
}

//jjeong
/* This function is recursive function 
 * 1. If some pages got freed update with size 2^i
 * the buddy structure(bud[i]) 
 * 2. and find its buddy
 * 3. and if its buddy is unused (set to 1) either
 * merge it and update buddy structure(bud[i+1])
 * 4. recursively repeat this procedure until its buddy is used 
 * */
void merge_bud(size_t page_idx, size_t page_cnt){
	size_t g_idx;
	size_t bud_idx;
	
	if(page_cnt == BUDDY_INDEX)
		return NULL;
	bud[page_cnt].used_bud[page_idx/USED_SIZE] += 1<<(page_idx%USED_SIZE);
	g_idx=page_idx /(1<<page_cnt);	

	if(g_idx%2){
		bud_idx=page_idx -(1<<page_cnt);
		size_t tmp = bud[page_cnt].used_bud[bud_idx/USED_SIZE];

		if(tmp&(1<<(bud_idx%USED_SIZE))){
			bud[page_cnt].used_bud[page_idx/USED_SIZE] -= 1<<(page_idx%USED_SIZE);
			bud[page_cnt].used_bud[bud_idx/USED_SIZE] -= 1<<(bud_idx%USED_SIZE);

			return merge_bud(bud_idx ,page_cnt+1);
		}
		else
			return NULL;
	}
	else{
		bud_idx=page_idx + (1<<page_cnt);
		size_t tmp = bud[page_cnt].used_bud[bud_idx/USED_SIZE];
		if(tmp&(1<<(bud_idx%USED_SIZE))){
			bud[page_cnt].used_bud[page_idx/USED_SIZE] -= 1<<(page_idx%USED_SIZE);
			bud[page_cnt].used_bud[bud_idx/USED_SIZE] -= 1<<(bud_idx%USED_SIZE);

			return merge_bud(page_idx,page_cnt+1);
		}
		else
			return NULL;
	}
}
//jjeong
/* find the value which is
 * 2^idx>= page_cnt > 2^(idx-1)
 * and call the function for merge the buddy
 * if it is necessary.
 * */
void
bitmap_merge_buddy (size_t page_idx, size_t page_cnt){
	size_t idx;
    	for(idx=0; idx<BUDDY_INDEX;idx++){
    		if(page_cnt<=(1<<idx))
			break;
   	 }
	
	merge_bud(page_idx, idx);
	return NULL;
}

/* Returns the number of bytes required to accomodate a bitmap
   with BIT_CNT bits (for use with bitmap_create_in_buf()). */
size_t
bitmap_buf_size (size_t bit_cnt) 
{
  return sizeof (struct bitmap) + byte_cnt (bit_cnt);
}

/* Destroys bitmap B, freeing its storage.
   Not for use on bitmaps created by bitmap_create_in_buf(). */
void
bitmap_destroy (struct bitmap *b) 
{
  if (b != NULL) 
    {
      free (b->bits);
      free (b);
    }
}

/* Bitmap size. */

/* Returns the number of bits in B. */
size_t
bitmap_size (const struct bitmap *b)
{
  return b->bit_cnt;
}

/* Setting and testing single bits. */

/* Atomically sets the bit numbered IDX in B to VALUE. */
void
bitmap_set (struct bitmap *b, size_t idx, bool value) 
{
  ASSERT (b != NULL);
  ASSERT (idx < b->bit_cnt);
  if (value)
    bitmap_mark (b, idx);
  else
    bitmap_reset (b, idx);
}

/* Atomically sets the bit numbered BIT_IDX in B to true. */
void
bitmap_mark (struct bitmap *b, size_t bit_idx) 
{
  size_t idx = elem_idx (bit_idx);
  elem_type mask = bit_mask (bit_idx);

  /* This is equivalent to `b->bits[idx] |= mask' except that it
     is guaranteed to be atomic on a uniprocessor machine.  See
     the description of the OR instruction in [IA32-v2b]. */
  asm ("orl %1, %0" : "=m" (b->bits[idx]) : "r" (mask) : "cc");
}

/* Atomically sets the bit numbered BIT_IDX in B to false. */
void
bitmap_reset (struct bitmap *b, size_t bit_idx) 
{
  size_t idx = elem_idx (bit_idx);
  elem_type mask = bit_mask (bit_idx);

  /* This is equivalent to `b->bits[idx] &= ~mask' except that it
     is guaranteed to be atomic on a uniprocessor machine.  See
     the description of the AND instruction in [IA32-v2a]. */
  asm ("andl %1, %0" : "=m" (b->bits[idx]) : "r" (~mask) : "cc");
}

/* Atomically toggles the bit numbered IDX in B;
   that is, if it is true, makes it false,
   and if it is false, makes it true. */
void
bitmap_flip (struct bitmap *b, size_t bit_idx) 
{
  size_t idx = elem_idx (bit_idx);
  elem_type mask = bit_mask (bit_idx);

  /* This is equivalent to `b->bits[idx] ^= mask' except that it
     is guaranteed to be atomic on a uniprocessor machine.  See
     the description of the XOR instruction in [IA32-v2b]. */
  asm ("xorl %1, %0" : "=m" (b->bits[idx]) : "r" (mask) : "cc");
}

/* Returns the value of the bit numbered IDX in B. */
bool
bitmap_test (const struct bitmap *b, size_t idx) 
{
  ASSERT (b != NULL);
  ASSERT (idx < b->bit_cnt);
  return (b->bits[elem_idx (idx)] & bit_mask (idx)) != 0;
}

/* Setting and testing multiple bits. */

/* Sets all bits in B to VALUE. */
void
bitmap_set_all (struct bitmap *b, bool value) 
{
  ASSERT (b != NULL);

  bitmap_set_multiple (b, 0, bitmap_size (b), value);
}

/* Sets the CNT bits starting at START in B to VALUE. */
void
bitmap_set_multiple (struct bitmap *b, size_t start, size_t cnt, bool value) 
{
  size_t i;
  
  ASSERT (b != NULL);
  ASSERT (start <= b->bit_cnt);
  ASSERT (start + cnt <= b->bit_cnt);

  for (i = 0; i < cnt; i++)
    bitmap_set (b, start + i, value);
}

/* Returns the number of bits in B between START and START + CNT,
   exclusive, that are set to VALUE. */
size_t
bitmap_count (const struct bitmap *b, size_t start, size_t cnt, bool value) 
{
  size_t i, value_cnt;

  ASSERT (b != NULL);
  ASSERT (start <= b->bit_cnt);
  ASSERT (start + cnt <= b->bit_cnt);

  value_cnt = 0;
  for (i = 0; i < cnt; i++)
    if (bitmap_test (b, start + i) == value)
      value_cnt++;
  return value_cnt;
}

/* Returns true if any bits in B between START and START + CNT,
   exclusive, are set to VALUE, and false otherwise. */
bool
bitmap_contains (const struct bitmap *b, size_t start, size_t cnt, bool value) 
{
  size_t i;
  
  ASSERT (b != NULL);
  ASSERT (start <= b->bit_cnt);
  ASSERT (start + cnt <= b->bit_cnt);

  for (i = 0; i < cnt; i++)
    if (bitmap_test (b, start + i) == value)
      return true;
  return false;
}

/* Returns true if any bits in B between START and START + CNT,
   exclusive, are set to true, and false otherwise.*/
bool
bitmap_any (const struct bitmap *b, size_t start, size_t cnt) 
{
  return bitmap_contains (b, start, cnt, true);
}

/* Returns true if no bits in B between START and START + CNT,
   exclusive, are set to true, and false otherwise.*/
bool
bitmap_none (const struct bitmap *b, size_t start, size_t cnt) 
{
  return !bitmap_contains (b, start, cnt, true);
}

/* Returns true if every bit in B between START and START + CNT,
   exclusive, is set to true, and false otherwise. */
bool
bitmap_all (const struct bitmap *b, size_t start, size_t cnt) 
{
  return !bitmap_contains (b, start, cnt, false);
}

/* Finding set or unset bits. */

/* Finds and returns the starting index of the first group of CNT
   consecutive bits in B at or after START that are all set to
   VALUE.
   If there is no such group, returns BITMAP_ERROR. */
size_t
bitmap_scan (const struct bitmap *b, size_t start, size_t cnt, bool value) 
{
  ASSERT (b != NULL);
  ASSERT (start <= b->bit_cnt);

  if (cnt <= b->bit_cnt) 
    {
      size_t last = b->bit_cnt - cnt;
      size_t i;
      for (i = start; i <= last; i++)
        if (!bitmap_contains (b, i, cnt, !value))
          return i; 
    }
  return BITMAP_ERROR;
}

/* Finds the first group of CNT consecutive bits in B at or after
   START that are all set to VALUE, flips them all to !VALUE,
   and returns the index of the first bit in the group.
   If there is no such group, returns BITMAP_ERROR.
   If CNT is zero, returns 0.
   Bits are set atomically, but testing bits is not atomic with
   setting them. */
size_t
bitmap_scan_and_flip (struct bitmap *b, size_t start, size_t cnt, bool value)
{
  size_t idx = bitmap_scan (b, start, cnt, value);
  if (idx != BITMAP_ERROR) 
    bitmap_set_multiple (b, idx, cnt, !value);
  return idx;
}

/* File input and output. */

#ifdef FILESYS
/* Returns the number of bytes needed to store B in a file. */
size_t
bitmap_file_size (const struct bitmap *b) 
{
  return byte_cnt (b->bit_cnt);
}

/* Reads B from FILE.  Returns true if successful, false
   otherwise. */
bool
bitmap_read (struct bitmap *b, struct file *file) 
{
  bool success = true;
  if (b->bit_cnt > 0) 
    {
      off_t size = byte_cnt (b->bit_cnt);
      success = file_read_at (file, b->bits, size, 0) == size;
      b->bits[elem_cnt (b->bit_cnt) - 1] &= last_mask (b);
    }
  return success;
}

/* Writes B to FILE.  Return true if successful, false
   otherwise. */
bool
bitmap_write (const struct bitmap *b, struct file *file)
{
  off_t size = byte_cnt (b->bit_cnt);
  return file_write_at (file, b->bits, size, 0) == size;
}
#endif /* FILESYS */

/* Debugging. */

/* Dumps the contents of B to the console as hexadecimal. */
void
bitmap_dump (const struct bitmap *b) 
{
  hex_dump (0, b->bits, byte_cnt (b->bit_cnt), false);
}

/* Dumps the contents of B to the console as binary. */
void
bitmap_dump2 (const struct bitmap *b)
{
  size_t i, j;

  printf ("========== bitmap dump start ==========\n");
  for (i=0; i<elem_cnt (b->bit_cnt); i++) {
    for (j=0; j<ELEM_BITS; j++) {
      if ((i * ELEM_BITS + j) < b->bit_cnt) {
        printf ("%u", (unsigned int) (b->bits[i] >> j) & 0x1);
      }      
    }
    printf ("\n");
  }
  printf ("========== bitmap dump end ==========\n");
}
