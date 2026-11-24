#!/usr/bin/env node
"use strict";
// Heuristic source analysis; not a C AST or permission decision.
function analyze(source,module){
 const s=source.replace(/"(?:\\[\s\S]|[^"\\])*"|'(?:\\[\s\S]|[^'\\])*'|\/\*[\s\S]*?\*\/|\/\/[^\n]*/g,m=>m.startsWith("/")?m.replace(/[^\n]/g," "):m);
 const line=i=>s.slice(0,i).split("\n").length;
 function end(start,left,right){let d=0,q=null;for(let i=start;i<s.length;i++){const c=s[i];if(q){if(c==="\\")i++;else if(c===q)q=null;continue}if(c==="\""||c==="'"){q=c;continue}if(c===left)d++;if(c===right&&--d===0)return i;}return -1}
 function split(t){let a=[],start=0,d=0,q=null;for(let i=0;i<t.length;i++){const c=t[i];if(q){if(c==="\\")i++;else if(c===q)q=null;continue}if(c==="\""||c==="'"){q=c;continue}if("([{".includes(c))d++;else if(")]}".includes(c))d--;else if(c===","&&d===0){a.push(t.slice(start,i).trim());start=i+1}}a.push(t.slice(start).trim());return a}
 const defs=[];let lastEnd=-1;
 for(const m of s.matchAll(/^[ \t]*(?:static\s+)?(?:inline\s+)?(?:[A-Za-z_]\w*[ \t*]+)+([A-Za-z_]\w*)[ \t]*\(([^;{}]*?)\)[ \t]*\{/gm)){
 if(m.index<=lastEnd||["if","while","switch","for"].includes(m[1])||/^\s*return\b/.test(m[0]))continue;
 const open=m.index+m[0].length-1,close=end(open,"{","}");if(close<0)continue;lastEnd=close;
 defs.push({name:m[1],params:split(m[2]).map(x=>x.match(/([A-Za-z_]\w*)\s*$/)?.[1]??null),start:open+1,end:close,line:line(m.index),calls:[]});}
 const byName=new Map(defs.map(d=>[d.name,d]));
 function sink(n){if(/^(?:service|tipc)Dispatch(?:InOut|In|Out|Impl)?$/.test(n))return 1;if((module==="fs"||module==="ssl")&&new RegExp("^_"+module+"ObjectDispatch(?:InOut|In|Out)?$").test(n))return 1;if((module==="bsd"||module==="sfdnsres")&&new RegExp("^_"+module+"Dispatch(?:InOut|In|Out|Impl)?$").test(n))return 0;return null}
 for(const d of defs){for(const m of s.slice(d.start,d.end).matchAll(/\b([A-Za-z_]\w*)\s*\(/g)){const n=m[1];if(sink(n)===null&&!byName.has(n))continue;const idx=d.start+m.index,open=idx+m[0].length-1,close=end(open,"(",")");if(close<0||close>d.end)continue;d.calls.push({name:n,args:split(s.slice(open+1,close)),line:line(idx)})}}
 const routes=new Map();let changed=true;
 while(changed){changed=false;for(const d of defs){for(const c of d.calls){const indices=sink(c.name)!==null?[sink(c.name)]:[...(routes.get(c.name)||[])];for(const i of indices){const p=d.params.indexOf(c.args[i]);if(p<0)continue;const set=routes.get(d.name)||new Set();if(!set.has(p)){set.add(p);routes.set(d.name,set);changed=true}}}}}
 const direct=[],forwarded=[];
 for(const d of defs){for(const c of d.calls){const indices=sink(c.name)!==null?[sink(c.name)]:[...(routes.get(c.name)||[])];for(const i of indices){const v=c.args[i];if(!/^(?:0x[0-9a-f]+|\d+)$/i.test(v||""))continue;const row={caller:d.name,line:c.line,callee:c.name,command:Number(v),command_argument_index:i,receiver_candidate:sink(c.name)===1?c.args[0]:null,status:"syntactic_candidate_not_admitted"};(sink(c.name)!==null?direct:forwarded).push(row)}}}
 return {module,functions:defs.length,forwarding_helpers:[...routes].map(([name,indices])=>({name,command_argument_indices:[...indices],line:byName.get(name).line})),direct,forwarded};
}
function selftest(){
 const check=(v,msg)=>{if(!v)throw Error(msg)};
 const fixture=`static Result inner(Service* s, u32 id) {\n return serviceDispatch(s,id);\n}\nstatic Result outer(Service* s, u32 id) {\n return inner(s,id);\n}\nResult read(Service* s) {\n return outer(s, 0x12);\n}\nResult direct(Service* s) {\n /* serviceDispatch(s, 99); */\n return serviceDispatchIn(s, 7, (struct A){1,2});\n}\n`;
 const a=analyze(fixture,"test");check(a.functions===4,"function count");check(a.forwarding_helpers.length===2,"two hop propagation");check(a.forwarded.length===1&&a.forwarded[0].command===18,"hex and indirect");check(a.direct.length===1&&a.direct[0].command===7,"comment and aggregate");
 const b=analyze("static int wrap(int id) {\n return _bsdDispatchIn(id, 1);\n}\nint f(void) {\n return wrap(14);\n}\nint g(void) {\n return _bsdDispatchIn(26, 7);\n}\n","bsd");
 check(b.forwarded.length===1&&b.forwarded[0].command===14,"BSD command slot");check(b.direct.length===1&&b.direct[0].command===26,"BSD direct");
 const c=analyze("Result f(Service* s) {\n return _fsObjectDispatch(s, 2);\n}\n","fs");check(c.direct.length===1&&c.direct[0].command===2,"FS custom sink");
 const d=analyze("Result f(Service* s, u32 cmd) {\n return serviceDispatch(s, cmd+1);\n}\n","test");check(d.direct.length===0&&d.forwarding_helpers.length===0,"dynamic unresolved");return 8;
}
if (require.main === module) {
 const fs=require("node:fs"),path=require("node:path"),cp=require("node:child_process");
 const args=process.argv.slice(2);
 if(args[0]==="--selftest"){console.log(selftest()+" checks passed");process.exit(0);}
 if(args.length!==2)throw Error("usage: node scan_forwarding.js LIBNX_CHECKOUT OUTPUT.json");
 const root=path.resolve(args[0]);const git=(...a)=>cp.execFileSync("git",["-C",root,...a],{encoding:"utf8"}).trim();
 const revision=git("rev-parse","HEAD");if(revision!=="b156b4774fb2df7b93402169509266da7547fc4c"||git("status","--porcelain"))throw Error("expected clean pinned libnx checkout");
 const dir=path.join(root,"nx/source/services");const files=fs.readdirSync(dir).filter(n=>n.endsWith(".c")).sort();
 const records=files.map(file=>({...analyze(fs.readFileSync(path.join(dir,file),"utf8"),file.slice(0,-2)),source_path:"nx/source/services/"+file}));
 fs.writeFileSync(args[1],JSON.stringify({schema:"exodus.horizon-ipc-forwarding/v1",source_revision:revision,scope:"heuristic candidates, no runtime admission; shadowing, reassignment, strings, macros and control flow unresolved",records},null,2)+"\n");
}
module.exports={analyze,selftest};
