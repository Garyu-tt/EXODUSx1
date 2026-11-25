function moduleKey(name){return name.toLowerCase().replace(/\.dll$/,"");}

function parseSpec(module,source,input){
 const rows=[],diagnostics=[];
 for(const [index,raw] of input.split(/\r?\n/).entries()){
  const line=raw.replace(/(^|\s)#.*$/,"").trim();if(!line)continue;
  const m=/^(@|\d+)\s+([a-z][a-z0-9]*)\s+((?:-[^\s]+\s+)*)([^\s(]+)\s*(?:\(([^()]*)\))?\s*(.*)$/i.exec(line);
  if(!m){diagnostics.push({source,line:index+1,reason:"unsupported declaration",raw});continue;}
  const [,ordinal,exportKind,flagsText,name,args,tail]=m,flags=flagsText.trim().split(/\s+/).filter(Boolean);
  if(!["stdcall","cdecl","varargs","pascal","thiscall","stub","extern","variable","equate","interrupt"].includes(exportKind)||(name==="@"&&ordinal==="@")||(["stdcall","cdecl","varargs","pascal","thiscall"].includes(exportKind)&&args===undefined)||(tail&&/\s/.test(tail))){
   diagnostics.push({source,line:index+1,reason:"unsupported declaration form",raw});continue;
  }
  const target=tail||null,dot=target?.lastIndexOf(".")??-1;
  const forwarded=dot>0&&dot<target.length-1&&!["equate","variable"].includes(exportKind);
  rows.push({module:moduleKey(module),source,line:index+1,ordinal:ordinal==="@"?null:Number(ordinal),name:name==="@"?null:name,exportKind,flags,args:args===undefined?null:args.trim().split(/\s+/).filter(Boolean),target,forward:forwarded?{module:moduleKey(target.slice(0,dot)),name:target.slice(dot+1)}:null,raw});
 }
 return {rows,diagnostics};
}

function buildGraph(inputs,provenance){
 const nodes=new Map(),edges=new Map(),diagnostics=[],rows=[];
 function node(id,kind,data={}){if(!nodes.has(id))nodes.set(id,{...data,id,kind});return id;}
 function edge(from,to,kind){edges.set(JSON.stringify([from,kind,to]),{from,to,kind});}
 function api(module,name){return node("api:"+module+"!"+name,"api",{module,name,declared:false});}
 for(const input of [...inputs].sort((a,b)=>a.source.localeCompare(b.source,"en"))){
  const p=parseSpec(input.module,input.source,input.text);rows.push(...p.rows);diagnostics.push(...p.diagnostics);
 }
 for(const row of rows){
  const module=node("module:"+row.module,"module",{name:row.module}),primary=api(row.module,row.name??"#"+row.ordinal);
  nodes.get(primary).declared=true;
  const decl=node("decl:"+row.source+":"+row.line,"declaration",{...row,provider:row.exportKind==="stub"?"stub":"wine_candidate",equivalence:"unverified"});
  edge(module,primary,"exports");edge(primary,decl,"declares");
  if(row.ordinal!==null&&row.name!==null){const ordinal=api(row.module,"#"+row.ordinal);nodes.get(ordinal).declared=true;edge(module,ordinal,"exports");edge(ordinal,decl,"declares");}
  if(row.forward)edge(decl,api(row.forward.module,row.forward.name),"forwarder");
  else if(!["stub","variable","equate"].includes(row.exportKind)){const symbol=row.target??row.name;if(symbol)edge(decl,node("symbol:"+row.module+"!"+symbol,"symbol_candidate",{module:row.module,name:symbol,resolved:false}),"implementation_candidate");}
 }
 const all=[...nodes.values()],links=[...edges.values()];
 return {schema_version:1,graph_kind:"wine_export_inventory",provenance,complete:diagnostics.length===0,nodes:all.sort((a,b)=>a.id.localeCompare(b.id,"en")),edges:links.sort((a,b)=>JSON.stringify(a).localeCompare(JSON.stringify(b),"en")),diagnostics,stats:{declarations:rows.length,apis:all.filter(n=>n.kind==="api"&&n.declared).length,stubs:rows.filter(r=>r.exportKind==="stub").length,forwarders:rows.filter(r=>r.forward).length,unresolved_forward_targets:all.filter(n=>n.kind==="api"&&!n.declared).length,modules:all.filter(n=>n.kind==="module").length}};
}

export {parseSpec,buildGraph};
