#include "sd_provider.hpp"
#include <cassert>
#include <cstring>
#include <iostream>
using namespace exodus::vfs;
static int mounts,unmounts,opens,closes,reads;
static bool mount_fail,open_fail,size_fail,read_fail,bad_count;
static std::string last;
Result fsOpenSdCardFileSystem(FsFileSystem*){++mounts;return mount_fail;}
void fsFsClose(FsFileSystem*){++unmounts;}
Result fsFsOpenFile(FsFileSystem*,const char* p,u32 mode,FsFile*){assert(mode==FsOpenMode_Read);last=p;++opens;return open_fail;}
void fsFileClose(FsFile*){++closes;}
Result fsFileGetSize(FsFile*,s64* size){*size=11;return size_fail;}
Result fsFileRead(FsFile*,s64 off,void* buf,u64 n,u32 option,u64* count){
    ++reads;assert(option==FsReadOption_None);assert(off>=0&&off+static_cast<s64>(n)<=11);
    if(read_fail)return 1;
    std::memcpy(buf,"hello world"+off,n);*count=bad_count?n+1:n;return 0;
}
int main(){
    int checks=0;
    auto check=[&](bool b){++checks;assert(b);};
    std::uint8_t buf[16]{};
    {
        SdProvider p;
        check(p.read("x",0,buf,1).error==Error::unavailable);
        mount_fail=true;check(!p.open());mount_fail=false;
        check(p.open());check(p.open());check(mounts==2);
        auto r=p.read("folder/hello",6,buf,12);
        check(r.error==Error::ok&&r.count==5&&!std::memcmp(buf,"world",5));
        check(last=="/switch/exodus/data/folder/hello");
        check(opens==1&&closes==1&&reads==1);
        r=p.read("hello",11,buf,2);check(r.error==Error::ok&&r.count==0);
        check(opens==2&&closes==2&&reads==1);
        for(auto path:{"","/a","../a","a/../b","a//b","a/","a\\b","a:b","a%2f","a.","a ","a?","a*","a<","a|","a\"","\xc3\xa9"})
            check(p.read(path,0,buf,1).error==Error::invalid_path);
        check(p.read(std::string(256,'a'),0,buf,1).error==Error::invalid_path);
        check(p.read(std::string("a\0b",3),0,buf,1).error==Error::invalid_path);
        check(p.read("hello",UINT64_MAX,buf,1).error==Error::invalid_buffer);
        check(p.read("hello",INT64_MAX,buf,1).error==Error::invalid_buffer);
        check(p.read("hello",0,nullptr,1).error==Error::invalid_buffer);
        check(p.read("hello",0,buf,1024*1024+1).error==Error::invalid_buffer);
        check(p.read("hello",0,nullptr,0).error==Error::ok);
        check(opens==2);
        open_fail=true;check(p.read("hello",0,buf,1).error==Error::backend_error);open_fail=false;
        check(opens==3&&closes==2);
        size_fail=true;check(p.read("hello",0,buf,1).error==Error::backend_error);size_fail=false;
        check(closes==3);
        read_fail=true;check(p.read("hello",0,buf,1).error==Error::backend_error);read_fail=false;
        check(closes==4);
        bad_count=true;check(p.read("hello",0,buf,1).error==Error::backend_error);bad_count=false;
        check(closes==5);
        Broker broker({{"sd",&p,mask(Op::read),true}});
        check(broker.read("/sd/hello",0,buf,5).count==5);
        check(broker.route("/sd/hello",Op::write).error==Error::denied);
        p.close();p.close();check(unmounts==1);
        check(p.read("hello",0,buf,1).error==Error::unavailable);
        check(p.open());
    }
    check(unmounts==2);
    std::cout<<checks<<" checks passed\n";
}

// R14 metadata functions are linked by the virtual table even in read-only tests.
Result fsFsGetEntryType(FsFileSystem*,const char*,FsDirEntryType*){return 1;}
Result fsFsOpenDirectory(FsFileSystem*,const char*,u32,FsDir*){return 1;}
Result fsDirRead(FsDir*,s64*,std::size_t,FsDirectoryEntry*){return 1;}
void fsDirClose(FsDir*){}
