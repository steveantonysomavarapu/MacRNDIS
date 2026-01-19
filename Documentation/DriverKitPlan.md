# MacRNDIS DriverKit Plan

This repository contains the initial scaffolding for a DriverKit-based USB RNDIS driver and
its companion host app. The driver is intentionally minimal and focuses on USB matching
first, with TODOs for the networking bridge and RNDIS control/data paths.

## Components

- `DriverExtension/` — DriverKit system extension (C++).
- `HostApp/` — Minimal installer app that activates the system extension using
  `SystemExtensions`.

## Next Steps

1. Enumerate USB endpoints and create control/bulk pipes.
2. Implement RNDIS control messages (INIT, SET/QUERY OIDs, KEEPALIVE).
3. Build bulk IN/OUT data paths for Ethernet frames.
4. Register an `IOUserNetworkEthernet` interface and bridge frames to macOS.
5. Handle disconnects and errors gracefully.
