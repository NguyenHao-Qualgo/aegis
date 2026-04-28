# Aegis Native Bundle Creation Guide

This document describes the native-side Aegis workflow used to create OTA `.swu` bundles from the Yocto build environment.

Aegis has two main operating modes:

1. **Native mode**: runs in the Yocto/native build environment and creates signed, optionally encrypted `.swu` bundles.
2. **Target mode**: runs on the target device as a daemon and CLI client for installing bundles.

This guide focuses only on **native mode**. For target-side daemon, DBus, systemd, and CLI usage, see `target.md`.

## 1. Native Mode Overview

Native mode is responsible for producing the final OTA artifact that will be copied or downloaded to the target.

In the product workflow, bundle creation is expected to happen from the Yocto repository. The normal flow is:

1. Create a Yocto bundle recipe.
2. Provide a `sw-description` template.
3. Select the payload images that should be included in the update.
4. Provide the signing private key.
5. Provide the AES material if payload encryption is enabled.
6. Build the bundle recipe with BitBake.
7. Collect the generated `.swu` from the Yocto deploy directory.
8. Copy, upload, or publish the `.swu` for target installation.

The target must have the matching public key and AES material configured in `/etc/skytrack/system.conf` before it can install the generated bundle.

## 2. Inputs Required By Native Bundle Creation

A complete bundle build needs these inputs:

| Input | Purpose |
| --- | --- |
| `sw-description` | Manifest template that describes the payloads, slots, handlers, devices, hashes, and encryption flags. |
| Payload images | Rootfs archive, raw image, ESP archive, or other update payloads produced by Yocto. |
| `AEGIS_PRIVATE_KEY` | Private key used to sign the final `sw-description`. This stays on the build machine only. |
| `AEGIS_AES_FILE` | AES key and IV material used to encrypt payloads when encryption is enabled. |
| Bundle recipe | BitBake recipe that connects all inputs and calls the bundle packaging flow. |
| `aegis-native` | Native Aegis tool used by the Yocto class to pack the final `.swu`. |

## 3. Example Yocto Bundle Recipe

Example bundle recipe:

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

# First and second loader are located here.
ESP_ARCHIVE ?= "${TEGRA_ESP_IMAGE}-${MACHINE}.tar.gz"

UPDATE_IMAGES ?= "${ROOTFS_FILENAME} ${ESP_ARCHIVE}"

