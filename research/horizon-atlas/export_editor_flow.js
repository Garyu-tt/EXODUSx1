#!/usr/bin/env node
"use strict";
function exportFlow(g,mode="overview"){
 if(g.schema!=="exodus.horizon-evidence-graph/v1"||!Array.isArray(g.nodes)||!Array.isArray(g.edges))throw Error("invalid graph");
 const nodes=new Map();for(const n of g.nodes){if(typeof n.id!=="string"||nodes.has(n.id)||n.executable!==false)throw Error("invalid/non-inert node");nodes.set(n.id,n)}
 const adjacency=new Map([...nodes.keys()].map(id=>[id,[]]));
 for(const e of g.edges){if(!Array.isArray(e)||e.length!==3||!nodes.has(e[0])||!nodes.has(e[1]))throw Error("invalid edge");adjacency.get(e[0]).push(["out",e[2],e[1]]);adjacency.get(e[1]).push(["in",e[2],e[0]])}
 if(!["overview","full"].includes(mode)&&!mode.startsWith("module:"))throw Error("invalid mode");
 if(mode.startsWith("module:")&&!nodes.has(mode))throw Error("unknown module");
 const safe=s=>String(s).replace(/[&<>`[\]*_!\\]/g,c=>"&#"+c.charCodeAt(0)+";").replace(/[\r\n]/g," ");
 const selected=g.nodes.filter(n=>mode==="full"||mode==="overview"&&["platform","domain","source_module","consumer_candidate","reviewed_source_atom"].includes(n.kind)||mode.startsWith("module:")&&(n.id===mode||n.module===mode.slice(7)||n.kind==="reviewed_source_atom"&&n.source_path?.endsWith("/"+mode.slice(7)+".c")));
 const domainFor=new Map(g.edges.filter(e=>e[2]==="contains"&&e[0].startsWith("domain:")).map(e=>[e[1],e[0].slice(7)]));
 const moduleDomain=new Map(g.edges.filter(e=>e[2]==="source_inventory").map(e=>[e[1],domainFor.get(e[0])||"other"]));
 function group(n){if(n.module)return "Detail "+n.module;if(n.kind==="reviewed_source_atom")return "Reviewed atoms";if(n.kind==="source_module")return "Domain "+moduleDomain.get(n.id);if(n.kind==="header")return "Domain "+(domainFor.get(n.id)||"other");if(n.kind==="svc_wrapper"||n.kind==="svc_instruction")return "Kernel SVC";if(n.kind==="consumer_candidate")return "Consumers";return "Overview"}
 const tabs=new Map(),out=[];let serial=0;const id=()=>("e70d"+(++serial).toString(16).padStart(12,"0"));
 for(const n of selected.sort((a,b)=>a.id.localeCompare(b.id))){
   const key=group(n);let t=tabs.get(key);if(!t){const tab={id:id(),type:"tab",label:"Horizon | "+key,disabled:true,info:"Read-only inventory; no executable nodes or wires. Relations are documented in each comment."};out.push(tab);t={tab,index:0};tabs.set(key,t)}
   const i=t.index++;const attrs=Object.entries(n).filter(([k])=>!["id","kind","label","executable"].includes(k)).map(([k,v])=>safe(k)+": "+safe(typeof v==="string"?v:JSON.stringify(v)));
   const links=adjacency.get(n.id).map(([dir,type,peer])=>safe(dir+" "+type+" "+peer));
   const info=["Evidence ID: "+safe(n.id),"Kind: "+safe(n.kind),"Executable: false",...attrs,"","Relations ("+links.length+"):",...links].join("\n\n");
   out.push({id:id(),type:"comment",z:t.tab.id,name:(n.kind==="ipc_call_candidate"?"IPC ":"")+String(n.label).slice(0,105),info,x:200+(i%3)*350,y:60+Math.floor(i/3)*52,wires:[]});
 }
 return out;
}
if(require.main===module){const fs=require("node:fs");const [input,output,mode="overview"]=process.argv.slice(2);if(!input||!output)throw Error("usage: node export_editor_flow.js GRAPH.json OUTPUT.json [overview|full|module:bsd]");fs.writeFileSync(output,JSON.stringify(exportFlow(JSON.parse(fs.readFileSync(input,"utf8")),mode),null,2)+"\n");}
module.exports={exportFlow};
