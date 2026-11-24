// EX-014: conservative native-provider candidate overlay. No runtime dispatch.
const contracts = [
  {
    "module": "kernel32",
    "name": "GetTickCount",
    "args": [],
    "contract": "clock.uptime_ms32",
    "candidate": "Horizon monotonic tick + Windows-compatible boot epoch",
    "gaps": [
      "boot epoch",
      "32-bit wrap",
      "sleep/resume",
      "resolution"
    ]
  },
  {
    "module": "kernel32",
    "name": "GetTickCount64",
    "args": [],
    "contract": "clock.uptime_ms64",
    "candidate": "Horizon monotonic tick + Windows-compatible boot epoch",
    "gaps": [
      "boot epoch",
      "sleep/resume",
      "resolution"
    ]
  },
  {
    "module": "kernel32",
    "name": "QueryPerformanceCounter",
    "args": [
      "ptr"
    ],
    "contract": "clock.performance_counter",
    "candidate": "Horizon monotonic tick",
    "gaps": [
      "signed 64-bit output",
      "guest pointer validation",
      "BOOL and last-error behavior",
      "counter monotonicity"
    ]
  },
  {
    "module": "kernel32",
    "name": "QueryPerformanceFrequency",
    "args": [
      "ptr"
    ],
    "contract": "clock.performance_frequency",
    "candidate": "Horizon tick frequency",
    "gaps": [
      "signed 64-bit output",
      "guest pointer validation",
      "stable frequency",
      "BOOL and last-error behavior"
    ]
  },
  {
    "module": "kernel32",
    "name": "Sleep",
    "args": [
      "long"
    ],
    "contract": "thread.sleep_ms",
    "candidate": "Horizon thread wait",
    "gaps": [
      "zero-duration yield",
      "INFINITE",
      "scheduler precision",
      "guest thread ownership"
    ]
  }
];

function mapProviders(graph){
 if(!graph||graph.schema_version!==1||graph.graph_kind!=="wine_export_inventory"||!Array.isArray(graph.nodes)||!Array.isArray(graph.edges))throw new TypeError("expected wine_export_inventory schema 1");
 const nodes=new Map(), declarations=new Map();
 for(const n of graph.nodes){if(!n||typeof n.id!=="string"||nodes.has(n.id))throw new TypeError("invalid or duplicate node");nodes.set(n.id,n);}
 for(const e of graph.edges){
  if(!e||!nodes.has(e.from)||!nodes.has(e.to))throw new TypeError("dangling graph edge");
  if(e.kind!=="declares")continue;
  if(nodes.get(e.from).kind!=="api"||nodes.get(e.to).kind!=="declaration")throw new TypeError("invalid declares edge");
  if(!declarations.has(e.from))declarations.set(e.from,new Set());
  declarations.get(e.from).add(e.to);
 }
 const mappings=contracts.map(c=>{
  const api="api:"+c.module+"!"+c.name, n=nodes.get(api);
  const rows=[...(declarations.get(api)||[])].sort().map(id=>nodes.get(id));
  const matches=rows.filter(r=>r.exportKind==="stdcall"&&Array.isArray(r.args)&&JSON.stringify(r.args)===JSON.stringify(c.args)&&r.name===c.name&&r.module===c.module&&Array.isArray(r.flags)&&!r.flags.includes("-noname")&&!r.flags.includes("-private")&&!r.flags.includes("-norelay"));
  const state=!n||n.kind!=="api"||!n.declared||!rows.length?"missing_declaration":matches.length!==rows.length?"needs_review":"candidate_unverified";
  return {api,contract:c.contract,candidate:c.candidate,state,dispatch:"disabled",gaps:[...c.gaps],evidence:rows.map(r=>({id:r.id,source:r.source,line:r.line,flags:r.flags,forward:r.forward??null})),matching_declarations:matches.length};
 });
 return {schema_version:1,graph_kind:"native_provider_candidates",inventory_provenance:graph.provenance??null,inventory_complete:graph.complete===true,mappings,dispatch_enabled:false};
}

function selftest(){
 const g={schema_version:1,graph_kind:"wine_export_inventory",complete:true,provenance:{revision:"fixture"},nodes:[],edges:[]};
 const add=(name,args,extra={})=>{const id="api:kernel32!"+name;if(!g.nodes.some(n=>n.id===id))g.nodes.push({id,kind:"api",declared:true});const d="decl:fixture:"+g.nodes.length;g.nodes.push({id:d,kind:"declaration",module:"kernel32",name,exportKind:"stdcall",args,flags:[],source:"fixture",line:1,...extra});g.edges.push({from:id,to:d,kind:"declares"});};
 const check=(v,m)=>{if(!v)throw new Error(m);};
 add("GetTickCount",[]);add("GetTickCount64",[],{forward:{module:"kernelbase",name:"GetTickCount64"}});add("QueryPerformanceCounter",["ptr"]);add("QueryPerformanceCounter",["long"]);add("Sleep",["long"],{exportKind:"stub"});
 const out=mapProviders(g), m=Object.fromEntries(out.mappings.map(x=>[x.contract,x]));
 check(m["clock.uptime_ms32"].state==="candidate_unverified","valid signature");
 check(m["clock.uptime_ms64"].evidence[0].forward.module==="kernelbase","forward provenance");
 check(m["clock.performance_counter"].state==="needs_review","conflicting variant");
 check(m["clock.performance_frequency"].state==="missing_declaration","missing");
 check(m["thread.sleep_ms"].state==="needs_review","stub");
 check(!out.dispatch_enabled&&out.mappings.every(x=>x.dispatch==="disabled"),"no dispatch");
 check(out.inventory_provenance.revision==="fixture","provenance");
 check(JSON.stringify(out)===JSON.stringify(mapProviders(g)),"determinism");
 g.edges.push({from:"missing",to:"also-missing",kind:"declares"});let rejected=false;try{mapProviders(g);}catch(e){rejected=e instanceof TypeError;}check(rejected,"dangling");
 return 9;
}

export {mapProviders, selftest};
