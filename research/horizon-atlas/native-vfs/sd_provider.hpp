#pragma once
#include "vfs.hpp"
#include <switch.h>
#include <limits>

namespace exodus::vfs {
// Research-only provider. The caller owns FS service initialization and must
// hold it until this provider is destroyed. No implicit mount or admission.
class SdProvider final : public Provider {
    FsFileSystem fs_{};
    bool opened_ = false;
    static constexpr std::string_view root = "/switch/exodus/data/";
    static bool make_path(std::string_view relative, std::string& out) {
        if(relative.empty() || relative.size() >= FS_MAX_PATH-root.size() ||
           relative.front()=='/' || relative.back()=='/') return false;
        auto rest=relative;
        while(!rest.empty()) {
            const auto pos=rest.find('/');
            const auto c=rest.substr(0,pos);
            if(c.empty() || c.size()>255 || c=="." || c==".." ||
               c.back()=='.' || c.back()==' ') return false;
            for(unsigned char x:c) {
                if(x<32 || x>=127 || x=='\\' || x==':' || x=='%' ||
                   x=='*' || x=='?' || x=='"' || x=='<' || x=='>' || x=='|') return false;
            }
            if(pos==std::string_view::npos) break;
            rest.remove_prefix(pos+1);
        }
        out.assign(root);
        out.append(relative);
        return true;
    }
    struct FileGuard {
        FsFile file{};
        bool opened=false;
        ~FileGuard(){if(opened) fsFileClose(&file);}
        FileGuard()=default;
        FileGuard(const FileGuard&)=delete;
        FileGuard& operator=(const FileGuard&)=delete;
    };
public:
    SdProvider()=default;
    SdProvider(const SdProvider&)=delete;
    SdProvider& operator=(const SdProvider&)=delete;
    ~SdProvider() override {close();}
    bool open() {
        if(opened_) return true;
        if(R_FAILED(fsOpenSdCardFileSystem(&fs_))) return false;
        opened_=true;
        return true;
    }
    void close() {
        if(opened_) {fsFsClose(&fs_);opened_=false;}
    }
    ReadResult read(std::string_view path,std::uint64_t offset,
                    std::uint8_t* buffer,std::size_t capacity) override {
        if(!opened_) return {Error::unavailable,0};
        std::string full;
        if(!make_path(path,full)) return {Error::invalid_path,0};
        constexpr auto max_offset=static_cast<std::uint64_t>(std::numeric_limits<s64>::max());
        if(capacity>1024*1024 || (capacity&&!buffer) || offset>max_offset ||
           capacity>max_offset-offset) return {Error::invalid_buffer,0};
        if(!capacity) return {Error::ok,0};
        FileGuard f;
        if(R_FAILED(fsFsOpenFile(&fs_,full.c_str(),FsOpenMode_Read,&f.file)))
            return {Error::backend_error,0};
        f.opened=true;
        s64 size=0;
        if(R_FAILED(fsFileGetSize(&f.file,&size))||size<0) return {Error::backend_error,0};
        if(offset>=static_cast<std::uint64_t>(size)) return {Error::ok,0};
        const auto available=static_cast<std::uint64_t>(size)-offset;
        const auto requested=available<capacity?available:capacity;
        u64 actual=0;
        if(R_FAILED(fsFileRead(&f.file,static_cast<s64>(offset),buffer,
                               requested,FsReadOption_None,&actual)) || actual>requested)
            return {Error::backend_error,0};
        return {Error::ok,static_cast<std::size_t>(actual)};
    }
};
} // namespace exodus::vfs
