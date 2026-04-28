# Overview Aegis tool

This document describes how Aegis installs an existing `.swu` bundle. For bundle generation, signing, and encryption setup, see [native.md](native.md).

## Overview

Aegis installs directly from the bundle stream.

- it does not unpack the whole `.swu` into a temporary directory first
- it verifies `sw-description` before payload installation starts
- it streams payload bytes directly to the selected installer handler
- encrypted payloads are decrypted while streaming

## Step By Step

1. Open the `.swu` bundle from a file path or `stdin`.
2. Read the first CPIO entry and require it to be `sw-description`.
3. Verify the CPIO checksum for `sw-description`.
4. Read the second CPIO entry and require it to be `sw-description.sig`.
5. Verify the CPIO checksum for `sw-description.sig`.
6. Verify the signature of `sw-description` with the configured public key.
7. Parse the manifest entries from `sw-description`.
8. Load AES material from `aes-key` if encrypted payloads are used.
9. Read the remaining CPIO entries one by one until `TRAILER!!!`.
10. If an entry is not referenced by the manifest, skip it but still verify its CPIO checksum.
11. If an entry matches the manifest, dispatch it to the installer handler for that entry type.
12. After `TRAILER!!!`, verify that every required manifest entry was installed.

## Handler Behavior

The current install path dispatches payloads to `raw` and `archive` handlers.

`raw` handler:

- opens the target device directly
- streams payload bytes straight to the device
- if `compress = "zlib"`, inflates the gzip payload while writing
- if `encrypted = true`, decrypts the payload while streaming
- calls `fsync()` before finishing

`archive` handler:

- prepares and mounts the target filesystem
- creates a FIFO for streamed extraction
- feeds payload bytes into `libarchive`
- extracts archive contents directly into the mounted target path
- supports compressed archive payloads such as `tar.gz` and `tar.bz2`
- if `encrypted = true`, decrypts the payload before extraction

## What Is Verified

During installation, Aegis checks:

- `sw-description` is the first archive entry
- `sw-description.sig` is the second archive entry
- the signature matches the configured public key
- CPIO checksums match for manifest, signature, payload entries, and skipped entries
- `sha256` matches the packed payload contents
- all required manifest entries are present in the bundle

## Streaming Notes

- Raw images are written directly to the target device while the bundle is being read.
- Archive payloads are extracted while the bundle is being read.
- Encrypted payloads are decrypted in-process while streaming.
- The outer `.swu` archive is consumed sequentially from start to finish.

See [native.md](native.md) for bundle generation, [target.md](target.md) for the DBus service flow, and cli commands.
