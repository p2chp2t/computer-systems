/*
 *  * mm-naive.c - The fastest, least memory-efficient malloc package.
 *   * 
 *    * In this naive approach, a block is allocated by simply incrementing
 *     * the brk pointer.  A block is pure payload. There are no headers or
 *      * footers.  Blocks are never coalesced or reused. Realloc is
 *       * implemented directly using mm_malloc and mm_free.
 *        *
 *         * NOTE TO STUDENTS: Replace this header comment with your own header
 *          * comment that gives a high level description of your solution.
 *           * 
 *            * 20220041 Yoojin Kim
 *             */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

#include "mm.h"
#include "memlib.h"

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)


#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

/* Basic constants and Macros */
#define WSIZE 4 /* Word and header/footer size (bytes) */
#define DSIZE 8 /* Double word size (bytes) */
#define CHUNKSIZE (1<<12) /* Extend heap by this amount (bytes) */
#define LISTLIMIT 20 /* Max size of list */

#define MAX(x, y) ((x) > (y) ? (x) : (y))

/* Pack a size and allocated bit into a word */
#define PACK(size, alloc) ((size) | (alloc))

/* Read and write a word at address p */
#define GET(p) (*(unsigned int *)(p))
#define PUT(p, val) (*(unsigned int *)(p) = (val))

/* Read the size and allocated fields from address p */
#define GET_SIZE(p) (GET(p) & ~0x7)
#define GET_ALLOC(p) (GET(p) & 0x1)

/* Read the size and allocated fields from address p */
#define HDRP(bp) ((char *)(bp) - WSIZE)
#define FTRP(bp) ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)

/* Given block ptr bp, compute address of its header and footer */
#define NEXT_BLKP(bp) ((char *)(bp) + GET_SIZE(((char *)(bp) - WSIZE)))
#define PREV_BLKP(bp) ((char *)(bp) - GET_SIZE(((char *)(bp) - DSIZE)))

/* Predecessor and successor pointer of free list */
#define PRED_FREE(bp) (*(void**)(bp))
#define SUCC_FREE(bp) (*(void**)(bp + WSIZE))

/* Global variables */
static void *heap_listp;
static void *segreg_list[LISTLIMIT];

/* Additional functions */
static void *extend_heap(size_t words);
static void *coalesce(void *bp);
static void *find_fit(size_t asize);
static void place(void *bp, size_t asize);
static void insert_block(void *bp, size_t size);
static void remove_block(void *bp);

/* mm_check related functions */
static int mm_check(void);
static int check_block(void *ptr);
static int check_segreg_list(void *ptr, size_t size);

/* 
 *  * mm_init - initialize the malloc package.
 *   * Explanation:
 *    *  Initializes segregation list and allocate heap.
 *     *  Returns -1 if there is a problem, otherwise 0.
 *      */
int mm_init(void)
{
    /* initialize segregation list */
    int i;
    for(i = 0; i < LISTLIMIT; i++) {
        segreg_list[i] = NULL;
    }
    /* create the initial empty heap */
    if((heap_listp = mem_sbrk(4*WSIZE)) == (void*)-1) {
        return -1;
    }
    PUT(heap_listp, 0); /* Alignment padding */
    PUT(heap_listp + (1*WSIZE), PACK(DSIZE, 1)); /* Prologue header */
    PUT(heap_listp + (2*WSIZE), PACK(DSIZE, 1)); /* Prologue footer */
    PUT(heap_listp + (3*WSIZE), PACK(0, 1)); /* Epilogue header */
    heap_listp += (2*WSIZE);
    /* extend the empty heap with a free block of CHUNKSIZE bytes */
    if(extend_heap(CHUNKSIZE/WSIZE) == NULL) {
        return -1;
    }
    return 0;
}


/* 
 *  * mm_malloc - Allocate a block by incrementing the brk pointer.
 *   *     Always allocate a block whose size is a multiple of the alignment.
 *    * Explanation:
 *     *  Allocates a block by searching for appropriate free block in segreg_list.
 *      *  If fit is found, it places the block. Otherwise extends the heap.
 *       *  Returns a pointer to allocated memory.
 *        */
