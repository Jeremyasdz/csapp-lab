/*
 * mm-naive.c - The fastest, least memory-efficient malloc package.
 * 
 * In this naive approach, a block is allocated by simply incrementing
 * the brk pointer.  A block is pure payload. There are no headers or
 * footers.  Blocks are never coalesced or reused. Realloc is
 * implemented directly using mm_malloc and mm_free.
 *
 * NOTE TO STUDENTS: Replace this header comment with your own header
 * comment that gives a high level description of your solution.
 */
//显式空闲链表+LIFO,序言块部分是16字节，4W,前驱后继都指向对应快的bp，这个方案只靠pred个succ指针来跳转方向
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

#include "mm.h"
#include "memlib.h"

/*********************************************************
 * NOTE TO STUDENTS: Before you do anything else, please
 * provide your team information in the following struct.
 ********************************************************/
team_t team = {
    /* Team name */
    "ateam",
    /* First member's full name */
    "Harry Bovik",
    /* First member's email address */
    "bovik@cs.cmu.edu",
    /* Second member's full name (leave blank if none) */
    "",
    /* Second member's email address (leave blank if none) */
    ""
};
/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)


#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))
/* */
#define WSIZE       4       /* Word and header/footer size (bytes) */ //line:vm:mm:beginconst
#define DSIZE       8       /* Doubleword size (bytes) */
#define CHUNKSIZE  (1<<12)  /* Extend heap by this amount (bytes) */  //line:vm:mm:endconst 

#define MAX(x, y) ((x) > (y)? (x) : (y))  

/* Pack a size and allocated bit into a word */
#define PACK(size, alloc)  ((size) | (alloc)) //line:vm:mm:pack

/* Read and write a word at address p */
#define GET(p)       (*(unsigned int *)(p))            //line:vm:mm:get
#define PUT(p, val)  (*(unsigned int *)(p) = (val))    //line:vm:mm:put

/* Read the size and allocated fields from address p */
#define GET_SIZE(p)  (GET(p) & ~0x7)                   //line:vm:mm:getsize
#define GET_ALLOC(p) (GET(p) & 0x1)                    //line:vm:mm:getalloc

/* Given block ptr bp, compute address of its header and footer */
#define HDRP(bp)       ((char *)(bp) - WSIZE)                      //line:vm:mm:hdrp
#define FTRP(bp)       ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE) //line:vm:mm:ftrp


/* Given block ptr bp, compute address of next and previous blocks */
#define NEXT_BLKP(bp)  ((char *)(bp) + GET_SIZE(((char *)(bp) - WSIZE))) //line:vm:mm:nextblkp
#define PREV_BLKP(bp)  ((char *)(bp) - GET_SIZE(((char *)(bp) - DSIZE))) //line:vm:mm:prevblkp


#define SUCC_INDEX(bp) ((char *)(bp) + WSIZE)
#define PRED_INDEX(bp) ((char *)(bp))

static char *heap_listp;
static void *extend_heap(size_t words);
static void *coalesce(void *bp);
static void place(void *bp, size_t asize);
static void *find_fit(size_t asize);
static void *best_fit(size_t asize);
static void* addlist(void *bp);//将合并的链表新增到头
/* */
/* 
 * mm_init - initialize the malloc package.
 */
