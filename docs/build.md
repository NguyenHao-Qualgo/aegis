# Aegis Build Guide

This document explains the Yocto-side workflow used to build an Aegis OTA bundle.

Use this guide when you want to:

- create a `.swu` bundle from Yocto
- understand which recipe variables control the bundle contents
- understand which keys are required on the build side
- understand which keys must also be provisioned on the target

For target runtime behavior, see [target.md](target.md).
For the install pipeline on the device, see [overview.md](overview.md).
For OTA state-machine behavior, see [ota-flow.md](ota-flow.md).

## 1. Scope

In this project, native bundle creation is expected to happen in the Yocto repository, not by manually running ad-hoc host commands in this repository alone.

The normal product flow is:

1. define a Yocto bundle recipe
2. provide `sw-description`
3. choose the payload artifacts that should go into the update
4. provide the signing key and AES material
5. build the bundle recipe with BitBake
6. collect the generated `.swu` from `${DEPLOY_DIR_IMAGE}`
7. distribute the `.swu` to the target
8. make sure the target has the matching public key and AES material configured in `/etc/skytrack/system.conf`

## 2. Required Keys And Materials

Example build-side variables:

```bash
AEGIS_PRIVATE_KEY="/home/hao-nna/uav-yocto-build/ota-keys/test.private.pem"
AEGIS_PUBLIC_KEY="/home/hao-nna/uav-yocto-build/ota-keys/test.public.pem"
AEGIS_AES_FILE="/home/hao-nna/uav-yocto-build/ota-keys/aes.key"
```

What each one means:

| Variable | Used where | Purpose |
| --- | --- | --- |
| `AEGIS_PRIVATE_KEY` | build side only | Signs the final `sw-description`. |
| `AEGIS_PUBLIC_KEY` | target provisioning | Matching public key that must be installed on the device. |
| `AEGIS_AES_FILE` | build side and target | AES key/IV material used to encrypt payloads during build and decrypt them on the target. |

Important rules:

- `AEGIS_PRIVATE_KEY` is required to sign the bundle manifest.
- `AEGIS_PRIVATE_KEY` must stay on the build side only.
- `AEGIS_PUBLIC_KEY` is not a bundle-recipe control variable in the same way as `AEGIS_PRIVATE_KEY` and `AEGIS_AES_FILE`, but it is still a required deployment artifact because the target must have the matching public key installed.
- `AEGIS_PUBLIC_KEY` is the matching public key that should be stored on the target device.
- `AEGIS_AES_FILE` is required when encrypted payloads are used.
- the same AES material used at build time must also be present on the target.

In short:

- private key stays on the build machine
- public key goes to the device
- AES file used for encrypted payloads must also exist on the device

## 3. Example Yocto Bundle Recipe

Example recipe:

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

This recipe tells the bundle flow to:

- stage the payloads listed in `UPDATE_IMAGES`
- encrypt those payloads when AES is enabled
- generate the final `sw-description`
- sign that manifest with `AEGIS_PRIVATE_KEY`
- pack the final `.swu`

## 4. Important Recipe Variables

### `inherit bundle image_types_tegra`

- `bundle` provides the SWU packaging flow
- `image_types_tegra` provides Tegra-specific helpers used by the platform build

If the platform does not need Tegra-specific helpers, `inherit bundle` is the part that matters for the Aegis flow.

### `SRC_URI += "file://sw-description"`

- includes the `sw-description` template in the recipe
- this file is the manifest source for the final bundle

### `IMAGE_DEPENDS ?= "core-image-base"`

`IMAGE_DEPENDS` names the main image dependency that should already exist before the bundle is assembled.

In most cases, this is the root filesystem image that becomes the main OTA payload.

In the example:

- `IMAGE_DEPENDS` is `core-image-base`
- `ROOTFS_FILENAME` is derived from it
- the build expects a matching rootfs archive in `${DEPLOY_DIR_IMAGE}`

### `ESP_ARCHIVE ?= "${TEGRA_ESP_IMAGE}-${MACHINE}.tar.gz"`

`ESP_ARCHIVE` names the ESP payload archive.

In your platform flow, this archive carries the EFI-side content, including the EFI applications used in the boot path.

That is why this archive matters separately from the root filesystem payload.

### `UPDATE_IMAGES ?= "${ROOTFS_FILENAME} ${ESP_ARCHIVE}"`

`UPDATE_IMAGES` is the list of payload artifacts that will be packaged into the final SWU.

In the example:

- `ROOTFS_FILENAME` provides the root filesystem update payload
- `ESP_ARCHIVE` provides the EFI / ESP-side payload
- both are packed into the same `.swu`

