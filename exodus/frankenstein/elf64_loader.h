#ifndef TUMOR_ELF64_LOADER_H
#define TUMOR_ELF64_LOADER_H
#include <stddef.h>
#include <stdint.h>
enum { ELF64_MAX_SEGMENTS=16, ELF64_PAGE=4096, ELF64_R=4, ELF64_W=2, ELF64_X=1 };
struct elf64_segment { uint64_t va, file_offset, filesz, memsz; uint32_t flags; };
struct elf64_plan { uint64_t entry; size_t count; struct elf64_segment seg[ELF64_MAX_SEGMENTS]; };
/* Caller supplies immutable source bytes and an aligned, exclusive guest VA window.
 * No dynamic linkage, relocations, interpreter, TLS or guest page tables. */
int elf64_plan_image(const uint8_t *src,size_t len,uint64_t lo,uint64_t hi,struct elf64_plan *out);
/* Caller supplies exclusive backing storage disjoint from source and output.
 * On validation failure backing storage and output are untouched.
 * On success the whole backing window is zeroed and PT_LOAD data copied. */
int elf64_load_image(const uint8_t *src,size_t len,uint64_t lo,uint64_t hi,
                     uint8_t *backing,size_t capacity,struct elf64_plan *out);
#endif
