# R11 VFS slices

Nine Kefir Hub source files inspected and twelve FetchContent dependencies indexed. Reuse is selective, not wholesale. No donor code copied. All source findings are tied to blobs in kefir-vfs-audit.r11.json.

## Proposed provider boundary
- Mount registry: explicit namespace, unique ID, admitted operation set, readonly default, caller ownership; no direct raw device paths.
- Native SD/owned files: use Horizon FS wrappers; restrict root, check actual object ownership and filesystem semantics. ROMFS read-only.
- Network clients: SMB2 and NFS read-only initially, verified TLS for HTTPS/WebDAV, authenticated SFTP separate; protocol-specific credential isolation. No anonymous FTP service or unrestricted redirect authentication.
- Virtual directory and archives: bounded entries and decompression, traversal checks, no implicit extraction or writeback.
- Broker: validate capability and path before dispatch, keep backend handles private, enforce quotas and cancellation, track lifetime and unmount.
- POSIX adaptation: map errno and descriptor semantics, check partial I/O and close behavior. No new native permissions implied.

The independent vfs_path_model.js models only lexical path routing and operation admission. It intentionally refuses rename until two-path policy is implemented. Synthetic mount records are fixtures, not real permissions. 24 assertions passed in the authoring JavaScript isolate. Run node research/horizon-atlas/test_vfs_path_model.js to reproduce; repository CLI not executed. Lexical checks alone cannot prevent symlink, alias, case-folding, Unicode or TOCTOU escapes. Actual native backend containment, byte limits, encoding, resource limits and device tests are mandatory. This is not runtime security enforcement.

Observed blockers in donor code: disabled TLS peer/hostname verification and unrestricted auth forwarding, anonymous FTP patch, truncating path representation. Do not adopt these behaviors. Dependency licenses and immutable revisions, vulnerability checks, source closure and native build pending. All capabilities remain non-admitted.
