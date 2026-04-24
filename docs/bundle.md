# Bundle Guide

This document describes the Yocto-based flow used to generate an Aegis OTA bundle, how `sw-description` is signed, how encrypted payloads are prepared, and what the installer checks while consuming the bundle.

## Bundle Layout

An Aegis bundle is a `.swu` file with this outer archive layout:

1. `sw-description`
2. `sw-description.sig`
3. one or more payload files
4. `TRAILER!!!`
5. zero padding to the next 512-byte boundary

The archive is written in `cpio crc/newc` format by `src/installer/packer.cpp`.

ASCII view of the outer archive:

```text
+--------------------------------------------------+
| cpio entry: sw-description                       |
|   header: newc/crc ("070702")                    |
|   name:   "sw-description\0"                     |
|   pad:    to 4-byte boundary                     |
|   data:   sw-description bytes                   |
|   pad:    to 4-byte boundary                     |
+--------------------------------------------------+
| cpio entry: sw-description.sig                   |
|   header: newc/crc ("070702")                    |
|   name:   "sw-description.sig\0"                 |
|   pad:    to 4-byte boundary                     |
|   data:   signature bytes                        |
|   pad:    to 4-byte boundary                     |
+--------------------------------------------------+
| cpio entry: <payload-1 basename>                 |
|   header: newc/crc ("070702")                    |
|   name:   "<payload-1 basename>\0"               |
|   pad:    to 4-byte boundary                     |
|   data:   payload bytes                          |
|   pad:    to 4-byte boundary                     |
+--------------------------------------------------+
| cpio entry: <payload-2 basename>                 |
|   header: newc/crc ("070702")                    |
|   name:   "<payload-2 basename>\0"               |
|   pad:    to 4-byte boundary                     |
|   data:   payload bytes                          |
|   pad:    to 4-byte boundary                     |
+--------------------------------------------------+
| cpio entry: TRAILER!!!                           |
|   header: newc/crc ("070702")                    |
|   name:   "TRAILER!!!\0"                         |
|   pad:    to 4-byte boundary                     |
|   no file data                                   |
+--------------------------------------------------+
| zero padding to next 512-byte archive boundary   |
+--------------------------------------------------+
```

Notes:

- payload archive names are written as basenames, not full source paths
- every CPIO entry is padded to a 4-byte boundary
- after `TRAILER!!!`, the whole archive is padded to a 512-byte boundary

At install time, `src/installer/installer.cpp` reads the archive sequentially:

1. `sw-description` must be the first entry
2. `sw-description.sig` must be the second entry
3. the signature is verified with the configured public key
4. manifest entries are matched by filename against later payload entries
5. matching payloads are streamed directly into the selected handler

If `sw-description` or `sw-description.sig` is missing, out of order, or invalid, installation stops before payload handling begins.

Signed bundles are required by the installer. `aegis pack` can technically write a bundle without `sw-description.sig`, but that bundle will be rejected at install time.

## Streaming Install

Aegis installs directly from the `.swu` stream.

- it does not unpack the whole bundle into a temporary directory first
- it reads each CPIO entry in order and sends payload bytes directly to the selected handler
- for encrypted payloads, decryption happens in-process while streaming
- for archive payloads, extraction happens while reading the bundle, not after expanding the full package

## Target-Side Requirements

The installer verifies the manifest signature with the public key configured on the device:

```ini
[update]
public-key=/etc/skytrack/public.pem
aes-key=/etc/skytrack/aes.key
bootloader-type=nvidia
data-directory=/data/aegis
```

Notes:

- `public-key` may point to a PEM public key or PEM certificate.
- `aes-key` is optional and is only required when the manifest marks a payload as encrypted.
- The installer reads the outer archive sequentially and does not unpack the full bundle first.

## AES Material Format

The `aes-key` file is read as two whitespace-separated hex strings:

```text
00112233445566778899aabbccddeeff00112233445566778899aabbccddeeff
aabbccddeeff00112233445566778899
```

Meaning:

- first token: AES key in hex
- second token: IV in hex

The recommended layout is one token per line, as shown above. The current parser accepts spaces or newlines between the two values.

A Yocto AES file for this flow typically contains a 32-byte key and a 16-byte IV for AES-256-CBC encryption. At install time, Aegis selects the AES-CBC variant from the key length in this file.

`openssl enc -P` output is not directly compatible with Aegis because it includes labels such as `key=` and `iv =`. If you want a directly usable file, write only the bare hex values.

## Yocto Build Flow

The main bundle-generation flow is:

1. `sw-description` is written as a template in the layer
2. the Yocto class stages the payload from `${DEPLOY_DIR_IMAGE}`
3. the class encrypts the payload, resolves `sha256`, and signs `sw-description`
4. the class runs `aegis pack` to generate the final `.swu`

Build it with:

```bash
bitbake <bundle-recipe>
```

In the current class:

- `AEGIS_PRIVATE_KEY`, `AEGIS_AES_FILE`, `AEGIS_SW_DESCRIPTION`, and `IMAGE_DEPENDS` must be set
- payloads are encrypted before packing
- `sw-description.sig` is generated with OpenSSL
- `aegis-native pack` produces the final bundle

Encryption step used by the class:

```bash
openssl enc -aes-256-cbc -in <src> -out <dst> -K <key-hex> -iv <iv-hex>
```

Signing step used by the class:

```bash
openssl dgst -sha256 -sign <private.pem> -out ${SWU_DIR}/sw-description.sig ${SWU_DIR}/sw-description
```

The class encrypts the staged payload first, then resolves `sha256` from the staged file, then signs the final `sw-description`.

## `sw-description` Template

A typical template looks like this:

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

In this template:

- `@@...@@` values come from BitBake variables
- `$get_sha256(...)` is resolved during the bundle build
- when encryption is enabled, the class rewrites `filename` to `<name>.enc` and injects `encrypted` and `ivt`

## Supported Payload Forms

Common payload forms supported by the current installer:

- `type = "archive"` for streamed archive extraction through `libarchive`
- `type = "raw"` with `compress = "zlib"` for gzip-compssered raw images

Archive example:

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

The archive handler reads the payload as a stream and `libarchive` handles compressed tar formats such as `tar.gz` and `tar.bz2`.

Compressed raw example:

```text
{
    filename = "rootfs.ext4.gz";
    type = "raw";
    compress = "zlib";
    device = "/dev/disk/by-partlabel/ROOTFS_A";
    sha256 = "...";
}
```

In this form, Aegis inflates the gzip payload while streaming it to the target device, instead of unpacking the whole bundle first.

## Bundle Recipe Example

A minimal bundle recipe looks like this:

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

This recipe selects:

- the manifest template
- the image artifact to bundle
- the signing key
- the AES file
- the output name

## Install-Time Checks

At install time, Aegis checks:

- `sw-description` is the first archive entry
- `sw-description.sig` is the second archive entry
- the signature matches the configured public key
- `filename` matches a payload inside the bundle
- `sha256` matches the packed payload contents

For encrypted entries, Aegis decrypts the payload while streaming it to the handler.

See [installation.md](installation.md) for the step-by-step install flow, [daemon.md](daemon.md) for the service path, and [cli.md](cli.md) for the client path.
