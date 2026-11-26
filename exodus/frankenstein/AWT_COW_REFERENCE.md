# R16.1 native PID + COW integration
Joins the confirmed #383 native clone and #386 single-core COW pool
gates. A57 requests real native child PID through AWS1. Clone shares
data/stack RO, child first write splits backing, child exit initiates
native proxy termination and invalidates the child mapping before
pool drop. Parent resumes with real PID and upgrades sole-owned page
without copy; two wait4 calls receive native status and ECHILD.
The parent copyout occurs after its private page has been upgraded.
Final pool check reallocates and scrubs the released page.
Expected four kernel replies, five SVC/HVC pairs, two actual permission
faults, one page copy. Uses the existing native KLD and relay unchanged.
Single core, two-page arena, private clone and exit operations, no
physical Tegra or complete FreeBSD ABI. Physical gate remains blocked.
