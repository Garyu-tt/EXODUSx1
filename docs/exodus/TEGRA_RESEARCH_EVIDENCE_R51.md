# R51 — Single-user shell at 40 MiB; correct observed prompt

R49 final: 48 MiB job 140653362932 reached multi-user shell, passed identity/VM/disk SHA256 (4 MiB), then /tmp tmpfs ENOSPC before writing 1 MiB. R50 run 46042857733: 40 MiB job 140654700992 and 48 MiB job 140654700614 reached single-user root@:~ # prompt; harness expected bare # and timed out, no workload PASS. 32 MiB job 140654700947 reached shell selection but sh was killed for failed memory reclamation. Thus bypassing rc alone is not sufficient for 32 MiB GENERIC in this reference.

R51 commit 04aa4069acf07de1bb445034ecb1f367a520a8d5 accepts observed anchored single-user root prompt (or bare #) and adds 36 MiB intermediate budget, retaining 40/48 MiB and unchanged gates. CI https://github.com/Garyu-tt/EXODUSx1/actions/runs/46043435418. Single-user mode is not equivalent to all system services. No physical ADSP or Switch access.
