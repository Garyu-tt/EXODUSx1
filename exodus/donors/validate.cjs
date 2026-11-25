#!/usr/bin/env node
"use strict";
const fs=require("node:fs");
const path=require("node:path");
function analyze(registry) {
  const errors=[], owners=new Map(), repositories=new Set(), groups=new Map();
  const requiredGates=["source_commit_and_license","dependency_graph","single_provider_owner","host_contract_tests","native_build","physical_device_probe","reversible_integration"];
  const nonempty=x=>typeof x==="string"&&x.trim().length>0;
  if (!registry||registry.schema_version!==1||!Array.isArray(registry.donors)||!registry.donors.length) return {errors:["Invalid registry schema"],donors:0,admitted:0,capabilities:[]};
  if (!Array.isArray(registry.gates)||requiredGates.some(g=>!registry.gates.includes(g))) errors.push("Missing required admission gates");
  let admitted=0;
  for(const [i,d] of registry.donors.entries()){
    const label=`donors[${i}]`;
    if(!d||typeof d!=="object"){errors.push(`${label}: invalid donor`);continue;}
    for(const k of ["repository","discovery_ref","capability","owner","blockers","first_slice"]) if(!nonempty(d[k]))errors.push(`${label}: invalid ${k}`);
    if(!nonempty(d.repository)||!/^[-\w.]+\/[-\w.]+$/.test(d.repository)){errors.push(`${label}: invalid repository identity`);continue;}
    const key=d.repository.toLowerCase();
    if(repositories.has(key))errors.push(`${label}: duplicate repository ${d.repository}`);
    repositories.add(key);
    if(!nonempty(d.capability)||!nonempty(d.owner))continue;
    if(owners.has(d.capability)&&owners.get(d.capability)!==d.owner)errors.push(`${label}: competing owners for ${d.capability}`);
    else owners.set(d.capability,d.owner);
    if(!groups.has(d.capability))groups.set(d.capability,[]);
    groups.get(d.capability).push(d.repository);
    if(d.pinned_commit!==null&&!(typeof d.pinned_commit==="string"&&/^[a-f0-9]{40}$/.test(d.pinned_commit)))errors.push(`${label}: invalid immutable SHA`);
    if(typeof d.license_verified!=="boolean"||typeof d.device_verified!=="boolean")errors.push(`${label}: invalid verification flags`);
    if(!["audit_required","admitted"].includes(d.admission))errors.push(`${label}: invalid admission state`);
    if(d.admission==="admitted"){
      const before=errors.length;
      if(!d.pinned_commit||d.license_verified!==true||d.device_verified!==true)errors.push(`${label}: missing verified admission prerequisites`);
      if(!d.evidence||requiredGates.some(g=>!nonempty(d.evidence[g])))errors.push(`${label}: missing evidence references`);
      if(errors.length===before)admitted++;
    }
  }
  return {errors,donors:registry.donors.length,admitted:errors.length?0:admitted,capabilities:[...groups].sort(([a],[b])=>a.localeCompare(b)).map(([id,donors])=>({id,owner:owners.get(id),donors:donors.sort()}))};
}
function selftest(analyze,registry,assert){
 const fixture={schema_version:1,gates:["source_commit_and_license","dependency_graph","single_provider_owner","host_contract_tests","native_build","physical_device_probe","reversible_integration"],donors:["First","Second"].map(name=>({repository:"Fixture/"+name,discovery_ref:"main",capability:"fixture.capability",owner:"fixture",blockers:"fixture",first_slice:"fixture",admission:"audit_required",pinned_commit:null,license_verified:false,device_verified:false}))};
 const copy=()=>JSON.parse(JSON.stringify(fixture));
 const clean=analyze(registry);assert(clean.errors.length===0,"registry valid");assert(clean.donors===registry.donors.length,"count");assert(analyze(fixture).admitted===0,"fixture not admitted");
 let r=copy();r.donors.push({...r.donors[0]});assert(analyze(r).errors.some(e=>e.includes("duplicate repository")),"duplicate");
 r=copy();r.donors[1].owner="competing";assert(analyze(r).errors.some(e=>e.includes("competing owners")),"owner conflict");
 r=copy();r.donors[0].admission="admitted";assert(analyze(r).errors.length>0&&analyze(r).admitted===0,"fail closed");
 r=copy();r.donors[0].pinned_commit="main";assert(analyze(r).errors.some(e=>e.includes("immutable SHA")),"mutable ref");
 r=copy();r.gates=[];assert(analyze(r).errors.some(e=>e.includes("required admission")),"missing gates");
 r=copy();r.donors[0].admission="admitted";r.donors[0].pinned_commit="a".repeat(40);r.donors[0].license_verified=true;r.donors[0].device_verified=true;r.donors[0].evidence=Object.fromEntries(r.gates.map(g=>[g,"fixture only"]));assert(analyze(r).admitted===1&&analyze(r).errors.length===0,"positive schema fixture");
 assert(analyze(null).errors.length>0,"invalid schema");
}
module.exports={analyze,selftest};
if(require.main===module){
 try {
  const args=process.argv.slice(2);
  if(args.some(a=>a!=="--self-test"))throw Error("Usage: node validate.cjs [--self-test]");
  const registry=JSON.parse(fs.readFileSync(path.join(__dirname,"registry.json"),"utf8"));
  const report=analyze(registry);
  if(args.includes("--self-test")){let count=0;selftest(analyze,registry,(v,m)=>{if(!v)throw Error(m);count++;});report.self_test_assertions=count;}
  console.log(JSON.stringify(report,null,2));
  if(report.errors.length)process.exitCode=1;
 } catch(e){console.error(e.message);process.exitCode=1;}
}
