#!/usr/bin/env node
"use strict";
function buildGraph(input){
 const {atlas,domains,svc,reviewed,forwardA,forwardB,coverage}=input;
 const pin=atlas.sources.libnx.revision;
 if(svc.source.revision!==pin||reviewed.revision!==pin||forwardA.source_revision!==pin||forwardB.source_revision!==pin)throw Error("revision mismatch");
 const nodes=new Map(),edges=new Map();
 function node(id,kind,label,attrs={}){if(nodes.has(id))throw Error("duplicate node "+id);nodes.set(id,{id,kind,label,...attrs,executable:false});return id}
 function edge(from,to,type){if(!nodes.has(from)||!nodes.has(to))throw Error("dangling edge "+from+" "+to);const key=JSON.stringify([from,to,type]);edges.set(key,[from,to,type]);}
 const root=node("root:horizon","platform","Horizon / Switch 1",{revision:pin});
 for(const d of domains.nodes){const id=node("domain:"+d.id,"domain",d.id,{description:d.description,constraints:d.constraints});edge(root,id,"contains");}
 for(const h of atlas.headers){const d="domain:"+h.domain;if(!nodes.has(d)){node(d,"domain",h.domain);edge(root,d,"contains")}node(h.id,"header",h.path,{policy:h.policy});edge(d,h.id,"contains");}
 for(const d of domains.nodes){for(const h of d.header_refs)edge("domain:"+d.id,h,"references");for(const c of d.potential_consumers){const id="consumer:"+c;if(!nodes.has(id))node(id,"consumer_candidate",c);edge("domain:"+d.id,id,"potentially_supports");}}
 for(const e of domains.edges)edge("domain:"+e.from,"domain:"+e.to,"architecture_candidate");
 const headers=new Map(atlas.headers.filter(x=>x.layer==="services").map(h=>[h.path.split("/").pop().replace(/\.h$/,""),h]));
 for(const c of coverage.coverage){const h=headers.get(c.module);if(!h)throw Error("missing module header "+c.module);const id=node("module:"+c.module,"source_module",c.module,{coverage:c.status,batch:c.batch,policy:h.policy});edge(h.id,id,"source_inventory");}
 for(const w of atlas.svc_wrappers){node(w.id,"svc_wrapper",w.name,{policy:w.policy});edge("header:switch/kernel/svc.h",w.id,"declares");}
 for(const r of svc.records){const wid="svc:"+r.wrapper;for(const num of r.instruction_numbers){const id="svc-number:"+num;if(!nodes.has(id))node(id,"svc_instruction",num);edge(wid,id,"emits_instruction");}}
 const latest=new Map([...forwardA.records,...forwardB.records].map(r=>[r.module,r]));
 for(const [mod,r] of latest){
   const mid="module:"+mod;if(!nodes.has(mid))throw Error("unknown module "+mod);
   function fn(name){const id=`function:${mod}:${name}`;if(!nodes.has(id)){node(id,"function_candidate",name,{module:mod,path:r.source_path,blob:r.blob});edge(mid,id,"contains_candidate")}return id}
   for(const h of r.forwarding_helpers){const id=fn(h.name);nodes.get(id).helper_command_arguments=h.command_argument_indices;nodes.get(id).definition_line=h.line;}
   for(const [kind,rows] of [["direct",r.direct],["forwarded",r.forwarded]])for(const q of rows){const owner=fn(q.caller);const id=`call:${mod}:${q.line}:${q.caller}:${q.callee}:${q.command}:${q.command_argument_index}:${kind}`;if(nodes.has(id))continue;
      node(id,"ipc_call_candidate",`${q.caller} / ${q.command}`,{module:mod,source_line:q.line,command:q.command,callee:q.callee,receiver:q.receiver_candidate,route:kind,policy:"inventory_only"});edge(owner,id,"contains_call_candidate");if(kind==="forwarded")edge(id,fn(q.callee),"passes_command_candidate");}
 }
 for(const a of reviewed.atoms){const id=node("atom:"+a.id,"reviewed_source_atom",a.id,{source_path:a.source_path,line:a.line,command:a.command,root_endpoint_candidate:a.root_endpoint_candidate,receiver_symbol:a.receiver_symbol,policy:"review_required",device:"not_tested",preconditions:a.preconditions});const mod=a.source_path.split("/").pop().replace(/\.c$/,"");edge("module:"+mod,id,"contains_reviewed_atom");const f=`function:${mod}:${a.wrapper}`;if(nodes.has(f))edge(f,id,"source_review");}
 const counts={};for(const n of nodes.values())counts[n.kind]=(counts[n.kind]||0)+1;
 return {schema:"exodus.horizon-evidence-graph/v1",revision:pin,scope:"Read-only evidence graph, no executable nodes; candidate edges are not verified dependencies. R06 replaces overlapping R05 BSD. Historical endpoint registry is not merged as current facts.",edge_columns:["from","to","type"],counts:{nodes:nodes.size,edges:edges.size,forwarding_modules:latest.size,by_kind:counts},nodes:[...nodes.values()],edges:[...edges.values()]};
}
if(require.main===module){const fs=require("node:fs"),path=require("node:path");const names=["atlas.r01.json","domain-graph.r01.json","svc-map.r02.json","ipc-atoms-reviewed.r02.json","ipc-forwarding.r05-a.json","ipc-forwarding.r06-b.json","coverage.r04.json"],keys=["atlas","domains","svc","reviewed","forwardA","forwardB","coverage"];const dir=process.argv[2]||__dirname;const input=Object.fromEntries(names.map((n,i)=>[keys[i],JSON.parse(fs.readFileSync(path.join(dir,n),"utf8"))]));const g=buildGraph(input);fs.writeFileSync(process.argv[3]||path.join(dir,"evidence-graph.r07.json"),JSON.stringify(g)+"\n");console.log(JSON.stringify(g.counts));}
module.exports={buildGraph};
