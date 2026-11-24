import {readdir,readFile} from 'node:fs/promises';
import {join} from 'node:path';
import {buildGraph} from './core.mjs';
async function main(){
 const [root,revision,...requested]=process.argv.slice(2);
 if(!root||!revision||!/^[0-9a-f]{40}$/i.test(revision)){
  console.error('Usage: node exodus/atomizer/cli.mjs WINE_CHECKOUT REVISION [MODULE ...]');return 2;
 }
 const modules=requested.length?requested:(await readdir(join(root,'dlls'),{withFileTypes:true})).filter(d=>d.isDirectory()).map(d=>d.name).sort();
 const inputs=[];
 for(const module of [...new Set(modules)]){
  if(!/^[A-Za-z0-9_.-]+$/.test(module)||module==='.'||module==='..')throw Error('invalid module');
  const source='dlls/'+module+'/'+module+'.spec';
  try{inputs.push({module,source,text:await readFile(join(root,source),'utf8')});}
  catch(e){if(e.code!=='ENOENT'||requested.length)throw e;}
 }
 if(!inputs.length)throw Error('no spec files');
 const graph=buildGraph(inputs,{revision,revision_verified:false,scope:inputs.map(i=>i.module)});
 console.log(JSON.stringify(graph,null,2));return graph.complete?0:1;
}
main().then(code=>{process.exitCode=code;}).catch(e=>{console.error(e.message);process.exitCode=2;});
