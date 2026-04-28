# Aegis

Aegis is a C++20 OTA tool for A/B systems using SWUpdate-style `.swu` bundles.

This repository contains the Aegis application code used to:

- create OTA bundles on the build side
- run an OTA daemon on the target device
- provide a CLI client that talks to the daemon over DBus

For the full Yocto product build and integration workflow, use:

- <https://github.com/uneycom/uav-yocto-build/tree/UAV-1708-develop-new-ota-tool>

## What This Repository Covers

Aegis supports:

- signed `sw-description` manifest verification
- optional AES-CBC encrypted payloads
- streaming installation without extracting the full `.swu` first
- archive, raw, and file-style payload handling
- A/B slot selection and activation
- U-Boot and NVIDIA boot-control backends
- target daemon mode over DBus
- CLI commands for status, install, and slot control

At a high level:

1. the build side creates a `.swu` bundle
2. the target daemon verifies and installs that bundle into the inactive slot
3. the CLI acts as a thin DBus client for daemon control

## Documentation Map

Start with these documents:

- [docs/build.md](docs/build.md)
  Yocto-side bundle creation, recipe variables, signing key, public key, AES material, and how to collect the final `.swu`.
- [docs/overview.md](docs/overview.md)
  Step-by-step summary of how the target consumes a `.swu` bundle.
- [docs/target.md](docs/target.md)
  Daemon mode, systemd, `/etc/skytrack/system.conf`, DBus API, CLI commands, and target-side key placement.
- [docs/ota-flow.md](docs/ota-flow.md)
  OTA state-machine behavior: Idle, Download, Install, Reboot, Commit, and Failure.

Optional / compatibility docs:

- [docs/native.md](docs/native.md)
  Compatibility pointer to `build.md`.
- [docs/partition-layout.md](docs/partition-layout.md)
  Platform partition-layout notes.

## Recommended Reading Order For A New Teammate

1. Read this README
2. Open the Yocto integration repository:
   <https://github.com/uneycom/uav-yocto-build/tree/UAV-1708-develop-new-ota-tool>
3. Read [docs/build.md](docs/build.md)
4. Read [docs/overview.md](docs/overview.md)
5. Read [docs/target.md](docs/target.md)
6. Read [docs/ota-flow.md](docs/ota-flow.md)
7. Read [src/core/application.cpp](src/core/application.cpp)
8. Read [src/installer/installer.cpp](src/installer/installer.cpp)
9. Read [src/core/ota_state_machine.cpp](src/core/ota_state_machine.cpp)

## Typical End-To-End Workflow

### 1. Build Side

In Yocto:

- define the bundle recipe
- provide `sw-description`
- select the payload artifacts that belong in the update
- provide the signing private key and AES material
- build the bundle recipe with BitBake
- collect the generated `.swu` from `${DEPLOY_DIR_IMAGE}`

See [docs/build.md](docs/build.md) for the exact recipe structure and key requirements.

### 2. Target Preparation

On the device:

- install the public key matching the build-time private key
- install the AES material matching the build-time AES file when encrypted payloads are used
- configure those paths in `/etc/skytrack/system.conf`
- make sure the daemon is running

See [docs/target.md](docs/target.md) for the exact target-side configuration.

### 3. OTA Execution

Start the update through:

- CLI: `aegis install /path/to/update.swu`
- DBus
- higher-level integration such as GCS

The daemon then:

- verifies the manifest signature
- selects the inactive slot
- parses the matching slot block from `sw-description`
- streams payloads into the correct handler
- updates boot state
- resumes and validates the result after reboot

See [docs/overview.md](docs/overview.md) and [docs/ota-flow.md](docs/ota-flow.md).

## Common Target Commands

```bash
aegis status
aegis install /data/update.swu
aegis mark-active A
aegis get-primary
aegis get-booted
```

The CLI does not install bundles by itself. It sends requests to the daemon over DBus.

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
It is not the primary product build or native bundle creation workflow.

## Quick References

- Yocto product repo:
  <https://github.com/uneycom/uav-yocto-build/tree/UAV-1708-develop-new-ota-tool>
- Build and bundle creation:
  [docs/build.md](docs/build.md)
- Target runtime and CLI:
  [docs/target.md](docs/target.md)
- Install pipeline:
  [docs/overview.md](docs/overview.md)
- OTA state machine:
  [docs/ota-flow.md](docs/ota-flow.md)
