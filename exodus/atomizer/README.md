# EX-014 / R01: Wine Atomizer

Target: native Horizon compatibility capability graph with Wine as reference and fallback, not a mandatory final monolith. Preserve existing runtime until tested alternatives exist.

## R01
Dependency-free Node.js 18+ .spec export inventory: API identities, ABI declaration variants, explicit DLL forwarders, ordinal aliases, stubs and unresolved implementation symbol candidates. Raw declarations and source lines retained. Versioned intermediate graph JSON, not yet native Adamantium-node import.

    node exodus/atomizer/selftest.mjs
    node exodus/atomizer/cli.mjs /path/to/autorun 1ac9b6cf37b90f2d5783dd7ef389bebbe1c68877 ntdll kernel32 kernelbase user32 gdi32 > graph.json

Revision is caller-declared, not verified by CLI. Use a clean pinned checkout. Without modules scans dlls/<module>/<module>.spec only. Exit 1 emits partial graph with unsupported declaration diagnostics. Not the full winebuild grammar. Names marked -noname are declaration labels, not guarantees of named PE exports. Forwarder edges are declaration evidence, not proof of target binary availability. Symbol candidate edges are not resolved call edges. No native backend dispatch yet.

## Evidence
Parsed five actual specs retrieved at Autorun 1ac9b6cf37b90f2d5783dd7ef389bebbe1c68877: 6080 declarations, 6304 declared API identities including ordinal aliases, 568 stub declarations, 410 forwarder declarations, 189 target identities not declared in this scope. 17344 nodes, 18120 edges, zero parse diagnostics. 10 synthetic assertions passed executing the same core JS functions. CLI filesystem wrapper and Node module execution not run; independent QA pending.

## Roadmap
R02 compiler-assisted C call graph, build/ABI, wineserver, Unix calls, indirect calls as unresolved. R03 typed Horizon mapping and ownership of NT state. R04 static/dynamic PE imports and traces (unobserved is not unused). R05 native adapters via explicit dispatch grouped by shared handle/memory/TLS semantics. R06 differential tests against Windows and Wine, ABI, failure codes and side effects. R07 per-game dependency closure with dynamic fallback. R08 AOT/JIT under verified memory semantics. R09 D3D-to-deko3d research. R10 remove mandatory Wine only when verified providers cover required closure. Preserve licenses/source notices.

Start native pilots with pure/time/query APIs and input, not VirtualAlloc or shared object handles. No fake success. Real-device benchmark gates remain mandatory.
