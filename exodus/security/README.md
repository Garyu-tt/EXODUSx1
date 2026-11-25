# Capability policy R01
Tracks #55 and #52. Portable C99 fail-closed reference. Only read-only status/catalog are admitted at this stage. SSH, LAN, file reads and local title/homebrew launches require independently reviewed provider rules; they are intentionally not yet admitted by this reference gate. No claims of effective runtime enforcement until integration.

Integration targets:
- #50/#51 HOME: filter discovery and recheck launch at trusted dispatch, including direct routes; never infer trust from catalog text.
- #43 terminal: classify each built-in operation, no unrestricted pass-through.
- #35 kernel shell/service supervisor: classify trusted aliases before spawn.
- #37 SSH: independently authorize destination, transport and provider; no blanket network allowance.
- Legacy 4IFIR shortcuts, MTP installation and official update integrations: audit packaged entry points and remove restricted ones from EXODUS release.
- Plugin execution: require trusted reviewed manifest, enforce isolation at provider boundaries. Untrusted native code in the same process cannot be contained by this header.

Run: `cc -std=c99 -Wall -Wextra -Werror -pedantic test_capability_policy.c -o /tmp/exodus-policy && /tmp/exodus-policy`.

CI is a portable contract test only; no network, native integration, device or ban-prevention guarantee.
