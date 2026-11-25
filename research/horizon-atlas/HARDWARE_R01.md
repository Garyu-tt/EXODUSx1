# Hardware atlas R01

Scope: first-generation Nintendo Switch (Erista, Mariko, Lite, OLED). Not Switch 2. 115 nodes, 252 edges, 45 capability groups/items, 52 distinct generic Tegra210 device-tree compatible types in 79 occurrences. 16 board-observed, 6 driver candidates, 23 generic silicon candidates. Edges to existing Horizon graph are candidate OS mappings and checked for valid endpoint IDs; dashed DOT edges are conceptual dependencies, not traced physical wiring. Generic Linux DTS is not a board DTS or evidence of Mariko parity. All executable=false, device_test=not_tested, horizon_access=unknown.

## First investigations
- CPU Cortex-A57: ARM64 native compute, shell and emulation
- Maxwell GM20B: Graphics and carefully bounded compute; no CUDA runtime assumed
- VIC image compositor: Scale, convert and composite images without 3D GPU workload
- NVDEC video decode: Media playback and remote desktop decode; codec/profile checks
- NVJPG JPEG engine: Thumbnail and image decoding
- LPDDR4 / LPDDR4X: 4 GB on documented retail boards; measure usable application memory
- Display controller / DSI / SOR: Native panel and dock output, independent displays not assumed
- LCD / OLED + touch: Desktop and touchscreen UI
- Dock HDMI / USB / Ethernet: Dock desktop mode; Lite not assumed capable
- Realtek audio codec / speakers / jack: Playback and microphone input subject to service access
- microSD: Owned application files and VFS
- USB host / device / PHY: Mass storage, HID and networking candidates; role/port tests
- USB-C port and power role: Peripheral and dock lifecycle, no arbitrary PD changes
- Wi-Fi 802.11ac: SSH, SFTP, network mounts
- PMIC and battery charging: Battery telemetry, no voltage override
- Thermal sensors / fan: Monitoring and thermal budget; no unsafe overrides

## Evidence and admission
Hardware presence, wiring, clocks/power, firmware, OS access, supported APIs, permissions, device observation and performance are distinct gates. Unknown remains disabled. Physical presence is not proof an engine is enabled, usable or independent of shared memory. Generic SoC PCIe/SATA/CSI/A53 do not establish usable connectors or cores on retail Switch. Official TV output remains up to 1080p on compatible models. No claims of 4K output, eGPU, eight accessible CPU cores, CUDA availability, new permissions or undocumented vendor knowledge.

Next: board-specific source inventory, physical block-to-IPC/driver mapping, non-mutating diagnostics with explicit review and revision-specific results. No protected storage, fuse or security-engine probing, no clock/voltage overrides. Candidate optimization: VIC for 2D transforms, NVJPG for assets, NVDEC for local/remote video, GPU for rendering/compute with resource quotas. Evaluate shared memory, clock and thermal budgets, copy overhead and driver permissions before adoption. NVENC needs independent Horizon route confirmation.

Machine-readable hardware-graph.r01.json contains source URLs, blob hashes, line references, group/board nodes and links to R07. hardware-overview.r01.dot is a reduced visual view, render with dot -Tsvg hardware-overview.r01.dot -o hardware.svg. Graph structural checks performed during generation; DOT rendering and device verification not performed.