int mm_init(void)
{
    //printf("进入mm_init\n");
    //6个W的空间，1个W是头,4个W的序言块，1个W的结尾块
    if((heap_listp = mem_sbrk(6*WSIZE)) == (void *)-1){
        //printf("退出mm_init_1\n");
        return -1;
    }
    PUT(heap_listp, 0);
    PUT(heap_listp + (1*WSIZE), PACK(2*DSIZE, 1));//序言块头
    PUT(heap_listp + (2*WSIZE), NULL);
    PUT(heap_listp + (3*WSIZE), NULL);
    PUT(heap_listp + (4*WSIZE), PACK(2*DSIZE, 1));//序言块尾
    PUT(heap_listp + (5*WSIZE), PACK(0, 1));//结尾块-1w

    heap_listp += (2*WSIZE);//相当于第一个块的bp

    if(extend_heap(CHUNKSIZE/WSIZE) == NULL){
        //printf("退出mm_init_2\n");
        return -1;
    }
    //printf("退出mm_init_3\n");
    return 0;
}
static void *extend_heap(size_t words){
    char *bp;
    size_t size;
    //printf("进入extend_heap\n");
    size = (words % 2)? (words + 1) * WSIZE : words * WSIZE;
    if(size < 2*DSIZE)
        size = 2*DSIZE;//显式空闲链表最小空间为4W
    if((long)(bp = mem_sbrk(size)) == (void *)-1){
        //printf("退出extend_heap_1\n");
        return NULL;
    }
    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));

    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1));
    //printf("退出extend_heap_2\n");
    return coalesce(bp);

}
/* 
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size)
{
    size_t asize;
    size_t extendsize;
    char *bp;
    //printf("进入mm_malloc\n");
    
    if(size == 0){
        //printf("退出mm_malloc\n");
        return NULL;
    }
    if(size < DSIZE){
        asize = 2*DSIZE;//8字节满足对齐要求，8字节满足head和foot空间
    }else{
        asize = DSIZE * ((size + DSIZE + (DSIZE - 1)) / DSIZE);//在这里多申请一个DISZE来存放foot和head
    }
    //printf("想找size:%d\n",asize);
    /*
    if((bp = find_fit(asize))!=NULL){
        //printf("退出mm_malloc_1\n");
        place(bp, asize);
        return bp;
    }
   */
    if((bp = best_fit(asize))!=NULL){
        place(bp, asize);
        //printf("退出mm_malloc_1\n");
        return bp;
    }
    extendsize = MAX(asize, CHUNKSIZE);
    if((bp = extend_heap(extendsize/WSIZE)) == NULL){
        //printf("退出mm_malloc_2\n");
        return NULL;
    }
    place(bp, asize);
    //printf("退出mm_malloc_3\n");
    return bp;
}

/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *bp)
{
    //printf("进入mm_free\n");
    size_t size = GET_SIZE(HDRP(bp));
    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));

    //LIFO

    //PUT(SUCC_INDEX(bp), GET(SUCC_INDEX(heap_listp)));//本节点的后继为原heap_listp的后继
    //PUT(PRED_INDEX(GET(SUCC_INDEX(heap_listp))), bp);//修改heap_listp的后继的前驱为本节点
    //PUT(PRED_INDEX(bp), heap_listp);//本节点的前驱为heap_listp
    //PUT(SUCC_INDEX(heap_listp), bp);//头指针的后继修改
    
    coalesce(bp);
    //printf("退出mm_free\n");
}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */

void *mm_realloc(void *ptr, size_t size){
    //printf("进入mm_realloc\n");
    
    void *newptr;
    size_t oldsize = GET_SIZE(HDRP(ptr));
    if(ptr==NULL){
        //printf("退出mm_realloc_1\n");
        return mm_malloc(size);
    }
    if(oldsize == size){
        //printf("退出mm_realloc_2\n");
        return ptr;    
    }
    if(size == 0){
        mm_free(ptr);
        //printf("退出mm_realloc_3\n");
        return NULL;
    }
    if(!(newptr = mm_malloc(size))){
        //printf("退出mm_realloc_4\n");
        return NULL;
    }
    if(size < oldsize)
        oldsize = size;
    memcpy(newptr, ptr, oldsize);
    mm_free(ptr);
    //printf("退出mm_realloc_5\n");
    return newptr;
    
}
static void* addlist(void *bp){//新的空闲块，foot和head已经填写完，需要在逻辑上挂到heap_listp后面
    //printf("进入addlist\n");
    unsigned int succ_num;
    //bp的前驱为heap_listp
    PUT(PRED_INDEX(bp), heap_listp);
    //bp的后继为原heap_listp的后继
    PUT(SUCC_INDEX(bp), GET(SUCC_INDEX(heap_listp)));//第一次运行时，heap_listp中的NULL后继就被继承到这个节点上
    //原heap_listp后继的前驱为bp
    if((succ_num = GET(SUCC_INDEX(heap_listp)))!=NULL)
        PUT(PRED_INDEX(succ_num), bp);
    //heap_listp的后继为bp
    PUT(SUCC_INDEX(heap_listp), bp);
    //printf("退出addlist\n");
    return bp;
}

