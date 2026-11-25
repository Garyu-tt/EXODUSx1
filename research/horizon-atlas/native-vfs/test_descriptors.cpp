#include "descriptor_table.hpp"
#include <cassert>
#include <cstring>
#include <iostream>
using namespace exodus::vfs;
struct State {int opened=0,closed=0,reads=0,dir_next=0;bool fail=false,bad=false;};
struct File:FileHandle {
    State& s; explicit File(State& x):s(x){++s.opened;} ~File(){++s.closed;}
    ReadResult read_at(std::uint64_t off,std::uint8_t* b,std::size_t n) override {
        ++s.reads;if(s.fail)return {Error::backend_error,0};if(s.bad)return {Error::ok,n+1};
        if(off>=6)return {Error::ok,0};
        auto len=std::min(n,static_cast<std::size_t>(6-off));
        std::memcpy(b,"abcdef"+off,len);return {Error::ok,len};
    }
    StatResult stat() override{return {Error::ok,{EntryType::file,6}};}
};
struct Dir:DirectoryHandle {
    State& s;int pos=0;explicit Dir(State& x):s(x){++s.opened;}~Dir(){++s.closed;}
    NextResult next() override{
        ++s.dir_next;if(s.fail)return {Error::backend_error};if(s.bad)return {Error::ok,false,{"../bad",{EntryType::file,1}}};
        if(pos==100)return {Error::ok,true};
        return {Error::ok,false,{"f"+std::to_string(pos++),{EntryType::file,1}}};
    }
};
struct Mock:Provider {
    State s;
    ReadResult read(std::string_view,std::uint64_t,std::uint8_t*,std::size_t) override{return {Error::backend_error,0};}
    OpenResult<FileHandle> open_file(std::string_view p) override{if(p!="file")return {Error::backend_error};return {Error::ok,std::make_unique<File>(s)};}
    OpenResult<DirectoryHandle> open_directory(std::string_view p) override{if(!p.empty())return {Error::backend_error};return {Error::ok,std::make_unique<Dir>(s)};}
};
int main(){
    Mock p;Broker b({{"sd",&p,7,true},{"ro",&p,1,true},{"off",&p,7,false}});int n=0;auto check=[&](bool v){++n;assert(v);};
    {
        DescriptorTable t(b);std::uint8_t buf[8]{};
        check(t.open("/sd/file",false).error==EROFS);check(t.open("/sd/../file").error==EINVAL);check(t.open("/off/file").error==ENODEV);check(t.open("/sd").error==EISDIR);check(t.open("/sd/missing").error==EIO);check(p.s.opened==0);
        auto a=t.open("/sd/file"),c=t.open("/sd/file");check(!a.error&&!c.error&&a.value!=c.value);check(p.s.opened==2);
        check(t.read(a.value,buf,2).value==2&&!std::memcmp(buf,"ab",2));check(t.read(a.value,buf,2).value==2&&!std::memcmp(buf,"cd",2));check(t.read(c.value,buf,2).value==2&&!std::memcmp(buf,"ab",2));
        check(t.lseek(a.value,-1,Seek::end).value==5);check(t.read(a.value,buf,8).value==1&&buf[0]=='f');check(t.read(a.value,buf,1).value==0);
        check(t.fstat(a.value).value.size==6);check(t.lseek(a.value,-7,Seek::end).error==EINVAL);check(t.lseek(a.value,0,Seek::current).value==6);
        check(!t.lseek(a.value,INT64_MAX,Seek::set).error);check(t.lseek(a.value,1,Seek::current).error==EOVERFLOW);check(t.read(a.value,buf,1).error==EOVERFLOW);check(t.lseek(a.value,INT64_MIN,Seek::current).error==EINVAL);
        check(!t.lseek(a.value,0,Seek::set).error);p.s.fail=true;check(t.read(a.value,buf,1).error==EIO);p.s.fail=false;p.s.bad=true;check(t.read(a.value,buf,1).error==EIO);p.s.bad=false;check(t.lseek(a.value,0,Seek::current).value==0);
        int reads=p.s.reads;check(!t.read(a.value,nullptr,0).error);check(t.read(a.value,nullptr,1).error==EINVAL);check(t.read(a.value,buf,1024*1024+1).error==EINVAL);check(p.s.reads==reads);
        check(!t.close(a.value));check(t.close(a.value)==EBADF);check(t.read(a.value,buf,1).error==EBADF);auto r=t.open("/ro/file");check(!r.error&&r.value>a.value);check(t.fstat(r.value).error==EACCES);check(t.lseek(r.value,0,Seek::end).error==EACCES);
        check(t.opendir("/ro").error==EACCES);auto d=t.opendir("/sd");check(!d.error);check(t.read(d.value,buf,1).error==EISDIR);check(t.lseek(d.value,0,Seek::set).error==ESPIPE);check(t.readdir(c.value).error==EBADF);
        for(int i=0;i<100;++i){auto e=t.readdir(d.value);check(!e.error&&!e.value.eof&&e.value.entry.name=="f"+std::to_string(i));}
        check(t.readdir(d.value).value.eof);int calls=p.s.dir_next;check(t.readdir(d.value).value.eof&&p.s.dir_next==calls);check(!t.close(d.value));
        d=t.opendir("/sd");p.s.bad=true;check(t.readdir(d.value).error==EIO);p.s.bad=false;check(t.readdir(d.value).error==EIO);check(!t.close(d.value));
        for(int i=0;i<62;++i)check(!t.open("/sd/file").error);
        int opens=p.s.opened;check(t.open("/sd/file").error==EMFILE);check(t.opendir("/sd").error==EMFILE);check(p.s.opened==opens);
    }
    check(p.s.opened==p.s.closed);std::cout<<n<<" checks passed\n";
}
