# R62 — ARM assembler correction

R61 run 46052946417 failed both 32/64 MiB jobs before boot at fixture compilation. Exact diagnostic at line 80: immediate expression for mov requires :lower16: or :upper16. No ping-pong runtime result. Commit e4075fb997573ceb84860b5a86a0c7586bececeb replaces both forward-label length immediates with literal loads, compares write return with register; same established pattern as R60. Existing R61 workflow is triggered by source path. No changes to kernel, RAM target, or IPC algorithm. R60 functional proof unchanged. No physical access.
Retry: https://github.com/Garyu-tt/EXODUSx1/actions/runs/46053252171
