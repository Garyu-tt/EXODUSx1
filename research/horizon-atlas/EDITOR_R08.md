# R08 editor export

The Adamantium-node default branch currently exposes Node-RED 5.0.10 in package.json and the built-in comment node with name/info fields and zero inputs/outputs. The native Universal Graph IR schema was not found at the two expected documentation paths. Therefore this adapter uses the observed Node-RED baseline, not an assumed native Adamantium IR.

## Files
- editor-overview.r08.json: 182 comments, 22 disabled tabs.
- editor-bsd.r08.json: 64 comments, 2 disabled tabs.
- export_editor_flow.js: deterministic exporter, modes overview, full, module:<name>.

From repository root:
```sh
node research/horizon-atlas/export_editor_flow.js research/horizon-atlas/evidence-graph.r07.json /tmp/horizon-full.json full
```

Full mode produced 2185 comments across 42 disabled tabs in the authoring JS isolate. The generated full flow (about 1.2 MB compact JSON) is not checked in; regenerate locally. Import the overview or module sample into an empty separate workspace; IDs are deterministic within each view, not collision-proof across multiple imported views. Do not combine view exports into the same workspace. No deploy needed for inspecting cards.

Graph edges are preserved as textual adjacency with direction/type and evidence ID in comment info; no executable wires, no live calls. The authoritative edge triples remain in evidence-graph.r07.json. This is a tabbed LOD approximation, not native nested graph portals. A native visual-edge adapter, stable cross-view IDs and actual editor import/render smoke test remain pending.

Nine checks passed in the authoring JS isolate (two checks for each view, full coverage, reject executable and dangling input). No local CLI, actual editor, native build or hardware test executed. No capabilities admitted. 