The bundle flow stages every file listed in `UPDATE_IMAGES`, then:

1. encrypts it if configured
2. calculates the final hash
3. injects the correct metadata into `sw-description`
4. packs it into the final bundle

### `AEGIS_PRIVATE_KEY`

This is the signing key used to create `sw-description.sig`.

The target never needs this file.

### `AEGIS_AES_FILE`

This is the AES material file used to encrypt payloads listed in `UPDATE_IMAGES`.

If encryption is enabled:

- the build uses this file to encrypt payloads
- the device must have matching AES material to decrypt those payloads during installation

### `ROOTFS_FILENAME`

This names the root filesystem archive that will be included in the update.

Example:

```bitbake
ROOTFS_FILENAME ?= "${IMAGE_DEPENDS}-${MACHINE}.rootfs.tar.gz"
```

This value must match the artifact that actually exists in `${DEPLOY_DIR_IMAGE}`.

### `ROOTFS_DEVICE_PATH`

This is the base path used by the `sw-description` template when generating target device names for slot `A` and slot `B`.

Example:

```bitbake
ROOTFS_DEVICE_PATH ?= "/dev/disk/by-partlabel"
```

That is typically used to produce entries such as:

```text
/dev/disk/by-partlabel/ROOTFS_A
/dev/disk/by-partlabel/ROOTFS_B
```

### `SWU_BASENAME`

This controls the intended base name of the generated SWU artifact.

Example:

```bitbake
SWU_BASENAME = "rootfs-update"
```

## 5. What The Build Actually Does

When you run:

```bash
bitbake image-bundle
```

the bundle flow uses:

- `AEGIS_PRIVATE_KEY` to sign `sw-description`
- `AEGIS_AES_FILE` to encrypt the payloads listed in `UPDATE_IMAGES`

Operationally, the build does this:

1. stage payload files from `${DEPLOY_DIR_IMAGE}`
2. encrypt each payload in `UPDATE_IMAGES` when AES is enabled
3. calculate SHA-256 using the final staged payload artifact
4. inject resolved values into `sw-description`
5. sign the final `sw-description`
6. run `aegis-native pack` to create the `.swu`

This means:

- `sw-description` is signed after it already contains the final payload names and hashes
- the hash in `sw-description` must match the actual packed payload bytes
- if encryption is enabled, the hash must match the encrypted payload, not the original plaintext input

## 6. Build Command

Build the bundle with:

```bash
bitbake image-bundle
```

You can replace `image-bundle` with the actual bundle recipe name if your layer uses a different recipe.

## 7. Collect The Output

After the build completes, get the generated `.swu` from the Yocto deploy directory.

In Yocto terms, the output lives under:

```text
${DEPLOY_DIR_IMAGE}
```

In a typical build tree, that is usually:

```text
tmp/deploy/images/<machine>/
```

Look for the generated `.swu` artifact there and distribute that file to the target.

## 8. Provision The Target

Before starting OTA on the device, make sure the target has:

- the public key matching `AEGIS_PRIVATE_KEY`
- the AES material matching `AEGIS_AES_FILE` when encrypted payloads are used

That means:

- `AEGIS_PUBLIC_KEY` should be stored on the device
- `AEGIS_AES_FILE` should also be stored on the device when encryption is enabled

The private key must never be copied to the target.

## 9. Target Configuration

The daemon reads the key locations from:

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

In this configuration:

- `public-key` should point to the deployed copy of `AEGIS_PUBLIC_KEY`
- `aes-key` should point to the deployed copy of `AEGIS_AES_FILE`

Match rules:

- the target `public-key` must match the build-time `AEGIS_PRIVATE_KEY`
- the target `aes-key` must match the build-time `AEGIS_AES_FILE`

If these do not match:

- signature verification fails when the public key is wrong
- payload decryption fails when the AES material is wrong

## 10. Practical Step-By-Step Workflow

1. prepare the Yocto bundle recipe
2. include `sw-description`
3. set `IMAGE_DEPENDS`
4. set `ESP_ARCHIVE` for the EFI-side payloads
5. set `UPDATE_IMAGES` to the payloads that should go into the bundle
6. set `AEGIS_PRIVATE_KEY`
7. set `AEGIS_AES_FILE` when encryption is enabled
8. run `bitbake image-bundle`
9. collect the `.swu` from `${DEPLOY_DIR_IMAGE}`
10. install the matching public key on the target
11. install the matching AES material on the target when encryption is enabled
12. make sure `/etc/skytrack/system.conf` points to those files
13. distribute the `.swu` to the target and start OTA

## 11. Related Documents

- [overview.md](overview.md)
- [target.md](target.md)
- [ota-flow.md](ota-flow.md)
