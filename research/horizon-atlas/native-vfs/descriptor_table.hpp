#pragma once
#include "vfs.hpp"
#include <map>
#include <limits>
#include <cerrno>
#include <utility>

namespace exodus::vfs {
// Internal POSIX-style results: positive errno, not libc interposition.
template<class T> struct FdResult { int error; T value{}; };
enum class Seek { set, current, end };
class DescriptorTable {
    struct Slot {
        std::unique_ptr<FileHandle> file;
        std::unique_ptr<DirectoryHandle> dir;
        std::int64_t offset=0;
        bool metadata=false, eof=false, failed=false;
    };
    const Broker& broker_;
    std::map<int,Slot> slots_;
    std::int64_t next_=3; // never recycle during this table's lifetime
    static constexpr std::size_t capacity_=64;
    static constexpr auto max_offset_=std::numeric_limits<std::int64_t>::max();
    static int err(Error e) {
        switch(e){
            case Error::ok:return 0;
            case Error::invalid_path:case Error::invalid_buffer:return EINVAL;
            case Error::unavailable:return ENODEV;
            case Error::denied:return EACCES;
            default:return EIO;
        }
    }
    Slot* get(int fd){auto it=slots_.find(fd);return it==slots_.end()?nullptr:&it->second;}
    bool full() const {return slots_.size()>=capacity_ || next_>std::numeric_limits<int>::max();}
    int insert(Slot s){int fd=static_cast<int>(next_++);slots_.emplace(fd,std::move(s));return fd;}
    static FdResult<Metadata> checked_stat(FileHandle& f){
        auto r=f.stat();if(r.error!=Error::ok)return {err(r.error)};
        if(r.metadata.type!=EntryType::file)return {EIO};
        if(r.metadata.size>static_cast<std::uint64_t>(max_offset_))return {EOVERFLOW};
        return {0,r.metadata};
    }
    static bool valid_entry(const DirectoryEntry& e){
        const auto& n=e.name;
        if(n.empty()||n.size()>255||n=="."||n==".."||n.back()=='.'||n.back()==' ')return false;
        for(unsigned char c:n)if(c<32||c>=127||std::string_view("/\\:%*?\"<>|").find(c)!=std::string_view::npos)return false;
        return e.metadata.type==EntryType::file || (e.metadata.type==EntryType::directory&&e.metadata.size==0);
    }
public:
    explicit DescriptorTable(const Broker& b):broker_(b){}
    DescriptorTable(const DescriptorTable&)=delete;
    DescriptorTable& operator=(const DescriptorTable&)=delete;
    FdResult<int> open(std::string_view path,bool read_only=true){
        if(!read_only)return {EROFS,-1};
        auto r=broker_.route(path,Op::read);if(r.error!=Error::ok)return {err(r.error),-1};
        if(r.relative.empty())return {EISDIR,-1};
        if(full())return {EMFILE,-1};
        auto h=r.mount->provider->open_file(r.relative);
        if(h.error!=Error::ok||!h.handle)return {h.error==Error::ok?EIO:err(h.error),-1};
        Slot s;s.file=std::move(h.handle);s.metadata=broker_.route(path,Op::stat).error==Error::ok;
        return {0,insert(std::move(s))};
    }
    FdResult<int> opendir(std::string_view path){
        auto r=broker_.route(path,Op::list);if(r.error!=Error::ok)return {err(r.error),-1};
        if(full())return {EMFILE,-1};
        auto h=r.mount->provider->open_directory(r.relative);
        if(h.error!=Error::ok||!h.handle)return {h.error==Error::ok?EIO:err(h.error),-1};
        Slot s;s.dir=std::move(h.handle);return {0,insert(std::move(s))};
    }
    int close(int fd){return slots_.erase(fd)?0:EBADF;}
    FdResult<std::size_t> read(int fd,std::uint8_t* buf,std::size_t count){
        auto s=get(fd);if(!s)return {EBADF};if(!s->file)return {EISDIR};
        if(count>1024*1024||(count&&!buf))return {EINVAL};
        if(count>static_cast<std::uint64_t>(max_offset_-s->offset))return {EOVERFLOW};
        if(!count)return {0,0};
        auto r=s->file->read_at(static_cast<std::uint64_t>(s->offset),buf,count);
        if(r.error!=Error::ok)return {err(r.error)};
        if(r.count>count)return {EIO};
        s->offset+=static_cast<std::int64_t>(r.count);return {0,r.count};
    }
    FdResult<Metadata> fstat(int fd){
        auto s=get(fd);if(!s||!s->file)return {EBADF};if(!s->metadata)return {EACCES};
        return checked_stat(*s->file);
    }
    FdResult<std::int64_t> lseek(int fd,std::int64_t delta,Seek origin){
        auto s=get(fd);if(!s)return {EBADF};if(!s->file)return {ESPIPE};
        std::int64_t base=0;
        if(origin==Seek::current)base=s->offset;
        else if(origin==Seek::end){auto m=fstat(fd);if(m.error)return {m.error};base=static_cast<std::int64_t>(m.value.size);}
        else if(origin!=Seek::set)return {EINVAL};
        if(delta<0 && delta < -base)return {EINVAL};
        if(delta>0 && delta>max_offset_-base)return {EOVERFLOW};
        s->offset=base+delta;return {0,s->offset};
    }
    FdResult<NextResult> readdir(int fd){
        auto s=get(fd);if(!s||!s->dir)return {EBADF};
        if(s->failed)return {EIO};
        if(s->eof)return {0,{Error::ok,true,{}}};
        auto r=s->dir->next();
        if(r.error!=Error::ok){s->failed=true;return {err(r.error)};}
        if(r.eof){s->eof=true;return {0,{Error::ok,true,{}}};}
        if(!valid_entry(r.entry)){s->failed=true;return {EIO};}
        return {0,std::move(r)};
    }
};
} // namespace exodus::vfs
