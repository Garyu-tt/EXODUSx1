#ifndef NX_TEST_SWITCH_H
#define NX_TEST_SWITCH_H
#include <stdint.h>
typedef uint32_t Handle;
typedef uint32_t Result;
typedef int32_t s32;
#define R_SUCCEEDED(r) ((r)==0)
#define R_VALUE(r) ((r)&0x3fffffU)
#define KERNELRESULT(x) (1U | (KernelError_##x << 9))
enum { KernelError_TimedOut=117, KernelError_Cancelled=118, KernelError_InvalidHandle=114 };
Result svcWaitSynchronization(s32 *, const Handle *, s32, uint64_t);
#endif
