"use strict";
const assert=require("node:assert/strict");
const {selectProvider}=require("./hardware_scheduler");
const cases=[
  [
    {
      "operation": "image.jpeg.decode",
      "context": "application",
      "profile": "baseline-rgb",
      "owned": true,
      "bytes": 1024
    },
    [
      {
        "id": "cpu",
        "operation": "image.jpeg.decode",
        "context": "application",
        "profile": "baseline-rgb",
        "admitted": true,
        "verified": true,
        "available": true,
        "safe": true,
        "inFlight": 0,
        "scratchBytes": 256,
        "kind": "cpu"
      },
      {
        "id": "nvjpg",
        "operation": "image.jpeg.decode",
        "context": "application",
        "profile": "baseline-rgb",
        "admitted": true,
        "verified": true,
        "available": true,
        "safe": true,
        "inFlight": 0,
        "scratchBytes": 512,
        "kind": "specialized"
      },
      {
        "id": "gpu",
        "operation": "image.jpeg.decode",
        "context": "application",
        "profile": "baseline-rgb",
        "admitted": true,
        "verified": true,
        "available": true,
        "safe": true,
        "inFlight": 0,
        "scratchBytes": 1024,
        "kind": "gpu"
      }
    ],
    {
      "freeBytes": 8192,
      "quarantineBytes": 0,
      "quarantineLimit": 2048,
      "thermal": "normal"
    },
    "nvjpg"
  ],
  [
    {
      "operation": "image.jpeg.decode",
      "context": "application",
      "profile": "baseline-rgb",
      "owned": true,
      "bytes": 1024
    },
    [
      {
        "id": "cpu",
        "operation": "image.jpeg.decode",
        "context": "application",
        "profile": "baseline-rgb",
        "admitted": true,
        "verified": true,
        "available": true,
        "safe": true,
        "inFlight": 0,
        "scratchBytes": 256,
        "kind": "cpu"
      },
      {
        "id": "nvjpg",
        "operation": "image.jpeg.decode",
        "context": "application",
        "profile": "baseline-rgb",
        "admitted": false,
        "verified": true,
        "available": true,
        "safe": true,
        "inFlight": 0,
        "scratchBytes": 512,
        "kind": "specialized"
      }
    ],
    {
      "freeBytes": 8192,
      "quarantineBytes": 0,
      "quarantineLimit": 2048,
      "thermal": "normal"
    },
    "cpu"
  ],
  [
    {
      "operation": "image.jpeg.decode",
      "context": "application",
      "profile": "baseline-rgb",
      "owned": true,
      "bytes": 1024
    },
    [
      {
        "id": "cpu",
        "operation": "image.jpeg.decode",
        "context": "application",
        "profile": "baseline-rgb",
        "admitted": true,
        "verified": true,
        "available": true,
        "safe": true,
        "inFlight": 0,
        "scratchBytes": 256,
        "kind": "cpu"
      },
      {
        "id": "nvjpg",
        "operation": "image.jpeg.decode",
        "context": "application",
        "profile": "baseline-rgb",
        "admitted": true,
        "verified": false,
        "available": true,
        "safe": true,
        "inFlight": 0,
        "scratchBytes": 512,
        "kind": "specialized"
      }
    ],
    {
      "freeBytes": 8192,
      "quarantineBytes": 0,
      "quarantineLimit": 2048,
      "thermal": "normal"
    },
    "cpu"
  ],
  [
    {
      "operation": "image.jpeg.decode",
      "context": "application",
      "profile": "baseline-rgb",
      "owned": true,
      "bytes": 1024
    },
    [
      {
        "id": "cpu",
        "operation": "image.jpeg.decode",
        "context": "application",
        "profile": "baseline-rgb",
        "admitted": true,
        "verified": true,
        "available": true,
        "safe": true,
        "inFlight": 0,
        "scratchBytes": 256,
        "kind": "cpu"
      },
      {
        "id": "nvjpg",
        "operation": "image.jpeg.decode",
        "context": "application",
        "profile": "baseline-rgb",
        "admitted": true,
        "verified": true,
        "available": false,
        "safe": true,
        "inFlight": 0,
        "scratchBytes": 512,
        "kind": "specialized"
      }
    ],
    {
      "freeBytes": 8192,
      "quarantineBytes": 0,
      "quarantineLimit": 2048,
      "thermal": "normal"
    },
    "cpu"
  ],
  [
    {
      "operation": "image.jpeg.decode",
      "context": "application",
      "profile": "baseline-rgb",
      "owned": true,
      "bytes": 1024
    },
    [
      {
        "id": "cpu",
        "operation": "image.jpeg.decode",
        "context": "application",
        "profile": "baseline-rgb",
        "admitted": true,
        "verified": true,
        "available": true,
        "safe": true,
        "inFlight": 0,
        "scratchBytes": 256,
        "kind": "cpu"
      },
      {
        "id": "nvjpg",
        "operation": "image.jpeg.decode",
        "context": "application",
        "profile": "baseline-rgb",
        "admitted": true,
        "verified": true,
        "available": true,
        "safe": false,
        "inFlight": 0,
        "scratchBytes": 512,
        "kind": "specialized"
      }
    ],
    {
      "freeBytes": 8192,
      "quarantineBytes": 0,
      "quarantineLimit": 2048,
      "thermal": "normal"
    },
    "cpu"
  ],
  [
    {
      "operation": "image.jpeg.decode",
      "context": "application",
      "profile": "baseline-rgb",
      "owned": true,
      "bytes": 1024
    },
    [
      {
        "id": "cpu",
        "operation": "image.jpeg.decode",
        "context": "application",
        "profile": "baseline-rgb",
        "admitted": true,
        "verified": true,
        "available": true,
        "safe": true,
        "inFlight": 0,
        "scratchBytes": 256,
        "kind": "cpu"
      },
      {
        "id": "nvjpg",
        "operation": "image.jpeg.decode",
        "context": "application",
        "profile": "baseline-rgb",
        "admitted": true,
        "verified": true,
        "available": true,
        "safe": true,
        "inFlight": 1,
        "scratchBytes": 512,
        "kind": "specialized"
      }
    ],
    {
      "freeBytes": 8192,
      "quarantineBytes": 0,
      "quarantineLimit": 2048,
      "thermal": "normal"
    },
    "cpu"
  ],
  [
    {
      "operation": "image.jpeg.decode",
      "context": "application",
      "profile": "baseline-rgb",
      "owned": true,
      "bytes": 1024
    },
    [
      {
        "id": "cpu",
        "operation": "image.jpeg.decode",
        "context": "application",
        "profile": "baseline-rgb",
        "admitted": true,
        "verified": true,
        "available": true,
        "safe": true,
        "inFlight": 0,
        "scratchBytes": 256,
        "kind": "cpu"
      },
      {
        "id": "nvjpg",
        "operation": "image.jpeg.decode",
        "context": "applet",
        "profile": "baseline-rgb",
        "admitted": true,
        "verified": true,
        "available": true,
        "safe": true,
        "inFlight": 0,
        "scratchBytes": 512,
        "kind": "specialized"
      }
    ],
    {
      "freeBytes": 8192,
      "quarantineBytes": 0,
      "quarantineLimit": 2048,
      "thermal": "normal"
    },
    "cpu"
  ],
  [
    {
      "operation": "image.jpeg.decode",
      "context": "application",
      "profile": "baseline-rgb",
      "owned": true,
      "bytes": 1024
    },
    [
      {
        "id": "cpu",
        "operation": "image.jpeg.decode",
        "context": "application",
        "profile": "baseline-rgb",
        "admitted": true,
        "verified": true,
        "available": true,
        "safe": true,
        "inFlight": 0,
        "scratchBytes": 256,
        "kind": "cpu"
      },
      {
        "id": "nvjpg",
        "operation": "image.jpeg.decode",
        "context": "application",
        "profile": "different",
        "admitted": true,
        "verified": true,
        "available": true,
        "safe": true,
        "inFlight": 0,
        "scratchBytes": 512,
        "kind": "specialized"
      }
    ],
    {
      "freeBytes": 8192,
      "quarantineBytes": 0,
      "quarantineLimit": 2048,
      "thermal": "normal"
    },
    "cpu"
  ],
  [
    {
      "operation": "image.jpeg.decode",
      "context": "application",
      "profile": "baseline-rgb",
      "owned": true,
      "bytes": 1024
    },
    [
      {
        "id": "cpu",
        "operation": "image.jpeg.decode",
        "context": "application",
        "profile": "baseline-rgb",
        "admitted": true,
        "verified": true,
        "available": true,
        "safe": true,
        "inFlight": 0,
        "scratchBytes": 256,
        "kind": "cpu"
      },
      {
        "id": "nvjpg",
        "operation": "video.h264.decode",
        "context": "application",
        "profile": "baseline-rgb",
        "admitted": true,
        "verified": true,
        "available": true,
        "safe": true,
        "inFlight": 0,
        "scratchBytes": 512,
        "kind": "specialized"
      }
    ],
    {
      "freeBytes": 8192,
      "quarantineBytes": 0,
      "quarantineLimit": 2048,
      "thermal": "normal"
    },
    "cpu"
  ],
  [
    {
      "operation": "image.jpeg.decode",
      "context": "application",
      "profile": "baseline-rgb",
      "owned": true,
      "bytes": 1024
    },
    [
      {
        "id": "cpu",
        "operation": "image.jpeg.decode",
        "context": "application",
        "profile": "baseline-rgb",
        "admitted": true,
        "verified": true,
        "available": true,
        "safe": true,
        "inFlight": 0,
        "scratchBytes": 256,
        "kind": "cpu"
      },
      {
        "id": "gpu",
        "operation": "image.jpeg.decode",
        "context": "application",
        "profile": "baseline-rgb",
        "admitted": true,
        "verified": true,
        "available": true,
        "safe": true,
        "inFlight": 0,
        "scratchBytes": 1024,
        "kind": "gpu"
      }
    ],
    {
      "freeBytes": 8192,
      "quarantineBytes": 0,
      "quarantineLimit": 2048,
      "thermal": "normal"
    },
    "gpu"
  ],
  [
    {
      "operation": "image.jpeg.decode",
      "context": "application",
      "profile": "baseline-rgb",
      "owned": true,
      "bytes": 1024
    },
    [
      {
        "id": "cpu",
        "operation": "image.jpeg.decode",
        "context": "application",
        "profile": "baseline-rgb",
        "admitted": true,
        "verified": true,
        "available": true,
        "safe": true,
        "inFlight": 0,
        "scratchBytes": 256,
        "kind": "cpu"
      },
      {
        "id": "nvjpg",
        "operation": "image.jpeg.decode",
        "context": "application",
        "profile": "baseline-rgb",
        "admitted": true,
        "verified": true,
        "available": true,
        "safe": true,
        "inFlight": 0,
        "scratchBytes": 512,
        "kind": "specialized"
      }
    ],
    {
      "freeBytes": 8192,
      "quarantineBytes": 0,
      "quarantineLimit": 2048,
      "thermal": "limited"
    },
    "cpu"
  ],
  [
    {
      "operation": "image.jpeg.decode",
      "context": "application",
      "profile": "baseline-rgb",
      "owned": true,
      "bytes": 1024
    },
    [
      {
        "id": "cpu",
        "operation": "image.jpeg.decode",
        "context": "application",
        "profile": "baseline-rgb",
        "admitted": true,
        "verified": true,
        "available": true,
        "safe": true,
        "inFlight": 0,
        "scratchBytes": 256,
        "kind": "cpu"
      },
      {
        "id": "nvjpg",
        "operation": "image.jpeg.decode",
        "context": "application",
        "profile": "baseline-rgb",
        "admitted": true,
        "verified": true,
        "available": true,
        "safe": true,
        "inFlight": 0,
        "scratchBytes": 512,
        "kind": "specialized"
      }
    ],
    {
      "freeBytes": 8192,
      "quarantineBytes": 0,
      "quarantineLimit": 2048,
      "thermal": "unknown"
    },
    "cpu"
  ],
  [
    {
      "operation": "image.jpeg.decode",
      "context": "application",
      "profile": "baseline-rgb",
      "owned": true,
      "bytes": 1024
    },
    [
      {
        "id": "cpu",
        "operation": "image.jpeg.decode",
        "context": "application",
        "profile": "baseline-rgb",
        "admitted": true,
        "verified": true,
        "available": true,
        "safe": true,
        "inFlight": 0,
        "scratchBytes": 256,
        "kind": "cpu"
      },
      {
        "id": "nvjpg",
        "operation": "image.jpeg.decode",
        "context": "application",
        "profile": "baseline-rgb",
        "admitted": true,
        "verified": true,
        "available": true,
        "safe": true,
        "inFlight": 0,
        "scratchBytes": 512,
        "kind": "specialized"
      }
    ],
    {
      "freeBytes": 8192,
      "quarantineBytes": 0,
      "quarantineLimit": 2048,
      "thermal": "critical"
    },
    "thermal_stop"
  ],
  [
    {
      "operation": "image.jpeg.decode",
      "context": "application",
      "profile": "baseline-rgb",
      "owned": false,
      "bytes": 1024
    },
    [
      {
        "id": "cpu",
        "operation": "image.jpeg.decode",
        "context": "application",
        "profile": "baseline-rgb",
        "admitted": true,
        "verified": true,
        "available": true,
        "safe": true,
        "inFlight": 0,
        "scratchBytes": 256,
        "kind": "cpu"
      }
    ],
    {
      "freeBytes": 8192,
      "quarantineBytes": 0,
      "quarantineLimit": 2048,
      "thermal": "normal"
    },
    "invalid_request"
  ],
  [
    {
      "operation": "image.jpeg.decode",
      "context": "application",
      "profile": "baseline-rgb",
      "owned": true,
      "bytes": -1
    },
    [
      {
        "id": "cpu",
        "operation": "image.jpeg.decode",
        "context": "application",
        "profile": "baseline-rgb",
        "admitted": true,
        "verified": true,
        "available": true,
        "safe": true,
        "inFlight": 0,
        "scratchBytes": 256,
        "kind": "cpu"
      }
    ],
    {
      "freeBytes": 8192,
      "quarantineBytes": 0,
      "quarantineLimit": 2048,
      "thermal": "normal"
    },
    "invalid_request"
  ],
  [
    {
      "operation": "image.jpeg.decode",
      "context": "application",
      "profile": "baseline-rgb",
      "owned": true,
      "bytes": 1024
    },
    [
      {
        "id": "cpu",
        "operation": "image.jpeg.decode",
        "context": "application",
        "profile": "baseline-rgb",
        "admitted": true,
        "verified": true,
        "available": true,
        "safe": true,
        "inFlight": 0,
        "scratchBytes": 256,
        "kind": "cpu"
      },
      {
        "id": "nvjpg",
        "operation": "image.jpeg.decode",
        "context": "application",
        "profile": "baseline-rgb",
        "admitted": true,
        "verified": true,
        "available": true,
        "safe": true,
        "inFlight": 0,
        "scratchBytes": 512,
        "kind": "specialized"
      }
    ],
    {
      "freeBytes": 1500,
      "quarantineBytes": 0,
      "quarantineLimit": 2048,
      "thermal": "normal"
    },
    "cpu"
  ],
  [
    {
      "operation": "image.jpeg.decode",
      "context": "application",
      "profile": "baseline-rgb",
      "owned": true,
      "bytes": 1024
    },
    [
      {
        "id": "cpu",
        "operation": "image.jpeg.decode",
        "context": "application",
        "profile": "baseline-rgb",
        "admitted": true,
        "verified": true,
        "available": true,
        "safe": true,
        "inFlight": 0,
        "scratchBytes": 256,
        "kind": "cpu"
      },
      {
        "id": "nvjpg",
        "operation": "image.jpeg.decode",
        "context": "application",
        "profile": "baseline-rgb",
        "admitted": true,
        "verified": true,
        "available": true,
        "safe": true,
        "inFlight": 0,
        "scratchBytes": 512,
        "kind": "specialized"
      }
    ],
    {
      "freeBytes": 1000,
      "quarantineBytes": 0,
      "quarantineLimit": 2048,
      "thermal": "normal"
    },
    "no_admitted_provider"
  ],
  [
    {
      "operation": "image.jpeg.decode",
      "context": "application",
      "profile": "baseline-rgb",
      "owned": true,
      "bytes": 1024
    },
    [
      {
        "id": "cpu",
        "operation": "image.jpeg.decode",
        "context": "application",
        "profile": "baseline-rgb",
        "admitted": true,
        "verified": true,
        "available": true,
        "safe": true,
        "inFlight": 0,
        "scratchBytes": 256,
        "kind": "cpu"
      }
    ],
    {
      "freeBytes": 8192,
      "quarantineBytes": 3000,
      "quarantineLimit": 2048,
      "thermal": "normal"
    },
    "memory_stop"
  ],
  [
    {
      "operation": "image.jpeg.decode",
      "context": "application",
      "profile": "baseline-rgb",
      "owned": true,
      "bytes": 1024
    },
    [
      {
        "id": "nvjpg",
        "operation": "image.jpeg.decode",
        "context": "application",
        "profile": "baseline-rgb",
        "admitted": true,
        "verified": true,
        "available": true,
        "safe": true,
        "inFlight": 0,
        "scratchBytes": 512,
        "kind": "specialized"
      },
      {
        "id": "nvjpg",
        "operation": "image.jpeg.decode",
        "context": "application",
        "profile": "baseline-rgb",
        "admitted": true,
        "verified": true,
        "available": true,
        "safe": true,
        "inFlight": 0,
        "scratchBytes": 512,
        "kind": "specialized"
      },
      {
        "id": "cpu",
        "operation": "image.jpeg.decode",
        "context": "application",
        "profile": "baseline-rgb",
        "admitted": true,
        "verified": true,
        "available": true,
        "safe": true,
        "inFlight": 0,
        "scratchBytes": 256,
        "kind": "cpu"
      }
    ],
    {
      "freeBytes": 8192,
      "quarantineBytes": 0,
      "quarantineLimit": 2048,
      "thermal": "normal"
    },
    "cpu"
  ],
  [
    {
      "operation": "image.jpeg.decode",
      "context": "application",
      "profile": "baseline-rgb",
      "owned": true,
      "bytes": 1024
    },
    [],
    {
      "freeBytes": 8192,
      "quarantineBytes": 0,
      "quarantineLimit": 2048,
      "thermal": "normal"
    },
    "no_admitted_provider"
  ],
  [
    {
      "operation": "image.jpeg.decode",
      "context": "application",
      "profile": "baseline-rgb",
      "owned": true,
      "bytes": 1024
    },
    [
      {
        "id": "cpu",
        "operation": "image.jpeg.decode",
        "context": "application",
        "profile": "baseline-rgb",
        "admitted": true,
        "verified": true,
        "available": true,
        "safe": true,
        "inFlight": 0,
        "scratchBytes": null,
        "kind": "cpu"
      }
    ],
    {
      "freeBytes": 8192,
      "quarantineBytes": 0,
      "quarantineLimit": 2048,
      "thermal": "normal"
    },
    "no_admitted_provider"
  ],
  [
    {
      "operation": "image.jpeg.decode",
      "context": "application",
      "profile": "baseline-rgb",
      "owned": true,
      "bytes": 1024
    },
    [
      {
        "id": "cpu",
        "operation": "image.jpeg.decode",
        "context": "application",
        "profile": "baseline-rgb",
        "admitted": true,
        "verified": true,
        "available": true,
        "safe": true,
        "inFlight": 0,
        "scratchBytes": 256,
        "kind": "unknown"
      }
    ],
    {
      "freeBytes": 8192,
      "quarantineBytes": 0,
      "quarantineLimit": 2048,
      "thermal": "normal"
    },
    "no_admitted_provider"
  ],
  [
    {
      "operation": "image.jpeg.decode",
      "context": "application",
      "profile": "baseline-rgb",
      "owned": true,
      "bytes": 1024
    },
    [
      {
        "id": "cpu",
        "operation": "image.jpeg.decode",
        "context": "application",
        "profile": "baseline-rgb",
        "admitted": true,
        "verified": "true",
        "available": true,
        "safe": true,
        "inFlight": 0,
        "scratchBytes": 256,
        "kind": "cpu"
      }
    ],
    {
      "freeBytes": 8192,
      "quarantineBytes": 0,
      "quarantineLimit": 2048,
      "thermal": "normal"
    },
    "no_admitted_provider"
  ],
  [
    {
      "operation": "image.jpeg.decode",
      "context": "application",
      "profile": "baseline-rgb",
      "owned": true,
      "bytes": 1024
    },
    [
      {
        "id": "z",
        "operation": "image.jpeg.decode",
        "context": "application",
        "profile": "baseline-rgb",
        "admitted": true,
        "verified": true,
        "available": true,
        "safe": true,
        "inFlight": 0,
        "scratchBytes": 512,
        "kind": "specialized"
      },
      {
        "id": "a",
        "operation": "image.jpeg.decode",
        "context": "application",
        "profile": "baseline-rgb",
        "admitted": true,
        "verified": true,
        "available": true,
        "safe": true,
        "inFlight": 0,
        "scratchBytes": 512,
        "kind": "specialized"
      }
    ],
    {
      "freeBytes": 8192,
      "quarantineBytes": 0,
      "quarantineLimit": 2048,
      "thermal": "normal"
    },
    "a"
  ]
];
for(const [r,p,s,w] of cases){const x=selectProvider(r,p,s);assert.equal(x.ok?x.provider:x.reason,w);}
assert.equal(selectProvider(cases[0][0],[cases[0][1][1]],cases[0][2]).reserveBytes,1536);
console.log(cases.length+1+" checks passed");
