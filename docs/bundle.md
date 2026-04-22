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
- one or more payload files
- optionally an AES key and IV if the payload should be encrypted

Useful repo files:

- `sw-description.default`
- `gen_update.sh`
- `gen_test_keys.sh`

## Generate Test Keys

For local testing, generate a private key, public key, and AES material file:

```bash
./gen_test_keys.sh
```

This creates:

```text
./test-keys/test.private.pem
./test-keys/test.public.pem
./test-keys/aes.key
```

The generated `aes.key` file contains two lines:

1. AES key in hex
2. IV in hex

## Manifest Template

The repository ships `sw-description.default` as an A/B template. It contains these placeholders:

- `__FILENAME__`
- `__ENCRYPTED__`
- `__SHA256__`
- `__IVT__`

The example template describes one install target for slot `A` and one for slot `B`, both using the same payload filename and metadata.

Common fields in each image entry:

- `filename`: payload name inside the outer archive
- `type`: `raw`, `archive`, or `tar`
- `device`: block device target for `raw` or mounted extraction target for `archive` and `tar`
- `path`: destination path for file extraction or file install
- `filesystem`: filesystem type used when mounting archive targets
- `encrypted`: whether the payload is AES-CBC encrypted
- `sha256`: expected SHA-256 for the payload stored in the archive
- `ivt`: IV override for encrypted payloads

## Quick Path: Use `gen_update.sh`

`gen_update.sh` is the easiest way to build a signed bundle from a template.

Plain bundle:

```bash
./gen_update.sh \
  --output /tmp/update.swu \
  --aegis ./build/aegis \
  --sw-description ./sw-description.default \
  --sign-key ./test-keys/test.private.pem \
  --payload ./rootfs.tar
```

Encrypted bundle:

```bash
./gen_update.sh \
  --output /tmp/update.swu \
  --aegis ./build/aegis \
  --sw-description ./sw-description.default \
  --sign-key ./test-keys/test.private.pem \
  --payload ./rootfs.tar \
  --aes-key-hex "$(sed -n '1p' test-keys/aes.key)" \
  --aes-iv-hex "$(sed -n '2p' test-keys/aes.key)"
```

What the helper does:

1. copies the template to a temporary `sw-description`
2. optionally encrypts the payload into `<payload>.enc`
3. computes the SHA-256 of the file that will actually be packed
4. fills the manifest placeholders
5. signs `sw-description` with `openssl dgst -sha256 -sign`
6. calls `aegis pack`

For encrypted bundles, the manifest hash is computed from the encrypted payload file, not the decrypted plaintext.

## Manual Path: Sign And Pack Yourself

If you already have a final `sw-description`, signature, and payloads, use `aegis pack` directly:

```bash
./build/aegis pack \
  --output update.swu \
  --sw-description sw-description \
  --sw-description-sig sw-description.sig \
  payload.bin
```

To generate the detached signature manually:

```bash
openssl dgst -sha256 -sign ./test-keys/test.private.pem sw-description > sw-description.sig
```

If you want to encrypt the payload yourself first:

```bash
openssl enc -aes-256-cbc \
  -in rootfs.tar \
  -out rootfs.tar.enc \
  -K "$(sed -n '1p' test-keys/aes.key)" \
  -iv "$(sed -n '2p' test-keys/aes.key)"
```

Then update the manifest to reference `rootfs.tar.enc`, set `encrypted = true`, fill `ivt`, compute the SHA-256 of the encrypted file, and pack the result.

## Packaging Rules

The packer enforces a few important rules:

- `--output` is required
- `--sw-description` is required
- every referenced file must be readable
- payload archive names may not be `sw-description` or `sw-description.sig`
- the final archive is padded to a 512-byte boundary

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

## Recommended Workflow

For most projects, the easiest repeatable flow is:

1. start from `sw-description.default`
2. generate local keys with `gen_test_keys.sh`
3. build `./build/aegis`
4. create the bundle with `gen_update.sh`
5. install it through the daemon or CLI

See [daemon.md](daemon.md) and [cli.md](cli.md) for runtime installation paths.
