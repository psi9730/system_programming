#include <stdio.h>
   #include <stdlib.h>
   #include <assert.h>
   #include <unistd.h>
   #include <string.h>

   #include "mm.h"
   #include "memlib.h"

   /*********************************************************
    * NOTE TO STUDENTS: Before you do anything else, please
    * provide your information in the following struct.
    ********************************************************/
   team_t team = {
           /* Team name : Your student ID */
           "2016-10760",
           /* Your full name */
           "Park Seung Il",
           /* Your student ID, again  */
           "2016-10760",
           /* leave blank */
           "",
           /* leave blank */
           ""
   };

   /* DON'T MODIFY THIS VALUE AND LEAVE IT AS IT WAS */
   static range_t **gl_ranges;

   /* single word (4) or double word (8) alignment */
   #define ALIGNMENT 8

   /* rounds up to the nearest multiple of ALIGNMENT */
   #define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)


   #define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

   #define WSIZE 4
   #define DSIZE 8
   #define CHUNKSIZE (1<<12) //BASE EXTEND BLOCK
   #define LIST_MAX 20 //SEGREGATED LIST NUMBER
   #define MAX(x,y) ((x) > (y) ? (x) : (y))

   #define PACK(size, alloc) ((size) | (alloc)) //allocate header with size and alloc
   #define GET(p)            (*(unsigned int *)(p))
   #define PUT(p, val)       (*(unsigned int *)(p) = (val))
   #define GET_SIZE(p)  (GET(p) & ~0x7)    //get size from pointer
   #define GET_ALLOC(p) (GET(p) & 0x1)     //get alloc from pointer
   #define GET_NEXTP(bp)      (*(unsigned int **)(bp)) //get next free pointer
   #define GET_PREVP(bp)      (*(unsigned int **)(bp + WSIZE)) //get prev free pointer
   #define SET_NEXTP(bp, p)      (*(unsigned int **)(bp) = p)  //set next free pointer
   #define SET_PREVP(bp, p)      (*(unsigned int **)(bp + WSIZE) = p)  //set prev free pointer
   #define HDRP(bp) ((char *)(bp) - WSIZE)     //get header pointer
   #define FTRP(bp) ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)        //get footer pointer
   #define NEXT_BLKP(bp)  ((char *)(bp) + GET_SIZE(((char *)(bp) - WSIZE)))    //get next pointer by size in current header
   #define PREV_BLKP(bp)  ((char *)(bp) - GET_SIZE(((char *)(bp) - DSIZE)))    //prev prev pointer by size in prev footer
   #define GET_SEG_CLASS(n, ptr)   (*(unsigned int *)(ptr + n*WSIZE))      //get n'th class pointer
   static char *heap_list_pointer;
   static char *seg_list_pointer;
   static void *heap_extend(size_t words);
   static void *coalesce(void *bp);
   static void allocate(char *bp, size_t size);
   static void *find_seg(size_t alloc_size);
   static void segregate_insert(char *bp);
   static void *segregate_delete(void *bp);

   /*
    * remove_range - manipulate range lists
    * DON'T MODIFY THIS FUNCTION AND LEAVE IT AS IT WAS
   */
   static void remove_range(range_t **ranges, char *lo)
   {
       range_t *p;
       range_t **prevpp = ranges;

       if (!ranges)
           return;

       for (p = *ranges;  p != NULL; p = p->next) {
           if (p->lo == lo) {
               *prevpp = p->next;
               free(p);
               break;
           }
           prevpp = &(p->next);
       }
   }
   /*
       heap_extend - extend heap when there is no free space to malloc
       allocate have to be even, use mem_sbrk and set free block header and footer and call coalesce
       */
   static void *heap_extend(size_t words)
   {
       char *bp;
       size_t size = (words % 2) ? (words + 1) * WSIZE : words * WSIZE;
       // allocate heap use mem_sbrk
       if((long)(bp = mem_sbrk(size)) == -1){
           return NULL;
       }
       // set header and footer and call coalesce
       PUT(HDRP(bp), PACK(size, 0));
       PUT(FTRP(bp), PACK(size, 0));
       PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1));

       return coalesce(bp);
   }
   /*
       segregate_insert - insert free block to segregate list after find appropriate segregate class
       insert is sort by size.
       */
   static void segregate_insert(char *bp) {
       char *curr, *prev = NULL;   // current, previous pointer
       int i;
        size_t size = GET_SIZE (HDRP (bp));
       /* find appropriate segregated class */
       for (i = 0; (i < LIST_MAX -1) && (size > 1); i++) {
           size >>= 1;
       }
       SET_NEXTP(bp, NULL);
       SET_PREVP(bp, NULL);
       size_t alloc_size = GET_SIZE(HDRP(bp));
       /* search appropriate block locate to segregate insert in segregated class */
       curr = GET_SEG_CLASS (i, seg_list_pointer);
       for (;curr != NULL; curr = GET_NEXTP (curr)) {
           if (alloc_size > GET_SIZE (HDRP (curr))) prev = curr;
           else break;
       }

       /* If found appropriate locate to insert  */
       if (prev == NULL) {
           if (curr == NULL) {         // 1. segregated class is empty
               GET_SEG_CLASS (i, seg_list_pointer) = bp;
           }
           else {               // 2.  bp is first node of list
               SET_NEXTP (bp, curr);
               SET_PREVP (curr, bp);
               GET_SEG_CLASS (i, seg_list_pointer) = bp;
           }
       }
       else if (curr == NULL){            // 3. bp is last node of list.
           SET_PREVP (bp, prev);
           SET_NEXTP (prev, bp);
       }
       else {                  // 4. bp is middle of list. there is prev, curr node.
           SET_NEXTP (prev, bp);
           SET_PREVP (bp, prev);
           SET_NEXTP (bp, curr);
           SET_PREVP (curr, bp);
       }

   }
   /*
       segregate_delete - delete segregate node from segregate list before alloc
       */
   static void *segregate_delete(void *bp) {
       size_t size = GET_SIZE (HDRP (bp));
       int i;
       char *next = GET_NEXTP (bp);
       char *prev = GET_PREVP (bp);

       /* find segregated list */
       for (i = 0; (i < LIST_MAX -1) && (size > 1); i++) {
           size >>= 1;
       }

       if (next == NULL && prev == NULL) {            // 1. list had only bp.
               GET_SEG_CLASS (i, seg_list_pointer) = NULL;
       }
       else if(next == NULL && prev != NULL){
            SET_NEXTP (prev, NULL);      // 2. bp was first node of list.
       }
       else if (prev == NULL){            // 3. bp was last node of list.
           SET_PREVP (next, NULL);
           GET_SEG_CLASS (i, seg_list_pointer) = next;
       }
       else {                  // 4. bp was in the middle of list. there was prev, next node.
           SET_NEXTP (prev, next);
           SET_PREVP (next, prev);
       }
       return bp;
   }
   /*
       coalesce - if block is freed, check there is front or next block which is free.
       If front or next block is free coalesce it and insert it in
       the segregated list.
       */
   static void *coalesce(void *bp)
   {
       //check is next block allocated
       size_t next = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
       //check is previous block is allocated
       size_t prev = GET_ALLOC(FTRP(PREV_BLKP(bp)));
       size_t size = GET_SIZE(HDRP(bp));

       if(prev && next){       // 1. prev and next block is allocated
           segregate_insert(bp);
           return bp;
       }
       else if(prev && !next){     // 2. prev block is allocated
           size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
          segregate_delete(NEXT_BLKP(bp));
           PUT(HDRP(bp), PACK(size, 0));
           PUT(FTRP(bp), PACK(size, 0));
       }
       else if(!prev && next){     // 3. next block is allocated
           size += GET_SIZE(HDRP(PREV_BLKP(bp)));
           segregate_delete(PREV_BLKP(bp));
           PUT(FTRP(bp), PACK(size, 0));
           PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
           bp = PREV_BLKP(bp);
       }
       else {          // 4. both block is freed
           size += GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(FTRP(NEXT_BLKP(bp)));
           segregate_delete(NEXT_BLKP(bp));
           segregate_delete(PREV_BLKP(bp));
           PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
           PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
           bp = PREV_BLKP(bp);
       }
       segregate_insert(bp);
       return bp;
   }
   /*
       allocate - after find appropriate block or extend heap and find appropriate block delete block from segregated list, and now
       allocate new block.
       */
   static void allocate(char *bp, size_t alloc_size)
   {
       size_t free_size = GET_SIZE(HDRP(bp));
       //when allocate check if there is enough place to split
       if((free_size - alloc_size) >= 2*DSIZE){    //space is enough to split
           PUT(HDRP(bp), PACK(alloc_size, 1));
           PUT(FTRP(bp), PACK(alloc_size, 1));
           bp = NEXT_BLKP(bp);
           PUT(HDRP(bp), PACK(free_size-alloc_size, 0));
           PUT(FTRP(bp), PACK(free_size-alloc_size, 0));
           segregate_insert(bp);
       }
       else{       //space is not enough to split
           PUT(HDRP(bp), PACK(free_size, 1));
           PUT(FTRP(bp), PACK(free_size, 1));
       }
   }
   /*
       find_seg - Find appropriate free block from segregated list. find fast use size
       */
   static void *find_seg(size_t alloc_size){
       void *bp;
       size_t size;
       size = alloc_size;
       int i;
       for(i = 0; i < LIST_MAX; i++){
           if(i == LIST_MAX - 1 || ((GET_SEG_CLASS(i, seg_list_pointer) != NULL) && size <= 1)){
               for(bp = GET_SEG_CLASS(i, seg_list_pointer); bp != NULL && alloc_size > GET_SIZE(HDRP(bp)); bp = GET_NEXTP(bp));
           }
           size >>= 1;
           if(bp != NULL)
               return bp;
       }
       return NULL;
   }

   /*
    * mm_init - initialize dynamic allocator
    */
   int mm_init(range_t **ranges)
   {
       /* YOUR IMPLEMENTATION */
       int i;
       //extend heap to allocate segregated class pointer and prolog, epilogue head and footer.
       if((heap_list_pointer = mem_sbrk(24*WSIZE)) == (void *) -1)
           return -1;
       // initial seg class pointer
       PUT(heap_list_pointer, 0);
       for(i = 1; i <= LIST_MAX; i++)
           *(char **)(heap_list_pointer + i*WSIZE) = NULL;

       seg_list_pointer = heap_list_pointer+ WSIZE;        // set prologue header and footer
       PUT(heap_list_pointer + (21*WSIZE), PACK(DSIZE, 1));
       PUT(heap_list_pointer + (22*WSIZE), PACK(DSIZE, 1));
       PUT(heap_list_pointer + (23*WSIZE), PACK(0, 1));         //epilogue header

       heap_list_pointer += 22*WSIZE;
       // extend heap for first empty heap by chunksize
       if(heap_extend(CHUNKSIZE / WSIZE) == NULL){
           return -1;
       }
       /* DON't MODIFY THIS STAGE AND LEAVE IT AS IT WAS */
       gl_ranges = ranges;
       return 0;
   }

   /*
    * mm_malloc - alloc block if find appropriate seg, seg delete and allocate. else extend heap and seg delete and allocate.
    *     Always allocated block size is a multiple of the alignment which is 8
    */
   void* mm_malloc(size_t size)
   {
       size_t alloc_size; // adjusted block size
       size_t extendsize; // extend heap if no fit
       char *bp;
       if(size <= 0)
           return NULL;

       if(size <= DSIZE)
           alloc_size = 2*DSIZE;
       else
           alloc_size = DSIZE * ((size + (DSIZE) + (DSIZE-1)) / DSIZE);
       if((bp = find_seg(alloc_size)) != NULL){
           segregate_delete(bp);
           allocate(bp, alloc_size);
           return bp;
       }

       extendsize = MAX(alloc_size, CHUNKSIZE);
       if((bp = heap_extend(extendsize / WSIZE)) == NULL){
           return NULL;
       }
       segregate_delete(bp);
       allocate(bp, alloc_size);
       return bp;
   }

   /*
    * mm_free - Freeing a block and call coalesce.
    */
   void mm_free(void *ptr)
   {
       /* YOUR IMPLEMENTATION */
       size_t size = GET_SIZE(HDRP(ptr));

       PUT(HDRP(ptr), PACK(size, 0));
       PUT(FTRP(ptr), PACK(size, 0));
       coalesce(ptr);

       /* DON't MODIFY THIS STAGE AND LEAVE IT AS IT WAS */
       if (gl_ranges){
           remove_range(gl_ranges, ptr);
       }
   }

   /*
    * mm_realloc - empty implementation; YOU DO NOT NEED TO IMPLEMENT THIS
    */
   void* mm_realloc(void *ptr, size_t t)
   {
       return NULL;
   }
   /*
    * mm_exit - for exit free all alloc malloc package.
    */
   void mm_exit(void)
   {
       char *p = NEXT_BLKP(heap_list_pointer);
       while(GET_SIZE(HDRP(p)) != 0){
           if(GET_ALLOC(HDRP(p))){
               mm_free(p);
           }
           p = NEXT_BLKP(p);
       }
   }
   /*
       mm_check - check if there is error on imple 1. free is coalescing appropriate 2. check is all seg list point valid free block
       */
   static int mm_check (){
   	int i=0;
   	char *p = NEXT_BLKP (heap_list_pointer);	// check all free block is coalescing appropriately
   	char *prev = NULL;
   	while (GET_SIZE (HDRP (p)) != 0) {
   		if (GET_ALLOC (HDRP (p)) == 0) {
   			if (prev != NULL) {
   				printf ("there is some free block escaped from coalesce\n");
   				abort();
   			}
   			prev = p;
   		}
   		else prev = NULL;
   		p = NEXT_BLKP (p);
   	}

   	char *iter= GET_SEG_CLASS(i, seg_list_pointer);		//check is all seg list point valid free block and mark
   	while (i<LIST_MAX) {
   		while (iter != NULL){
   			if ((heap_list_pointer > iter) | (iter > mem_heap_hi())) {
   				printf ("lies out of heap \n");
   				abort();
   			}
   			if (GET_ALLOC (HDRP (iter)) != 0) {
   				printf ("there is free block which is not marked\n");
   				abort();
   			}
   			iter = GET_NEXTP (iter);
   		}
   		i ++;
   		iter = GET_SEG_CLASS (i,seg_list_pointer);
   	}
   }