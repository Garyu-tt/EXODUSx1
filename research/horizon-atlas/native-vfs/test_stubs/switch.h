#pragma once
#include <cstdint>
using s64=std::int64_t; using u64=std::uint64_t; using u32=std::uint32_t;
using Result=unsigned;
#define FS_MAX_PATH 0x301
#define R_FAILED(r) ((r)!=0)
struct FsFileSystem { int unused; };
struct FsFile { int unused; };
enum { FsOpenMode_Read=1, FsReadOption_None=0 };
Result fsOpenSdCardFileSystem(FsFileSystem*);
void fsFsClose(FsFileSystem*);
Result fsFsOpenFile(FsFileSystem*,const char*,u32,FsFile*);
void fsFileClose(FsFile*);
Result fsFileGetSize(FsFile*,s64*);
Result fsFileRead(FsFile*,s64,void*,u64,u32,u64*);
