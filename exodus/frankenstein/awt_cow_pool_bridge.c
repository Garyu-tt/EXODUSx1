#include "cow_pool.h"
static struct cp_pool pool;
uint64_t pool_boot(uint8_t *backing){
    return cp_init(&pool,backing,2*CP_PAGE,2)&&cp_create(&pool,0)&&
        cp_alloc(&pool,0,0)&&pool.map[0][0]==0&&cp_used(&pool)==1;
}
uint64_t pool_clone(void){
    return cp_clone(&pool,0,1)&&cp_used(&pool)==1&&pool.refs[0]==2&&!pool.copies;
}
uint64_t pool_split(uint64_t task){
    unsigned f;
    if(task>1||!cp_split(&pool,(unsigned)task,0,&f))return 0;
    if(task==1 && (f!=1||pool.copies!=1||cp_used(&pool)!=2))return 0;
    if(task==0 && (f!=0||pool.copies!=1||cp_used(&pool)!=1))return 0;
    return (uintptr_t)(pool.bytes+(size_t)f*CP_PAGE);
}
uint64_t pool_drop_child(void){
    return cp_drop(&pool,1)&&cp_used(&pool)==1&&pool.refs[0]==1&&!pool.refs[1];
}
uint64_t pool_finish(void){
    /* Prove released frame can be allocated and scrubbed; its stale
     * contents have already been verified by EL2 before this call. */
    if(!cp_create(&pool,2)||!cp_alloc(&pool,2,0)||pool.map[2][0]!=1)return 0;
    for(unsigned i=0;i<CP_PAGE;i++)if(pool.bytes[CP_PAGE+i])return 0;
    return cp_drop(&pool,2)&&cp_used(&pool)==1&&cp_valid(&pool)&&pool.copies==1;
}
