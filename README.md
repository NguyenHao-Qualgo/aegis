# Aegis

Aegis is a C++20 OTA tool for SWUpdate-style `.swu` bundles.

This repository contains the Aegis application code used to:

- create OTA bundles on the native/Yocto side
- run an OTA daemon on the target device
- provide a CLI client that talks to the daemon over DBus

For the full Yocto product build and image integration, see:

- <https://github.com/uneycom/uav-yocto-build/tree/UAV-1708-develop-new-ota-tool>

## What Aegis Does

Aegis provides a simple OTA update flow for A/B root filesystem systems.

It supports:

- signed `sw-description` manifest verification
- AES-CBC encrypted payloads
- streaming installation without extracting the full `.swu` bundle first
- archive payload installation, such as rootfs `tar.gz`
- raw payload installation, such as compressed raw images
- A/B slot selection and activation
- U-Boot and NVIDIA boot-control backends
- DBus daemon mode on the target
- CLI commands for status, install, and slot control

## High-Level Flow

### Native / Yocto Side

On the build machine, Aegis is used to create the final `.swu` bundle.

The native flow prepares payload files, fills the `sw-description` manifest, signs the manifest, optionally encrypts payloads, and packs everything into one `.swu` file.

Typical responsibilities:

- collect payload images from Yocto deploy output
- generate or update `sw-description`
- calculate payload SHA-256 hashes
- sign `sw-description`
- encrypt payloads when AES is enabled
- create the final `.swu` archive

Read more:

- [Native Bundle Creation](docs/native.md)

### Target Side

On the target device, Aegis runs as a systemd-managed daemon.

The daemon receives install requests from GCS or from the local CLI, verifies the bundle, chooses the inactive slot, and streams the payload into the correct installer handler.

Typical responsibilities:

- load `/etc/skytrack/system.conf`
- verify `sw-description.sig` using the configured public key
- decrypt encrypted payloads when required
- stream payloads directly from the `.swu` bundle
- write the update into the inactive slot
- update boot-control state
- persist OTA state under the configured data directory
- expose status and control through DBus

Read more:

- [Target Runtime](docs/target.md)
- [OTA flow](docs/ota-flow.md)

## Runtime Commands

Common target-side commands:

```bash
aegis status
aegis install /data/update.swu
aegis mark-active A
aegis get-primary
aegis get-booted
```

The CLI does not install bundles by itself. It sends requests to the daemon over DBus.

## Target Configuration

The daemon normally reads:

```text
/etc/skytrack/system.conf
```

Example:

```ini
[update]
hw-compatibility=jetson-orin-nano-devkit-nvme
public-key=/etc/skytrack/public.pem
aes-key=/etc/skytrack/aes.key
bootloader-type=nvidia
data-directory=/data/aegis
log-level=4
```

Important files:

- `public-key`: verifies the signed `sw-description`
- `aes-key`: decrypts encrypted payloads
- `data-directory`: stores OTA state and downloaded bundles
- `bootloader-type`: selects the boot-control backend

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
src/core/        application, CLI, daemon, state machine
src/states/      OTA state implementations
src/installer/   packer, manifest parser, streaming installer, handlers
src/bootloader/  U-Boot and NVIDIA slot-control backends
src/common/      config, logging, crypto, persistence, utilities
```

## Local Verification

For local repository verification:

```bash
./build_and_run_test.sh
```

This is only for local development and tests. The main product bundle creation flow is done through Yocto.

## Documentation

- [Overview of this tool](docs/overview.md)
- [Native Bundle Creation](docs/native.md)
- [Target Runtime](docs/target.md)
- [OTA flow](docs/ota-flow.md)