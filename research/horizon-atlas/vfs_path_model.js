"use strict";
// Research-only host model; lexical checks are not a filesystem sandbox.
function resolveMountPath(mounts,request){
 if(!Array.isArray(mounts)||!request||typeof request.path!=="string"||!["read","list","stat","write","mkdir","rename","delete"].includes(request.operation))return {ok:false,error:"invalid_request"};
 const p=request.path;
 if(p.length>1024||!/^\/[a-z][a-z0-9_-]*(?:\/|$)/.test(p)||/[\u0000-\u001f\u007f\\:%]/.test(p)||p.includes("//"))return {ok:false,error:"invalid_path"};
 const parts=p.slice(1).split("/");if(parts.at(-1)==="")parts.pop();
 if(parts.some(x=>!x||x==="."||x===".."||x.length>255||x.endsWith(".")||x.endsWith(" ")))return {ok:false,error:"invalid_path"};
 const matches=mounts.filter(m=>m&&m.name===parts[0]);
 if(matches.length!==1||matches[0].admitted!==true)return {ok:false,error:"mount_unavailable"};
 const m=matches[0];if(!Array.isArray(m.operations)||!m.operations.includes(request.operation))return {ok:false,error:"operation_denied"};
 if(parts.length===1&&["write","mkdir","rename","delete"].includes(request.operation))return {ok:false,error:"mount_root_protected"};
 if(["write","mkdir","rename","delete"].includes(request.operation)&&m.readOnly!==false)return {ok:false,error:"read_only"};
 if(request.operation==="rename")return {ok:false,error:"two_path_operation_not_implemented"};
 return {ok:true,mount:m.name,relative:parts.slice(1).join("/"),operation:request.operation};
}
module.exports={resolveMountPath};
