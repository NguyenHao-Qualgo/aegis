# Bundle Guide

This document describes how to generate an Aegis OTA bundle, what goes into it, and what the installer expects when it consumes the bundle.

## Overview

An Aegis bundle is a `.swu` file with this outer archive layout:

1. `sw-description`
2. `sw-description.sig`
3. one or more payload files
4. `TRAILER!!!`
5. zero padding to the next 512-byte boundary

The archive is written in `cpio crc/newc` format by `src/installer/packer.cpp`.

At install time, `src/installer/installer.cpp` reads the archive sequentially:

1. `sw-description` must be the first entry.
2. `sw-description.sig` must be the second entry.
3. the manifest signature is verified with the configured public key.
4. payload entries are matched by filename against the manifest and streamed directly into the selected handler.

If the manifest or signature is missing, out of order, or invalid, installation stops before any payload is written.

## Inputs You Need

To build a bundle you need:

- the `aegis` binary with the `pack` command
- a `sw-description` manifest or template
- a private key for signing `sw-description`
- one or more payload files (encrypted payloads)

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

## Example of sw-description
```bash
software =
{
    version = "1.0.0";

    A:
    {
        images: (
        {
            filename = "core-image-base-jetson-orin-nano-devkit-nvme.rootfs.tar.gz.enc";
            type = "archive";
            path = "/";
            filesystem = "ext4";
            device = "/dev/disk/by-partlabel/ROOTFS_A";
            sha256 = "fad02ac3ff8a1f03db80f869cdcc3501cf1ed5727dba8415b67c5ed0d39d0901";
            encrypted = true;
            ivt = "94ff9c39d8420849c94c963d3f7e0696";
        }
        );
    };

    B:
    {
        images: (
        {
            filename = "core-image-base-jetson-orin-nano-devkit-nvme.rootfs.tar.gz.enc";
            type = "archive";
            path = "/";
            filesystem = "ext4";
            device = "/dev/disk/by-partlabel/ROOTFS_B";
            sha256 = "fad02ac3ff8a1f03db80f869cdcc3501cf1ed5727dba8415b67c5ed0d39d0901";
            encrypted = true;
            ivt = "94ff9c39d8420849c94c963d3f7e0696";
        }
        );
    };
}
```

See [daemon.md](daemon.md) and [cli.md](cli.md) for runtime installation paths.
