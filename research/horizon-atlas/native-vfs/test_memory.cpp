#include "romfs_snapshot.hpp"
#include "descriptor_table.hpp"
#include <cassert>
#include <iostream>
using namespace exodus::vfs;
int main(){
    int n=0;auto check=[&](bool b){++n;assert(b);};
    using M=MemoryProvider;
    auto m=M::create({{"docs/a",{'a','b','c'}},{"docs/b",{}},{"root",{'r'}}});
    check(m.error==Error::ok&&bool(m.handle));
    auto& p=*m.handle;Broker b({{"tmp",&p,7,true},{"off",&p,7,false}});DescriptorTable t(b);
    check(b.stat("/tmp").metadata.type==EntryType::directory);
    check(b.stat("/tmp/docs/a").metadata.size==3);
    check(b.stat("/tmp/docs").metadata.type==EntryType::directory);
    check(b.stat("/tmp/unknown").error==Error::backend_error);
    check(b.stat("/off").error==Error::unavailable);
    auto l=b.list("/tmp",1);check(l.error==Error::ok&&l.truncated&&l.entries[0].name=="docs");
    l=b.list("/tmp/docs",2);check(l.error==Error::ok&&!l.truncated&&l.entries.size()==2&&l.entries[1].name=="b");
    check(b.list("/tmp/docs/a",1).error==Error::backend_error);
    check(b.list("/tmp",65).error==Error::invalid_buffer);
    auto f=t.open("/tmp/docs/a"),g=t.open("/tmp/docs/a");check(!f.error&&!g.error);
    std::uint8_t buf[4]{};
    check(t.read(f.value,buf,2).value==2&&buf[0]=='a'&&buf[1]=='b');
    check(t.read(f.value,buf,2).value==1&&buf[0]=='c');
    check(t.read(g.value,buf,1).value==1&&buf[0]=='a');
    check(t.fstat(f.value).value.size==3);check(t.lseek(f.value,-1,Seek::end).value==2);
    check(t.read(f.value,buf,1).value==1&&buf[0]=='c');
    check(t.read(f.value,buf,1).value==0);
    check(t.open("/tmp/root",false).error==EROFS);
    auto d=t.opendir("/tmp/docs");check(!d.error);
    check(t.readdir(d.value).value.entry.name=="a");check(t.readdir(d.value).value.entry.name=="b");check(t.readdir(d.value).value.eof);
    check(p.read("docs/b",0,buf,1).count==0);
    check(p.read("root",UINT64_MAX,buf,1).error==Error::invalid_buffer);
    check(p.read("root",0,nullptr,1).error==Error::invalid_buffer);
    check(p.read("root",0,buf,1024*1024+1).error==Error::invalid_buffer);
    for(auto s:{"","/a","a/","a//b","a/../b","a/./b","a:b","a%2f","a\\b","a.","a ","a?","\xc3\xa9"})check(M::create({{s,{}}}).error==Error::invalid_path);
    check(M::create({{"a",{}},{"a",{}}}).error==Error::invalid_path);
    check(M::create({{"a",{}},{"a/b",{}}}).error==Error::invalid_path);
    check(M::create({{"a/b",{}},{"a",{}}}).error==Error::invalid_path);
    check(M::create({{"x",std::vector<std::uint8_t>(M::byte_limit+1)}}).error==Error::invalid_buffer);
    std::vector<M::Seed> many;for(int i=0;i<100;++i)many.push_back({"d/f"+std::to_string(i),{}});
    auto big=M::create(std::move(many));check(big.error==Error::ok);
    auto stream=big.handle->open_directory("d");check(stream.error==Error::ok);
    big.handle.reset();for(int i=0;i<100;++i){auto e=stream.handle->next();check(e.error==Error::ok&&!e.eof);}
    check(stream.handle->next().eof);
    auto rom=load_romfs_snapshot({"config.txt","icons/empty"});check(rom.error==Error::ok);
    check(rom.handle->read("config.txt",0,buf,4).count==4&&!std::memcmp(buf,"test",4));
    check(rom.handle->stat("icons/empty").metadata.size==0);
    check(load_romfs_snapshot({"../secret"}).error==Error::invalid_path);
    check(load_romfs_snapshot({"missing"}).error==Error::backend_error);
    check(load_romfs_snapshot({"large"}).error==Error::invalid_buffer);
    check(load_romfs_snapshot({"config.txt","config.txt"}).error==Error::invalid_path);
    check(load_romfs_snapshot({}).error==Error::ok);
    std::cout<<n<<" checks passed\n";
}