void *mm_malloc(size_t size)
{
    int newsize = ALIGN(size + SIZE_T_SIZE);
    char *bp;
    size_t extendsize;
    /* search the free list */
    if((bp = find_fit(newsize)) != NULL) {
        place(bp, newsize);
        return bp;
    }
    /* no fit found, get more memory and place the block */
    extendsize = MAX(newsize, CHUNKSIZE);
    if((bp = extend_heap(extendsize/WSIZE)) == NULL) {
        return NULL;
    }
    place(bp, newsize);
    return bp;
}

/*
 *  * mm_free - Freeing a block does nothing.
 *   * Explanation:
 *    *  Free the given block by updating header and footer, then coalesce.
 *     */
void mm_free(void *ptr)
{
    if(ptr == NULL) {
        return;
    }
    size_t size = GET_SIZE(HDRP(ptr));
    PUT(HDRP(ptr), PACK(size, 0));
    PUT(FTRP(ptr), PACK(size, 0));
    coalesce(ptr);
}

/*
 *  * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 *   * Explanation: 
 *    *  Reallocate a block by either extending, splitting, or allocating a new block.
 *     *  Copy old data if needed.
 *      */
void *mm_realloc(void *ptr, size_t size)
{
    size_t oldsize;
    void *newptr;
    if(size == 0) {
        mm_free(ptr);
        return NULL;
    }
    if(ptr == NULL) {
        return mm_malloc(size);
    }
    size_t curr_size = GET_SIZE(HDRP(ptr));
    size_t total_size = size + 2*WSIZE; /* include header and footer */
    if(size < curr_size - 2*WSIZE) { /* no need to reallocate */
        return ptr;
    }
    void *next = NEXT_BLKP(ptr);
    int next_alloc = GET_ALLOC(HDRP(next));
    size_t next_size = GET_SIZE(HDRP(next));
    /* check if coalescing is possible */
    if(!next_alloc && total_size <= (curr_size + next_size - 2*WSIZE)) {
        size_t copySize = curr_size + next_size;
        remove_block(next);
        PUT(HDRP(ptr), PACK(copySize, 1));
        PUT(FTRP(ptr), PACK(copySize, 1));
        return ptr;
    }
    /* if next block is free, but not enough to coalesce, allocate new block */
    if (!next_alloc && next_size == 0) {
        size_t remainder = curr_size + next_size - total_size;
        size_t extendSize = MAX(-remainder, CHUNKSIZE);
        if (remainder < 0) {
            if (extend_heap(extendSize) == NULL) {
                return NULL;
            }
            remainder += extendSize;
        }
        remove_block(next);
        PUT(HDRP(ptr), PACK(curr_size + remainder, 1));
        PUT(FTRP(ptr), PACK(curr_size + remainder, 1));
        return ptr;
    }
    /* allocate new block */
    newptr = mm_malloc(size);
    if(newptr == NULL) {/* allocation fail */
        return NULL; 
    }
    /* copy old data */
    oldsize = curr_size;
    if (size < oldsize) {
        oldsize = size;
    }     
    memcpy(newptr, ptr, oldsize);
    mm_free(ptr);
    return newptr;
}

/* Additional functions */
/*
 *  * extend_heap
 *   * Explanation: 
 *    *  Extend the heap by specified number of words, creating a new free block.
 *     *  Return block pointer of new free block.
 *      *  
 *      */
static void *extend_heap(size_t words)
{
    char *bp;
    size_t size = ALIGN(words * WSIZE);

    /* allocate an even number of words to maintain alignment */
    if((long)(bp = mem_sbrk(size)) == -1) {
        return NULL;
    }
    /* initialize free block header/footer and the epilogue header */
    PUT(HDRP(bp), PACK(size, 0)); /* Free block header */
    PUT(FTRP(bp), PACK(size, 0)); /* Free block footer */
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1)); /* New epilogue header */

    /* coalesce if the previous block was free */
    return coalesce(bp);
}

/*
 *  * coalesce
 *   * Explanation: 
 *    *  Coalesce a free block with its adjacent free blocks to create larger free blocks.
 *     *  Return block pointer of coalesced block.
 *      *  
 *      */
