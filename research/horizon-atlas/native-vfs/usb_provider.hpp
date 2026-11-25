#pragma once
#include "usb_gate.hpp"
#include "memory_provider.hpp"

namespace exodus::vfs {
// Backend contract: each instance is bound to one physical mount generation.
// Every operation AND handle destructor must be safe after physical removal.
// A future native backend must enforce this under its own lifecycle lock.
struct UsbBackend : Provider {
    virtual UsbVolume volume() const = 0;
};
class UsbProvider final : public Provider {
    UsbGate::Lease lease_;
    std::shared_ptr<UsbBackend> backend_;
    static bool live(const UsbGate::Lease& l,const std::shared_ptr<UsbBackend>& b) {
        return l.valid() && b && l.matches(b->volume());
    }
    struct File final : FileHandle {
        UsbGate::Lease lease;
        std::shared_ptr<UsbBackend> backend;
        std::unique_ptr<FileHandle> inner;
        File(UsbGate::Lease l,std::shared_ptr<UsbBackend> b,std::unique_ptr<FileHandle> h)
            :lease(std::move(l)),backend(std::move(b)),inner(std::move(h)){}
        ReadResult read_at(std::uint64_t off,std::uint8_t* buf,std::size_t n) override {
            if(!live(lease,backend))return {Error::unavailable,0};
            if(n>1024*1024||(n&&!buf)||off>UINT64_MAX-n)return {Error::invalid_buffer,0};
            if(!n)return {Error::ok,0};
            auto r=inner->read_at(off,buf,n);
            if(r.error!=Error::ok)return {r.error,0};
            if(r.count>n)return {Error::backend_error,0};
            return r;
        }
        StatResult stat() override {
            if(!live(lease,backend))return {Error::unavailable};
            return inner->stat();
        }
    };
    struct Dir final : DirectoryHandle {
        UsbGate::Lease lease;
        std::shared_ptr<UsbBackend> backend;
        std::unique_ptr<DirectoryHandle> inner;
        Dir(UsbGate::Lease l,std::shared_ptr<UsbBackend> b,std::unique_ptr<DirectoryHandle> h)
            :lease(std::move(l)),backend(std::move(b)),inner(std::move(h)){}
        NextResult next() override {
            if(!live(lease,backend))return {Error::unavailable};
            return inner->next();
        }
    };
public:
    UsbProvider(UsbGate::Lease l,std::shared_ptr<UsbBackend> b):lease_(std::move(l)),backend_(std::move(b)){}
    OpenResult<FileHandle> open_file(std::string_view p) override {
        if(!live(lease_,backend_))return {Error::unavailable};
        if(!MemoryProvider::valid_path(p))return {Error::invalid_path};
        auto r=backend_->open_file(p);
        if(r.error!=Error::ok)return {r.error};
        if(!r.handle)return {Error::backend_error};
        return {Error::ok,std::make_unique<File>(lease_,backend_,std::move(r.handle))};
    }
    OpenResult<DirectoryHandle> open_directory(std::string_view p) override {
        if(!live(lease_,backend_))return {Error::unavailable};
        if(!p.empty()&&!MemoryProvider::valid_path(p))return {Error::invalid_path};
        auto r=backend_->open_directory(p);
        if(r.error!=Error::ok)return {r.error};
        if(!r.handle)return {Error::backend_error};
        return {Error::ok,std::make_unique<Dir>(lease_,backend_,std::move(r.handle))};
    }
    ReadResult read(std::string_view p,std::uint64_t off,std::uint8_t* buf,std::size_t n) override {
        auto h=open_file(p);if(h.error!=Error::ok)return {h.error,0};
        return h.handle->read_at(off,buf,n);
    }
    StatResult stat(std::string_view p) override {
        if(!live(lease_,backend_))return {Error::unavailable};
        if(!p.empty()&&!MemoryProvider::valid_path(p))return {Error::invalid_path};
        return backend_->stat(p);
    }
    ListResult list(std::string_view p,std::size_t limit) override {
        if(!limit||limit>max_list_entries)return {Error::invalid_buffer};
        auto h=open_directory(p);if(h.error!=Error::ok)return {h.error};
        ListResult out{Error::ok};
        for(std::size_t i=0;i<=limit;++i){
            auto r=h.handle->next();if(r.error!=Error::ok)return {r.error};
            if(r.eof)return out;
            if(i==limit){out.truncated=true;return out;}
            out.entries.push_back(std::move(r.entry));
        }
        return {Error::backend_error};
    }
};
} // namespace exodus::vfs
