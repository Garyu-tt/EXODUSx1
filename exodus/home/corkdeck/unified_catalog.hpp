// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include "../ulaunch/catalog.hpp"
#include <algorithm>
#include <charconv>
#include <fstream>
#include <map>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <dirent.h>
#include <sys/stat.h>

namespace exodus::home {
inline void MergeProvider(Catalog &base, Catalog incoming, Provider provider)
{
    if (incoming.status != CatalogStatus::Ready ||
        !std::all_of(incoming.entries.begin(), incoming.entries.end(),
                     [provider](const Entry &e){return e.provider==provider;})) return;
    size_t retained=std::count_if(base.entries.begin(),base.entries.end(),
                                  [provider](const Entry &e){return e.provider!=provider;});
    if(incoming.entries.size()>CatalogEntryLimit-retained) return;
    base.entries.erase(std::remove_if(base.entries.begin(),base.entries.end(),
                                     [provider](const Entry &e){return e.provider==provider;}),base.entries.end());
    for(auto &e:incoming.entries) base.entries.push_back(std::move(e));
    base.status=CatalogStatus::Ready;
}
inline bool HexValue(char c,unsigned &n)
{
    if(c>='0'&&c<='9') n=c-'0';
    else if(c>='a'&&c<='f') n=c-'a'+10;
    else if(c>='A'&&c<='F') n=c-'A'+10;
    else return false;
    return true;
}
inline bool DecodeIni(std::string_view raw,std::string &out,size_t limit)
{
    out.clear();
    for(size_t i=0;i<raw.size();++i) {
        unsigned c=(unsigned char)raw[i];
        if(c=='%') {
            unsigned a,b;
            if(i+2>=raw.size()||!HexValue(raw[i+1],a)||!HexValue(raw[i+2],b)) return false;
            c=a*16+b; i+=2;
        }
        if(c<32||c==127||out.size()+1>=limit) return false;
        out.push_back((char)c);
    }
    // Reject malformed UTF-8, including overlong encodings and surrogates.
    for(size_t i=0;i<out.size();) {
        unsigned a=(unsigned char)out[i++];
        if(a<128) continue;
        unsigned n=a>=0xc2&&a<=0xdf?1:a>=0xe0&&a<=0xef?2:a>=0xf0&&a<=0xf4?3:0;
        if(!n||out.size()-i<n) return false;
        unsigned b=(unsigned char)out[i];
        if((a==0xe0&&b<0xa0)||(a==0xed&&b>=0xa0)||(a==0xf0&&b<0x90)||(a==0xf4&&b>=0x90)) return false;
        while(n--) if(((unsigned char)out[i++]&0xc0)!=0x80) return false;
    }
    return true;
}
inline std::string TrimIni(std::string_view x)
{
    auto a=x.find_first_not_of(" \t\r"), b=x.find_last_not_of(" \t\r");
    return a==std::string_view::npos?"":std::string(x.substr(a,b-a+1));
}
inline Catalog ParseCorkDeck(std::string_view text)
{
    if(text.size()>CatalogByteLimit) return {CatalogStatus::TooLarge,{}};
    Catalog result{CatalogStatus::Ready,{}};
    std::map<std::string,std::string> fields;
    std::set<unsigned> numbers;
    std::set<std::string> ids;
    bool game=false;
    auto flush=[&](){
        if(!game) return fields["version"]=="2"||fields["version"]=="3";
        std::string id,name,art;
        if(!DecodeIni(fields["path"],id,512)||id.empty()||
           !DecodeIni(fields["title"],name,128)) return false;
        std::string folded=id;
        for(char &c:folded) if(c>='A'&&c<='Z') c+=32;
        if(!ids.insert(folded).second) return false;
        for(auto key:{"portrait-art","landscape-art","square-art","hero-art"}) {
            std::string value;
            if(!DecodeIni(fields[key],value,512)) return false;
            if(art.empty()) art=std::move(value);
        }
        if(name.empty()) name=id.substr(id.find_last_of('/')+1);
        if(name.empty()) name=id;
        result.entries.push_back({Provider::CorkDeck,std::move(name),std::move(id),std::move(art),0});
        return true;
    };
    size_t start=0;
    while(start<text.size()) {
        auto end=text.find('\n',start);
        if(end==std::string_view::npos) end=text.size();
        auto raw=text.substr(start,end-start); start=end+1;
        if(raw.size()>1198||raw.find('\0')!=std::string_view::npos) return {CatalogStatus::Invalid,{}};
        auto line=TrimIni(raw);
        if(line.empty()||line[0]=='#'||line[0]==';') continue;
        if(line[0]=='[') {
            if(line.size()<8||line.compare(0,6,"[game ")||line.back()!=']'||
               numbers.size()>=256||!flush()) return {CatalogStatus::Invalid,{}};
            unsigned n=0;
            auto begin=line.data()+6, last=line.data()+line.size()-1;
            auto p=std::from_chars(begin,last,n);
            if(*begin=='0'||p.ec!=std::errc{}||p.ptr!=last||!n||n==UINT32_MAX||!numbers.insert(n).second)
                return {CatalogStatus::Invalid,{}};
            fields.clear(); game=true;
        } else {
            auto eq=line.find('=');
            if(eq==std::string::npos) return {CatalogStatus::Invalid,{}};
            auto key=TrimIni(std::string_view(line).substr(0,eq));
            if(key.empty()||!fields.emplace(key,TrimIni(std::string_view(line).substr(eq+1))).second)
                return {CatalogStatus::Invalid,{}};
        }
    }
    if(!flush()) return {CatalogStatus::Invalid,{}};
    return result;
}
inline Catalog ReadCorkDeck(const char *path)
{
    struct stat st{};
    if(stat(path,&st)||!S_ISREG(st.st_mode)||st.st_size<0) return {CatalogStatus::Unavailable,{}};
    if((uint64_t)st.st_size>CatalogByteLimit) return {CatalogStatus::TooLarge,{}};
    std::ifstream f(path,std::ios::binary);
    std::string text((size_t)st.st_size,'\0');
    if(!f.read(text.data(),text.size())||f.peek()!=std::istream::traits_type::eof()||f.bad())
        return {CatalogStatus::Unavailable,{}};
    return ParseCorkDeck(text);
}
inline Catalog ReadSteamLatest(const char *directory,size_t remaining)
{
    DIR *d=opendir(directory);
    if(!d) return {CatalogStatus::Unavailable,{}};
    std::vector<std::string> names;
    unsigned visited=0;
    while(auto *e=readdir(d)) {
        if(++visited>4096) {closedir(d);return {CatalogStatus::TooLarge,{}};}
        std::string n=e->d_name;
        if(n.size()!=39||n.compare(0,14,"steam-catalog-")||n.compare(34,5,".json")) continue;
        uint64_t generation=0;
        auto p=std::from_chars(n.data()+14,n.data()+34,generation);
        if(p.ec==std::errc{}&&p.ptr==n.data()+34&&generation) names.push_back(std::move(n));
    }
    closedir(d);
    std::sort(names.rbegin(),names.rend());
    unsigned attempts=0;
    for(const auto &name:names) {
        if(++attempts>8) break;
        std::string path=std::string(directory)+"/"+name;
        struct stat st{};
        if(stat(path.c_str(),&st)||!S_ISREG(st.st_mode)) continue;
        auto c=ReadCatalog(path.c_str());
        if(c.status==CatalogStatus::Ready&&c.entries.size()<=remaining&&
           std::all_of(c.entries.begin(),c.entries.end(),[](const Entry &e){return e.provider==Provider::Steam;}))
            return c;
    }
    return {CatalogStatus::Unavailable,{}};
}
}
