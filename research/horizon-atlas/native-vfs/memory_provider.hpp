#pragma once
#include "vfs.hpp"
#include <algorithm>
#include <cstring>
#include <map>
#include <utility>

namespace exodus::vfs {
// Immutable, bounded, application-owned snapshot. No host filesystem access.
class MemoryProvider final : public Provider {
public:
    static constexpr std::size_t byte_limit=4*1024*1024, node_limit=512;
    struct Seed { std::string path; std::vector<std::uint8_t> data; };
    static bool valid_path(std::string_view p) {
        if(p.empty()||p.size()>1024||p.front()=='/'||p.back()=='/')return false;
        while(!p.empty()) {
            auto end=p.find('/');auto c=p.substr(0,end);
            if(c.empty()||c.size()>255||c=="."||c==".."||c.back()=='.'||c.back()==' ')return false;
            for(unsigned char x:c)if(x<32||x>=127||std::string_view("\\:%*?\"<>|").find(x)!=std::string_view::npos)return false;
            if(end==std::string_view::npos)break;
            p.remove_prefix(end+1);
        }
        return true;
    }
private:
    struct Node { Metadata meta{}; std::vector<std::uint8_t> bytes; };
    using Tree=std::map<std::string,Node,std::less<>>;
    std::shared_ptr<const Tree> tree_;
    explicit MemoryProvider(std::shared_ptr<const Tree> t):tree_(std::move(t)){}
    struct File final : FileHandle {
        std::shared_ptr<const Tree> tree; const Node& node;
        File(std::shared_ptr<const Tree> t,const Node& n):tree(std::move(t)),node(n){}
        StatResult stat() override{return {Error::ok,node.meta};}
        ReadResult read_at(std::uint64_t off,std::uint8_t* out,std::size_t n) override {
            if(n>1024*1024||(n&&!out)||off>UINT64_MAX-n)return {Error::invalid_buffer,0};
            if(!n||off>=node.bytes.size())return {Error::ok,0};
            auto pos=static_cast<std::size_t>(off);
            auto take=std::min(n,node.bytes.size()-pos);
            std::memcpy(out,node.bytes.data()+pos,take);return {Error::ok,take};
        }
    };
    struct Dir final : DirectoryHandle {
        std::shared_ptr<const Tree> tree; std::string prefix; Tree::const_iterator it;
        Dir(std::shared_ptr<const Tree> t,std::string_view path):tree(std::move(t)),prefix(path),it(tree->end()){
            if(!prefix.empty())prefix+='/';
            it=tree->lower_bound(prefix);
        }
        NextResult next() override {
            while(it!=tree->end()) {
                const auto& item=*it++;
                if(item.first.compare(0,prefix.size(),prefix)!=0){it=tree->end();break;}
                auto name=std::string_view(item.first).substr(prefix.size());
                if(!name.empty()&&name.find('/')==std::string_view::npos)
                    return {Error::ok,false,{std::string(name),item.second.meta}};
            }
            return {Error::ok,true};
        }
    };
public:
    static OpenResult<MemoryProvider> create(std::vector<Seed> seeds) {
        if(seeds.size()>=node_limit)return {Error::invalid_buffer};
        auto t=std::make_shared<Tree>();t->emplace("",Node{});
        std::size_t used=0;
        for(auto& seed:seeds){
            if(!valid_path(seed.path))return {Error::invalid_path};
            if(seed.data.size()>byte_limit-used)return {Error::invalid_buffer};
            for(auto pos=seed.path.find('/');pos!=std::string::npos;pos=seed.path.find('/',pos+1)){
                auto [it,inserted]=t->try_emplace(seed.path.substr(0,pos),Node{});
                if(it->second.meta.type!=EntryType::directory)return {Error::invalid_path};
                if(inserted&&t->size()>node_limit)return {Error::invalid_buffer};
            }
            used+=seed.data.size();
            if(!t->emplace(seed.path,Node{{EntryType::file,seed.data.size()},std::move(seed.data)}).second)
                return {Error::invalid_path};
            if(t->size()>node_limit)return {Error::invalid_buffer};
        }
        return {Error::ok,std::unique_ptr<MemoryProvider>(new MemoryProvider(std::move(t)))};
    }
    StatResult stat(std::string_view path) override {
        if(!path.empty()&&!valid_path(path))return {Error::invalid_path};
        auto it=tree_->find(path);if(it==tree_->end())return {Error::backend_error};
        return {Error::ok,it->second.meta};
    }
    OpenResult<FileHandle> open_file(std::string_view path) override {
        if(!valid_path(path))return {Error::invalid_path};
        auto it=tree_->find(path);if(it==tree_->end()||it->second.meta.type!=EntryType::file)return {Error::backend_error};
        return {Error::ok,std::make_unique<File>(tree_,it->second)};
    }
    OpenResult<DirectoryHandle> open_directory(std::string_view path) override {
        auto m=stat(path);if(m.error!=Error::ok)return {m.error};
        if(m.metadata.type!=EntryType::directory)return {Error::backend_error};
        return {Error::ok,std::make_unique<Dir>(tree_,path)};
    }
    ReadResult read(std::string_view p,std::uint64_t off,std::uint8_t* b,std::size_t n) override {
        auto f=open_file(p);if(f.error!=Error::ok)return {f.error,0};return f.handle->read_at(off,b,n);
    }
    ListResult list(std::string_view p,std::size_t limit) override {
        if(!limit||limit>max_list_entries)return {Error::invalid_buffer};
        auto d=open_directory(p);if(d.error!=Error::ok)return {d.error};
        ListResult out{Error::ok};
        for(std::size_t i=0;i<=limit;++i){auto x=d.handle->next();if(x.error!=Error::ok)return {x.error};
            if(x.eof)return out;
            if(i==limit){out.truncated=true;return out;}
            out.entries.push_back(std::move(x.entry));}
        return {Error::backend_error};
    }
};
} // namespace exodus::vfs
