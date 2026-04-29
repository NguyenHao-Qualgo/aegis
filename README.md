# Aegis

Aegis is a C++20 OTA tool for A/B systems using SWUpdate-style `.swu` bundles.

This repository contains the Aegis application code for:

- bundle packing
- target-side OTA installation
- daemon mode over DBus
- CLI commands that talk to the daemon

## Start Here

For the high-level product overview, read these first:

- [docs/overview.md](docs/overview.md)
  One-page summary of how Aegis is organized, how `.swu` is built, how A/B slot selection works, and what happens on the device.
- [docs/overview.pptx](docs/overview.pptx)
  Presentation version of the same flow for walkthrough meetings.

For native build and bundle creation, use the Yocto integration repository:

- <https://github.com/uneycom/uav-yocto-build/tree/UAV-1708-develop-new-ota-tool>

## Main Modes

1. Native bundle creation
   Done from the Yocto side. Payloads, `sw-description`, signing, encryption, and final `.swu` output are described in [docs/build.md](docs/build.md).
2. Target daemon mode
   `aegis daemon` reads `/etc/skytrack/system.conf`, verifies and installs bundles, controls slot state, and resumes OTA after reboot. See [docs/target.md](docs/target.md) and [docs/ota-flow.md](docs/ota-flow.md).
3. CLI mode
   `aegis status`, `aegis install`, `aegis mark-active`, `aegis get-primary`, and `aegis get-booted` are DBus clients for the daemon. See [docs/target.md](docs/target.md).

## Documentation Map

- [docs/build.md](docs/build.md)
  Canonical guide for Yocto-side bundle creation, recipe variables, key requirements, and `.swu` output.
- [docs/overview.md](docs/overview.md)
  Canonical high-level overview of the repository, SWU generation model, A/B selection, and target-side install flow.
- [docs/target.md](docs/target.md)
  Canonical guide for daemon mode, `system.conf`, DBus API, CLI behavior, and target provisioning.
- [docs/ota-flow.md](docs/ota-flow.md)
  OTA state machine: Idle, Download, Install, Reboot, Commit, and Failure.
- [docs/partition-layout.md](docs/partition-layout.md)
  Platform partition layout notes for A/B systems.
- [docs/native.md](docs/native.md)
  Compatibility pointer to `build.md`.

## Repository Map

```text
docs/       Documentation
include/    Public headers
src/        Implementation
tests/      Unit tests
dbus/       DBus interface and policy
systemd/    systemd service unit
```

Useful source areas:

```text
src/core/        application, CLI, daemon, DBus service, state machine
src/states/      OTA state implementations
src/installer/   packer, manifest parser, streaming installer, handlers
src/bootloader/  U-Boot and NVIDIA slot-control backends
src/common/      config, logging, crypto, persistence, utilities
```

## Local Verification

For local repository verification before commit:

```bash
./build_and_run_test.sh
```

This helper is only for local validation in this source tree.
It is not the native product build or bundle creation workflow.
