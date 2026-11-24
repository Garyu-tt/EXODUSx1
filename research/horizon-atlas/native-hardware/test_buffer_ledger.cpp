#include "buffer_ledger.hpp"
#include <atomic>
#include <cassert>
#include <iostream>
#include <thread>
#include <vector>
using L=exodus::hardware::BufferLedger;
using R=L::Result;using E=L::Event;
int main(){
    int n=0;auto check=[&](bool b){++n;assert(b);};
    L l(1024,256,4);
    check(l.reserve(0,true).result==R::invalid);
    check(l.reserve(1,false).result==R::invalid);
    check(l.reserve(1025,true).result==R::budget);
    auto a=l.reserve(128,true);check(a.result==R::ok);
    check(l.snapshot().used==128);
    check(l.apply(a.id,E::submit_ok)==R::wrong_stage);
    check(l.apply(a.id,E::map_ok)==R::ok);
    check(l.apply(a.id,E::release)==R::wrong_stage);
    check(l.apply(a.id,E::cache_ok)==R::wrong_stage);
    check(l.apply(a.id,E::submit_ok)==R::ok);
    check(l.apply(a.id,E::unmap_ok)==R::wrong_stage);
    check(l.apply(a.id,E::fence_ok)==R::ok);
    check(l.apply(a.id,E::release)==R::wrong_stage);
    check(l.apply(a.id,E::unmap_ok)==R::ok);
    check(l.apply(a.id,E::release)==R::wrong_stage);
    check(l.apply(a.id,E::cache_ok)==R::ok);
    check(l.apply(a.id,E::release)==R::ok);
    check(l.apply(a.id,E::release)==R::missing);
    check(l.snapshot().used==0);
    auto b=l.reserve(64,true);check(b.id>a.id);
    check(l.apply(b.id,E::map_ok)==R::ok);
    check(l.apply(b.id,E::unmap_ok)==R::ok);
    check(l.apply(b.id,E::cache_ok)==R::ok);
    check(l.apply(b.id,E::release)==R::ok);
    auto c=l.reserve(128,true);check(l.apply(c.id,E::map_ok)==R::ok);
    check(l.apply(c.id,E::submit_ok)==R::ok);
    check(l.apply(c.id,E::uncertain)==R::ok);
    check(l.apply(c.id,E::release)==R::wrong_stage);
    check(l.apply(c.id,E::uncertain)==R::wrong_stage);
    check(l.snapshot().quarantined==128&&!l.snapshot().stopped);
    auto d=l.reserve(128,true),e=l.reserve(64,true);
    check(l.apply(e.id,E::map_ok)==R::ok);
    check(l.apply(d.id,E::uncertain)==R::ok);
    check(l.snapshot().stopped&&l.snapshot().used==320&&l.snapshot().quarantined==256);
    check(l.reserve(1,true).result==R::stopped);
    check(l.apply(e.id,E::submit_ok)==R::wrong_stage);
    check(l.apply(e.id,E::unmap_ok)==R::ok);
    check(l.apply(e.id,E::cache_ok)==R::ok);
    check(l.apply(e.id,E::release)==R::ok);
    check(l.snapshot().used==256&&l.snapshot().count==2);
    L slots(100,100,2);auto f=slots.reserve(1,true),g=slots.reserve(1,true);
    check(f.result==R::ok&&g.result==R::ok);
    check(slots.reserve(1,true).result==R::budget);
    check(slots.apply(f.id,E::release)==R::ok);
    check(slots.reserve(1,true).result==R::ok);
    L invalid(10,11);check(invalid.reserve(1,true).result==R::invalid);
    L concurrent(1024,1024,64);std::atomic<int> wins{0};std::vector<std::thread> threads;
    for(int i=0;i<32;++i)threads.emplace_back([&]{if(concurrent.reserve(64,true).result==R::ok)++wins;});
    for(auto& t:threads)t.join();
    check(wins==16);check(concurrent.snapshot().used==1024&&concurrent.snapshot().count==16);
    L race(128,128);auto r=race.reserve(128,true);std::atomic<int> releases{0};
    threads.clear();for(int i=0;i<16;++i)threads.emplace_back([&]{if(race.apply(r.id,E::release)==R::ok)++releases;});
    for(auto& t:threads)t.join();
    check(releases==1);check(race.snapshot().used==0&&race.snapshot().count==0);
    std::cout<<n<<" checks passed\n";
}
