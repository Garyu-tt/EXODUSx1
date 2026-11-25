import {buildGraph} from './core.mjs';
function selfTest(){
 function check(v,msg){if(!v)throw Error(msg);}
 const inputs=[{module:"KERNEL32.dll",source:"dlls/kernel32/kernel32.spec",text:"@ stdcall -arch=i386 Foo(ptr long) NTDLL.Bar # comment\n@ stdcall -arch=arm64 Foo(ptr) NTDLL.Bar\n12 stub @\n13 stdcall -noname Ord() other.#42\n@ extern Thing\n# disabled\n@ stdcall Alias() LocalImpl\n@ stub Missing\n"},{module:"ntdll",source:"dlls/ntdll/ntdll.spec",text:"@ stdcall -syscall=0x18 Bar(ptr long)\n"}];
 const g=buildGraph(inputs,{fixture:true});
 check(g.complete&&g.stats.declarations===8&&g.stats.stubs===2,"counts");
 check(g.nodes.filter(n=>n.kind==="declaration"&&n.name==="Foo").length===2,"variants");
 check(g.nodes.some(n=>n.id==="api:kernel32!#12"&&n.declared),"ordinal-only");
 check(g.nodes.some(n=>n.id==="api:kernel32!#13"&&n.declared),"ordinal alias");
 check(g.edges.some(e=>e.kind==="forwarder"&&e.to==="api:other!#42"),"ordinal forward");
 check(g.nodes.some(n=>n.id==="api:ntdll!Bar"&&n.declared),"declared target");
 check(g.nodes.some(n=>n.id==="symbol:kernel32!LocalImpl"&&!n.resolved),"local candidate");
 check(!g.edges.some(e=>e.kind==="implementation_candidate"&&e.from==="decl:dlls/kernel32/kernel32.spec:8"),"stub not implemented");
 check(JSON.stringify(g)===JSON.stringify(buildGraph(inputs.slice().reverse(),{fixture:true})),"determinism");
 check(!buildGraph([{module:"x",source:"x.spec",text:"invalid line\n"}],{}).complete,"diagnostics");
 return 10;
}
console.log(`${selfTest()} assertions passed`);
