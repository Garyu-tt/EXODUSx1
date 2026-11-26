# Kernel Bridge R07: portable scene compositor

Stacked on #35. First desktop compositor core in C11. Bounded scene with 16 independently positioned opaque rectangle surfaces, generation-safe identifiers, painter order, focus, move/resize, destruction, clipping and pointer hit-testing with local coordinates. Rendering is a callback so it can later target a software framebuffer, deko3d or a verified Mesa path. The host test actually paints a small pixel buffer and checks overlap and focus rather than merely inspecting a window list.

Single-threaded trusted in-process scene. Initialize once per lifetime; do not reinitialize while IDs survive. No client isolation, shared buffers, alpha, GPU acceleration, cross-process IPC, protocol, keyboard or pointer capture. Focus is the topmost surface (even if offscreen); clicking background does not change it. Pointer callback failure does not undo focus. Rendering may be partial on backend error; future frame presentation must only publish completed frames. Callbacks must not mutate/reenter the scene. This is not a Wayland server or a working KWin port.

Host: cc -std=c11 -Wall -Wextra -Werror -pedantic -fsanitize=undefined tools/kernel_bridge/compositor_test.c -o /tmp/exc-test && /tmp/exc-test
CI covers GCC and Clang; no execution verified at submission. No changes to firmware.

Next: framebuffer adapter and native HID routing, double-buffer presentation and two native surfaces; then QPA adapter candidate with actual Qt window. Preserve recovery to existing HOME and enforce memory limits. Do not confuse local surface demo with independent process multitasking. Horizon process admission remains an independent gate (#31/#33).
