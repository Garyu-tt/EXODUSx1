#include "sd_provider.hpp"
#include "descriptor_table.hpp"
#include <cassert>
#include <cstring>
#include <iostream>
using namespace exodus::vfs;
static int mounts,unmounts,files,dirs,file_opens,dir_opens,reads,steps;
static bool fail_open,fail_read,bad_name;
Result fsOpenSdCardFileSystem(FsFileSystem*){++mounts;return 0;}
void fsFsClose(FsFileSystem*){assert(!files&&!dirs);++unmounts;}
Result fsFsOpenFile(FsFileSystem*,const char* p,u32 mode,FsFile*){
    assert(mode==FsOpenMode_Read);assert(std::string(p)=="/switch/exodus/data/file");
    if(fail_open)return 1;
    ++files;++file_opens;return 0;
}
void fsFileClose(FsFile*){assert(files>0);--files;}
Result fsFileGetSize(FsFile*,s64* n){*n=6;return 0;}
Result fsFileRead(FsFile*,s64 off,void* buf,u64 n,u32,u64* out){
    ++reads;if(fail_read)return 1;
    assert(off>=0&&off+static_cast<s64>(n)<=6);std::memcpy(buf,"abcdef"+off,n);*out=n;return 0;
}
Result fsFsGetEntryType(FsFileSystem*,const char*,FsDirEntryType*){return 1;}
Result fsFsOpenDirectory(FsFileSystem*,const char* p,u32 mode,FsDir* d){
    assert(std::string(p)=="/switch/exodus/data");assert(mode==3);
    if(fail_open)return 1;
    d->unused=0;++dirs;++dir_opens;return 0;
}
void fsDirClose(FsDir*){assert(dirs>0);--dirs;}
Result fsDirRead(FsDir* d,s64* count,std::size_t max,FsDirectoryEntry* e){
    assert(max==1);++steps;
    if(d->unused==100){*count=0;return 0;}
    auto name="f"+std::to_string(d->unused++);*count=1;
    std::memcpy(e->name,name.c_str(),name.size()+1);e->type=FsDirEntryType_File;e->file_size=6;
    if(bad_name)std::memset(e->name,'x',sizeof(e->name));
    return 0;
}
int main(){
    int n=0;auto check=[&](bool b){++n;assert(b);};std::uint8_t buf[8]{};
    {
        SdProvider p;check(p.open_file("file").error==Error::unavailable);check(p.open());
        Broker b({{"sd",&p,7,true},{"ro",&p,1,true}});DescriptorTable t(b);
        check(t.open("/sd/../file").error==EINVAL);check(p.open_file("a?").error==Error::invalid_path);
        fail_open=true;check(t.open("/sd/file").error==EIO);check(t.opendir("/sd").error==EIO);fail_open=false;check(!files&&!dirs);
        auto f=t.open("/sd/file"),g=t.open("/ro/file");check(!f.error&&!g.error);check(file_opens==2&&files==2);
        check(t.read(f.value,buf,2).value==2&&!std::memcmp(buf,"ab",2));check(t.read(f.value,buf,2).value==2&&!std::memcmp(buf,"cd",2));check(t.read(g.value,buf,2).value==2&&!std::memcmp(buf,"ab",2));check(file_opens==2);
        check(t.fstat(f.value).value.size==6);check(t.fstat(g.value).error==EACCES);check(t.lseek(f.value,-1,Seek::end).value==5);check(t.read(f.value,buf,8).value==1&&buf[0]=='f');check(t.read(f.value,buf,1).value==0);
        fail_read=true;check(t.read(g.value,buf,1).error==EIO);fail_read=false;check(t.lseek(g.value,0,Seek::current).value==2);
        auto d=t.opendir("/sd"),e=t.opendir("/sd");check(!d.error&&!e.error&&dirs==2);check(t.readdir(e.value).value.entry.name=="f0");
        for(int i=0;i<100;++i){auto x=t.readdir(d.value);check(!x.error&&!x.value.eof&&x.value.entry.name=="f"+std::to_string(i));}
        check(t.readdir(d.value).value.eof);int prev=steps;check(t.readdir(d.value).value.eof&&steps==prev);check(dir_opens==2);
        bad_name=true;check(t.readdir(e.value).error==EIO);bad_name=false;check(t.readdir(e.value).error==EIO);
        p.close();check(unmounts==0);check(!p.open());check(t.open("/sd/file").error==ENODEV);check(t.read(f.value,buf,1).error==ENODEV);check(t.fstat(f.value).error==ENODEV);
        check(!t.close(f.value));check(!t.close(g.value));check(!t.close(d.value));check(unmounts==0);check(!t.close(e.value));check(unmounts==1);check(p.open());
        auto f2=t.open("/sd/file");check(!f2.error);auto d2=t.opendir("/sd");check(!d2.error);p.close();check(t.readdir(d2.value).error==ENODEV);check(unmounts==1);
    }
    check(unmounts==2&&!files&&!dirs);
    std::unique_ptr<FileHandle> orphan;
    {SdProvider p;check(p.open());orphan=p.open_file("file").handle;check(bool(orphan));}
    check(unmounts==2);check(orphan->read_at(0,buf,1).error==Error::unavailable);orphan.reset();check(unmounts==3&&!files);
    std::cout<<n<<" checks passed\n";
}
