#pragma once
#include <cstdint>
#include <cstddef>
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
struct FsDir { int unused; };
enum FsDirEntryType { FsDirEntryType_Dir=0, FsDirEntryType_File=1 };
enum { FsDirOpenMode_ReadDirs=1, FsDirOpenMode_ReadFiles=2 };
struct FsDirectoryEntry { char name[FS_MAX_PATH]; std::uint8_t pad[3]; std::int8_t type; std::uint8_t pad2[3]; s64 file_size; };
Result fsFsGetEntryType(FsFileSystem*,const char*,FsDirEntryType*);
Result fsFsOpenDirectory(FsFileSystem*,const char*,u32,FsDir*);
Result fsDirRead(FsDir*,s64*,std::size_t,FsDirectoryEntry*);
void fsDirClose(FsDir*);
