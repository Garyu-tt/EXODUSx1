"use strict";
// Original portable reference policy. No GPU, vblank, donor code or device access.
function viewport(sw,sh,dw,dh,mode="fit"){
 if(![sw,sh,dw,dh].every(n=>Number.isInteger(n)&&n>=1&&n<=8192)||!["fit","integer","stretch"].includes(mode))throw Error("invalid viewport");
 let w=dw,h=dh;
 if(mode!=="stretch"){
   const factor=Math.min(Math.floor(dw/sw),Math.floor(dh/sh));
   if(mode==="integer"&&factor>=1){w=sw*factor;h=sh*factor;}
   else if(dw*sh<=dh*sw){w=dw;h=Math.max(1,Math.floor(dw*sh/sw));}
   else{h=dh;w=Math.max(1,Math.floor(dh*sw/sh));}
 }
 return {x:Math.floor((dw-w)/2),y:Math.floor((dh-h)/2),w,h};
}
function cadence(refresh,target,phase=0){
 if(!Number.isInteger(refresh)||refresh<1||refresh>1000000||!Number.isInteger(target)||target<1||target>refresh||!Number.isInteger(phase)||phase<0||phase>=refresh)throw Error("invalid cadence");
 const next=phase+target;return {present:next>=refresh,phase:next%refresh};
}
function tests(assert){
 const eq=(a,b)=>JSON.stringify(a)===JSON.stringify(b);
 assert(eq(viewport(1280,720,1920,1080),{x:0,y:0,w:1920,h:1080}),"16:9");
 assert(eq(viewport(640,480,1280,720),{x:160,y:0,w:960,h:720}),"pillarbox");
 assert(eq(viewport(320,240,1280,720,"integer"),{x:160,y:0,w:960,h:720}),"integer");
 assert(eq(viewport(1920,1080,1280,720,"integer"),{x:0,y:0,w:1280,h:720}),"integer downscale fallback");
 assert(eq(viewport(320,240,1280,720,"stretch"),{x:0,y:0,w:1280,h:720}),"stretch");
 assert(viewport(8192,1,1,8192).h===1,"extreme dimensions");
 for(const [refresh,target] of [[60,30],[60,40],[60,60],[60000,29970]]){
  let phase=0,count=0;for(let i=0;i<refresh;i++){const p=cadence(refresh,target,phase);phase=p.phase;count+=Number(p.present);}
  assert(count===target&&phase===0,`cadence ${refresh}/${target}`);
 }
 for(const f of [()=>viewport(0,1,1,1),()=>viewport(1,1,1,1,"invalid"),()=>cadence(60,61),()=>cadence(60,30,60)]){
 let threw=false;try{f();}catch{threw=true;}assert(threw,"invalid rejected");
 }
}
module.exports={viewport,cadence,tests};
if(require.main===module){let n=0;tests((v,m)=>{if(!v)throw Error(m);n++;});console.log(n+" graphics policy assertions passed");}
