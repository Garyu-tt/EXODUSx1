#include "vfs.hpp"
#include <cassert>
#include <cstring>
#include <iostream>
using namespace exodus::vfs;
struct Memory final: Provider {
    int calls=0; bool invalid=false;
    ReadResult read(std::string_view path,std::uint64_t off,std::uint8_t* out,std::size_t n) override {
        ++calls;
        if(invalid) return {Error::ok,n+1};
        if(path!="hello") return {Error::backend_error,0};
        constexpr std::string_view data="hello world";
        if(off>=data.size()) return {Error::ok,0};
        auto part=data.substr(static_cast<std::size_t>(off),n);
        std::memcpy(out,part.data(),part.size()); return {Error::ok,part.size()};
    }
};
int main(){
    Memory m;
    const unsigned ro=mask(Op::read)|mask(Op::list)|mask(Op::stat);
    Broker b({{"sd",&m,ro,true},{"rom",&m,ro,false},{"null",nullptr,ro,true},{"write",&m,mask(Op::write),true}});
    int checks=0;
    auto check=[&](bool yes){++checks;assert(yes);};
    std::uint8_t out[8]{};
    auto r=b.read("/sd/hello",6,out,5);check(r.error==Error::ok&&r.count==5&&!std::memcmp(out,"world",5));
    r=b.read("/sd/hello",99,out,5);check(r.error==Error::ok&&r.count==0);
    int calls=m.calls;
    for(auto path:{"sd/hello","/sd/../hello","/sd/./hello","/sd//hello","/sd/hello/","/sd/a:b","/sd/%2e","/sd/a\\b","/sd/a.","/sd/a ","/SD/hello","/sd/\xc3\xa9"}) check(b.read(path,0,out,5).error==Error::invalid_path);
    check(b.read(std::string("/sd/")+std::string(256,'x'),0,out,5).error==Error::invalid_path);
    check(b.read(std::string("/sd/a\0b",7),0,out,5).error==Error::invalid_path);
    check(b.read("/sd",0,out,5).error==Error::invalid_path);
    check(b.read("/rom/hello",0,out,5).error==Error::unavailable);
    check(b.read("/nand/hello",0,out,5).error==Error::unavailable);
    check(b.read("/null/hello",0,out,5).error==Error::unavailable);
    check(b.read("/write/hello",0,out,5).error==Error::denied);
    for(auto op:{Op::write,Op::mkdir,Op::remove,Op::rename,static_cast<Op>(3)}) check(b.route("/sd/hello",op).error==Error::denied);
    check(b.route("/sd",Op::list).error==Error::ok);
    check(b.route("/sd/hello",Op::stat).relative=="hello");
    check(b.read("/sd/hello",0,nullptr,1).error==Error::invalid_buffer);
    check(b.read("/sd/hello",0,out,1024*1024+1).error==Error::invalid_buffer);
    check(b.read("/sd/hello",UINT64_MAX,out,1).error==Error::invalid_buffer);
    check(b.read("/sd/hello",0,nullptr,0).error==Error::ok);
    Broker dup({{"sd",&m,ro,true},{"sd",&m,ro,true}});
    check(dup.read("/sd/hello",0,out,5).error==Error::unavailable);
    check(m.calls==calls);
    m.invalid=true;check(b.read("/sd/hello",0,out,5).error==Error::backend_error);
    std::cout<<checks<<" checks passed\n";
}
