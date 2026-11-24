#pragma once
#include <cstdint>
#include <string>
#include <string_view>
#include <memory>
#include <utility>

namespace exodus::vfs {
// Policy model only; never controls the physical USB driver.
struct UsbVolume {
    std::string name;
    unsigned fs_type=0, flags=0;
    std::uint64_t generation=0; // supplied by serialized backend lifecycle
};
class UsbGate {
    struct State { UsbVolume volume; bool active=true; };
    std::shared_ptr<State> active_;
    std::uint64_t last_generation_=0;
    static bool valid_name(std::string_view s){
        if(s.size()<5||s.size()>15||s.substr(0,3)!="ums"||s.back()!=':')return false;
        for(char c:s.substr(3,s.size()-4))if(c<'0'||c>'9')return false;
        return true;
    }
public:
    class Lease {
        friend class UsbGate;
        std::shared_ptr<State> state_;
        explicit Lease(std::shared_ptr<State> s):state_(std::move(s)){}
    public:
        Lease()=default;
        bool valid() const {return state_&&state_->active;}
        bool matches(const UsbVolume& current) const {
            return valid()&&current.name==state_->volume.name&&
                current.generation==state_->volume.generation&&
                current.fs_type==state_->volume.fs_type&&current.flags==1;
        }
    };
    UsbGate()=default;
    UsbGate(const UsbGate&)=delete;
    UsbGate& operator=(const UsbGate&)=delete;
    ~UsbGate(){revoke();}
    bool admit(UsbVolume v,bool explicit_consent){
        if(active_||!explicit_consent||!valid_name(v.name)||v.fs_type<1||v.fs_type>4||
           v.flags!=1||!v.generation||v.generation<=last_generation_)return false;
        auto state=std::make_shared<State>(State{std::move(v),true});
        last_generation_=state->volume.generation;active_=std::move(state);return true;
    }
    Lease lease() const{return Lease(active_);}
    void revoke(){if(active_){active_->active=false;active_.reset();}}
};
} // namespace exodus::vfs
