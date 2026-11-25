"use strict";
const assert=require("node:assert/strict");
const {resolveMountPath}=require("./vfs_path_model");
const mounts=[
  {
    "name": "sd",
    "admitted": true,
    "readOnly": false,
    "operations": [
      "read",
      "list",
      "stat",
      "write",
      "mkdir",
      "delete",
      "rename"
    ]
  },
  {
    "name": "rom",
    "admitted": true,
    "readOnly": true,
    "operations": [
      "read",
      "write"
    ]
  },
  {
    "name": "nfs",
    "admitted": false,
    "readOnly": true,
    "operations": [
      "read"
    ]
  }
];
const cases=[
  [
    "/sd/a.txt",
    "read",
    true
  ],
  [
    "/sd",
    "list",
    true
  ],
  [
    "/sd/folder/",
    "list",
    true
  ],
  [
    "/sd/a",
    "write",
    true
  ],
  [
    "/rom/x",
    "read",
    true
  ],
  [
    "/rom/x",
    "write",
    false
  ],
  [
    "/nfs/x",
    "read",
    false
  ],
  [
    "/nand/x",
    "read",
    false
  ],
  [
    "/sd/../rom/x",
    "read",
    false
  ],
  [
    "/sd/./x",
    "read",
    false
  ],
  [
    "/sd//x",
    "read",
    false
  ],
  [
    "/sd/a\\b",
    "read",
    false
  ],
  [
    "/sd/%2e%2e",
    "read",
    false
  ],
  [
    "/sd/a:b",
    "read",
    false
  ],
  [
    "/sd/a\u0000b",
    "read",
    false
  ],
  [
    "/sd/xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx",
    "read",
    false
  ],
  [
    "/sd/x.",
    "read",
    false
  ],
  [
    "/sd/x ",
    "read",
    false
  ],
  [
    "/sd",
    "delete",
    false
  ],
  [
    "/sd/a",
    "rename",
    false
  ],
  [
    "/sd/a",
    "exec",
    false
  ],
  [
    "sd/a",
    "read",
    false
  ],
  [
    "/SD/a",
    "read",
    false
  ]
];
for(const [path,operation,want] of cases)assert.equal(resolveMountPath(mounts,{path,operation}).ok,want,JSON.stringify([path,operation]));
assert.equal(resolveMountPath([...mounts,mounts[0]],{path:"/sd/a",operation:"read"}).ok,false);
console.log(cases.length+1+" cases passed");
