# R05 admission matrix

Each capability requires independent evidence for source availability, firmware, permission and runtime behavior. An SDK declaration is not an access grant. An observed permission is specific to firmware, hardware, caller and owned object; it is not portable to another context.

| Context | Initial state | Admission evidence |
| --- | --- | --- |
| NRO in applet context | Unknown / disabled | Native build, exact HOS and loader context, permitted read-only probe, resource/cleanup check |
| NRO in application context | Unknown / disabled | Same evidence, independent permissions and resource limits |
| Dedicated EXODUS sysmodule | Unknown / disabled | Explicit least-privilege manifest review, authorized deployment and separate runtime tests |
| Protected or Nintendo-linked operation | Denied / inventory only | No runtime admission under Safe Mode |

Use normal supported SDK calls in the explicitly authorized context; do not expand permissions to make a failed probe pass. Separate SVC ACL, service ACL, filesystem scope and object ownership. No global allow-list of privileged raw calls. Read-only query and handle acquisition are different effects, and access to a service does not authorize all its commands.

The JSON schema is a structural guard, not a cryptographic proof or complete policy engine. A separate review must verify context-specific evidence, dependencies and target device before admission. No atoms are promoted by this change.

## R05 analysis limitations
The eight-module forwarding inventory uses a heuristic parser with balanced argument lists and a fixed-point propagation of unchanged formal parameters. It is not a C compiler AST: conditional compilation, shadowing, reassigned parameters, function pointers, indirect macro calls and receiver identity remain unresolved. FS/SSL specialized macro names are treated as candidate dispatch sinks. BSD remains largely unresolved. Counters are call-site candidates, not unique capabilities. No native build or device testing performed.
