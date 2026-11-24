#include "unified_catalog.hpp"
#include <cassert>
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
}
