/* Compile/disassembly reference ONLY. Do not execute this fixture.
 * These architectural instructions do not establish Tegra APE/A57 visibility.
 * Mapping attributes, privilege, external caches and errata remain platform
 * responsibilities. Never invalidate unowned or dirty neighboring data.
 */
typedef __UINTPTR_TYPE__ addr_t;
typedef __SIZE_TYPE__ size_t;
enum { CACHE_CLEAN=0, CACHE_INVALIDATE=1 };
int cache_range_shape(addr_t start,size_t length,size_t line_size){
    if(!line_size || (line_size&(line_size-1)) || !length)return 0;
    if((start&(line_size-1)) || (length&(line_size-1)))return 0;
    if(length>(addr_t)-1-start)return 0;
    return 1;
}
#if defined(__arm__) || defined(__aarch64__)
static void clean_line(addr_t va){
#if defined(__aarch64__)
    __asm__ volatile("dc cvac, %0"::"r"(va):"memory");
#else
    __asm__ volatile("mcr p15, 0, %0, c7, c10, 1"::"r"(va):"memory");
#endif
}
static void invalidate_line(addr_t va){
#if defined(__aarch64__)
    __asm__ volatile("dc ivac, %0"::"r"(va):"memory");
#else
    __asm__ volatile("mcr p15, 0, %0, c7, c6, 1"::"r"(va):"memory");
#endif
}
int cache_range_reference(addr_t start,size_t length,size_t line_size,int operation){
    if(!cache_range_shape(start,length,line_size) ||
       (operation!=CACHE_CLEAN && operation!=CACHE_INVALIDATE))return 0;
    __asm__ volatile("dsb sy":::"memory");
    for(size_t off=0;off<length;off+=line_size){
        if(operation==CACHE_CLEAN)clean_line(start+off);
        else invalidate_line(start+off);
    }
    __asm__ volatile("dsb sy":::"memory");
    return 1;
}
#endif
#ifdef CACHE_SHAPE_TEST
#include <assert.h>
#include <stdio.h>
int main(void){
    assert(cache_range_shape(0x1000,64,32));
    assert(cache_range_shape(0x1000,64,64));
    assert(!cache_range_shape(0x1001,64,64));
    assert(!cache_range_shape(0x1000,63,64));
    assert(!cache_range_shape(0x1000,64,0));
    assert(!cache_range_shape(0x1000,64,3));
    assert(!cache_range_shape(0x1000,0,64));
    assert(!cache_range_shape((addr_t)-64,64,64));
    puts("PASS: cache range shape rejects unsafe boundaries");
    return 0;
}
#endif
