# EXODUS Task Center — reference contract (#298)

FreeBSD remains the control plane; Horizon and FreeBSD keep their own process schedulers. This model is for integration into the existing sessiond and typed mediator, not a second resident daemon.

Backend observations are complete snapshots, scoped by backend, connection epoch and sequence. A session is identified by backend, epoch, ID and instance. Lost or expired observations are shown as unknown. Do not equate an accepted operation with a confirmed lifecycle change.

GPU/display/HID are one exclusive foreground bundle in this conservative first reference. The mediator must positively acknowledge release of the old bundle before an acquire request can be issued. Timeout, disconnect, replacement or mismatched observation leaves ownership unknown. No implicit grant. Recovery requires fresh mediator reconciliation. A suspended session is not an eligible target until its backend confirms running. Actual suspend, resume and graceful termination remain backend capability gates.

Three-second snapshot expiry and two-second handoff deadline are provisional reference constants, not measured hardware limits. No OS calls, telemetry, transport, overlay or physical multitasking are implemented by this model. Authenticated adapters and serialized execution are prerequisites.