SWU_BASENAME = "rootfs-update"
```

This recipe creates one `.swu` bundle that contains all files listed in `UPDATE_IMAGES`. The bundle flow stages those payloads, encrypts them when configured, injects final hashes into `sw-description`, signs the final manifest, then packs the final `.swu`.

## 4. Important Recipe Variables

### `inherit bundle image_types_tegra`

- `bundle` provides the Aegis/SWU packaging flow.
- `image_types_tegra` provides Tegra-specific image helpers used by the platform build.

Use only `inherit bundle` for platforms that do not need Tegra-specific helpers.

### `SRC_URI += "file://sw-description"`

Includes the `sw-description` template in the recipe. This file is the source manifest used to generate the final signed manifest inside the bundle.

### `IMAGE_DEPENDS ?= "core-image-base"`

Defines the image dependency that should already be built before the bundle is assembled. This is usually the root filesystem image.

### `AEGIS_PRIVATE_KEY ??= ""`

Path to the private key used to sign `sw-description`.

Important rules:

- The private key is used only during bundle creation.
- The private key must not be installed on the target.
- The target needs only the matching public key or public certificate.

### `AEGIS_AES_FILE ??= ""`

Path to the AES material used to encrypt payloads.

Important rules:

- The same AES material must be available on the target if encrypted payloads are used.
- The AES file should contain only bare hex values, not OpenSSL labels such as `key=` or `iv =`.

### `ROOTFS_FILENAME`

Names the root filesystem artifact that will be included in the bundle.

Examples:

```bitbake
ROOTFS_FILENAME ?= "${IMAGE_DEPENDS}-${MACHINE}.rootfs.tar.gz"
ROOTFS_FILENAME ?= "${IMAGE_DEPENDS}-${MACHINE}.rootfs.ext4.gz"
```

The value must match the artifact produced in `${DEPLOY_DIR_IMAGE}`.

### `ROOTFS_DEVICE_PATH`

Base path used by the `sw-description` template to build target device names.

Example:

```bitbake
ROOTFS_DEVICE_PATH ?= "/dev/disk/by-partlabel"
```

This is normally used to produce paths such as:

```text
/dev/disk/by-partlabel/ROOTFS_A
/dev/disk/by-partlabel/ROOTFS_B
```

### `ESP_ARCHIVE`

Names the ESP payload archive. In the Tegra flow, this archive can carry EFI-side applications and boot-side content used by the device boot path.

Example:

```bitbake
ESP_ARCHIVE ?= "${TEGRA_ESP_IMAGE}-${MACHINE}.tar.gz"
```

### `UPDATE_IMAGES`

Lists all payload files that should be included in the `.swu`.

Example:

```bitbake
UPDATE_IMAGES ?= "${ROOTFS_FILENAME} ${ESP_ARCHIVE}"
```

The bundle class stages these payloads, optionally encrypts them, calculates hashes, and passes them to `aegis-native pack`.

### `SWU_BASENAME`

Controls the output bundle naming intent.

Example:

```bitbake
SWU_BASENAME = "rootfs-update"
```

The final filename also depends on the surrounding Yocto class and deploy naming rules.

## 5. Bundle Layout

An Aegis `.swu` bundle uses a CPIO `crc/newc` outer archive layout:

```text
sw-description
sw-description.sig
<payload-1>
<payload-2>
TRAILER!!!
<zero padding to 512-byte boundary>
```

Rules:

- `sw-description` must be the first entry.
- `sw-description.sig` must be the second entry.
- Payload entries are stored by basename, not full source path.
- Every CPIO entry is padded to a 4-byte boundary.
- After `TRAILER!!!`, the archive is padded to a 512-byte boundary.

At install time, the target reads the archive sequentially and streams matching payloads directly into the selected handler. It does not unpack the full `.swu` into a temporary directory first.

## 6. Signing And Encryption Order

The bundle generation flow performs these steps:

1. Stage payload files from `${DEPLOY_DIR_IMAGE}`.
2. Encrypt each payload listed in `UPDATE_IMAGES` when AES is enabled.
3. Calculate SHA-256 from the final staged payload artifact.
4. Inject resolved filenames, hashes, encryption flags, and IV values into `sw-description`.
5. Sign the final `sw-description`.
6. Run `aegis-native pack` to create the final `.swu`.

This order is important:

- The signed manifest must contain the final payload names and hashes.
- The payload hash must match the bytes stored in the `.swu`.
- If encryption is enabled, the hash must match the encrypted payload, not the original plaintext input.

Encryption command used by the class:

```bash
openssl enc -aes-256-cbc -in <src> -out <dst> -K <key-hex> -iv <iv-hex>
```

Signing command used by the class:

```bash
openssl dgst -sha256 -sign <private.pem> -out ${SWU_DIR}/sw-description.sig ${SWU_DIR}/sw-description
```

## 7. AES Material File Format

The AES file must contain two whitespace-separated hex values:

```text
00112233445566778899aabbccddeeff00112233445566778899aabbccddeeff
aabbccddeeff00112233445566778899
```

Meaning:

- First token: AES key in hex.
- Second token: IV in hex.

Recommended format is one token per line.

For AES-256-CBC, use:

- 32-byte key: 64 hex characters.
- 16-byte IV: 32 hex characters.

Do not use raw `openssl enc -P` output directly because it includes labels such as `key=` and `iv =`. Aegis expects only the bare hex values.

## 8. `sw-description` Template

`sw-description` is the manifest for the bundle. It tells Aegis:

- which payload files are expected in the `.swu`
- which handler should process each payload
- which target device or path each payload applies to
- whether a payload is encrypted
- which SHA-256 hash each payload must match
- which slot-specific entries apply to the current install target

Example A/B archive payload template:

```text
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
    };
}
```

Template behavior:

- `@@...@@` values are replaced using BitBake variables.
- `$get_sha256(...)` is resolved during bundle creation.
- When encryption is enabled, the class rewrites payload filenames to `<name>.enc` and injects `encrypted` and `ivt` fields.

## 9. A/B Slot Behavior

Aegis uses an A/B update model:

- If the device is currently booted from slot `A`, the update is written to slot `B`.
- If the device is currently booted from slot `B`, the update is written to slot `A`.

The manifest parser does not choose the target slot by itself. The install flow chooses the inactive target slot first, then passes that slot name into manifest parsing.

Implementation responsibility split:

| Component | Responsibility |
| --- | --- |
| Boot-control backend | Reads the booted slot and selects the inactive target slot. |
| Manifest parser | Parses only the matching `A` or `B` block. |
| Handler | Writes the selected payload to the device/path from the chosen slot block. |

Example:

```text
software =
{
    A: {
        images: (
            { filename = "rootfs-a.ext4"; type = "raw"; device = "/dev/sda1"; }
        );
    };

    B: {
        images: (
            { filename = "rootfs-b.ext4"; type = "raw"; device = "/dev/sdb1"; }
        );
    };
}
```

Behavior:

- `parse_manifest(..., "A")` returns only entries from the `A` block.
- `parse_manifest(..., "B")` returns only entries from the `B` block.

## 10. Supported Payload Forms

### Archive Payload

Use `type = "archive"` when the payload is a filesystem archive that should be extracted into the target filesystem.

Example:

```text
{
    filename = "rootfs.tar.gz";
    type = "archive";
    path = "/";
    filesystem = "ext4";
    device = "/dev/disk/by-partlabel/ROOTFS_A";
    sha256 = "...";
}
```

The archive handler reads the payload as a stream. `libarchive` handles compressed tar formats such as `tar.gz` and `tar.bz2`.

### Compressed Raw Payload

Use `type = "raw"` with `compress = "zlib"` when the payload is a gzip-compressed raw image.

Example:

```text
{
    filename = "rootfs.ext4.gz";
    type = "raw";
    compress = "zlib";
    device = "/dev/disk/by-partlabel/ROOTFS_A";
    sha256 = "...";
}
```

Aegis inflates the gzip stream while writing to the target device. It does not unpack the full bundle first.

## 11. Build Command

Build the bundle recipe with:

```bash
bitbake image-bundle
```

Or use your actual bundle recipe name:

```bash
bitbake <bundle-recipe>
```

After the build completes, collect the `.swu` from:

```text
${DEPLOY_DIR_IMAGE}
```

In a normal Yocto build tree, this is usually:

```text
tmp/deploy/images/<machine>/
```

## 12. Deploy Handoff To Target

After creating the `.swu`:

1. Copy or publish the `.swu` for the target device.
2. Confirm the target has the matching public key.
3. Confirm the target has matching AES material if encrypted payloads are used.
4. Start the install through the target daemon, CLI, GCS integration, or DBus API.

The target configuration usually looks like this:

```ini
[update]
hw-compatibility=jetson-orin-nano-devkit-nvme
public-key=/etc/skytrack/public.pem
aes-key=/etc/skytrack/aes.key
bootloader-type=nvidia
data-directory=/data/aegis
log-level=4
```

Rules:

- `public-key` must match the private key used by `AEGIS_PRIVATE_KEY`.
- `aes-key` must match the AES material used by `AEGIS_AES_FILE`.
- The private key must never be copied to the target.

## 13. Install-Time Checks Performed On Target

When the target installs a bundle, Aegis checks:

1. `sw-description` is the first CPIO entry.
2. `sw-description.sig` is the second CPIO entry.
3. The signature verifies against the configured public key.
4. Each manifest `filename` matches a payload entry in the bundle.
5. Each payload SHA-256 matches the packed payload bytes.
6. Encrypted entries can be decrypted using the configured AES material or per-entry `ivt`.

If any required check fails, installation stops before writing the affected payload.

## 14. Practical Native Checklist

Before building:

- [ ] The rootfs or payload images exist in `${DEPLOY_DIR_IMAGE}`.
- [ ] The bundle recipe inherits the correct bundle class.
- [ ] `SRC_URI` includes `sw-description`.
- [ ] `UPDATE_IMAGES` lists every payload that should be packed.
- [ ] `AEGIS_PRIVATE_KEY` points to the signing private key.
- [ ] `AEGIS_AES_FILE` points to a valid AES material file when encryption is enabled.
- [ ] The `sw-description` template uses the correct device paths for slot `A` and slot `B`.
- [ ] The target has the matching public key and AES material.

Build:

```bash
bitbake <bundle-recipe>
```

Find output:

```bash
ls tmp/deploy/images/<machine>/*.swu
```

## 15. Troubleshooting

### Bundle builds but target rejects signature

Check that:

- `AEGIS_PRIVATE_KEY` was used to sign the bundle.
- The target `public-key` matches that private key.
- `sw-description` was not modified after signing.

### Payload hash mismatch

Check that:

- The SHA-256 was calculated after encryption if encryption is enabled.
- The payload filename in `sw-description` matches the actual packed filename.
- The `.swu` was not modified or corrupted after generation.

### Payload decryption fails

Check that:

- The target `aes-key` matches the build-time `AEGIS_AES_FILE`.
- The AES file contains only bare hex key and IV values.
- The key length and IV length are valid for AES-CBC.
- If `ivt` is present in the manifest, it is valid and expected.

### Payload file not found during packing

Check that:

- The payload exists in `${DEPLOY_DIR_IMAGE}`.
- `ROOTFS_FILENAME`, `ESP_ARCHIVE`, and `UPDATE_IMAGES` match the actual filenames.
- The image dependency was built before the bundle recipe.

## 16. Minimal Example For Rootfs-Only Bundle

```bitbake
DESCRIPTION = "OTA bundle image"
LICENSE = "CLOSED"

inherit bundle

SRC_URI += "file://sw-description"

IMAGE_DEPENDS ?= "core-image-base"

AEGIS_PRIVATE_KEY ??= ""
AEGIS_AES_FILE ??= ""

ROOTFS_FILENAME ?= "${IMAGE_DEPENDS}-${MACHINE}.rootfs.ext4.gz"
ROOTFS_DEVICE_PATH ?= "/dev/disk/by-partlabel"
UPDATE_IMAGES ?= "${ROOTFS_FILENAME}"

SWU_BASENAME = "rootfs-update"
```

Use this when the update contains only a compressed raw rootfs image.