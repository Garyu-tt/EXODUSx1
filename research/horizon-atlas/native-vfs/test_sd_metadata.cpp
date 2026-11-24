#include "sd_provider.hpp"
#include <cassert>
#include <cstring>
#include <iostream>
using namespace exodus::vfs;
static int opens,closes,queries,dir_opens,dir_closes,dir_reads,cursor,total=2;
static bool stat_fail,dir_fail,size_fail,read_fail,bad_count,bad_name,bad_type,bad_size;
static std::string last;
Result fsOpenSdCardFileSystem(FsFileSystem*){return 0;}
void fsFsClose(FsFileSystem*){}
Result fsFsOpenFile(FsFileSystem*,const char*,u32 mode,FsFile*){assert(mode==FsOpenMode_Read);++opens;return 0;}
void fsFileClose(FsFile*){++closes;}
Result fsFileGetSize(FsFile*,s64* out){*out=42;return size_fail;}
Result fsFileRead(FsFile*,s64,void*,u64,u32,u64*){return 1;}
Result fsFsGetEntryType(FsFileSystem*,const char* p,FsDirEntryType* t){++queries;last=p;*t=last.find("file")!=std::string::npos?FsDirEntryType_File:FsDirEntryType_Dir;return stat_fail;}
Result fsFsOpenDirectory(FsFileSystem*,const char* p,u32 mode,FsDir*){++dir_opens;last=p;cursor=0;assert(mode==(FsDirOpenMode_ReadDirs|FsDirOpenMode_ReadFiles));return dir_fail;}
void fsDirClose(FsDir*){++dir_closes;}
Result fsDirRead(FsDir*,s64* count,std::size_t max,FsDirectoryEntry* e){
    ++dir_reads;assert(max==1);
    if(read_fail)return 1;
    if(bad_count){*count=2;return 0;}
    if(cursor==total){*count=0;return 0;}
    *count=1;
    const auto name="item"+std::to_string(cursor);
    std::memcpy(e->name,name.c_str(),name.size()+1);
    if(bad_name)std::memset(e->name,'x',sizeof(e->name));
    e->type=bad_type?9:(cursor?FsDirEntryType_File:FsDirEntryType_Dir);
    e->file_size=bad_size?-1:42;
    ++cursor;return 0;
}
int main(){
    int checks=0;auto check=[&](bool x){++checks;assert(x);};
    SdProvider p;check(p.stat("").error==Error::unavailable);check(p.list("",1).error==Error::unavailable);check(p.open());
    Broker b({{"sd",&p,mask(Op::read)|mask(Op::list)|mask(Op::stat),true},{"ro",&p,mask(Op::read),true}});
    auto s=b.stat("/sd");check(s.error==Error::ok&&s.metadata.type==EntryType::directory&&s.metadata.size==0);check(last=="/switch/exodus/data");
    s=b.stat("/sd/file");check(s.error==Error::ok&&s.metadata.type==EntryType::file&&s.metadata.size==42);check(opens==1&&closes==1);
    size_fail=true;check(b.stat("/sd/file").error==Error::backend_error);size_fail=false;check(opens==2&&closes==2);
    stat_fail=true;check(b.stat("/sd/file").error==Error::backend_error);stat_fail=false;
    int q=queries;check(b.stat("/ro/file").error==Error::denied);check(b.stat("/sd/../file").error==Error::invalid_path);check(queries==q);
    auto l=b.list("/sd",2);check(l.error==Error::ok&&l.entries.size()==2&&!l.truncated);check(l.entries[0].metadata.type==EntryType::directory&&l.entries[0].metadata.size==0);check(l.entries[1].metadata.size==42&&l.entries[1].name=="item1");check(dir_reads==3&&dir_closes==1);
    l=b.list("/sd/folder",1);check(l.error==Error::ok&&l.entries.size()==1&&l.truncated);check(last=="/switch/exodus/data/folder");check(dir_reads==5&&dir_closes==2);
    total=0;l=b.list("/sd",64);check(l.error==Error::ok&&l.entries.empty()&&!l.truncated);check(dir_closes==3);
    total=100;int old=dir_reads;l=b.list("/sd",64);check(l.error==Error::ok&&l.entries.size()==64&&l.truncated);check(dir_reads-old==65);check(dir_closes==4);
    int o=dir_opens;check(b.list("/sd",0).error==Error::invalid_buffer);check(b.list("/sd",65).error==Error::invalid_buffer);check(b.list("/ro",1).error==Error::denied);check(p.list("../bad",1).error==Error::invalid_path);check(dir_opens==o);
    dir_fail=true;check(b.list("/sd",2).error==Error::backend_error);dir_fail=false;check(dir_closes==4);
    for(bool* flag:{&read_fail,&bad_count,&bad_name,&bad_type,&bad_size}){
        *flag=true;int c=dir_closes;l=b.list("/sd",2);check(l.error==Error::backend_error&&l.entries.empty());check(dir_closes==c+1);*flag=false;
    }
    p.close();check(b.list("/sd",1).error==Error::unavailable);
    std::cout<<checks<<" checks passed\n";
}
