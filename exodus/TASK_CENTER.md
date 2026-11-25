# EXODUS Task Center — reference contract (#298)

FreeBSD remains the control plane; Horizon and FreeBSD keep their own process schedulers. This model is for integration into the existing sessiond and typed mediator, not a second resident daemon.

Backend observations are complete snapshots, scoped by backend, connection epoch and sequence. A session is identified by backend, epoch, ID and instance. Lost or expired observations are shown as unknown. Do not equate an accepted operation with a confirmed lifecycle change.

GPU/display/HID are one exclusive foreground bundle in this conservative first reference. The mediator must positively acknowledge release of the old bundle before an acquire request can be issued. Timeout, disconnect, replacement or mismatched observation leaves ownership unknown. No implicit grant. Recovery requires fresh mediator reconciliation. A suspended session is not an eligible target until its backend confirms running. Actual suspend, resume and graceful termination remain backend capability gates.

Three-second snapshot expiry and two-second handoff deadline are provisional reference constants, not measured hardware limits. No OS calls, telemetry, transport, overlay or physical multitasking are implemented by this model. Authenticated adapters and serialized execution are prerequisites.

## Lifecycle requests (R02)

Suspend, resume and graceful termination are admitted only for fresh, non-foreground sessions with explicit backend capabilities. Foreground ownership must already be known. A typed request has a single token and provisional five-second deadline. Backend acceptance does not change observed state; a subsequent newer snapshot with the same full session identity is required. Disappearance is unknown, not proof of exit. No automatic retry or forced kill. The controller serializes lifecycle and handoff requests. A real authenticated adapter, process-control implementation, and hardware evidence are still required.

## Uncertain lifecycle recovery

A timed-out, lost or ambiguous command is quarantined: even fresh backend snapshots or foreground reconciliation alone do not prove that the old command cannot execute later. Block new lifecycle requests and foreground handoffs until the trusted adapter explicitly attests quiescence of the exact old token and target. The recorded result remains unknown, not success. Loss of known foreground ownership during a lifecycle operation also quarantines it. This is a conservative global gate in the reference model, not an implemented transport or proof of adapter quiescence.
