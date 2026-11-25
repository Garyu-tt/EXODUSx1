# EXODUS GPU Relay R01

The game executes on Switch. The PC is an optional resource/precompute worker, not a game host, frame streamer, or transparent extension of Tegra memory. Related #5, #36, #40. Future integration with FramePulse and DVRAM.

## R01 host prototype
Python 3, Windows/Linux; RAM default, explicit CUDA mode requires a compatible CuPy installation, NVIDIA GPU/driver. Set EXODUS_RELAY_TOKEN to a random ASCII secret (at least 32 characters), then run:
```
python exodus/gpu_relay/relay.py --backend ram --budget-mib 64
python exodus/gpu_relay/relay.py --backend cuda --budget-mib 256
python -m unittest discover -s exodus/gpu_relay -v
```
Loopback port 8765; authenticated GET /v1/status, POST /v1/cache (raw bytes), GET /v1/cache/<sha256>. SHA-256 deduplication, bounded logical storage and LRU. CUDA mode stores raw bytes on the device, reads them back for response; dedicated memory pool, actual allocator/context overhead may exceed logical payload size. No automatic fallback or GPU speedup claim. No production network listener, persistence, remote jobs or Switch client in R01. CPU tests and CUDA execution need verification.

## Future resource worker
- Tier 0 Switch hot GPU assets; tier 1 local shared RAM/SD; tier 2 PC RAM/SSD and VRAM. Do not treat Wi-Fi as GPU memory bus. Keep long-lived raw blobs in RAM/SSD unless GPU reuse justifies VRAM.
- Typed content IDs include game/version, asset content hash, operation/version, target GPU format and variant. No arbitrary remote command execution. Support texture decode, mipmaps, transcoding, geometry LOD/meshlets, static lightmaps, map tile prefetch and scene-dependent work only when an engine supplies inputs. GPU-specific compiled shader binaries are not portable between unrelated GPUs/drivers.
- Client predicts demand from engine-provided area/asset hints, sends a bounded batch with deadline, priority and generation. Worker advertises devices, memory, operation and format support; returns verified portable results. Dedicated staging and bandwidth caps. Switch validates sizes and hashes before use.
- Never wait for network on render thread, never read a remote pointer, never present incomplete data. Late results may be cached for future use; local fallback immediately on miss, disconnect, deadline or generation mismatch. No automatic change to original game files.
- Admission estimates queue + input transfer + GPU compute + output transfer + local upload + safety margin; dispatch only if useful before demand or valuable for future cache hits. Compare with local compute and SD read cost, not just PC GPU throughput.
- Authenticated encrypted transport, pairing, per-client quotas and namespace isolation before non-loopback mode; USB/Ethernet/Wi-Fi through supported network providers. No arbitrary file reads, shader execution from untrusted clients, or distribution of copyrighted game assets.
- Native Switch titles cannot automatically exploit engine-specific resource hints: require supported engine/plugin/runtime instrumentation and per-title compatibility testing. Start with EXODUS-native or controlled homebrew sample; preserve stock game execution.
- Test disconnect mid-frame, stale generations, corrupt hashes, 8 MiB limits, evictions, slow link, VRAM exhaustion; compare frame-time P99, input age, bytes transferred, local peak RAM, useful prefetch ratio and GPU work saved. No requirement to offload frame rendering.
