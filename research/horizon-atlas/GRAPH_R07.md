# R07 evidence graph

Generated snapshot: evidence-graph.r07.json. Rebuild: node build_evidence_graph.js research/horizon-atlas /tmp/evidence-graph.json (from repository root, using full script path). The graph builder checks revision consistency, duplicate node IDs and dangling edges. Snapshot includes input blob provenance recorded during generation; the local CLI does not currently reproduce the input blob map.

Counts: 2185 nodes, 2659 edges, 15 forwarding modules. Eight structural assertions passed during authoring: non-executable nodes, overlapping BSD replaced, coverage 91, 124 unique SVC numbers, 8 reviewed atoms, unique nodes, closed edges, unique edges. Local Node CLI and native/device execution have not been run.

## Graph semantics
- nodes: stable typed identifiers within the pinned snapshot, kind, label, source/metadata, executable:false.
- edges: triples [from,to,type], unique and endpoint-closed.
- contains/references/declares/source_inventory/emits_instruction: source organization and evidence, not access grants.
- architecture_candidate/potentially_supports: hypotheses, not verified runtime dependencies.
- contains_call_candidate/passes_command_candidate: syntactic call-site relationships; may have false positives.
- source_review: manually traced source evidence, not device validation.
- No executable exports, no automatic code generation or invocation. Restricted families remain inventory only under #55.

## Adamantium-node interoperability
The JSON is a neutral graph interchange candidate, not a verified native Adamantium-node IR. An adapter should map IDs to graph cards, kinds to node types and edge triples to labeled links; preserve source evidence, group domains for LOD, mark candidate vs observed relationships and refuse executable import. Native schema mapping and import smoke tests remain pending.

## Coverage limitations
The graph uses the prior R05 and R06 forwarding batches, not a fresh run of the corrected reusable scanner. It includes source inventory for 91 header entries but deeper forwarding for only 15 modules. Historical endpoint names are intentionally not asserted as current facts. Permissions, firmware ranges, actual object identity, hardware and runtime behavior remain unverified. No node grants access. 
