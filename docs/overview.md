# Aegis Installer Overview

This document explains what happens on the target when Aegis consumes an existing `.swu` bundle.

Use this guide when you want a concise, step-by-step summary of the install pipeline.

For Yocto-side bundle generation, see [build.md](build.md).
For daemon, DBus, CLI, and target configuration, see [target.md](target.md).
For OTA state-machine transitions, see [ota-flow.md](ota-flow.md).

## 1. What This Document Covers

This document focuses on bundle consumption on the target:

- how the `.swu` file is read
- what is verified before payload installation starts
- how payloads are matched to manifest entries
- how payloads are streamed into the selected handler

It does not explain the full Yocto bundle creation flow. That is covered in [build.md](build.md).

## 2. High-Level Install Pipeline

When the target installs a bundle, Aegis performs this sequence:

1. open the `.swu` bundle from a local path or another input source
2. read the first CPIO entry and require it to be `sw-description`
3. verify the CPIO checksum for `sw-description`
4. read the second CPIO entry and require it to be `sw-description.sig`
5. verify the CPIO checksum for `sw-description.sig`
6. verify the manifest signature using the configured device public key
7. determine the target slot for A/B installation
8. parse the relevant manifest entries from `sw-description`
9. load AES material when encrypted payloads are used
10. read the remaining payload entries sequentially
11. match payload entries by filename against the manifest
12. stream matched payloads into the selected installer handler
13. verify that all required manifest entries were installed

## 3. What Aegis Verifies

Before and during installation, Aegis checks:

- `sw-description` is the first archive entry
- `sw-description.sig` is the second archive entry
- the manifest signature matches the configured public key
- CPIO checksums match for manifest, signature, and payload entries
- manifest payload filenames match actual bundle entries
- payload hashes match the packed payload contents
- encrypted entries can be decrypted using the configured AES material or per-entry IV

If any required check fails, installation stops before completing the OTA.

## 4. A/B Selection During Install

For A/B systems:

- if the device is currently booted from `A`, Aegis targets `B`
- if the device is currently booted from `B`, Aegis targets `A`

The selected slot is determined before payload installation begins.
After that, only the matching slot block from `sw-description` is used.

See [build.md](build.md) for a more detailed explanation of how `sw-description` is structured for `A:` and `B:` sections.

## 5. Streaming Behavior

Aegis installs directly from the `.swu` stream.

That means:

- it does not unpack the full bundle into a temporary directory first
- it reads each CPIO entry in order
- it sends matching payload data directly to the selected handler
- encrypted payloads are decrypted while streaming
- archive payloads are extracted while the bundle is being read

This reduces temporary storage requirements on the device.

## 6. Installer Handlers

The current install path uses these handler families:

- `raw`
- `archive`
- `file`

Typical behavior:

- `raw`: writes directly to a target block device or file, optionally inflating gzip-compressed raw images while streaming
- `archive`: mounts/prepares the target and extracts archive contents through `libarchive`
- `file`: writes a payload directly to a target filesystem path

## 7. Relationship To The Other Docs

Use the docs together like this:

- [build.md](build.md): how the bundle is created in Yocto
- [overview.md](overview.md): how the bundle is consumed on the target
- [target.md](target.md): how the daemon, DBus API, CLI, and `/etc/skytrack/system.conf` are used
- [ota-flow.md](ota-flow.md): how the OTA state machine moves through Idle, Download, Install, Reboot, Commit, and Failure