static void *coalesce(void *bp)
{
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp))); /* Previous block footer */
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp))); /* Next block header */
    size_t size = GET_SIZE(HDRP(bp));

    if(prev_alloc && next_alloc) { /* prev: alloc, next: alloc */
        insert_block(bp, size);
        return bp;
    }
    else if(prev_alloc && !next_alloc) { /* prev: alloc, next: free */
        remove_block(NEXT_BLKP(bp));
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
    }
    else if(!prev_alloc && next_alloc) { /* prev: free, next: alloc */
        remove_block(PREV_BLKP(bp));
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));
        PUT(FTRP(bp), PACK(size, 0));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }
    else { /* prev: free, next: free */
        remove_block(PREV_BLKP(bp));
        remove_block(NEXT_BLKP(bp));
        size += GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(FTRP(NEXT_BLKP(bp)));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }
    insert_block(bp, size);
    return bp;
}

/*
 *  * find_fit
 *   * Explanation: 
 *    *  Find a free block in the segregated free list that is sufficient to admit the given size.
 *     *  Return block pointer of the fit if it is found, otherwise NULL.
 *      *  
 *      */
static void *find_fit(size_t asize) 
{
    /* first-fit search */
    void *bp;
    int i;
    for (i = 0; i < LISTLIMIT; i++) {
        bp = segreg_list[i];
        while (bp != NULL) {
            size_t block_size = GET_SIZE(HDRP(bp));
            if (asize <= block_size) {
                return bp;
            }
            bp = SUCC_FREE(bp);
        }
    }
    return NULL;
}

/*
 *  * place
 *   * Explanation: 
 *    *  Place the allocated block in the heap after finding a fit in segreg_list.
 *     *  Split the block if it is larger than needed.
 *      *  
 *      */
static void place(void *bp, size_t asize)
{
    size_t csize = GET_SIZE(HDRP(bp));
    /* rearrange free list */
    remove_block(bp);
    if((csize - asize) >= 2*(SIZE_T_SIZE)) { /* split the block */
        PUT(HDRP(bp), PACK(asize, 1));
        PUT(FTRP(bp), PACK(asize, 1));
        bp = NEXT_BLKP(bp);
        PUT(HDRP(bp), PACK(csize-asize, 0));
        PUT(FTRP(bp), PACK(csize-asize, 0));
        insert_block(bp, csize - asize);
    }
    else {
        PUT(HDRP(bp), PACK(csize, 1));
        PUT(FTRP(bp), PACK(csize, 1));
    }
}

/*
 *  * insert_block
 *   * Explanation: 
 *    *  Insert free block into segreg_list based on its size.
 *     *  
 *     */
static void insert_block(void *bp, size_t size)
{
    int i = 0;
    size_t target_size = size;
    void *target_ptr = NULL;
    void *insert_ptr = NULL; 
    /* find the appropriate list in the segregated free list */
    while((i < LISTLIMIT - 1) && (target_size > 1)) {
        target_size >>= 1;
        i++;
    }
    /* find the correct poisition in the list to insert the block */
    target_ptr = segreg_list[i];
    while((target_ptr != NULL) && (size > GET_SIZE(HDRP(target_ptr)))) {
        insert_ptr = target_ptr;
        target_ptr = SUCC_FREE(target_ptr);
    }
    /* update pointers to insert the block in the list */
    if(target_ptr != NULL) {
        if(insert_ptr != NULL) { /* insert btw insert_ptr and target_ptr */
            PRED_FREE(bp) = insert_ptr;
            SUCC_FREE(bp) = target_ptr;
            PRED_FREE(target_ptr) = bp;
            SUCC_FREE(insert_ptr) = bp;
        }
        else { /* insert at the front of the list */
            PRED_FREE(bp) = NULL;
            SUCC_FREE(bp) = target_ptr;
            PRED_FREE(target_ptr) = bp;
            segreg_list[i] = bp;
        }
    }
    else {
        if(insert_ptr != NULL) { /* insert at the end of the list */
            PRED_FREE(bp) = insert_ptr;
            SUCC_FREE(bp) = NULL;
            SUCC_FREE(insert_ptr) = bp;
        }
        else { /* first block in the list */
            PRED_FREE(bp) = NULL;
            SUCC_FREE(bp) = NULL;
            segreg_list[i] = bp;
        }
    }
    return;
}

/*
 *  * remove_block
 *   * Explanation: 
 *    *  Remove block from segreg_list.
 *     *  
 *     */
