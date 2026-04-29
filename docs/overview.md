# Aegis Overview

This document is the high-level introduction to the Aegis OTA system.

Use it as the first read when you want to understand:

- why OTA updates matter on embedded targets
- which update strategy Aegis uses
- the main Aegis feature set
- how Yocto creates an Aegis `.swu`
- how `sw-description` is structured for A/B updates
- what the target daemon does during install, reboot, and commit

For native build and bundle creation, use the Yocto integration repository:

- <https://github.com/uneycom/uav-yocto-build/tree/UAV-1708-develop-new-ota-tool>

For detailed native build steps, see [build.md](build.md).
For daemon, DBus, CLI, and target configuration, see [target.md](target.md).
For OTA state-machine transitions, see [ota-flow.md](ota-flow.md).

## 1. Why OTA Updates Matter

OTA exists because embedded devices still need continuous software maintenance after deployment.

Typical reasons to update:

- bug fixes
- security patches and CVE remediation
- feature delivery
- hardware enablement and boot-flow fixes
- field recovery without reflashing the device manually

Embedded systems make update harder than desktop software:

- devices may be remote or unattended
- connectivity may be slow or unreliable
- power loss can happen during update
- the system may need to recover safely after partial failure
- products often stay in service for many years

That is why OTA design is not only about moving files. It is also about integrity, recoverability, and controlled reboot behavior.

## 2. Update Strategies

Common OTA strategies include:

### In-place / downtime update

This updates the currently installed software directly.

Pros:

- simple storage model
- smaller disk footprint

Tradeoffs:

- failure during update can leave the device unusable
- rollback is harder
- reboot safety depends heavily on each update step

### Dual-copy / fail-safe update

This keeps two software copies and updates the inactive copy first.

Pros:

- safer reboot behavior
- easier rollback path
- update and validation are easier to reason about

Tradeoffs:

- requires more storage
- needs bootloader coordination

### Aegis approach: symmetric A/B

Aegis uses a symmetric A/B model.

That means:

- `ROOTFS_A` and `ROOTFS_B` are equivalent rootfs slots
- the device boots from one slot and updates the other slot
- after installation, Aegis changes the next boot target
- after reboot, `Commit` checks whether the expected slot actually booted

In short:

1. boot from `A`
2. install into `B`
3. reboot into `B`
4. commit success if `B` really booted

If the booted slot does not match the expected slot after reboot, Aegis treats the OTA as failure.

## 3. Aegis Key Features

Aegis provides:

- bootloader interaction for NVIDIA and U-Boot style systems
- signed `sw-description` verification
- optional AES-encrypted payload support
- SHA-256 payload integrity checking
- streaming install from the `.swu` without extracting the full archive first
- installer handlers for `raw`, `archive`, and `file` payload types
- daemon mode on the target
- CLI commands that communicate with the daemon over DBus
- persisted OTA state across reboot with explicit `Reboot` and `Commit` behavior

## 4. Runtime Model

Aegis has three practical modes:

1. Native bundle creation
   The Yocto build prepares payloads, generates `sw-description`, signs it, optionally encrypts payloads, and produces the final `.swu`.
2. Target daemon mode
   `aegis daemon` reads `/etc/skytrack/system.conf`, verifies the bundle, installs to the inactive slot, persists OTA state, reboots, and resumes in `Commit`.
3. CLI mode
   The CLI is a DBus client. It requests work from the daemon and prints daemon status. It does not install payloads by itself.

## 5. Practical With Yocto: Bundle Recipe

The practical Yocto entry point is a bundle recipe like this:

```bitbake
DESCRIPTION = "Aegis SWU image"
LICENSE = "CLOSED"

inherit bundle image_types_tegra

SRC_URI += "file://sw-description"

IMAGE_DEPENDS ?= "core-image-base"

AEGIS_PRIVATE_KEY ??= ""
AEGIS_AES_FILE ??= ""

ROOTFS_FILENAME ?= "${IMAGE_DEPENDS}-${MACHINE}.rootfs.tar.gz"
ROOTFS_DEVICE_PATH ?= "/dev/disk/by-partlabel"

# first and secondloader are located here
ESP_ARCHIVE ?= "${TEGRA_ESP_IMAGE}-${MACHINE}.tar.gz"

UPDATE_IMAGES ?= "${ROOTFS_FILENAME} ${ESP_ARCHIVE}"

SWU_BASENAME = "rootfs-update"
```

What this recipe means:

- `inherit bundle image_types_tegra`
  `bundle` provides the Aegis bundle flow and `image_types_tegra` brings the Tegra-side helpers used by your platform
- `IMAGE_DEPENDS`
  names the main rootfs image dependency that must exist before the bundle is assembled
- `ESP_ARCHIVE`
  is the EFI-side archive used in your flow for ESP content and EFI applications
- `UPDATE_IMAGES`
  is the payload list that will go into the final `.swu`
- `AEGIS_PRIVATE_KEY`
  is the build-side key used to sign `sw-description`
- `AEGIS_AES_FILE`
  is the AES material used to encrypt payloads when encryption is enabled