static void *coalesce(void *bp){//物理上相邻的将其变为逻辑上相邻
    //1.断掉物理上的所有块的逻辑上前后的指针
    //2.逻辑上其前驱后继更新。(addlist)
    //PREV_BLKP(bp)->GET(PRED_INDEX(bp))
    //NEXT_BLKP(bp)->GET(SUCC_INDEX(bp))
    //printf("进入coalesce\n");
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));//物理上的前驱
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));//物理上的后继,不能用foot来求，因为结尾快没有foot
    size_t size = GET_SIZE(FTRP(bp));
    void *bp1,*bp2,*bp3;
    //printf("%d\t%d\t\n",prev_alloc, next_alloc);
    if(prev_alloc && next_alloc){//case 1：两边都在用
        PUT(SUCC_INDEX(bp), NULL);
        PUT(PRED_INDEX(bp), NULL);
        //printf("退出coalesce\n");
    }
    else if(prev_alloc && !next_alloc){//case 2:下面空闲块
        //printf("qk2\n");
        bp1 = bp;
        bp2 = NEXT_BLKP(bp);
        size += (GET_SIZE(HDRP(bp2)));
        //这里是物理上将其相邻，逻辑上需要加pred和succ指针在addlist中
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
        //这里的目的是断开逻辑上的块
        //第一块前驱的后继
        //PUT(SUCC_INDEX(GET(PRED_INDEX(bp1))), GET(SUCC_INDEX(bp1)));
        //第一块后继的前驱
        //PUT(PRED_INDEX(GET(SUCC_INDEX(bp1))), GET(PRED_INDEX(bp1)));
        //第二块前驱的后继
        PUT(SUCC_INDEX(GET(PRED_INDEX(bp2))), GET(SUCC_INDEX(bp2)));
        //第二块后继的前驱
        if(GET(SUCC_INDEX(bp2))!=NULL)
            PUT(PRED_INDEX(GET(SUCC_INDEX(bp2))), GET(PRED_INDEX(bp2)));
        PUT(SUCC_INDEX(bp), NULL);
        PUT(PRED_INDEX(bp), NULL);
    }
    else if(!prev_alloc && next_alloc){//case 3:上面空闲块
        //printf("qk3\n");
        bp1 = PREV_BLKP(bp);
        bp2 = bp;
        size += (GET_SIZE(HDRP(bp1)));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
        //第一块前驱的后继
        PUT(SUCC_INDEX(GET(PRED_INDEX(bp1))), GET(SUCC_INDEX(bp1)));
        //第一块后继的前驱
        if((GET(SUCC_INDEX(bp1)))!=NULL)//如果他没有后继（链表最后一块）则啥也不用管
            PUT(PRED_INDEX(GET(SUCC_INDEX(bp1))), GET(PRED_INDEX(bp1)));
        //第二块前驱的后继
        //PUT(SUCC_INDEX(GET(PRED_INDEX(bp2))), GET(SUCC_INDEX(bp2)));
        //第二块后继的前驱
        //PUT(PRED_INDEX(GET(SUCC_INDEX(bp2))), GET(PRED_INDEX(bp2)));
        bp = bp1;
        PUT(SUCC_INDEX(bp), NULL);
        PUT(PRED_INDEX(bp), NULL);
    }
    else {//case 4:两边都空闲块
        //printf("qk4\n");
        void *bp1 = PREV_BLKP(bp);//第一块
        void *bp2 = bp;//第二块
        void *bp3 = NEXT_BLKP(bp);//第三块

        size += (GET_SIZE(HDRP(bp1)) + GET_SIZE(HDRP(bp3)));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
        
        //第一块前驱的后继
        PUT(SUCC_INDEX(GET(PRED_INDEX(bp1))), GET(SUCC_INDEX(bp1)));

        //第一块后继的前驱
        if((GET(SUCC_INDEX(bp1)))!=NULL)
            PUT(PRED_INDEX(GET(SUCC_INDEX(bp1))), GET(PRED_INDEX(bp1)));
        //第二块前驱的后继
        //PUT(SUCC_INDEX(GET(PRED_INDEX(bp2))), GET(SUCC_INDEX(bp2)));
        //第二块后继的前驱
        //PUT(PRED_INDEX(GET(SUCC_INDEX(bp2))), GET(PRED_INDEX(bp2)));
        //第三块前驱的后继
        PUT(SUCC_INDEX(GET(PRED_INDEX(bp3))), GET(SUCC_INDEX(bp3)));
        //第三块后继的前驱
        if((GET(SUCC_INDEX(bp3)))!=NULL)
            PUT(PRED_INDEX(GET(SUCC_INDEX(bp3))), GET(PRED_INDEX(bp3)));
        bp = bp1;
        PUT(SUCC_INDEX(bp), NULL);
        PUT(PRED_INDEX(bp), NULL);
    }
    //printf("退出coalesce\n");
    return addlist(bp);

}
static void place(void *bp, size_t asize){
    //printf("进入place\n");
    size_t size = GET_SIZE(HDRP(bp));
    void *new_bp;
    if((size - asize) >= (2*DSIZE)){//剩下的空间足以构成一个最小块了。
        //printf("qka\n");
        PUT(HDRP(bp), PACK(asize, 1));
        PUT(FTRP(bp), PACK(asize, 1));
        new_bp = NEXT_BLKP(bp);//根据长度，分割块
        PUT(HDRP(new_bp), PACK(size - asize, 0));
        PUT(FTRP(new_bp), PACK(size - asize, 0));

        PUT(PRED_INDEX(new_bp), GET(PRED_INDEX(bp)));//余下空闲部分的前驱为原本的前驱
        PUT(SUCC_INDEX(new_bp), GET(SUCC_INDEX(bp)));//余下空闲部分的后继为原本的后继
        PUT(SUCC_INDEX(GET(PRED_INDEX(bp))), new_bp);//前驱空闲快的后继为新bp
        if((GET(SUCC_INDEX(bp)))!=NULL)
            PUT(PRED_INDEX(GET(SUCC_INDEX(bp))), new_bp);//后继空闲快的前驱为新bp
        bp = new_bp;
    }
    else{//化为内部碎片 
        //printf("qkb\n");
        if(GET(SUCC_INDEX(bp))!=NULL)
            PUT(PRED_INDEX(GET(SUCC_INDEX(bp))), GET(PRED_INDEX(bp)));//修改后继的前驱(后继空闲快的前驱为自己的前驱)
        PUT(SUCC_INDEX(GET(PRED_INDEX(bp))), GET(SUCC_INDEX(bp)));//修改前驱的后继(前驱空闲快的后继为自己的后继)/如果此节点后继为NULL即，空闲链表自后一个节点，则这个节点的后继NULL则被逻辑上的pred节点继承
        PUT(HDRP(bp), PACK(size, 1));
        PUT(FTRP(bp), PACK(size, 1));
    }
    //printf("退出place\n");
}
static void *find_fit(size_t asize){//首次适配
    //printf("进入find_fit\n");
    void *bp;
    for(bp = heap_listp; bp&&(GET_SIZE(HDRP(bp)) > 0); bp = GET(SUCC_INDEX(bp))){
        if((!GET_ALLOC(HDRP(bp))) && (GET_SIZE(HDRP(bp)) >= asize)){
            //printf("退出find_fit_1\n");
            return bp;
        }
    }
    //printf("退出find_fir_2\n");
    return NULL;
}

static void *best_fit(size_t asize){
    //printf("进入best_fit\n");
    void *bp;
    size_t min_size, size;
    void *best_bp = NULL;
    int flag = 1;
    for(bp = heap_listp; bp&&(size = GET_SIZE(HDRP(bp))) > 0;bp = GET(SUCC_INDEX(bp))){
        //printf("bp:%u\t%u\tsucc:%u\tpred:%u\n",bp,GET(bp),GET(SUCC_INDEX(bp)), GET(PRED_INDEX(bp)));
        if((!GET_ALLOC(HDRP(bp))) && (size >= asize)){
            if(flag == 1){
                min_size = size;
                best_bp = bp;
                flag = 0;
                continue;
            }
            if(size < min_size){
                min_size = size;
                best_bp = bp;
            }
        }
    }
    //printf("退出best_fit\n");
    return best_bp;
}