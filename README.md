# Simple C++ Layer

This folder contains the new C++ implementation for the simplified SWUpdate flow.

It is intentionally isolated from the original SWUpdate build. You can configure
and build this folder on its own with its local `CMakeLists.txt`.

- [swupdate.cpp](/home/hao-nna/swupdate/cpp/swupdate.cpp) exposes only two flows:
  `pack` to create `.swu` archives in `cpio` `crc/newc` format, and
  `install` to parse and install `.swu` packages from C++ code.
- [install_core.hpp](/home/hao-nna/swupdate/cpp/install_core.hpp) contains the
  first standalone installer core:
  streaming cpio parsing, detached `sw-description` signature verification,
  minimal `sw-description` parsing, handler dispatch, and
  AES-CBC payload decryption.
- [raw_handler.cpp](/home/hao-nna/swupdate/cpp/raw_handler.cpp) is the first
  C++ port shaped after SWUpdate's original `raw_handler.c`.
- [archive_handler.cpp](/home/hao-nna/swupdate/cpp/archive_handler.cpp) is the
  first C++ port shaped after SWUpdate's original `archive_handler.c`.
- The supported `sw-description` subset for now is the `images:` list with
  `filename`, `type`, `device`, `path`, `sha256`, `encrypted`, `ivt`,
  `preserve-attributes`, `create-destination`, and `atomic-install`.
- `sw-description.sig` is expected directly after `sw-description` in the archive.
- Runtime dependency for install is `libarchive`; signature verification and
  AES-CBC decryption are handled in-process via OpenSSL library calls.

## Standalone build

From the `cpp/` directory:

```bash
cmake -S . -B build
cmake --build build
```

The produced binary is:

```bash
./build/swupdate_cpp
```

## Quick SWU generation

Generate a test RSA keypair for this C++ flow:

```bash
chmod +x ./gen_test_keys.sh
./gen_test_keys.sh --outdir ./keys --name qemuarm64
```

This produces:

```bash
./keys/qemuarm64.key.pem
./keys/qemuarm64.public.pem
```

There is also a helper script to generate a signed `.swu` for testing:

```bash
chmod +x ./gen_swu.sh
./gen_swu.sh \
  --output update.swu \
  --swupdate ./build/swupdate_cpp \
  --type raw \
  --payload rootfs.ext4 \
  --dest /dev/mmcblk0p2 \
  --sign-key priv.pem
```

To add more payloads into the same `.swu`, use `--add-payload`:

```bash
./gen_swu.sh \
  --output update.swu \
  --swupdate ./build/swupdate_cpp \
  --type tar \
  --payload rootfs.tar.bz2 \
  --dest /tmp/swupdate-test \
  --add-payload raw rootfs.ext4 /dev/vda3 \
  --sign-key priv.pem \
  --create-destination \
  --preserve-attributes
```

For encrypted payloads, add:

```bash
  --encrypt \
  --aes-key-hex <64-hex-key> \
  --aes-iv-hex <32-hex-iv>
```
