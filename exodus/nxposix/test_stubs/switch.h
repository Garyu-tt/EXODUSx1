/* Test-only subset of libnx. Never include in a Switch build. */
#ifndef NXPOSIX_TEST_SWITCH_H
#define NXPOSIX_TEST_SWITCH_H
#include <stdint.h>
typedef uint32_t Handle;
typedef uint32_t Result;
typedef int32_t s32;
typedef uint64_t u64;
#define INVALID_HANDLE 0
#define MAX_WAIT_OBJECTS 64
#define R_VALUE(r) ((r) & 0x3fffffU)
#define R_FAILED(r) ((r) != 0)
#define KernelError_TimedOut 117
#define KernelError_InvalidHandle 114
#define KERNELRESULT(e) (1U | ((uint32_t)KernelError_##e << 9))
Result waitHandles(s32 *, const Handle *, s32, u64);
#endif
