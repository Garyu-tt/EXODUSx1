"use strict";
// Offline selection model; no hardware I/O, reservation or actual admission.
function selectProvider(request,providers,state){
 const integer=n=>Number.isSafeInteger(n)&&n>=0;
 if(!request||typeof request.operation!=="string"||!request.operation||typeof request.context!=="string"||!request.context||typeof request.profile!=="string"||!request.profile||request.owned!==true||!integer(request.bytes)||!state||!integer(state.freeBytes)||!integer(state.quarantineBytes)||!integer(state.quarantineLimit)||!["normal","limited","critical","unknown"].includes(state.thermal)||!Array.isArray(providers))return {ok:false,reason:"invalid_request"};
 if(state.thermal==="critical")return {ok:false,reason:"thermal_stop"};
 if(state.quarantineBytes>state.quarantineLimit||state.quarantineBytes>=state.freeBytes)return {ok:false,reason:"memory_stop"};
 const eligible=providers.filter(p=>p&&typeof p.id==="string"&&p.id&&p.operation===request.operation&&p.context===request.context&&p.profile===request.profile&&p.admitted===true&&p.verified===true&&p.available===true&&p.safe===true&&p.inFlight===0&&integer(p.scratchBytes)&&p.scratchBytes<=state.freeBytes-state.quarantineBytes-request.bytes&&["cpu","gpu","specialized"].includes(p.kind)&&((state.thermal==="normal")||p.kind==="cpu"));
 const counts=new Map();for(const p of providers){if(p&&typeof p.id==="string")counts.set(p.id,(counts.get(p.id)||0)+1);}
 const rank={specialized:0,gpu:1,cpu:2};
 const sorted=eligible.filter(p=>counts.get(p.id)===1).sort((a,b)=>rank[a.kind]-rank[b.kind]||a.scratchBytes-b.scratchBytes||(a.id<b.id?-1:a.id>b.id?1:0));
 if(!sorted.length)return {ok:false,reason:"no_admitted_provider"};
 const p=sorted[0];return {ok:true,provider:p.id,reserveBytes:request.bytes+p.scratchBytes,kind:p.kind};
}
module.exports={selectProvider};