static void remove_block(void *bp) 
{
    int i = 0;
    size_t size = GET_SIZE(HDRP(bp));
    /* find the appropriate list in the segregated free list */
    while((i < LISTLIMIT - 1) && (size > 1)) {
        size >>= 1;
        i++;
    }
    if(SUCC_FREE(bp) != NULL) {
        if(PRED_FREE(bp) != NULL) { /* remove block in the middle */
            PRED_FREE(SUCC_FREE(bp)) = PRED_FREE(bp);
            SUCC_FREE(PRED_FREE(bp)) = SUCC_FREE(bp);
        }
        else { /* remove block at the front of the list */
            PRED_FREE(SUCC_FREE(bp)) = NULL;
            segreg_list[i] = SUCC_FREE(bp);
        }
    }
    else {
        if(PRED_FREE(bp) != NULL) { /* remvove block at the end of the list */
            SUCC_FREE(PRED_FREE(bp)) = NULL;
        }
        else { /* when it was the only block in the list, nullify */
            segreg_list[i] = NULL;
        }
    }
    return;
}

/* Heap Consistency Checker */
/*
 *  * check_block
 *   * Explanation: 
 *    *  Check alignment and consistency of a single block.
 *     *  Return 1 if the block is consistent, otherwise 0.
 *      *  
 *      */
static int check_block(void *bp)
{
    /* Check if the block is properly aligned */
    if((size_t)bp % ALIGNMENT) {
        printf("Error: Block at address %p is not properly aligned\n", bp);
        return 0;
    }
    /* Check if the header and footer sizes match */
    size_t header_size = GET_SIZE(HDRP(bp));
    size_t footer_size = GET_SIZE(FTRP(bp));
    if (header_size != footer_size) {
        printf("Error: Header and footer size mismatch for block at address %p\n", bp);
        return 0;
    }
    return 1;
}

/*
 *  * check_segreg_list
 *   * Explanation: 
 *    *  Check consistency of segreg_list.
 *     *  Return 1 if the list is consistent, otherwise 0.
 *      *  
 *      */
static int check_segreg_list(void *ptr, size_t size)
{
    void *prev = NULL;
    for (; ptr != NULL; ptr = SUCC_FREE(ptr)) {
        if (PRED_FREE(ptr) != prev) {
            printf("Error: Predecessor pointer mismatch in segregated list of size %zu\n", size);
            return 0;
        }
        if (prev != NULL && SUCC_FREE(prev) != ptr) {
            printf("Error: Successor pointer mismatch in segregated list of size %zu\n", size);
            return 0;
        }
        /* check if block size is within the range */
        size_t block_size = GET_SIZE(HDRP(ptr));
        if (block_size < size || (size != LISTLIMIT && block_size > (size << 1) - 1)) {
            printf("Error: Block size in segregated list of size %zu is incorrect\n", size);
            return 0;
        }
        prev = ptr;
    }
    return 1;
}

/*
 *  * mm_check
 *   * Explanation: 
 *    *  Perform a consistency check on entire heap and segreg_list.
 *     *  Return 1 if the heap is consistent, otherwise 0.
 *      *  
 *      */
static int mm_check(void)
{
    char *bp = heap_listp;
    /* prologue header consistency */
    if (GET_SIZE(HDRP(heap_listp)) != DSIZE || !GET_ALLOC(HDRP(heap_listp))) {
        printf("Error: Prologue header is inconsistent\n");
        return 0;
    }
    /* first block consistency */
    if (!check_block(heap_listp)) {
        return 0;
    }
    /* check consistency of each blocks in heap */
    int prev_list = 0;
    for (bp = heap_listp; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp)) {
        if (!check_block(bp)) {
            return 0;
        }
        int curr_list = check_block(bp);
        /* not coalesced */
        if (prev_list && curr_list) {
            printf("Error: Uncoalesced free blocks\n");
            return 0;
        }
    }
    /* check consistency of each segreg_list */
    int i;
    size_t size = 1;
    for (i = 0; i < LISTLIMIT; i++) {
        if (!check_segreg_list(segreg_list[i], size)) {
            return 0;
        }
        size <<= 1;
    }
    /* epilogue header consistency */
    if (GET_SIZE(HDRP(bp)) != 0 || !GET_ALLOC(HDRP(bp))) {
        printf("Error: Epilogue header is inconsistent\n");
        return 0;
    }
    return 1;
}

