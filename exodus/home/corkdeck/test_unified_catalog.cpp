#include "unified_catalog.hpp"
#include <cassert>
#include <unistd.h>
#include <cstdlib>
#include <cstring>
using namespace exodus::home;
int main() {
    auto c=ParseCorkDeck("version=3\nnext-id=3\n[game 1]\npath=sdmc:/switch/wine/drive_c/game.exe\ntitle=Game%20One\nportrait-art=sdmc:/switch/wine/a.jpg\n");
    assert(c.status==CatalogStatus::Ready&&c.entries.size()==1&&c.entries[0].title=="Game One");
    assert(c.entries[0].provider==Provider::CorkDeck);
    assert(ParseCorkDeck("version=3\n[game 1]\npath=x%GG\n").status==CatalogStatus::Invalid);
    assert(ParseCorkDeck("version=2\n[game 1]\npath=X\n[game 2]\npath=x\n").status==CatalogStatus::Invalid);
    assert(ParseCorkDeck("version=1\n").status==CatalogStatus::Invalid);
    assert(ParseCorkDeck("version=2\n[game 1]\npath=x\ntitle=%C0%AF\n").status==CatalogStatus::Invalid);
    Catalog base{CatalogStatus::Ready,{{Provider::Switch,"Native","123","",0},{Provider::CorkDeck,"Old","old","",0}}};
    MergeProvider(base,std::move(c),Provider::CorkDeck);
    assert(base.entries.size()==2&&base.entries[0].provider==Provider::Switch&&base.entries[1].title=="Game One");
    MergeProvider(base,{CatalogStatus::Invalid,{}},Provider::CorkDeck);
    assert(base.entries.size()==2);
    MergeProvider(base,{CatalogStatus::Ready,{}},Provider::CorkDeck);
    assert(base.entries.size()==1);
    char temp[]="/tmp/exodus-steam-XXXXXX";
    assert(mkdtemp(temp));
    std::string dir=temp;
    const std::string old=dir+"/steam-catalog-00000000000000000001.json";
    const std::string newer=dir+"/steam-catalog-00000000000000000002.json";
    const std::string unrelated=dir+"/.steam-catalog-00000000000000000003.json";
    const char *valid=R"({"schema_version":1,"entries":[{"provider":"steam","provider_id":"123","name":"Steam title","artwork":null,"installation_state":"unknown","last_played":null,"compatibility_state":"unknown","launch_mode":null}]})";
    { std::ofstream f(old); f<<valid; }
    { std::ofstream f(newer); f<<"broken"; }
    { std::ofstream f(unrelated); f<<valid; }
    // Inject an I/O failure after a valid older generation was observed.
    // Partial enumeration must never replace a previously known catalog.
    auto failed_read = +[](DIR *) -> struct dirent * {
        static unsigned calls=0;
        static struct dirent entry{};
        if(calls++==0) {
            std::strcpy(entry.d_name,"steam-catalog-00000000000000000001.json");
            return &entry;
        }
        errno=EIO;
        return nullptr;
    };
    auto incomplete=ReadSteamLatest(temp,4096,failed_read);
    assert(incomplete.status==CatalogStatus::Unavailable);
    Catalog known{CatalogStatus::Ready,{{Provider::Steam,"Existing","999","",0}}};
    MergeProvider(known,std::move(incomplete),Provider::Steam);
    assert(known.entries.size()==1&&known.entries[0].id=="999");
    auto steam=ReadSteamLatest(temp,4096);
    assert(steam.status==CatalogStatus::Ready&&steam.entries.size()==1&&steam.entries[0].id=="123");
    assert(ReadSteamLatest(temp,0).status==CatalogStatus::Unavailable);
    MergeProvider(base,std::move(steam),Provider::Steam);
    assert(base.entries.size()==2&&base.entries[0].provider==Provider::Switch);
    assert(!unlink(old.c_str())&&!unlink(newer.c_str())&&!unlink(unrelated.c_str())&&!rmdir(temp));
}
