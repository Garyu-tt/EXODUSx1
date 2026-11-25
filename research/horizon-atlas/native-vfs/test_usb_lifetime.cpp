#include "usb_lifetime.hpp"
#include <atomic>
#include <cassert>
#include <condition_variable>
#include <iostream>
#include <thread>
using namespace exodus::vfs;
struct Context {
    std::atomic<int>& destroyed;
    int reads=0;
    explicit Context(std::atomic<int>& d):destroyed(d){}
    ~Context(){++destroyed;}
};
int main(){
    int n=0;auto check=[&](bool v){++n;assert(v);};
    std::atomic<int> destroyed{0};
    using Life=UsbLifetime<Context>;
    Life life;
    check(!life.acquire().with_live([](Context&){}));
    check(!life.attach(0,std::make_unique<Context>(destroyed)));
    check(destroyed==1);
    check(!life.attach(1,nullptr));
    check(life.attach(1,std::make_unique<Context>(destroyed)));
    check(!life.attach(2,std::make_unique<Context>(destroyed)));
    auto old=life.acquire();check(old.generation()==1);
    check(old.with_live([](Context& c){++c.reads;}));
    std::mutex m;std::condition_variable cv;bool entered=false,release=false;
    std::atomic<bool> read_ok{false},retire_started{false},retired{false};
    std::thread reader([&]{read_ok=old.with_live([&](Context& c){
        std::unique_lock<std::mutex> lock(m);entered=true;cv.notify_all();
        cv.wait(lock,[&]{return release;});++c.reads;
    });});
    {std::unique_lock<std::mutex> lock(m);cv.wait(lock,[&]{return entered;});}
    std::thread detach([&]{retire_started=true;life.retire();retired=true;});
    while(!retire_started.load())std::this_thread::yield();
    check(!retired.load());
    {std::lock_guard<std::mutex> lock(m);release=true;}cv.notify_all();
    reader.join();detach.join();check(read_ok&&retired);
    check(!old.with_live([](Context&){assert(false);}));
    check(!life.acquire().with_live([](Context&){}));
    check(destroyed==2);
    check(!life.attach(1,std::make_unique<Context>(destroyed)));
    check(life.attach(2,std::make_unique<Context>(destroyed)));
    auto fresh=life.acquire();check(fresh.generation()==2);
    check(!old.with_live([](Context&){}));
    check(fresh.with_live([](Context& c){assert(c.reads==0);++c.reads;}));
    old={};check(destroyed==4);
    life.retire();life.retire();check(!fresh.with_live([](Context&){}));
    check(destroyed==4);fresh={};check(destroyed==5);
    Life::Lease orphan;
    {Life other;check(other.attach(1,std::make_unique<Context>(destroyed)));orphan=other.acquire();}
    check(!orphan.with_live([](Context&){}));check(destroyed==5);orphan={};check(destroyed==6);
    std::cout<<n<<" checks passed\n";
}
