#pragma once
#include "memory_provider.hpp"
#include <array>
#include <cstdio>

namespace exodus::vfs {
// Caller initializes libnx RomFS first. Only explicitly listed application assets.
// Copies assets into a bounded snapshot, not a live RomFS mount or parser.
inline OpenResult<MemoryProvider> load_romfs_snapshot(const std::vector<std::string>& manifest){
    if(manifest.size()>=MemoryProvider::node_limit)return {Error::invalid_buffer};
    std::vector<MemoryProvider::Seed> seeds;
    std::size_t used=0;
    for(const auto& path:manifest){
        if(!MemoryProvider::valid_path(path))return {Error::invalid_path};
        const auto full="romfs:/exodus/"+path;
        auto close_file=[](std::FILE* f){std::fclose(f);};
        std::unique_ptr<std::FILE,decltype(close_file)> file(std::fopen(full.c_str(),"rb"),close_file);
        if(!file)return {Error::backend_error};
        MemoryProvider::Seed seed{path,{}};
        std::array<std::uint8_t,4096> buf{};
        for(;;){
            const auto want=std::min(buf.size(),MemoryProvider::byte_limit-used+1);
            const auto got=std::fread(buf.data(),1,want,file.get());
            if(std::ferror(file.get()))return {Error::backend_error};
            if(got>MemoryProvider::byte_limit-used)return {Error::invalid_buffer};
            seed.data.insert(seed.data.end(),buf.begin(),buf.begin()+got);used+=got;
            if(got<want){if(!std::feof(file.get()))return {Error::backend_error};break;}
        }
        seeds.push_back(std::move(seed));
    }
    return MemoryProvider::create(std::move(seeds));
}
} // namespace exodus::vfs
