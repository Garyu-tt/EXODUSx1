// EXODUS Kernel Bridge R01: planning graph, not a device probe.
// Run: node kernel_bridge.mjs [exodus.kde|exodus.headless|--test]
import { pathToFileURL } from "node:url";
export const nodes = [
  {
    "id": "kernel.svc",
    "layer": "kernel",
    "requires": [],
    "status": "documented"
  },
  {
    "id": "kernel.handles",
    "layer": "kernel",
    "requires": [
      "kernel.svc"
    ],
    "status": "documented"
  },
  {
    "id": "kernel.memory",
    "layer": "kernel",
    "requires": [
      "kernel.svc"
    ],
    "status": "documented"
  },
  {
    "id": "kernel.threads",
    "layer": "kernel",
    "requires": [
      "kernel.svc"
    ],
    "status": "documented"
  },
  {
    "id": "kernel.ipc",
    "layer": "kernel",
    "requires": [
      "kernel.handles",
      "kernel.memory"
    ],
    "status": "documented"
  },
  {
    "id": "horizon.sm",
    "layer": "bootstrap",
    "requires": [
      "kernel.ipc"
    ],
    "status": "documented"
  },
  {
    "id": "horizon.loader",
    "layer": "bootstrap",
    "requires": [
      "horizon.sm"
    ],
    "status": "documented"
  },
  {
    "id": "horizon.fs",
    "layer": "bootstrap",
    "requires": [
      "horizon.sm"
    ],
    "status": "documented"
  },
  {
    "id": "horizon.vi",
    "layer": "bootstrap",
    "requires": [
      "horizon.sm"
    ],
    "status": "documented"
  },
  {
    "id": "horizon.hid",
    "layer": "bootstrap",
    "requires": [
      "horizon.sm"
    ],
    "status": "documented"
  },
  {
    "id": "horizon.bsd",
    "layer": "bootstrap",
    "requires": [
      "horizon.sm"
    ],
    "status": "documented"
  },
  {
    "id": "horizon.audio",
    "layer": "bootstrap",
    "requires": [
      "horizon.sm"
    ],
    "status": "documented"
  },
  {
    "id": "horizon.power",
    "layer": "bootstrap",
    "requires": [
      "horizon.sm"
    ],
    "status": "documented"
  },
  {
    "id": "exodus.supervisor",
    "layer": "exodus",
    "requires": [
      "kernel.threads",
      "kernel.ipc",
      "horizon.loader"
    ],
    "status": "planned"
  },
  {
    "id": "exodus.vfs",
    "layer": "exodus",
    "requires": [
      "horizon.fs"
    ],
    "status": "planned"
  },
  {
    "id": "exodus.process",
    "layer": "exodus",
    "requires": [
      "exodus.supervisor",
      "kernel.memory",
      "kernel.handles"
    ],
    "status": "planned"
  },
  {
    "id": "exodus.pty",
    "layer": "exodus",
    "requires": [
      "exodus.process",
      "exodus.vfs"
    ],
    "status": "planned"
  },
  {
    "id": "exodus.shell",
    "layer": "exodus",
    "requires": [
      "exodus.pty"
    ],
    "status": "planned"
  },
  {
    "id": "exodus.network",
    "layer": "exodus",
    "requires": [
      "horizon.bsd"
    ],
    "status": "planned"
  },
  {
    "id": "exodus.graphics",
    "layer": "exodus",
    "requires": [
      "horizon.vi",
      "kernel.memory"
    ],
    "status": "planned"
  },
  {
    "id": "exodus.input",
    "layer": "exodus",
    "requires": [
      "horizon.hid"
    ],
    "status": "planned"
  },
  {
    "id": "exodus.audio",
    "layer": "exodus",
    "requires": [
      "horizon.audio"
    ],
    "status": "planned"
  },
  {
    "id": "exodus.power",
    "layer": "exodus",
    "requires": [
      "horizon.power"
    ],
    "status": "planned"
  },
  {
    "id": "exodus.session",
    "layer": "exodus",
    "requires": [
      "exodus.process",
      "exodus.graphics",
      "exodus.input",
      "exodus.power"
    ],
    "status": "planned"
  },
  {
    "id": "exodus.qt",
    "layer": "exodus",
    "requires": [
      "exodus.session",
      "exodus.vfs",
      "exodus.network"
    ],
    "status": "planned"
  },
  {
    "id": "exodus.kde",
    "layer": "exodus",
    "requires": [
      "exodus.qt",
      "exodus.pty",
      "exodus.audio"
    ],
    "status": "planned"
  },
  {
    "id": "exodus.headless",
    "layer": "exodus",
    "requires": [
      "exodus.shell",
      "exodus.network"
    ],
    "status": "planned"
  }
];
export function analyze(nodes, target) {
  if (!Array.isArray(nodes)) throw new Error("nodes must be an array");
  const map = new Map();
  for (const n of nodes) {
    if (!n || typeof n.id !== "string" || !n.id || map.has(n.id) ||
        !["kernel","bootstrap","exodus"].includes(n.layer) ||
        !["documented","planned","device_verified"].includes(n.status) ||
        !Array.isArray(n.requires) || new Set(n.requires).size !== n.requires.length ||
        n.requires.some(x => typeof x !== "string")) throw new Error("invalid or duplicate node");
    map.set(n.id,n);
  }
  const visited = new Set(), active = new Set(), order = [];
  function visit(id) {
    if (!map.has(id)) throw new Error("missing dependency: "+id);
    if (active.has(id)) throw new Error("cycle: "+id);
    if (visited.has(id)) return;
    active.add(id);
    const n = map.get(id);
    for (const d of n.requires) visit(d);
    active.delete(id); visited.add(id); order.push(id);
  }
  for (const id of map.keys()) visit(id);
  if (!map.has(target)) throw new Error("unknown target: "+target);
  visited.clear(); order.length=0; visit(target);
  return {target,order,bootstrapDependencies:order.filter(id=>map.get(id).layer==="bootstrap"),
    unverified:order.filter(id=>map.get(id).status!=="device_verified"),
    deviceReady:order.every(id=>map.get(id).status==="device_verified")};
}
function selfTest(nodes) {
  function check(x,msg){if(!x) throw new Error(msg);}
  function fails(fn){try{fn();return false;}catch{return true;}}
  const kde=analyze(nodes,"exodus.kde");
  check(kde.bootstrapDependencies.includes("horizon.vi"),"graphics dependency");
  check(kde.bootstrapDependencies.includes("horizon.loader"),"loader dependency");
  check(!kde.deviceReady,"no false readiness");
  check(kde.order.indexOf("kernel.svc")<kde.order.indexOf("exodus.kde"),"dependency order");
  const head=analyze(nodes,"exodus.headless");
  check(!head.bootstrapDependencies.includes("horizon.vi"),"headless has no display dependency");
  check(fails(()=>analyze([...nodes,nodes[0]],"exodus.kde")),"duplicate rejected");
  check(fails(()=>analyze([...nodes,{id:"bad",layer:"exodus",status:"planned",requires:["absent"]}],"exodus.kde")),"missing rejected globally");
  check(fails(()=>analyze([...nodes,{id:"bad",layer:"exodus",status:"planned",requires:["bad"]}],"exodus.kde")),"cycle rejected globally");
  check(fails(()=>analyze(nodes,"absent")),"unknown target rejected");
  check(analyze([{id:"verified",layer:"kernel",status:"device_verified",requires:[]}],"verified").deviceReady,"verified closure");
  return 10;
}
if (process.argv[1] && import.meta.url === pathToFileURL(process.argv[1]).href) {
  const target = process.argv[2] || "exodus.kde";
  if (target === "--test") console.log(selfTest(nodes) + " checks passed");
  else console.log(JSON.stringify(analyze(nodes,target),null,2));
}
