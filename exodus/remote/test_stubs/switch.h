#ifndef TEST_SWITCH_H
#define TEST_SWITCH_H
#include <stdint.h>
typedef uint64_t u64;
typedef uint32_t u32;
typedef int64_t s64;
typedef int Result;
typedef struct { int unused; } FsFileSystem;
#define R_FAILED(rc) ((rc)!=0)
#define R_SUCCEEDED(rc) ((rc)==0)
#define CUR_PROCESS_HANDLE 0
#define InfoType_TotalMemorySize 1
#define InfoType_UsedMemorySize 2
static unsigned psm_opens, psm_closes, fs_opens, fs_closes;
static int fail_reads;
static inline Result svcGetInfo(u64 *v, int info, int handle, int sub) { (void)handle; (void)sub; *v=info==1?2000:1000; return fail_reads; }
static inline Result psmInitialize(void) { ++psm_opens; return 0; }
static inline Result psmGetBatteryChargePercentage(u32 *v) { *v=75; return fail_reads; }
static inline void psmExit(void) { ++psm_closes; }
static inline Result fsOpenSdCardFileSystem(FsFileSystem *fs) { (void)fs; ++fs_opens; return 0; }
static inline Result fsFsGetTotalSpace(FsFileSystem *fs,const char *path,s64 *v) { (void)fs; (void)path; *v=1000; return fail_reads; }
static inline Result fsFsGetFreeSpace(FsFileSystem *fs,const char *path,s64 *v) { (void)fs; (void)path; *v=500; return fail_reads; }
static inline void fsFsClose(FsFileSystem *fs) { (void)fs; ++fs_closes; }
#endif
