#include "usb_gate.hpp"
#include <cassert>
#include <iostream>
using namespace exodus::vfs;
int main(){
    int n=0;auto check=[&](bool b){++n;assert(b);};
    UsbGate gate;UsbVolume v{"ums0:",3,1,1};
    check(!gate.lease().valid());check(!gate.admit(v,false));
    for(auto s:{"sdmc:","ums:","ums0:/", "ums-1:","ums0:foo","ums123456789012345:"}){
        auto x=v;x.name=s;check(!gate.admit(x,true));
    }
    for(unsigned type:{0,5,6,7,8,99}){auto x=v;x.fs_type=type;check(!gate.admit(x,true));}
    for(unsigned flags:{0,2,3,5,9,255}){auto x=v;x.flags=flags;check(!gate.admit(x,true));}
    auto zero=v;zero.generation=0;check(!gate.admit(zero,true));
    check(gate.admit(v,true));auto a=gate.lease();check(a.valid());check(a.matches(v));
    check(!gate.admit(v,true));auto x=v;x.generation=2;check(!a.matches(x));
    x=v;x.flags=3;check(!a.matches(x));x=v;x.name="ums1:";check(!a.matches(x));
    x=v;x.fs_type=4;check(!a.matches(x));
    gate.revoke();gate.revoke();check(!a.valid());check(!a.matches(v));check(!gate.lease().valid());
    check(!gate.admit(v,true));v.generation=2;check(gate.admit(v,true));check(!a.valid());check(gate.lease().matches(v));
    gate.revoke();for(unsigned t=1;t<=4;++t){v.fs_type=t;++v.generation;check(gate.admit(v,true));check(gate.lease().matches(v));gate.revoke();}
    UsbGate::Lease orphan;{UsbGate other;check(other.admit(v,true));orphan=other.lease();check(orphan.valid());}
    check(!orphan.valid());std::cout<<n<<" checks passed\n";
}