Operationally, the bundle class helps by:

- staging payloads from `UPDATE_IMAGES`
- resolving placeholders in `sw-description`
- calculating final SHA-256 values
- encrypting payloads when `AEGIS_AES_FILE` is set
- signing `sw-description` with `AEGIS_PRIVATE_KEY`
- packing the final `.swu`

## 6. Practical With Yocto: `sw-description` Template

The Yocto-side template can look like this:

```sw-description
hw-compatibility = "@@MACHINE@@";
software =
{
    version = "1.0.0";
    A:
    {
        images: (
        {
            filename = "@@ROOTFS_FILENAME@@";
            type = "archive";
            path = "/";
            filesystem = "ext4";
            device = "@@ROOTFS_DEVICE_PATH@@/ROOTFS_A";
            sha256 = "$get_sha256(@@ROOTFS_FILENAME@@)";
        }
        );
        files: (
        {
            filename = "@@ESP_ARCHIVE@@";
            type = "archive";
            path = "/boot/efi";
            sha256 = "$get_sha256(@@ESP_ARCHIVE@@)";
        }
        );
    };

    B:
    {
        images: (
        {
            filename = "@@ROOTFS_FILENAME@@";
            type = "archive";
            path = "/";
            filesystem = "ext4";
            device = "@@ROOTFS_DEVICE_PATH@@/ROOTFS_B";
            sha256 = "$get_sha256(@@ROOTFS_FILENAME@@)";
        }
        );
        files: (
        {
            filename = "@@ESP_ARCHIVE@@";
            type = "archive";
            path = "/boot/efi";
            sha256 = "$get_sha256(@@ESP_ARCHIVE@@)";
        }
        );
    };
}
```

Important points:

- `@@MACHINE@@`, `@@ROOTFS_FILENAME@@`, `@@ROOTFS_DEVICE_PATH@@`, and `@@ESP_ARCHIVE@@` are build-time placeholders
- `$get_sha256(...)` is resolved during bundle generation
- `A` and `B` are symmetric update blocks
- the same payload names can appear in both blocks
- the main rootfs difference between `A` and `B` is the destination device
- the ESP archive is installed through the `files:` list to `/boot/efi`

When encrypted payloads are enabled, the final generated manifest can also carry fields such as:

```sw-description
encrypted = true;
ivt = "...";
```

## 7. How The Target Uses `sw-description`

`sw-description` is the manifest consumed by the target and trusted after signature verification.

It answers:

- which payload names are expected inside the `.swu`
- which entries belong to slot `A` and which belong to slot `B`
- where each payload should be written
- which hash must match before installation can succeed
- whether the payload is encrypted

`sw-description` does not choose whether the device updates `A` or `B`.

The selection happens on the target:

1. Aegis asks the boot-control backend which slot is currently booted
2. the inactive slot becomes the install target
3. Aegis parses only the matching slot block from `sw-description`
4. the `device` or `path` field tells the handler where to write

That means:

- if the device is booted from `A`, Aegis targets `B`
- if the device is booted from `B`, Aegis targets `A`

## 8. Target-Side Install, Reboot, And Commit

When the target installs a bundle, Aegis performs this sequence:

1. open the `.swu`
2. read `sw-description` as the first CPIO entry
3. read `sw-description.sig` as the second CPIO entry
4. verify the signature using the configured public key
5. select the inactive slot
6. parse the matching `A` or `B` block
7. validate `hw-compatibility` if it is present
8. stream payload entries to the correct handler
9. decrypt encrypted payloads on the fly when required
10. verify CPIO checksums and SHA-256 values
11. mark the updated slot as the next boot target
12. persist OTA state and reboot
13. resume in `Commit` after reboot
14. compare the booted slot with the expected target slot

If any verification or write step fails, installation stops and the OTA moves to failure handling.

## 9. Keys And Target Configuration

Example build-side values:

```bash
AEGIS_PRIVATE_KEY="/home/hao-nna/uav-yocto-build/ota-keys/test.private.pem"
AEGIS_PUBLIC_KEY="/home/hao-nna/uav-yocto-build/ota-keys/test.public.pem"
AEGIS_AES_FILE="/home/hao-nna/uav-yocto-build/ota-keys/aes.key"
```

Rules:

- `AEGIS_PRIVATE_KEY` is required on the build side and must never be stored on the device
- `AEGIS_PUBLIC_KEY` is the matching public key and must be stored on the device
- `AEGIS_AES_FILE` must also be stored on the device when encrypted payloads are used

Target example:

```ini
[update]
hw-compatibility=jetson-orin-nano-devkit-nvme
public-key=/etc/skytrack/public.pem
aes-key=/etc/skytrack/aes.key
bootloader-type=nvidia
data-directory=/data/aegis
log-level=4
```

## 10. Read Next

Use the docs together like this:

1. [overview.md](overview.md)
2. the Yocto integration repository
3. [build.md](build.md)
4. [target.md](target.md)
5. [ota-flow.md](ota-flow.md)
6. [partition-layout.md](partition-layout.md) when you need platform partition details
