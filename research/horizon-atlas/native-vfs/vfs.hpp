#pragma once
#include <cstdint>
#include <cstddef>
#include <string>
#include <string_view>
#include <vector>
#include <memory>

namespace exodus::vfs {
enum class Error { ok, invalid_path, unavailable, denied, invalid_buffer, backend_error };
enum class Op : unsigned { read=1, list=2, stat=4, write=8, mkdir=16, remove=32, rename=64 };
constexpr unsigned mask(Op op) { return static_cast<unsigned>(op); }
struct ReadResult { Error error; std::size_t count; };
enum class EntryType { file, directory };
struct Metadata { EntryType type=EntryType::directory; std::uint64_t size=0; };
struct StatResult { Error error; Metadata metadata{}; };
struct DirectoryEntry { std::string name; Metadata metadata; };
struct ListResult { Error error; std::vector<DirectoryEntry> entries{}; bool truncated=false; };
constexpr std::size_t max_list_entries=64;
struct NextResult { Error error; bool eof=false; DirectoryEntry entry{}; };
struct FileHandle {
    virtual ~FileHandle() = default;
    virtual ReadResult read_at(std::uint64_t, std::uint8_t*, std::size_t) = 0;
    virtual StatResult stat() = 0;
};
struct DirectoryHandle {
    virtual ~DirectoryHandle() = default;
    virtual NextResult next() = 0;
};
template<class T> struct OpenResult { Error error; std::unique_ptr<T> handle{}; };
struct Provider {
    virtual ~Provider() = default;
    // Relative path, never an OS path. Provider must independently confine access.
    virtual ReadResult read(std::string_view path, std::uint64_t offset,
                            std::uint8_t* buffer, std::size_t capacity) = 0;
    virtual OpenResult<FileHandle> open_file(std::string_view) { return {Error::denied}; }
    virtual OpenResult<DirectoryHandle> open_directory(std::string_view) { return {Error::denied}; }
    virtual StatResult stat(std::string_view) { return {Error::denied}; }
    virtual ListResult list(std::string_view, std::size_t) { return {Error::denied}; }
};
struct Mount {
    std::string name;
    Provider* provider = nullptr; // borrowed; must outlive broker
    unsigned operations = 0;
    bool admitted = false;
};
struct Route { Error error; const Mount* mount; std::string_view relative; };
class Broker {
    std::vector<Mount> mounts_;
    static bool component(std::string_view s) {
        if(s.empty() || s.size()>255 || s=="." || s==".." || s.back()=='.' || s.back()==' ') return false;
        for(unsigned char c:s) if(c<32 || c>=127 || c=='\\' || c==':' || c=='%') return false;
        return true;
    }
    static bool mount_name(std::string_view s) {
        if(s.empty() || s.front()<'a' || s.front()>'z') return false;
        for(char c:s) if(!((c>='a'&&c<='z')||(c>='0'&&c<='9')||c=='_'||c=='-')) return false;
        return true;
    }
public:
    explicit Broker(const std::vector<Mount>& mounts): mounts_(mounts) {}
    Route route(std::string_view path, Op op) const {
        auto fail=[](Error e){return Route{e,nullptr,{}};};
        if(path.size()<2 || path.size()>1024 || path.front()!='/' || path.back()=='/') return fail(Error::invalid_path);
        auto rest=path.substr(1);
        const auto slash=rest.find('/');
        const auto name=rest.substr(0,slash);
        if(!mount_name(name)||!component(name)) return fail(Error::invalid_path);
        const auto relative=slash==std::string_view::npos?std::string_view{}:rest.substr(slash+1);
        auto parts=relative;
        while(!parts.empty()) {
            const auto end=parts.find('/');
            if(!component(parts.substr(0,end))) return fail(Error::invalid_path);
            if(end==std::string_view::npos) break;
            parts.remove_prefix(end+1);
            if(parts.empty()) return fail(Error::invalid_path);
        }
        const Mount* chosen=nullptr;
        for(const auto& m:mounts_) if(m.name==name) {
            if(chosen) return fail(Error::unavailable);
            chosen=&m;
        }
        if(!chosen || !chosen->admitted || !chosen->provider) return fail(Error::unavailable);
        if(op!=Op::read && op!=Op::list && op!=Op::stat) return fail(Error::denied);
        if(!(chosen->operations & mask(op))) return fail(Error::denied);
        return {Error::ok,chosen,relative};
    }
    StatResult stat(std::string_view path) const {
        auto r=route(path,Op::stat);
        if(r.error!=Error::ok) return {r.error};
        auto result=r.mount->provider->stat(r.relative);
        if(result.error!=Error::ok) return {result.error};
        if(result.metadata.type!=EntryType::file && result.metadata.type!=EntryType::directory)
            return {Error::backend_error};
        if(result.metadata.type==EntryType::directory && result.metadata.size!=0)
            return {Error::backend_error};
        return result;
    }
    ListResult list(std::string_view path,std::size_t limit) const {
        auto r=route(path,Op::list);
        if(r.error!=Error::ok) return {r.error};
        if(!limit || limit>max_list_entries) return {Error::invalid_buffer};
        auto result=r.mount->provider->list(r.relative,limit);
        if(result.error!=Error::ok) return {result.error};
        if(result.entries.size()>limit || (result.truncated && result.entries.size()!=limit))
            return {Error::backend_error};
        for(const auto& e:result.entries) {
            if(!component(e.name) || e.name.find('/')!=std::string::npos ||
               (e.metadata.type!=EntryType::file && e.metadata.type!=EntryType::directory) ||
               (e.metadata.type==EntryType::directory && e.metadata.size!=0))
                return {Error::backend_error};
        }
        return result;
    }
    ReadResult read(std::string_view path, std::uint64_t offset, std::uint8_t* buffer, std::size_t capacity) const {
        auto r=route(path,Op::read);
        if(r.error!=Error::ok) return {r.error,0};
        if(r.relative.empty()) return {Error::invalid_path,0};
        if(capacity>1024*1024 || (capacity && !buffer) || offset>UINT64_MAX-capacity) return {Error::invalid_buffer,0};
        if(!capacity) return {Error::ok,0};
        auto result=r.mount->provider->read(r.relative,offset,buffer,capacity);
        if(result.error!=Error::ok) return {result.error,0};
        if(result.count>capacity) return {Error::backend_error,0};
        return result;
    }
};
} // namespace exodus::vfs
