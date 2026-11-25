#include "usb_provider.hpp"
#include "descriptor_table.hpp"
#include <cassert>
#include <iostream>
using namespace exodus::vfs;
struct Mock final : UsbBackend {
    UsbVolume v{"ums0:",3,1,1};
    int opens=0,stats=0;
    std::unique_ptr<MemoryProvider> mem=MemoryProvider::create({{"file",{'a','b','c'}},{"dir/x",{'x'}}}).handle;
    UsbVolume volume() const override{return v;}
    ReadResult read(std::string_view p,std::uint64_t off,std::uint8_t* b,std::size_t n) override{return mem->read(p,off,b,n);}
    StatResult stat(std::string_view p) override{++stats;return mem->stat(p);}
    OpenResult<FileHandle> open_file(std::string_view p) override{++opens;return mem->open_file(p);}
    OpenResult<DirectoryHandle> open_directory(std::string_view p) override{++opens;return mem->open_directory(p);}
};
int main(){
    int n=0;auto check=[&](bool b){++n;assert(b);};
    auto backend=std::make_shared<Mock>();UsbGate gate;std::uint8_t buf[4]{};
    UsbProvider off(gate.lease(),backend);check(off.stat("").error==Error::unavailable);
    check(gate.admit(backend->v,true));UsbProvider p(gate.lease(),backend);
    Broker broker({{"usb",&p,7,true}});DescriptorTable table(broker);
    check(p.open_file("../file").error==Error::invalid_path);check(p.open_directory("/dir").error==Error::invalid_path);check(backend->opens==0);
    check(broker.stat("/usb/file").metadata.size==3);
    auto f=table.open("/usb/file"),d=table.opendir("/usb");check(!f.error&&!d.error);
    check(table.read(f.value,buf,1).value==1&&buf[0]=='a');check(table.readdir(d.value).value.entry.name=="dir");
    check(broker.list("/usb",1).truncated);check(broker.route("/usb/file",Op::write).error==Error::denied);
    backend->v.generation=2;
    check(table.read(f.value,buf,1).error==ENODEV);check(table.fstat(f.value).error==ENODEV);
    check(table.readdir(d.value).error==ENODEV);check(table.open("/usb/file").error==ENODEV);
    gate.revoke();check(gate.admit(backend->v,true));check(p.stat("").error==Error::unavailable);
    UsbProvider fresh(gate.lease(),backend);check(fresh.stat("file").metadata.size==3);
    auto handle=fresh.open_file("file");check(handle.error==Error::ok);
    backend->v.flags=3;check(handle.handle->read_at(0,buf,1).error==Error::unavailable);
    backend->v.flags=1;gate.revoke();check(handle.handle->stat().error==Error::unavailable);
    check(!table.close(f.value));check(!table.close(d.value));
    std::weak_ptr<Mock> weak;std::unique_ptr<FileHandle> orphan;
    {UsbGate g;auto m=std::make_shared<Mock>();weak=m;check(g.admit(m->v,true));UsbProvider u(g.lease(),m);orphan=u.open_file("file").handle;check(bool(orphan));}
    check(!weak.expired());check(orphan->read_at(0,buf,1).error==Error::unavailable);orphan.reset();check(weak.expired());
    std::cout<<n<<" checks passed\n";
}
