#pragma once
#include <cstdint>
#include <memory>
#include <mutex>
#include <utility>

namespace exodus::vfs {
// Model for a future native backend. Context destruction must be host-only
// after detach. Callbacks must not reenter the controller or the same lease.
template<class Context> class UsbLifetime {
    struct State {
        const std::uint64_t generation;
        std::mutex io;
        bool active=true;
        std::unique_ptr<Context> context;
        State(std::uint64_t g,std::unique_ptr<Context> c):generation(g),context(std::move(c)){}
    };
    mutable std::mutex registry_;
    std::shared_ptr<State> current_;
    std::uint64_t last_=0;
public:
    class Lease {
        friend class UsbLifetime;
        std::shared_ptr<State> state_;
        explicit Lease(std::shared_ptr<State> s):state_(std::move(s)){}
    public:
        Lease()=default;
        std::uint64_t generation() const {return state_?state_->generation:0;}
        template<class F> bool with_live(F&& f) const {
            auto s=state_;
            if(!s)return false;
            std::lock_guard<std::mutex> lock(s->io);
            if(!s->active)return false;
            std::forward<F>(f)(*s->context);
            return true;
        }
    };
    UsbLifetime()=default;
    UsbLifetime(const UsbLifetime&)=delete;
    UsbLifetime& operator=(const UsbLifetime&)=delete;
    ~UsbLifetime(){retire();}
    bool attach(std::uint64_t generation,std::unique_ptr<Context> context){
        std::lock_guard<std::mutex> lock(registry_);
        if(current_||!context||!generation||generation<=last_)return false;
        current_=std::make_shared<State>(generation,std::move(context));
        last_=generation;
        return true;
    }
    Lease acquire() const {
        std::lock_guard<std::mutex> lock(registry_);
        return Lease(current_);
    }
    // Waits for the current operation to finish. New attach is serialized
    // against retirement. Old leases keep only the retired context alive.
    void retire(){
        std::shared_ptr<State> s;
        {
            std::lock_guard<std::mutex> lock(registry_);
            s=current_;
            if(!s)return;
            std::lock_guard<std::mutex> io(s->io);
            s->active=false;
            current_.reset();
        }
    }
};
} // namespace exodus::vfs
